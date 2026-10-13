/**
 * @file matrix.h
 * @brief Pairwise RMSD matrix (port of Rust matrix.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_MATRIX_H
#define RMSD_MATRIX_H

#include "rmsd/pipeline.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Rmsd
{

uint64_t estimate_molecule_bytes(const Molecule& m);
uint64_t estimate_set_bytes(const std::vector<Molecule>& mols);
void check_budget(const std::vector<Molecule>& mols, uint64_t cap);
uint64_t parse_mem_size(const std::string& s);
std::pair<size_t, size_t> parse_row_range(const std::string& s, size_t n);
size_t block_rows(uint64_t row_bytes, uint64_t max_mem);

using MatrixOpt = std::vector<std::vector<std::optional<double>>>;
using ProgressFn = std::function<void(size_t, size_t)>;

MatrixOpt pairwise_rows(const std::vector<Molecule>& mols, const std::vector<std::string>& labels,
    const Options& opts, size_t range_start, size_t range_end, const ProgressFn& progress, unsigned thread_count);
MatrixOpt pairwise_matrix(const std::vector<Molecule>& mols, const std::vector<std::string>& labels,
    const Options& opts, const ProgressFn& progress, unsigned thread_count);

std::string csv_header(const std::vector<std::string>& labels);
std::string rows_to_csv(const std::vector<std::string>& labels, const MatrixOpt& rows, size_t offset);
std::string to_csv(const std::vector<std::string>& labels, const MatrixOpt& matrix);
std::pair<size_t, size_t> resume_state(const std::string& text, const std::vector<std::string>& labels);
std::pair<std::vector<std::string>, std::vector<std::string>> merge_partials(const std::vector<std::string>& texts);

} // namespace Rmsd

#endif // RMSD_MATRIX_H
