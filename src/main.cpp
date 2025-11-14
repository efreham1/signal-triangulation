#include "network/Server.h"
#include "core/TriangulationService.h"
#include <iostream>
#include <memory>
#include "core/JsonSignalParser.h"
#include "core/ClusteredTriangulationAlgorithm.h"
#include <iomanip>

int main(int argc, char *argv[])
{
    // Fetch json file from first argument
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <signals_file.json>" << std::endl;
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