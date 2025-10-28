/**
 * @file parallel_utils.h
 * @brief Thread-safe utilities for parallel thermochemistry calculations
 * @author Le Nhan Pham
 * @date 2025
 *
 * This file provides infrastructure for parallel file processing in the thermo module.
 * It includes thread-safe memory monitoring, file handle management, and error collection.
 * Implementation pattern adapted from gaussian_extractor.cpp for consistency.
 */

#ifndef THERMO_PARALLEL_UTILS_H
#define THERMO_PARALLEL_UTILS_H

#include "job_management/job_scheduler.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if __cpp_lib_semaphore >= 201907L
    #include <semaphore>
#else
    #include <condition_variable>
#endif

namespace ThermoParallel
{

/**
 * @brief Thread-safe memory usage monitor
 *
 * Tracks memory allocation and usage across multiple threads to prevent
 * system memory exhaustion during parallel file processing.
 */
class MemoryMonitor
{
  public:
    explicit MemoryMonitor(size_t max_memory_mb);

    bool   can_allocate(size_t bytes);
    void   add_usage(size_t bytes);
    void   remove_usage(size_t bytes);
    size_t get_current_usage() const;
    size_t get_peak_usage() const;
    size_t get_max_usage() const;
    void   set_memory_limit(size_t max_memory_mb);

    static size_t get_system_memory_mb();
    static size_t calculate_optimal_memory_limit(unsigned int thread_count, size_t system_memory_mb = 0);

  private:
    std::atomic<size_t> current_usage_bytes{0};
    std::atomic<size_t> peak_usage_bytes{0};
    size_t              max_bytes;

    static constexpr size_t DEFAULT_MEMORY_MB = 4096;
    static constexpr size_t MIN_MEMORY_MB     = 512;
    static constexpr size_t MAX_MEMORY_MB     = 65536;
};

/**
 * @brief RAII-based file handle manager
 *
 * Prevents file descriptor exhaustion by limiting concurrent file operations.
 * Uses RAII pattern for automatic resource release.
 */
class FileHandleManager
{
  public:
    explicit FileHandleManager(unsigned int max_handles = 100)
        : available_handles(max_handles)
#if __cpp_lib_semaphore >= 201907L
          ,
          semaphore(max_handles)
#endif
    {
    }

    class FileGuard
    {
      public:
        explicit FileGuard(FileHandleManager* mgr);
        ~FileGuard();

        FileGuard(const FileGuard&) = delete;
        FileGuard& operator=(const FileGuard&) = delete;

        FileGuard(FileGuard&& other) noexcept;
        FileGuard& operator=(FileGuard&& other) noexcept;

        bool is_acquired() const { return acquired; }

      private:
        FileHandleManager* manager;
        bool               acquired;
    };

    FileGuard acquire();
    void      release();

  private:
    std::atomic<size_t> available_handles;

#if __cpp_lib_semaphore >= 201907L
    std::counting_semaphore<> semaphore;
#else
    std::mutex              mutex_;
    std::condition_variable cv_;
#endif
};

/**
 * @brief Thread-safe error and warning collector
 *
 * Centralizes error reporting from multiple worker threads.
 */
class ThreadSafeErrorCollector
{
  public:
    void add_error(const std::string& error);
    void add_warning(const std::string& warning);

    std::vector<std::string> get_errors() const;
    std::vector<std::string> get_warnings() const;
    bool                     has_errors() const;
    void                     clear();

  private:
    mutable std::mutex       error_mutex;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/**
 * @brief Format memory size for human-readable output
 */
std::string formatMemorySize(size_t bytes);

// Memory limit constants (exposed for external use)
constexpr size_t MIN_MEMORY_MB = 512;    // Minimum 512 MB
constexpr size_t MAX_MEMORY_MB = 65536;  // Maximum 64 GB

/**
 * @brief Calculate safe memory limit considering job scheduler constraints
 * @param requested_memory_mb User-requested memory limit (MB, 0 = auto-detect)
 * @param thread_count Number of threads that will be used
 * @param job_resources Job scheduler resource allocation information
 * @return Safe memory limit in megabytes
 *
 * This function:
 * - Uses auto-calculation if requested_memory_mb is 0
 * - Respects job scheduler memory allocations (SLURM, PBS, SGE)
 * - Applies safety margins (95% of job allocation)
 * - Enforces minimum and maximum bounds
 */
size_t calculateSafeMemoryLimit(size_t requested_memory_mb, 
                               unsigned int thread_count,
                               const ::JobResources& job_resources);

}  // namespace ThermoParallel

#endif  // THERMO_PARALLEL_UTILS_H
