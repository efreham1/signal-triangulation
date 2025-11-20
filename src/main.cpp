#include "core/ITriangulationAlgorithm.h"
#include "core/ClusteredTriangulationAlgorithm.h"
#include "core/JsonSignalParser.h"

#include <iostream>
#include <memory>
#include <iomanip>
#include <string>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include <chrono>
#include <ctime>

int main(int argc, char *argv[])
{
    // Default logging configuration (can be overridden via command-line)
    std::string log_level_str = "info";
    std::string signalsFile = "signals.json";
    std::string algorithmType = "CTA1";
    bool plottingEnabled = false;

    // Simple argument scan for --log-level=LEVEL, --signals-file=FILE, --algorithm=TYPE, and --plotting-output
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        const std::string lvl_prefix = "--log-level";
        if (a.rfind(lvl_prefix, 0) == 0)
        {
            log_level_str = argv[i+1];
        }
        else if (a.rfind("--signals-file", 0) == 0)
        {
            signalsFile = argv[i+1];
        }
        else if (a.rfind("--algorithm", 0) == 0)
        {
            algorithmType = argv[i+1];
        }
        else if (a == "--plotting-output")
        {
            plottingEnabled = true;
        }
    }

    try
    {
        std::filesystem::path logs_dir("logs");
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
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tmnow);
        std::string filename = std::string("signal-triangulation_") + buf + ".log";
        std::filesystem::path log_file_path = (std::filesystem::path("logs") / filename).string();

        auto file_logger = spdlog::basic_logger_mt("file_logger", log_file_path);
        spdlog::set_default_logger(file_logger);
        // map string to spdlog level (case-insensitive)
        std::string lvl = log_level_str;
        std::transform(lvl.begin(), lvl.end(), lvl.begin(), ::tolower);
        if (lvl == "trace")
            spdlog::set_level(spdlog::level::trace);
        else if (lvl == "debug")
            spdlog::set_level(spdlog::level::debug);
        else if (lvl == "info")
            spdlog::set_level(spdlog::level::info);
        else if (lvl == "warn" || lvl == "warning")
            spdlog::set_level(spdlog::level::warn);
        else if (lvl == "err" || lvl == "error")
            spdlog::set_level(spdlog::level::err);
        else if (lvl == "critical")
            spdlog::set_level(spdlog::level::critical);
        else if (lvl == "off")
            spdlog::set_level(spdlog::level::off);
        else
            spdlog::set_level(spdlog::level::info); // fallback

        spdlog::info("Logging initialized. level={}", log_level_str);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cerr << "spdlog init failed: " << ex.what() << std::endl;
    }

    std::unique_ptr<core::ITriangulationAlgorithm> algorithm;

    if (algorithmType == "CTA1")
    {
        algorithm = std::make_unique<core::ClusteredTriangulationAlgorithm>();
    }
    else
    {
        spdlog::error("Unknown algorithm type: {}", algorithmType);
        return 1;
    }

    if (plottingEnabled)
    {
        algorithm->plottingEnabled = true;
    }
    std::vector<core::DataPoint> dps;
    try
    {
        dps = core::JsonSignalParser::parseFileToVector(signalsFile);
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Failed to parse signals file '{}': {}", signalsFile, ex.what());
        return 1;
    }
    try
    {
        for (auto &dp : dps)
        {
            dp.computeCoordinates();
            algorithm->processDataPoint(dp);
        }
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Error processing data points: {}", ex.what());
        return 1;
    }

    double latitude = 0.0;
    double longitude = 0.0;

    try
    {
        algorithm->calculatePosition(latitude, longitude);
    }
    catch(const std::exception &ex)
    {
        spdlog::error("Error calculating position: {}", ex.what());
        return 1;
    }
    
    if (!plottingEnabled)
    {
        std::cout << "Calculated Position: Latitude = " << std::setprecision(10) << latitude
                  << ", Longitude = " << std::setprecision(10) << longitude << std::endl;
    }
    else
    {
        std::pair<double, double> sourcePos = core::JsonSignalParser::parseFileToSourcePos(signalsFile);
        std::cout << "Source position from file: x=" << std::setprecision(10) << sourcePos.first << ", y=" << std::setprecision(10) << sourcePos.second << std::endl;
    }

    return 0;
}