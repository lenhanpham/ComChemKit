/**
 * @file marcus_io.h
 * @brief I/O utilities for Marcus barrier calculations: directory management,
 * manifest reading/writing, and barrier report output.
 */

#ifndef MARCUS_IO_H
#define MARCUS_IO_H

#include "marcus/marcus_types.h"
#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace Marcus {

/**
 * @brief Create the step-1 and step-2 project directories if they don't exist.
 * @param project_name Project name (directories will be created from name).
 * @param dir_step1 Output: step 1 directory path.
 * @param dir_step2 Output: step 2 directory path.
 * @return true on success, false on failure.
 */
bool create_project_directories(const std::string& project_name,
                                std::string& dir_step1,
                                std::string& dir_step2);

/**
 * @brief Write the step-1 manifest file recording checkpoint names.
 * @param dir_step1 Step 1 directory path.
 * @param states Map of state key -> checkpoint filename.
 * @return true on success.
 */
bool write_step1_manifest(const std::string& dir_step1,
                          const std::map<std::string, std::string>& states);

/**
 * @brief Read the step-1 manifest file.
 * @param dir_step1 Step 1 directory path.
 * @return Map of state key -> checkpoint filename.
 */
std::map<std::string, std::string> read_step1_manifest(const std::string& dir_step1);

/**
 * @brief Write the step-2 manifest file recording all single-point input paths.
 * @param dir_step2 Step 2 directory path.
 * @param inputs Map of input key -> input filename.
 * @return true on success.
 */
bool write_step2_manifest(const std::string& dir_step2,
                          const std::map<std::string, std::string>& inputs);

/**
 * @brief Read the step-2 manifest file.
 * @param dir_step2 Step 2 directory path.
 * @return Map of input key -> input filename.
 */
std::map<std::string, std::string> read_step2_manifest(const std::string& dir_step2);

/**
 * @brief Check if a step-1 checkpoint file exists.
 * @param dir_step1 Step 1 directory.
 * @param chk_file Checkpoint filename.
 * @return true if exists.
 */
bool step1_checkpoint_exists(const std::string& dir_step1,
                             const std::string& chk_file);

/**
 * @brief Check if a step-2 log file exists.
 * @param dir_step2 Step 2 directory.
 * @param log_file Log filename.
 * @return true if exists.
 */
bool step2_log_exists(const std::string& dir_step2,
                      const std::string& log_file);

/**
 * @brief Check if a step-1 log file exists.
 * @param dir_step1 Step 1 directory.
 * @param log_file Log filename.
 * @return true if exists.
 */
bool step1_log_exists(const std::string& dir_step1,
                      const std::string& log_file);

/**
 * @brief Write the barrier calculation report to a file.
 * @param project Project configuration.
 * @param result Calculation results.
 * @param out_file Optional explicit output file path.
 * @param completion_footer Optional output: the footer written for a successful calculation.
 * @return true on success.
 */
bool write_barrier_report(const MarcusProject& project,
                          const MarcusResult& result,
                          const std::string& out_file = "",
                          std::string* completion_footer = nullptr);

/**
 * @brief Construct the full path to a step-1 file.
 * @param dir_step1 Step 1 directory.
 * @param filename Filename.
 * @return Full path string.
 */
std::string step1_path(const std::string& dir_step1, const std::string& filename);

/**
 * @brief Construct the full path to a step-2 file.
 * @param dir_step2 Step 2 directory.
 * @param filename Filename.
 * @return Full path string.
 */
std::string step2_path(const std::string& dir_step2, const std::string& filename);

}  // namespace Marcus

#endif  // MARCUS_IO_H