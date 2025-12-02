#include "CliParser.h"
#include <iostream>

CliParser::Result CliParser::parse(int argc, char *argv[])
{
    Result r;

    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];

        if (a == "--help" || a == "-h")
        {
            r.show_help = true;
            return r;
        }
        else if (a == "--signals-file" || a == "-s")
        {
            if (i + 1 >= argc)
            {
                r.valid = false;
                r.error_message = "Missing value for --signals-file";
                return r;
            }
            r.signals_file = argv[++i];
        }
        else if (a == "--algorithm" || a == "-a")
        {
            if (i + 1 >= argc)
            {
                r.valid = false;
                r.error_message = "Missing value for --algorithm";
                return r;
            }
            r.algorithm = argv[++i];
        }
        else if (a == "--precision" || a == "-p")
        {
            if (i + 1 >= argc)
            {
                r.valid = false;
                r.error_message = "Missing value for --precision";
                return r;
            }
            r.precision = std::stod(argv[++i]);
        }
        else if (a == "--timeout" || a == "-t")
        {
            if (i + 1 >= argc)
            {
                r.valid = false;
                r.error_message = "Missing value for --timeout";
                return r;
            }
            r.cost_calculation_timeout = std::stod(argv[++i]);
        }
        else if (a == "--plotting-output" || a == "-o")
        {
            r.plotting_enabled = true;
        }
        else if (a == "--log-level" || a == "-l")
        {
            if (i + 1 >= argc)
            {
                r.valid = false;
                r.error_message = "Missing value for --log-level";
                return r;
            }
            r.log_level = parseLogLevel(argv[++i]);
        }
        else
        {
            r.valid = false;
            r.error_message = "Unknown argument: " + a;
            return r;
        }
    }

    return r;
}

void CliParser::printHelp(const std::string &exeName)
{
    std::cout << "Usage: " << exeName << " [options]\n"
                                         "Options:\n"
                                         "  --signals-file, -s FILE      Path to signals JSON file\n"
                                         "  --algorithm, -a TYPE         CTA1 or CTA2\n"
                                         "  --precision, -p VALUE        Algorithm precision (default 0.1)\n"
                                         "  --timeout, -t VALUE          Timeout in seconds (default 60)\n"
                                         "  --plotting-output, -o        Enable plotting mode\n"
                                         "  --log-level, -l LEVEL        Logging level\n"
                                         "  --help, -h                   Show this help message\n"
              << std::endl;
}

spdlog::level::level_enum CliParser::parseLogLevel(const std::string &s)
{
    std::string lvl = s;
    for (char &c : lvl)
        c = char(std::tolower(c));

    if (lvl == "trace")
        return spdlog::level::trace;
    if (lvl == "debug")
        return spdlog::level::debug;
    if (lvl == "info")
        return spdlog::level::info;
    if (lvl == "warn" || lvl == "warning")
        return spdlog::level::warn;
    if (lvl == "err" || lvl == "error")
        return spdlog::level::err;
    if (lvl == "critical")
        return spdlog::level::critical;
    if (lvl == "off")
        return spdlog::level::off;

    return spdlog::level::info;
}
