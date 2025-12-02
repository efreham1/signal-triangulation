#pragma once

#include <string>
#include <optional>
#include <spdlog/spdlog.h>

class CliParser
{
public:
    struct Result
    {
        bool valid = true;
        bool show_help = false;
        std::string error_message;

        // Default values
        std::string signals_file = "signals.json";
        std::string algorithm = "CTA2";
        bool plotting_enabled = false;
        double precision = 0.1;
        double cost_calculation_timeout = 60.0;

        spdlog::level::level_enum log_level = spdlog::level::info;
    };

    static Result parse(int argc, char *argv[]);
    static void printHelp(const std::string &exeName);

private:
    static spdlog::level::level_enum parseLogLevel(const std::string &s);
};
