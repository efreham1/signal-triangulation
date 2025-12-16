#include "rest/PolarisServer.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#define LOG_FILE_PATH "logs"

void setupFileLogging()
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
    std::string filename = std::string("rest-api-server_") + buf + ".log";
    std::filesystem::path log_file_path = logs_dir / filename;

    auto file_logger = spdlog::basic_logger_mt("file_logger", log_file_path.string());
    spdlog::set_default_logger(file_logger);
    spdlog::set_level(spdlog::level::info);

    spdlog::info("REST API logging initialized.");
}

void printHelp(const char *prog_name)
{
    std::cout << "Usage: " << prog_name << " [OPTIONS]\n"
              << "\nPolaris REST API server\n"
              << "\nOptions:\n"
              << "  --port PORT       Listen port (default: 8080)\n"
              << "  --output DIR      Directory to save files (default: uploads)\n"
              << "  --help            Show this help message\n"
              << std::endl;
}

int main(int argc, char *argv[])
{
    setupFileLogging();

    uint16_t port = 8080;
    std::string output_dir = "uploads";

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            printHelp(argv[0]);
            return 0;
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        }
        else if (arg == "--output" && i + 1 < argc)
        {
            output_dir = argv[++i];
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            printHelp(argv[0]);
            return 1;
        }
    }

    rest::PolarisServer server(port, output_dir);
    server.start();

    return 0;
}