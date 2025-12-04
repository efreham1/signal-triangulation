#include "CliParser.h"
#include <iostream>

namespace
{
    // List of known CLI arguments (not algorithm parameters)
    const std::vector<std::string> KNOWN_ARGS = {
        "--help", "-h",
        "--param-help",
        "--signals-file", "-s",
        "--algorithm", "-a",
        "--precision", "-p",
        "--timeout", "-t",
        "--plotting-output", "-o",
        "--log-level", "-l"};

    std::string normalizeParamName(const std::string &name)
    {
        std::string normalized = name;
        for (char &c : normalized)
        {
            if (c == '-')
                c = '_';
        }
        return normalized;
    }
} // namespace

bool CliParser::isKnownArg(const std::string &arg)
{
    // Check if it's a known argument or starts with a known argument prefix
    std::string base_arg = arg;
    auto eq_pos = arg.find('=');
    if (eq_pos != std::string::npos)
    {
        base_arg = arg.substr(0, eq_pos);
    }

    for (const auto &known : KNOWN_ARGS)
    {
        if (base_arg == known)
            return true;
    }
    return false;
}

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
        else if (a == "--param-help")
        {
            r.show_param_help = true;
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
        else if (a.rfind("--", 0) == 0)
        {
            // Unknown --arg, treat as algorithm parameter
            std::string param_part = a.substr(2);
            std::string name, value;

            auto eq_pos = param_part.find('=');
            if (eq_pos != std::string::npos)
            {
                // --param-name=value format
                name = param_part.substr(0, eq_pos);
                value = param_part.substr(eq_pos + 1);
            }
            else
            {
                // --param-name value format
                name = param_part;
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    value = argv[++i];
                }
                else
                {
                    // Treat as boolean flag
                    value = "true";
                }
            }

            // Normalize name and store
            name = normalizeParamName(name);

            try
            {
                r.algorithm_params.setFromString(name, value);
            }
            catch (const std::exception &e)
            {
                r.valid = false;
                r.error_message = "Invalid parameter value for --" + name + ": " + e.what();
                return r;
            }
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
    std::cout << "Usage: " << exeName << " [options] [--param-name value ...]\n"
              << "\n"
              << "Options:\n"
              << "  --signals-file, -s FILE      Path to signals JSON file\n"
              << "  --algorithm, -a TYPE         CTA1 or CTA2\n"
              << "  --precision, -p VALUE        Algorithm precision (default 0.1)\n"
              << "  --timeout, -t VALUE          Timeout in seconds (default 60)\n"
              << "  --plotting-output, -o        Enable plotting mode\n"
              << "  --log-level, -l LEVEL        Logging level (trace/debug/info/warn/error)\n"
              << "  --param-help                 Show algorithm parameter help\n"
              << "  --help, -h                   Show this help message\n"
              << "\n"
              << "Algorithm parameters can be passed as --param-name=value or --param-name value.\n"
              << "Use --param-help to see available algorithm parameters.\n"
              << std::endl;
}

void CliParser::printParamHelp()
{
    std::cout << "Algorithm Parameters:\n"
              << "\n"
              << "Clustering:\n"
              << "  --coalition-distance FLOAT   Distance for coalescing points (default: 2.0)\n"
              << "  --cluster-min-points INT     Minimum points per cluster (default: 3)\n"
              << "  --cluster-ratio-threshold FLOAT  Ratio threshold for clustering (default: 0.25)\n"
              << "  --max-internal-distance INT  Max distance between cluster points (default: 20)\n"
              << "\n"
              << "Geometric Constraints:\n"
              << "  --min-geometric-ratio FLOAT  Minimum geometric ratio (default: 0.15)\n"
              << "  --ideal-geometric-ratio FLOAT  Ideal geometric ratio (default: 1.0)\n"
              << "  --min-area FLOAT             Minimum cluster area (default: 10.0)\n"
              << "  --ideal-area FLOAT           Ideal cluster area (default: 50.0)\n"
              << "  --max-area FLOAT             Maximum cluster area (default: 1000.0)\n"
              << "\n"
              << "RSSI:\n"
              << "  --min-rssi-variance FLOAT    Minimum RSSI variance (default: 5.0)\n"
              << "  --bottom-rssi FLOAT          Bottom RSSI threshold (default: -90.0)\n"
              << "\n"
              << "Overlap:\n"
              << "  --max-overlap FLOAT          Maximum cluster overlap 0-1 (default: 0.05)\n"
              << "\n"
              << "Weights:\n"
              << "  --weight-geometric-ratio FLOAT  Weight for geometric ratio (default: 1.0)\n"
              << "  --weight-area FLOAT          Weight for area (default: 1.0)\n"
              << "  --weight-rssi-variance FLOAT Weight for RSSI variance (default: 1.0)\n"
              << "  --weight-rssi FLOAT          Weight for RSSI (default: 1.0)\n"
              << "  --extra-weight FLOAT         Extra weight factor for cost function (default: 1.0)\n"
              << "\n"
              << "Timing:\n"
              << "  --per-seed-timeout FLOAT     Timeout per seed in seconds (default: 1.0)\n"
              << "\n"
              << "Grid Search:\n"
              << "  --grid-half-size INT         Half-size of search grid (default: 500)\n"
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