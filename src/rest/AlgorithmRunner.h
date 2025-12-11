#pragma once
#include <string>
#include <vector>

namespace rest
{

    class AlgorithmRunner
    {
    public:
        // Accepts a JSON string, returns a JSON string with results
        static std::string runFromJson(const std::string &json_input);
        static std::string runFromJsons(const std::vector<std::string> &json_inputs);
    };

} // namespace rest