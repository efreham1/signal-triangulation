#pragma once

#include "AlgorithmParameters.h"

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
        bool show_param_help = false;
        std::string error_message;

        // Default values
        std::string signals_file = "signals.json";
        std::string algorithm = "CTA2";
        bool plotting_enabled = false;
        double precision = 0.1;
        double cost_calculation_timeout = 60.0;

        spdlog::level::level_enum log_level = spdlog::level::info;

        // Algorithm parameters
        core::AlgorithmParameters algorithm_params;
    };

    static Result parse(int argc, char *argv[]);
    static void printHelp(const std::string &exeName);
    static void printParamHelp();

private:
    static spdlog::level::level_enum parseLogLevel(const std::string &s);
    static bool isKnownArg(const std::string &arg);
};