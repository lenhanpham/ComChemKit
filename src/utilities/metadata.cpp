/**
 * @file metadata.cpp
 * @brief Implementation of metadata utilities
 */

#include "utilities/metadata.h"
#include "utilities/version.h"

#include <sstream>

using namespace ComChemKit;

namespace Metadata
{

    static std::string create_header_line(const std::string& content)
    {
        const size_t total_width = 61;  // Including asterisks
        std::string  line        = "* " + content;
        while (line.length() < total_width - 2)
            line += " ";
        line += " *";
        return line;
    }

    std::string header()
    {
        std::ostringstream oss;
        oss << "                                                             \n"
            << "*************************************************************\n"
            << create_header_line(ComChemKit::get_header_info()) << "\n"
            << create_header_line(COMCHEMKIT_REPOSITORY) << "\n"
            << "*************************************************************\n"
            << "                                                             \n";
        return oss.str();
    }

}  // namespace Metadata
