#include "network/Server.h"
#include "core/TriangulationService.h"
#include <iostream>
#include <memory>
#include "core/JsonSignalParser.h"
#include "core/ClusteredTriangulationAlgorithm.h"
#include <iomanip>
// test helper
#include "tools/plane_test.h"

int main(int argc, char *argv[])
{
    // Special test mode
    if (argc == 2 && std::string(argv[1]) == "--run-plane-test") {
        // run_plane_fit_test expects a double parameter (e.g., tolerance); pass a reasonable default
        bool ok = tools::run_plane_fit_test(1e-6);
        return ok ? 0 : 2;
    }

    // Fetch json file from first argument
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <signals_file.json>" << std::endl;
        std::cerr << "Or: " << argv[0] << " --run-plane-test" << std::endl;
        return 1;
    }

    const std::string signalsFile = argv[1];

    try
    {
        // Create and configure triangulation service
        auto triangulationService = std::make_shared<core::TriangulationService>();
        triangulationService->setPositionCallback(
            [](double latitude, double longitude)
            {
                std::cout << "New position calculated: Lat=" << std::setprecision(10) << latitude << ", Lon=" << std::setprecision(10) << longitude << std::endl;
            });
        triangulationService->setAlgorithm(
            std::make_unique<core::ClusteredTriangulationAlgorithm>());

        std::vector<core::DataPoint> dps = core::JsonSignalParser::parseFileToVector(signalsFile);
        for (auto &dp : dps)
        {
            dp.computeCoordinates();
            triangulationService->addDataPoint(dp);
        }
        triangulationService->calculatePosition();

        // Wait for termination signal
        // TODO: Implement proper signal handling
    }
    catch (const std::exception &eeehhh)
    {
        std::cerr << "Error: " << eeehhh.what() << std::endl;
        return 1;
    }

    return 0;
}