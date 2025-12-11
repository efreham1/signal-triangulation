#include "rest/PolarisServer.h"
#include <iostream>
#include <cstdlib>

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