/**
 * @file struct_list.h
 * @brief Structure-list files for matrix mode (port of Rust list.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_STRUCT_LIST_H
#define RMSD_STRUCT_LIST_H

#include <string>
#include <vector>

namespace Rmsd
{

struct ListEntry
{
    std::string label;
    std::string path;
};

std::vector<std::string> parse_list_text(const std::string& text);
std::vector<ListEntry> load_list_file(const std::string& list_path);
std::string csv_path_for(const std::string& list_path);

} // namespace Rmsd

#endif // RMSD_STRUCT_LIST_H
