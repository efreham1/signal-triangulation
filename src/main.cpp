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

int main(int argc, char *argv[])
{
    CliParser::Result cli = CliParser::parse(argc, argv);
    if (!cli.valid)
    {
        std::cout << "Error: " << cli.error_message << std::endl;
        return 1;
    }

    if (cli.show_help)
    {
        CliParser::printHelp(argv[0]);
        return 0;
    }

    // Setup logging
    try
    {
        std::filesystem::path logs_dir(LOG_FILE_PATH);
        if (!std::filesystem::exists(logs_dir))
        {
            std::filesystem::create_directories(logs_dir);
        }

        auto now = std::chrono::system_clock::now();
        std::time_t ts = std::chrono::system_clock::to_time_t(now);
        std::tm tmnow;
#ifdef _WIN32
        localtime_s(&tmnow, &ts);
#else
        localtime_r(&ts, &tmnow);
#endif

        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d", &tmnow);

        std::string filename = std::string("signal-triangulation_") + buf + ".log";
        std::filesystem::path filepath = std::filesystem::path(LOG_FILE_PATH) / filename;

        auto file_logger = spdlog::basic_logger_mt("file_logger", filepath.string());
        spdlog::set_default_logger(file_logger);
        spdlog::set_level(cli.log_level);

        spdlog::info("Logging initialized");
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cerr << "Logging init failed: " << ex.what() << std::endl;
    }

    spdlog::info("CLI config: signals={}, algorithm={}, plotting={}, precision={}, timeout={}",
                 cli.signals_file,
                 cli.algorithm,
                 cli.plotting_enabled ? "true" : "false",
                 cli.precision,
                 cli.cost_calculation_timeout);

    // Select algorithm
    std::unique_ptr<core::ITriangulationAlgorithm> algorithm;

    if (cli.algorithm == "CTA1")
    {
        algorithm = std::make_unique<core::ClusteredTriangulationAlgorithm1>();
    }
    else if (cli.algorithm == "CTA2")
    {
        algorithm = std::make_unique<core::ClusteredTriangulationAlgorithm2>();
    }
    else
    {
        spdlog::error("Unknown algorithm type: {}", cli.algorithm);
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
