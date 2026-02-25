/**
 * @file extract_coords_command.h
 * @brief Defines the ExtractCoordsCommand class for extracting molecular coordinates.
 * @author Le Nhan Pham
 * @date 2026
 *
 * This command specifically extracts atomic coordinates from Gaussian output logs
 * and converts them into standard `.xyz` files. It handles parsing the specific
 * format blocks where coordinates reside.
 */

#ifndef EXTRACT_COORDS_COMMAND_H
#define EXTRACT_COORDS_COMMAND_H

#include "commands/icommand.h"

/**
 * @class ExtractCoordsCommand
 * @brief Command for parsing and extracting atomic coordinates from Gaussian log files.
 *
 * This command handles operations specifically targeting the extraction of final
 * or tracked geometry coordinates and supports saving them in .xyz format.
 */
class ExtractCoordsCommand : public ICommand {
public:
    std::string get_name() const override;
    std::string get_description() const override;
    void parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int execute(const CommandContext& context) override;

private:
    std::vector<std::string> specific_files;
};

#endif // EXTRACT_COORDS_COMMAND_H
