#include "core/ITriangulationAlgorithm.h"
#include "core/ClusteredTriangulationAlgorithm.h"
#include "core/JsonSignalParser.h"

#include <iostream>
#include <memory>
#include <iomanip>

enum class AlgorithmType
{
    CTA1,
};

int main(int argc, char *argv[])
{
    // Fetch json file from first argument
    std::string signalsFile;
    AlgorithmType algorithm_type;
    if (argc == 2)
    {
        signalsFile = argv[1];
        algorithm_type = AlgorithmType::CTA1;
    }
    else if (argc == 3)
    {
        signalsFile = argv[1];
        if (std::string(argv[2]) == "CTA1")
        {
            algorithm_type = AlgorithmType::CTA1;
        }
        else
        {
            std::cerr << "Unknown algorithm type: " << argv[2] << std::endl;
            return 1;
        }
    }
    else
    {
        std::cerr << "Usage: " << argv[0] << " <signals_file.json> [algorithm]" << std::endl;
        return 1;
    }
    try
    {
        std::unique_ptr<core::ITriangulationAlgorithm> algorithm;

        switch (algorithm_type)
        {
        case AlgorithmType::CTA1:
        {
            algorithm = std::make_unique<core::ClusteredTriangulationAlgorithm>();
        }
        break;
        default:
            std::cerr << "Unsupported algorithm type." << std::endl;
            return 1;
            break;
        }

        std::vector<core::DataPoint> dps = core::JsonSignalParser::parseFileToVector(signalsFile);
        for (auto &dp : dps)
        {
            dp.computeCoordinates();
            algorithm->processDataPoint(dp);
        }
        double latitude = 0.0;
        double longitude = 0.0;
        algorithm->calculatePosition(latitude, longitude);

        std::cout << "Calculated position: latitude=" << std::setprecision(10) << latitude << ", longitude=" << std::setprecision(10) << longitude << std::endl;

        std::pair<double, double> sourcePos = core::JsonSignalParser::parseFileToSourcePos(signalsFile);
        std::cout << "Source position from file: x=" << std::setprecision(10) << sourcePos.first << ", y=" << std::setprecision(10) << sourcePos.second << std::endl;
    }
    catch (const std::exception &eeehhh)
    {
        std::cerr << "Error: " << eeehhh.what() << std::endl;
        return 1;
    }

    return 0;
}