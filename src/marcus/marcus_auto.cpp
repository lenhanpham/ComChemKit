#include "marcus/marcus_auto.h"
#include "marcus/marcus_calculator.h"
#include "marcus/marcus_input_gen.h"
#include "marcus/marcus_io.h"
#include "utilities/metadata.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <csignal>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <thread>
#include <utility>
#include <vector>
#include <ctime>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern std::atomic<bool> g_shutdown_requested;
extern volatile std::sig_atomic_t g_shutdown_signal;

namespace Marcus {
namespace {
namespace fs = std::filesystem;

int close_fd(int fd) {
#ifdef _WIN32
    return _close(fd);
#else
    return ::close(fd);
#endif
}

int create_exclusive(const fs::path& path) {
#ifdef _WIN32
    return _open(path.string().c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY | _O_NOINHERIT,
                 _S_IREAD | _S_IWRITE);
#else
    int flags = O_CREAT | O_EXCL | O_WRONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    return ::open(path.string().c_str(), flags, 0666);
#endif
}

int open_input(const fs::path& path) {
#ifdef _WIN32
    return _open(path.string().c_str(), _O_RDONLY | _O_BINARY | _O_NOINHERIT);
#else
#ifdef O_CLOEXEC
    return ::open(path.string().c_str(), O_RDONLY | O_CLOEXEC);
#else
    return ::open(path.string().c_str(), O_RDONLY);
#endif
#endif
}

// A stream over the exclusively created file descriptor. Writes are immediate;
// flushes are still checked so a full filesystem stops the workflow.
class DescriptorBuffer : public std::streambuf {
public:
    explicit DescriptorBuffer(int fd) : fd_(fd) {}
    ~DescriptorBuffer() override { if (fd_ >= 0) close_fd(fd_); }
    bool close() {
        if (fd_ < 0) return true;
        int result = close_fd(fd_);
        fd_ = -1;
        return result == 0;
    }
protected:
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        std::streamsize written = 0;
        while (written < count) {
            auto remaining = count - written;
#ifdef _WIN32
            int n = _write(fd_, data + written, static_cast<unsigned int>(std::min<std::streamsize>(remaining, INT_MAX)));
#else
            ssize_t n = ::write(fd_, data + written, static_cast<size_t>(remaining));
#endif
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;
            written += n;
        }
        return written;
    }
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
        char c = traits_type::to_char_type(ch);
        return xsputn(&c, 1) == 1 ? ch : traits_type::eof();
    }
    int sync() override { return 0; }
private:
    int fd_;
};

// Directory creation is atomic and works on both supported platforms. Never
// remove a lock we did not create, or recursively remove a lock's contents.
class DirectoryLock {
public:
    bool acquire(const fs::path& path, std::string& error) {
        std::error_code ec;
        if (!fs::create_directory(path, ec)) {
            error = "Cannot acquire lock " + path.string() +
                    (ec ? ": " + ec.message() : ": already exists");
            return false;
        }
        path_ = path;
        owned_ = true;
        return true;
    }
    ~DirectoryLock() {
        if (owned_) {
            std::error_code ec;
            fs::remove(path_, ec);
        }
    }
private:
    fs::path path_;
    bool owned_ = false;
};

int interruption_status() {
    return g_shutdown_signal == SIGTERM ? 143 : 130;
}

bool safe_component(const std::string& value) {
    return !value.empty() && value != "." && value != ".." &&
           value.find('/') == std::string::npos && value.find('\\') == std::string::npos;
}

bool directory_empty_or_absent(const fs::path& path, std::string& error) {
    std::error_code ec;
    auto st = fs::symlink_status(path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        error = "Cannot inspect " + path.string() + ": " + ec.message();
        return false;
    }
    if (!fs::exists(st)) return true;
    if (fs::is_symlink(st) || !fs::is_directory(st)) {
        error = "Workflow path is not a real directory: " + path.string();
        return false;
    }
    bool empty = fs::is_empty(path, ec);
    if (ec || !empty) {
        error = "Automatic mode will not overwrite existing workflow files in " + path.string();
        if (ec) error += ": " + ec.message();
        return false;
    }
    return true;
}

// Keep lock directories hidden so users do not mistake them for inputs or
// outputs. The lock stays next to its target file, with a dot-prefixed name.
fs::path hidden_lock_path(const fs::path& target) {
    fs::path directory = target.has_parent_path() ? target.parent_path() : fs::path(".");
    std::string filename = target.filename().string();
    if (filename.empty()) filename = target.string();
    return directory / ("." + filename + ".marcus.lock");
}

bool inside_directory(const fs::path& path, const fs::path& directory) {
    auto normalize = [](const fs::path& value) {
        std::error_code ec;
        fs::path normalized = fs::weakly_canonical(value, ec);
        return ec ? fs::absolute(value).lexically_normal() : normalized;
    };
    auto p = normalize(path);
    auto d = normalize(directory);
    auto pi = p.begin();
    for (auto di = d.begin(); di != d.end(); ++di, ++pi)
        if (pi == p.end() || *pi != *di) return false;
    return true;
}

std::string resolve_engine(const std::string& name) {
#ifdef _WIN32
    DWORD needed = SearchPathA(nullptr, name.c_str(), ".exe", 0, nullptr, nullptr);
    if (needed == 0) return "";
    std::vector<char> buffer(static_cast<size_t>(needed) + 1);
    DWORD len = SearchPathA(nullptr, name.c_str(), ".exe", static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (len == 0 || len >= buffer.size()) return "";
    return fs::absolute(fs::path(buffer.data())).string();
#else
    auto usable = [](const fs::path& p) {
        std::error_code ec;
        return fs::is_regular_file(p, ec) && !ec && ::access(p.string().c_str(), X_OK) == 0;
    };
    if (name.find('/') != std::string::npos) {
        fs::path p = fs::absolute(name);
        return usable(p) ? p.lexically_normal().string() : "";
    }
    const char* env = std::getenv("PATH");
    if (!env) return "";
    std::string paths(env);
    size_t start = 0;
    while (true) {
        size_t end = paths.find(':', start);
        std::string component = paths.substr(start, end == std::string::npos ? end : end - start);
        fs::path p = fs::absolute((component.empty() ? fs::path(".") : fs::path(component)) / name);
        if (usable(p)) return p.lexically_normal().string();
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return "";
#endif
}

bool validate_resources(const MarcusProject& project, const JobResources& resources, std::string& error) {
    unsigned long cpu_limit = resources.has_cpu_limit && resources.nodes == 1 ? resources.allocated_cpus : 0;
    if (resources.scheduler_type == SchedulerType::SLURM) {
        const char* per_task = std::getenv("SLURM_CPUS_PER_TASK");
        if (per_task && *per_task) {
            try {
                size_t parsed = 0;
                std::string value(per_task);
                if (!std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); }))
                    throw std::invalid_argument("invalid allocation");
                auto n = std::stoul(value, &parsed);
                if (parsed == value.size() && n > 0) cpu_limit = cpu_limit ? std::min(cpu_limit, n) : n;
            } catch (const std::exception&) { /* allocation is ambiguous; use detected limits */ }
        }
    }
    for (const auto* setting : {&project.step1_nprocshared, &project.step2_nprocshared}) {
        if (setting->empty()) continue;
        try {
            if (!std::all_of(setting->begin(), setting->end(), [](unsigned char c) { return std::isdigit(c); }))
                throw std::invalid_argument("invalid count");
            size_t parsed = 0;
            auto n = std::stoul(*setting, &parsed);
            if (parsed != setting->size() || n == 0) throw std::invalid_argument("invalid count");
            if (cpu_limit > 0 && n > cpu_limit) {
                error = "Gaussian nprocshared=" + *setting + " exceeds the allocated CPU count " +
                        std::to_string(cpu_limit);
                return false;
            }
        } catch (const std::exception&) {
            error = "Invalid Gaussian nprocshared value: " + *setting;
            return false;
        }
    }
    // Only compare explicitly recognized byte units against an unambiguous
    // single-node memory allocation. Gaussian word-based formats remain valid.
    if (resources.has_memory_limit && resources.nodes == 1 && resources.allocated_memory_mb > 0) {
        const std::regex bytes(R"(^([0-9]+(?:\.[0-9]+)?)\s*([KMG]B)$)", std::regex::icase);
        for (const auto* setting : {&project.step1_mem, &project.step2_mem}) {
            std::smatch match;
            if (!std::regex_match(*setting, match, bytes)) continue;
            try {
                double mb = std::stod(match[1].str());
                char unit = static_cast<char>(std::toupper(static_cast<unsigned char>(match[2].str()[0])));
                if (unit == 'G') mb *= 1024.0;
                else if (unit == 'K') mb /= 1024.0;
                if (mb > static_cast<double>(resources.allocated_memory_mb)) {
                    error = "Gaussian memory=" + *setting + " exceeds allocated memory " +
                            std::to_string(resources.allocated_memory_mb) + " MB";
                    return false;
                }
            } catch (const std::exception&) {
                error = "Invalid Gaussian memory value: " + *setting;
                return false;
            }
        }
    }
    return true;
}

std::string progress_time() {
    const std::time_t now = std::time(nullptr);
    std::tm value{};
#ifdef _WIN32
    bool valid = localtime_s(&value, &now) == 0;
#else
    bool valid = localtime_r(&now, &value) != nullptr;
#endif
    if (!valid) return "time unavailable";
    std::ostringstream text;
    text << std::put_time(&value, "%Y-%m-%d %H:%M:%S");
    return text.str();
}

class CaptureStderr {
public:
    explicit CaptureStderr(std::ostream& target) : original_(std::cerr.rdbuf(target.rdbuf())) {}
    ~CaptureStderr() { std::cerr.rdbuf(original_); }
private:
    std::streambuf* original_;
};

struct ProcessResult {
    bool success = false;
    bool interrupted = false;
    int exit_code = -1;
    std::string error;
};

ProcessResult run_engine(const std::string& engine, const fs::path& directory,
                         const fs::path& input, const fs::path& log, std::ostream& progress) {
    ProcessResult result;
    int input_fd = open_input(input);
    if (input_fd < 0) { result.error = "Cannot open input " + input.string() + ": " + std::strerror(errno); return result; }
    int output_fd = create_exclusive(log);
    if (output_fd < 0) {
        result.error = "Cannot create job log " + log.string() + ": " + std::strerror(errno);
        close_fd(input_fd);
        return result;
    }
#ifdef _WIN32
    HANDLE in = reinterpret_cast<HANDLE>(_get_osfhandle(input_fd));
    HANDLE out = reinterpret_cast<HANDLE>(_get_osfhandle(output_fd));
    if (!SetHandleInformation(in, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) ||
        !SetHandleInformation(out, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
        result.error = "Cannot configure Gaussian standard handles";
        close_fd(input_fd); close_fd(output_fd); return result;
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in; si.hStdOutput = out; si.hStdError = out;
    PROCESS_INFORMATION pi{};
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        if (job) CloseHandle(job);
        result.error = "Cannot create a Gaussian process group";
        close_fd(input_fd); close_fd(output_fd); return result;
    }
    bool created = CreateProcessA(engine.c_str(), nullptr, nullptr, nullptr, TRUE,
                                  CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP, nullptr,
                                  directory.string().c_str(), &si, &pi) != 0;
    DWORD creation_error = created ? 0 : GetLastError();
    close_fd(input_fd); close_fd(output_fd);
    if (!created) { CloseHandle(job); result.error = "Cannot launch Gaussian (Windows error " + std::to_string(creation_error) + ")"; return result; }
    if (!AssignProcessToJobObject(job, pi.hProcess)) {
        TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(job);
        result.error = "Cannot attach Gaussian to a process group"; return result;
    }
    if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) {
        TerminateJobObject(job, 1); WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(job);
        result.error = "Cannot start Gaussian process"; return result;
    }
    CloseHandle(pi.hThread);
    auto next_heartbeat = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    while (true) {
        DWORD wait = WaitForSingleObject(pi.hProcess, 200);
        if (wait == WAIT_OBJECT_0) break;
        if (wait == WAIT_FAILED) { result.error = "Cannot wait for Gaussian"; TerminateJobObject(job, 1); WaitForSingleObject(pi.hProcess, INFINITE); break; }
        if (g_shutdown_requested.load() || !progress) {
            result.interrupted = g_shutdown_requested.load();
            result.error = result.interrupted ? "Interrupted; Gaussian process group terminated" : "Cannot write progress output";
            TerminateJobObject(job, 1); WaitForSingleObject(pi.hProcess, INFINITE); break;
        }
        if (std::chrono::steady_clock::now() >= next_heartbeat) {
            progress << "  Still running; see " << log.string() << "\n" << std::flush;
            next_heartbeat = std::chrono::steady_clock::now() + std::chrono::minutes(5);
        }
    }
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code) && result.error.empty()) result.error = "Cannot obtain Gaussian exit status";
    result.exit_code = static_cast<int>(exit_code);
    CloseHandle(pi.hProcess); CloseHandle(job);
    result.success = result.error.empty() && result.exit_code == 0;
#else
    // Prepare strings before fork; the child uses only async-signal-safe calls.
    const std::string directory_string = directory.string();
    const char* executable = engine.c_str();
    pid_t pid = fork();
    if (pid < 0) {
        result.error = "Cannot start Gaussian: " + std::string(std::strerror(errno));
        close_fd(input_fd); close_fd(output_fd); return result;
    }
    if (pid == 0) {
        setpgid(0, 0);
        if (chdir(directory_string.c_str()) != 0 || dup2(input_fd, STDIN_FILENO) < 0 ||
            dup2(output_fd, STDOUT_FILENO) < 0 || dup2(output_fd, STDERR_FILENO) < 0) _exit(126);
        for (int fd : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
            int flags = fcntl(fd, F_GETFD);
            if (flags >= 0) fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
        }
        if (input_fd > STDERR_FILENO) close_fd(input_fd);
        if (output_fd > STDERR_FILENO && output_fd != input_fd) close_fd(output_fd);
        char* const args[] = {const_cast<char*>(executable), nullptr};
        execv(executable, args);
        _exit(127);
    }
    close_fd(input_fd); close_fd(output_fd);
    setpgid(pid, pid);
    int status = 0;
    bool termination_requested = false, kill_sent = false;
    auto kill_deadline = std::chrono::steady_clock::time_point::max();
    auto next_heartbeat = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    while (true) {
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) break;
        if (waited < 0 && errno != EINTR) {
            result.error = "Cannot wait for Gaussian: " + std::string(std::strerror(errno));
            if (kill(-pid, SIGKILL) != 0) kill(pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
            break;
        }
        auto now = std::chrono::steady_clock::now();
        if (!termination_requested && (g_shutdown_requested.load() || !progress)) {
            termination_requested = true;
            result.interrupted = g_shutdown_requested.load();
            result.error = result.interrupted ? "Interrupted; terminating Gaussian process group" : "Cannot write progress output";
            if (kill(-pid, SIGTERM) != 0) kill(pid, SIGTERM);
            kill_deadline = now + std::chrono::seconds(5);
        }
        if (termination_requested && !kill_sent && now >= kill_deadline) {
            if (kill(-pid, SIGKILL) != 0) kill(pid, SIGKILL);
            kill_sent = true;
        }
        if (now >= next_heartbeat) {
            progress << "  Still running; see " << log.string() << "\n" << std::flush;
            next_heartbeat = now + std::chrono::minutes(5);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) result.exit_code = 128 + WTERMSIG(status);
    result.success = result.error.empty() && result.exit_code == 0;
#endif
    if (!result.success && result.error.empty()) result.error = "Gaussian exited with status " + std::to_string(result.exit_code);
    return result;
}

bool validate_log(const fs::path& log, bool optimization, bool gas_phase, std::string& error) {
    std::ifstream input(log, std::ios::binary);
    if (!input) { error = "Cannot read Gaussian log " + log.string(); return false; }
    // Stream the log rather than retaining a second full copy in memory. A
    // later Link1 start invalidates an earlier normal-termination marker.
    const std::regex link1(R"(Link1:\s*Proceeding to internal job step)", std::regex::icase);
    bool normal = false, terminated_with_error = false, opt_completed = false;
    std::string line;
    while (std::getline(input, line)) {
        if (g_shutdown_requested.load()) { error = "Automatic workflow interrupted"; return false; }
        if (line.find("Error termination") != std::string::npos) terminated_with_error = true;
        if (line.find("Optimization completed") != std::string::npos) opt_completed = true;
        if (normal && (line.find("Entering Gaussian System") != std::string::npos ||
                       line.find("Entering Link 1 =") != std::string::npos ||
                       line.find("--Link1--") != std::string::npos || std::regex_search(line, link1)))
            normal = false;
        if (line.find("Normal termination of Gaussian") != std::string::npos) normal = true;
    }
    if (input.bad()) { error = "Cannot finish reading Gaussian log " + log.string(); return false; }
    if (!normal || terminated_with_error) {
        error = "Gaussian did not finish normally; inspect " + log.string();
        return false;
    }
    if (optimization && !opt_completed) {
        error = "Geometry optimization did not complete; inspect " + log.string();
        return false;
    }
    double energy = 0.0;
    if (!extract_electronic_energy(log.string(), energy, optimization || gas_phase)) {
        error = "Required electronic energy is missing; inspect " + log.string();
        return false;
    }
    if (optimization) {
        double thermal = 0.0;
        if (!extract_thermal_correction_gibbs(log.string(), thermal)) {
            error = "Frequency/Gibbs correction is missing; inspect " + log.string();
            return false;
        }
    }
    return true;
}

bool valid_checkpoint(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) return false;
    auto size = fs::file_size(path, ec);
    return !ec && size > 0;
}

struct Job {
    std::string role;
    fs::path directory;
    fs::path input;
    fs::path log;
    fs::path checkpoint;
    bool optimization = false;
    bool requires_checkpoint = false;
};

bool make_jobs(const MarcusProject& project, const std::string& dir1, const std::string& dir2,
               std::vector<Job>& optimizations, std::vector<Job>& equilibrium,
               std::vector<Job>& cross, std::string& error) {
    auto manifest1 = read_step1_manifest(dir1);
    auto manifest2 = read_step2_manifest(dir2);
    for (const auto* mol : {&project.donor, &project.acceptor}) {
        for (const auto* state : {&mol->init_state, &mol->final_state}) {
            std::string key = mol->name + "-" + state->state_label;
            auto i = manifest1.find(key);
            auto j = manifest2.find(key + "-eq");
            if (i == manifest1.end() || j == manifest2.end()) { error = "Generated manifest is missing state " + key; return false; }
            fs::path chk(i->second), gau(j->second);
            if (!safe_component(chk.string()) || !safe_component(gau.string())) { error = "Invalid generated manifest filename"; return false; }
            fs::path opt = chk; opt.replace_extension(".gau");
            fs::path optlog = chk; optlog.replace_extension(".log");
            fs::path splog = gau; splog.replace_extension(".log");
            optimizations.push_back({key + " optimization", dir1, fs::path(dir1) / opt, fs::path(dir1) / optlog,
                                     fs::path(dir1) / chk, true, true});
            equilibrium.push_back({key + " equilibrium", dir2, fs::path(dir2) / gau, fs::path(dir2) / splog,
                                   fs::path(dir2) / chk, false, true});
        }
        for (const auto& pair : {std::pair<const MarcusState*, const MarcusState*>{&mol->final_state, &mol->init_state},
                                 {&mol->init_state, &mol->final_state}}) {
            std::string key = mol->name + "-" + pair.first->state_label + "-from-" + pair.second->state_label;
            auto i = manifest2.find(key);
            if (i == manifest2.end() || !safe_component(i->second)) { error = "Generated manifest is missing cross state " + key; return false; }
            fs::path gau(i->second), log(gau); log.replace_extension(".log");
            cross.push_back({key + " cross", dir2, fs::path(dir2) / gau, fs::path(dir2) / log, {}, false, false});
        }
    }
    return optimizations.size() == 4 && equilibrium.size() == 4 && cross.size() == 4;
}

} // namespace

int run_automatic(MarcusProject project, const std::string& input_file, const JobResources& resources) {
    if (g_shutdown_requested.load()) { std::cerr << "Error: Automatic workflow interrupted before start." << std::endl; return interruption_status(); }
    if (!safe_component(project.name) || !safe_component(project.donor.name) ||
        !safe_component(project.acceptor.name) || project.donor.name == project.acceptor.name) {
        std::cerr << "Error: Automatic workflow requires distinct donor/acceptor names and project names without path separators." << std::endl;
        return 1;
    }
    std::string engine = resolve_engine(project.qc_engine);
    if (engine.empty()) { std::cerr << "Error: Cannot find executable qc_engine: " << project.qc_engine << std::endl; return 1; }
    std::string error;
    if (!validate_resources(project, resources, error)) { std::cerr << "Error: " << error << std::endl; return 1; }
    for (const auto& xyz : {project.donor.init_state.xyz_path, project.acceptor.init_state.xyz_path}) {
        std::error_code ec;
        if (!fs::is_regular_file(xyz, ec) || ec) { std::cerr << "Error: Cannot find XYZ input: " << xyz << std::endl; return 1; }
    }
    std::string dir1 = project.name + "_step_1", dir2 = project.name + "_step_2";
    fs::path report = project.out_file.empty() ? fs::path(project.name + ".log") : fs::path(project.out_file);
    std::error_code ec;
    fs::path parent = report.has_parent_path() ? report.parent_path() : fs::path(".");
    if (!fs::is_directory(parent, ec) || ec) { std::cerr << "Error: Report parent directory does not exist: " << parent << std::endl; return 1; }
    if (inside_directory(report, dir1) || inside_directory(report, dir2)) {
        std::cerr << "Error: Automatic progress output must be outside the step directories." << std::endl;
        return 1;
    }
    if (fs::exists(fs::symlink_status(report, ec)) || (ec && ec != std::errc::no_such_file_or_directory)) {
        std::cerr << "Error: Automatic workflow will not overwrite output: " << report << std::endl; return 1;
    }
    fs::path project_lock = hidden_lock_path(project.name);
    fs::path output_lock = hidden_lock_path(report);
    DirectoryLock lock_project, lock_output;
    if (!lock_project.acquire(project_lock, error)) { std::cerr << "Error: " << error << std::endl; return 1; }
    if (fs::absolute(project_lock).lexically_normal() != fs::absolute(output_lock).lexically_normal() &&
        !lock_output.acquire(output_lock, error)) { std::cerr << "Error: " << error << std::endl; return 1; }
    if (!directory_empty_or_absent(dir1, error) || !directory_empty_or_absent(dir2, error)) {
        std::cerr << "Error: " << error << std::endl; return 1;
    }
    int output_fd = create_exclusive(report);
    if (output_fd < 0) { std::cerr << "Error: Cannot create progress output " << report << ": " << std::strerror(errno) << std::endl; return 1; }
    DescriptorBuffer buffer(output_fd);
    std::ostream progress(&buffer);
    int completed = 0;
    bool job_in_progress = false;
    auto fail = [&](const std::string& reason, int code = 1) {
        int failed = job_in_progress ? 1 : 0;
        progress << "\nERROR (" << progress_time() << "): " << reason
                 << "\nJobs: " << completed << " completed, " << failed << " failed/interrupted, "
                 << (12 - completed - failed) << " pending.\n"
                 << "Automatic workflow stopped; existing job files were preserved.\n" << std::flush;
        std::cerr << "Error: " << reason << "\nProgress output: " << report.string() << std::endl;
        return code;
    };
    progress << Metadata::header() << "AUTOMATIC MARCUS WORKFLOW\n"
             << "==========================================================================\n"
             << "Project: " << project.name << "\nInput: " << input_file << "\nGaussian: " << engine
             << "\nStarted: " << progress_time() << "\n"
             << "Execution: sequential in the current allocation (one Gaussian process at a time)\n"
             << "Input step setting is ignored; automatic mode runs every stage.\n"
             << "Progress and final report: " << report.string() << "\n" << std::flush;
    if (!progress) return fail("Cannot write progress output");
    std::cout << "Automatic Marcus workflow started; progress: " << report.string() << std::endl;
    if (resources.nodes > 1) progress << "Note: automatic mode runs sequentially on one node; other allocated nodes are not used.\n" << std::flush;

    project.dir_step1_opt = dir1;
    project.dir_step2_sp = dir2;
    auto generate = [&](bool step1) {
        std::ostringstream diagnostic;
        bool ok;
        {
            CaptureStderr capture(diagnostic);
            ok = step1 ? generate_step1_inputs(project) : generate_step2_inputs(project);
        }
        if (!diagnostic.str().empty()) std::cerr << diagnostic.str();
        if (!ok) error = std::string(step1 ? "Step 1" : "Step 2") +
                         " input generation failed" +
                         (diagnostic.str().empty() ? "" : ":\n" + diagnostic.str());
        return ok;
    };
    if (!generate(true)) return fail(error);
    // Generate step-2 inputs only after all optimizations have completed.
    auto manifest1 = read_step1_manifest(dir1);
    std::vector<Job> optimizations;
    for (const auto* mol : {&project.donor, &project.acceptor}) {
        for (const auto* state : {&mol->init_state, &mol->final_state}) {
            std::string key = mol->name + "-" + state->state_label;
            auto it = manifest1.find(key);
            if (it == manifest1.end() || !safe_component(it->second)) return fail("Generated step-1 manifest is missing " + key);
            fs::path chk(it->second), input(chk), log(chk);
            input.replace_extension(".gau"); log.replace_extension(".log");
            optimizations.push_back({key + " optimization", dir1, fs::path(dir1) / input,
                                     fs::path(dir1) / log, fs::path(dir1) / chk, true, true});
        }
    }
    auto execute_job = [&](const Job& job) -> bool {
        if (g_shutdown_requested.load()) { error = "Automatic workflow interrupted"; return false; }
        progress << "\n[" << completed + 1 << "/12] " << job.role << " (" << progress_time() << ")\n"
                 << "  Input: " << job.input.string() << "\n  Gaussian log: " << job.log.string() << "\n"
                 << "  Status: running\n" << std::flush;
        if (!progress) { error = "Cannot write progress output"; return false; }
        job_in_progress = true;
        ProcessResult process;
        try {
            process = run_engine(engine, fs::absolute(job.directory), fs::absolute(job.input), fs::absolute(job.log), progress);
        } catch (const std::exception& e) {
            error = job.role + ": could not launch Gaussian: " + e.what();
            return false;
        }
        if (!process.success) { error = job.role + ": " + process.error + "; inspect " + job.log.string(); return false; }
        if (g_shutdown_requested.load()) { error = "Automatic workflow interrupted"; return false; }
        if (!validate_log(job.log, job.optimization, project.step2_solvent.empty(), error)) { error = job.role + ": " + error; return false; }
        if (job.requires_checkpoint && !valid_checkpoint(job.checkpoint)) {
            error = job.role + ": checkpoint missing or empty: " + job.checkpoint.string(); return false;
        }
        ++completed;
        job_in_progress = false;
        progress << "[" << completed << "/12] Completed: " << job.role << " (" << progress_time() << ")\n" << std::flush;
        if (!progress) { error = "Cannot write progress output"; return false; }
        return true;
    };
    progress << "\nSTEP 1: geometry optimization and frequencies\n" << std::flush;
    for (const auto& job : optimizations)
        if (!execute_job(job)) return fail(error, g_shutdown_requested.load() ? interruption_status() : 1);

    progress << "\nStep 1 complete; generating step-2 inputs.\n" << std::flush;
    if (!progress) return fail("Cannot write progress output");
    if (!generate(false)) return fail(error);
    std::vector<Job> generated_opt, equilibrium, cross;
    if (!make_jobs(project, dir1, dir2, generated_opt, equilibrium, cross, error))
        return fail(error.empty() ? "Generated manifests do not contain 12 jobs" : error);
    progress << "\nSTEP 2a: equilibrium single points\n" << std::flush;
    for (const auto& job : equilibrium)
        if (!execute_job(job)) return fail(error, g_shutdown_requested.load() ? interruption_status() : 1);
    progress << "\nSTEP 2b: cross single points\n" << std::flush;
    for (const auto& job : cross)
        if (!execute_job(job)) return fail(error, g_shutdown_requested.load() ? interruption_status() : 1);

    if (g_shutdown_requested.load()) return fail("Automatic workflow interrupted", interruption_status());
    progress << "\nAll 12 Gaussian jobs completed. Calculating Marcus barriers...\n\n" << std::flush;
    if (!progress) return fail("Cannot write progress output");
    MarcusResult result;
    std::ostringstream diagnostic;
    bool calculated;
    {
        CaptureStderr capture(diagnostic);
        calculated = calculate_barrier_step3(project, result);
    }
    if (!diagnostic.str().empty()) {
        std::cerr << diagnostic.str();
        progress << "Calculation diagnostics:\n" << diagnostic.str() << std::flush;
    }
    if (!calculated) return fail("Barrier calculation failed: " + result.error_message);
    if (g_shutdown_requested.load()) return fail("Automatic workflow interrupted", interruption_status());
    std::string footer;
    if (!write_barrier_report_to_stream(progress, project, result, false, &footer))
        return fail("Cannot write final barrier report");
    if (!result.gibbs_valid || !result.barrier_sym_valid) return fail("Barrier calculation is incomplete");
    progress.flush();
    if (!progress || !buffer.close()) { std::cerr << "Error: Cannot finish writing " << report.string() << std::endl; return 1; }
    std::cout << "Automatic Marcus workflow complete. Report: " << report.string() << "\n" << footer;
    return 0;
}

} // namespace Marcus
