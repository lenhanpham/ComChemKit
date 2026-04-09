#ifndef IVCOORD_COMMAND_H
#define IVCOORD_COMMAND_H

#include "commands/icommand.h"
#include <string>

/**
 * @file ivcoord_command.h
 * @brief Defines the IVCoordCommand class for displacing coordinates along imaginary modes
 * @author Le Nhan Pham
 * @date 2026
 *
 * Invocation:
 *   cck ivcoord [options] [file1.log file2.log ...]
 *
 * If no files are provided the current directory is scanned for .log / .out files.
 *
 * Options:
 *   --iamp <float>          Displacement amplitude (default: 1.0)
 *   --idirection <int>      +1 = plus only, -1 = minus only, 0 = both (default: 1)
 *   --param-file [path]     Load ivcoord_parameters.params
 *   --gen-ivcoord-params    Write a template ivcoord_parameters.params and exit
 *
 * Output is written to <parent_dir_basename>_ivcoord/ inside the parent directory
 * of each input file. Output filenames: <stem>_p.xyz and/or <stem>_m.xyz.
 */
class IVCoordCommand : public ICommand
{
public:
    std::string get_name()        const override;
    std::string get_description() const override;
    void        parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int         execute(const CommandContext& context) override;

private:
    double ivc_amp       = 1.0;  ///< Displacement amplitude
    int    ivc_direction = 1;    ///< +1 = plus, -1 = minus, 0 = both
};

#endif  // IVCOORD_COMMAND_H
