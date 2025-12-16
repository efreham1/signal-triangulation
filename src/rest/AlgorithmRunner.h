#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace rest
{

    class AlgorithmRunner
    {
    public:
        static std::string runFromJsons(const std::vector<std::string> &json_inputs);
    };


} // namespace rest