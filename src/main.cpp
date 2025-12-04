#include "core/ITriangulationAlgorithm.h"
#include "core/ClusteredTriangulationAlgorithm1.h"
#include "core/ClusteredTriangulationAlgorithm2.h"
#include "core/JsonSignalParser.h"
#include "core/CliParser.h"

#include <iostream>
#include <iomanip>
#include <memory>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#define LOG_FILE_PATH "logs"

void setupFileLogging(spdlog::level::level_enum log_level)
{
    std::filesystem::path logs_dir(LOG_FILE_PATH);
    if (!std::filesystem::exists(logs_dir))
    {
        std::filesystem::create_directories(logs_dir);
    }

    auto now = std::chrono::system_clock::now();
    std::time_t tnow = std::chrono::system_clock::to_time_t(now);
    std::tm tmnow;
#ifdef _WIN32
    localtime_s(&tmnow, &tnow);
#else
    localtime_r(&tnow, &tmnow);
#endif

    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d", &tmnow);
    std::string filename = std::string("signal-triangulation_") + buf + ".log";
    std::filesystem::path log_file_path = logs_dir / filename;

    auto file_logger = spdlog::basic_logger_mt("file_logger", log_file_path.string());
    spdlog::set_default_logger(file_logger);
    spdlog::set_level(log_level);

    spdlog::info("Logging initialized. level={}", spdlog::level::to_string_view(log_level));
}

int main(int argc, char *argv[])
{
    auto cli = CliParser::parse(argc, argv);

    if (cli.show_help)
    {
        CliParser::printHelp(argv[0]);
        return 0;
    }

    if (cli.show_param_help)
    {
        CliParser::printParamHelp();
        return 0;
    }

    if (!cli.valid)
    {
        std::cerr << "Error: " << cli.error_message << std::endl;
        return 1;
    }

    // Setup file logging with CLI log level
    setupFileLogging(cli.log_level);

    // Create algorithm with CLI parameters
    std::unique_ptr<core::ITriangulationAlgorithm> algorithm;

    if (cli.algorithm == "CTA2")
    {
        algorithm = std::make_unique<core::ClusteredTriangulationAlgorithm2>(cli.algorithm_params);
    }
    else if (cli.algorithm == "CTA1")
    {
        // TODO: Add parameters for CTA1 aswell
        algorithm = std::make_unique<core::ClusteredTriangulationAlgorithm1>(cli.algorithm_params);
    }
    else
    {
        std::cerr << "Unknown algorithm: " << cli.algorithm << std::endl;
        return 1;
    }

    algorithm->plottingEnabled = cli.plotting_enabled;

    // Load data
    std::vector<core::DataPoint> points;
    try
    {
        points = core::JsonSignalParser::parseFileToVector(cli.signals_file);
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Failed to parse signals: {}", ex.what());
        return 1;
    }

    // Process points
    try
    {
        for (auto &dp : points)
        {
            dp.computeCoordinates();
            algorithm->processDataPoint(dp);
        }
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Processing error: {}", ex.what());
        return 1;
    }

    // Compute position
    double lat = 0.0;
    double lon = 0.0;

    try
    {
        algorithm->calculatePosition(lat, lon, cli.precision, cli.cost_calculation_timeout);
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Calculation error: {}", ex.what());
        return 1;
    }

    if (!cli.plotting_enabled)
    {
        std::cout << "Calculated Position: Latitude = "
                  << std::setprecision(10) << lat
                  << ", Longitude = " << std::setprecision(10) << lon
                  << std::endl;
    }
    else
    {
        auto src = core::JsonSignalParser::parseFileToSourcePos(cli.signals_file);
        std::cout << "Source position from file: x="
                  << std::setprecision(10) << src.first
                  << ", y=" << std::setprecision(10) << src.second
                  << std::endl;
    }

    return 0;
}
