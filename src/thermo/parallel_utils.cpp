/**
 * @file parallel_utils.cpp
 * @brief Implementation of thread-safe utilities for parallel processing
 * @author Le Nhan Pham
 * @date 2025
 */

#include "parallel_utils.h"
#include "job_management/job_scheduler.h"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <unistd.h>
    #ifdef __linux__
        #include <sys/sysinfo.h>
    #endif
#endif

namespace ThermoParallel
{

// ============================================================================
// MemoryMonitor Implementation
// ============================================================================

MemoryMonitor::MemoryMonitor(size_t max_memory_mb) : max_bytes(max_memory_mb * 1024 * 1024) {}

bool MemoryMonitor::can_allocate(size_t bytes)
{
    return (current_usage_bytes.load() + bytes) < max_bytes;
}

void MemoryMonitor::add_usage(size_t bytes)
{
    size_t new_usage    = current_usage_bytes.fetch_add(bytes) + bytes;
    size_t current_peak = peak_usage_bytes.load();
    while (new_usage > current_peak)
    {
        if (peak_usage_bytes.compare_exchange_weak(current_peak, new_usage))
        {
            break;
        }
    }
}

void MemoryMonitor::remove_usage(size_t bytes)
{
    current_usage_bytes.fetch_sub(bytes);
}

size_t MemoryMonitor::get_current_usage() const
{
    return current_usage_bytes.load();
}

size_t MemoryMonitor::get_peak_usage() const
{
    return peak_usage_bytes.load();
}

size_t MemoryMonitor::get_max_usage() const
{
    return max_bytes;
}

void MemoryMonitor::set_memory_limit(size_t max_memory_mb)
{
    max_bytes = max_memory_mb * 1024 * 1024;
}

size_t MemoryMonitor::get_system_memory_mb()
{
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo))
    {
        return static_cast<size_t>(memInfo.ullTotalPhys / (1024 * 1024));
    }
    return DEFAULT_MEMORY_MB;
#else
    // Method 1: sysconf (POSIX)
    long pages     = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0)
    {
        return static_cast<size_t>((pages * page_size) / (1024 * 1024));
    }

    // Method 2: Linux-specific sysinfo
    #if defined(__linux__) && defined(__GLIBC__)
    struct sysinfo si;
    if (sysinfo(&si) == 0)
    {
        return static_cast<size_t>((si.totalram * si.mem_unit) / (1024 * 1024));
    }
    #endif

    return DEFAULT_MEMORY_MB;
#endif
}

size_t MemoryMonitor::calculate_optimal_memory_limit(unsigned int thread_count, size_t system_memory_mb)
{
    if (system_memory_mb == 0)
    {
        system_memory_mb = get_system_memory_mb();
    }

    double memory_percentage = 0.5;  // Default 50%

    if (thread_count <= 4)
    {
        memory_percentage = 0.3;
    }
    else if (thread_count <= 8)
    {
        memory_percentage = 0.4;
    }
    else if (thread_count <= 16)
    {
        memory_percentage = 0.5;
    }
    else
    {
        memory_percentage = 0.6;
    }

    // Be conservative in cluster environments
    bool is_cluster = (std::getenv("SLURM_JOB_ID") != nullptr || std::getenv("PBS_JOBID") != nullptr ||
                       std::getenv("SGE_JOB_ID") != nullptr || std::getenv("LSB_JOBID") != nullptr);

    if (is_cluster)
    {
        memory_percentage *= 0.7;
    }

    size_t calculated_memory = static_cast<size_t>(system_memory_mb * memory_percentage);

    calculated_memory = std::max(calculated_memory, MIN_MEMORY_MB);
    calculated_memory = std::min(calculated_memory, MAX_MEMORY_MB);

    return calculated_memory;
}

// ============================================================================
// FileHandleManager Implementation
// ============================================================================

FileHandleManager::FileGuard::FileGuard(FileHandleManager* mgr) : manager(mgr), acquired(false)
{
    if (manager)
    {
#if __cpp_lib_semaphore >= 201907L
        manager->semaphore.acquire();
#else
        std::unique_lock<std::mutex> lock(manager->mutex_);
        manager->cv_.wait(lock, [mgr] { return mgr->available_handles.load() > 0; });
        manager->available_handles.fetch_sub(1);
#endif
        acquired = true;
    }
}

FileHandleManager::FileGuard::~FileGuard()
{
    if (acquired && manager)
    {
#if __cpp_lib_semaphore >= 201907L
        manager->semaphore.release();
#else
        {
            std::lock_guard<std::mutex> lock(manager->mutex_);
            manager->available_handles.fetch_add(1);
        }
        manager->cv_.notify_one();
#endif
    }
}

FileHandleManager::FileGuard::FileGuard(FileGuard&& other) noexcept
    : manager(other.manager), acquired(other.acquired)
{
    other.manager  = nullptr;
    other.acquired = false;
}

FileHandleManager::FileGuard& FileHandleManager::FileGuard::operator=(FileGuard&& other) noexcept
{
    if (this != &other)
    {
        if (acquired && manager)
        {
#if __cpp_lib_semaphore >= 201907L
            manager->semaphore.release();
#else
            {
                std::lock_guard<std::mutex> lock(manager->mutex_);
                manager->available_handles.fetch_add(1);
            }
            manager->cv_.notify_one();
#endif
        }
        manager        = other.manager;
        acquired       = other.acquired;
        other.manager  = nullptr;
        other.acquired = false;
    }
    return *this;
}

FileHandleManager::FileGuard FileHandleManager::acquire()
{
    return FileGuard(this);
}

void FileHandleManager::release()
{
#if __cpp_lib_semaphore >= 201907L
    semaphore.release();
#else
    {
        std::lock_guard<std::mutex> lock(mutex_);
        available_handles.fetch_add(1);
    }
    cv_.notify_one();
#endif
}

// ============================================================================
// ThreadSafeErrorCollector Implementation
// ============================================================================

void ThreadSafeErrorCollector::add_error(const std::string& error)
{
    std::lock_guard<std::mutex> lock(error_mutex);
    errors.push_back(error);
}

void ThreadSafeErrorCollector::add_warning(const std::string& warning)
{
    std::lock_guard<std::mutex> lock(error_mutex);
    warnings.push_back(warning);
}

std::vector<std::string> ThreadSafeErrorCollector::get_errors() const
{
    std::lock_guard<std::mutex> lock(error_mutex);
    return errors;
}

std::vector<std::string> ThreadSafeErrorCollector::get_warnings() const
{
    std::lock_guard<std::mutex> lock(error_mutex);
    return warnings;
}

bool ThreadSafeErrorCollector::has_errors() const
{
    std::lock_guard<std::mutex> lock(error_mutex);
    return !errors.empty();
}

void ThreadSafeErrorCollector::clear()
{
    std::lock_guard<std::mutex> lock(error_mutex);
    errors.clear();
    warnings.clear();
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string formatMemorySize(size_t bytes)
{
    const char* units[] = {"B", "KB", "MB", "GB"};
    int         unit    = 0;
    double      size    = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 3)
    {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

// ============================================================================
// Memory Limit Calculation with Job Scheduler Integration
// ============================================================================

size_t calculateSafeMemoryLimit(size_t requested_memory_mb, 
                               unsigned int thread_count,
                               const ::JobResources& job_resources)
{
    size_t calculated_memory = requested_memory_mb;
    
    // If no explicit request, calculate based on system and threads
    if (requested_memory_mb == 0) {
        calculated_memory = MemoryMonitor::calculate_optimal_memory_limit(thread_count);
    }
    
    // Apply job scheduler memory limits if available
    if (job_resources.has_memory_limit && job_resources.allocated_memory_mb > 0) {
        // Leave 5% overhead for system processes
        size_t job_memory_with_overhead = job_resources.allocated_memory_mb * 95 / 100;
        calculated_memory = std::min(calculated_memory, job_memory_with_overhead);
    }
    
    // Apply bounds
    calculated_memory = std::max(calculated_memory, MIN_MEMORY_MB);
    calculated_memory = std::min(calculated_memory, MAX_MEMORY_MB);
    
    return calculated_memory;
}

}  // namespace ThermoParallel
