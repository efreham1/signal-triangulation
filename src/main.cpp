#include "network/Server.h"
#include "core/TriangulationService.h"
#include <iostream>
#include <memory>
#include "core/ClusteredTriangulationAlgorithm.h"
#include "core/JsonSignalParser.h"
#include <iomanip>

int main(int argc, char* argv[]) {
    try {
        // Configuration (could be moved to config file)
        const std::string SERVER_ADDRESS = "127.0.0.1";
        const int SERVER_PORT = 12345;

        // Create and configure triangulation service
        auto triangulationService = std::make_shared<core::TriangulationService>();
        triangulationService->setPositionCallback(
            [](double latitude, double longitude) {
                std::cout << "New position calculated: Lat=" << std::setprecision(10) << latitude << ", Lon=" << std::setprecision(10) << longitude << std::endl;
            }
        );
        triangulationService->setAlgorithm(
            std::make_unique<core::ClusteredTriangulationAlgorithm>()
        );
        
        std::vector<core::DataPoint> dps = core::JsonSignalParser::parseFileToVector("signals_water.json");
        for (auto& dp : dps) {
            dp.computeCoordinates();
            triangulationService->addDataPoint(dp);
        }
        triangulationService->calculatePosition();

        
        // // Create server instance
        // network::Server server(SERVER_ADDRESS, SERVER_PORT);
        
        // // Configure server with necessary components
        // // These would be replaced with actual implementations
        // server.setConnectionHandler(nullptr);  // TODO: Implement connection handler
        // server.setMessageParser(nullptr);      // TODO: Implement message parser
        // server.setTriangulationService(triangulationService);

        // // Start server
        // if (!server.start()) {
        //     std::cerr << "Failed to start server" << std::endl;
        //     return 1;
        // }

        // std::cout << "Server running on " << SERVER_ADDRESS << ":" << SERVER_PORT << std::endl;
        
        // Wait for termination signal
        // TODO: Implement proper signal handling

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}