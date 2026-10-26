/**
 * @file rmsd_command.h
 * @brief ICommand wrapper for the rmsd module (port of Rust bin/rmsd.rs).
 * @author Le Nhan Pham
 * @date 2026
 *
 * Invocation:
 *   cck rmsd FILE_A FILE_B [options]
 *   cck rmsd --from-list LIST [options]
 *   cck rmsd LIST (single positional = list file)
 *   cck rmsd --merge-csv OUT PART... 
 */

#ifndef RMSD_COMMAND_H
#define RMSD_COMMAND_H

#include "commands/icommand.h"
#include "rmsd/pipeline.h"
#include <cstddef>
#include <string>
#include <vector>

class RmsdCommand : public ICommand
{
public:
    RmsdCommand();
    std::string get_name() const override;
    std::string get_description() const override;
    void print_help(const std::string& program_name = "cck") const override;
    void parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int execute(const CommandContext& context) override;

private:
    Rmsd::Options options_;
    std::string rotation_str_ = "kabsch";
    std::string reorder_method_str_ = "inertia-hungarian";
    bool only_alpha_ = false;
    std::string format_str_;
    std::string from_list_;
    std::string rows_;
    std::string out_;
    std::string max_mem_str_;
    size_t memory_limit_mb_ = 0;
    bool overwrite_ = false;
    bool resume_ = false;
    std::string merge_csv_;
    bool format_forced_ = false;

    int run_pair(const CommandContext& context) const;
    int run_matrix(const CommandContext& context) const;
    int run_merge() const;
};

#endif // RMSD_COMMAND_H
