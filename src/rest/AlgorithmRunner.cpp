#include "AlgorithmRunner.h"
#include <nlohmann/json.hpp>
#include <iostream>

namespace rest
{

    std::string AlgorithmRunner::runFromJsons(const std::vector<std::string> &json_inputs)
    {
        // Print number of inputs received
        std::cout << "Running algorithm on " << json_inputs.size() << " JSON inputs." << std::endl;
    
        return "{}"; // Placeholder implementation
    }

} // namespace rest