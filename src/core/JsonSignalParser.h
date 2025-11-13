#ifndef JSON_SIGNAL_PARSER_H
#define JSON_SIGNAL_PARSER_H

#include "DataPoint.h"
#include <string>
#include <vector>
#include <functional>

namespace core {

class JsonSignalParser {
public:
    // Parse entire file and return vector of DataPoint. zero_lat/zero_lon are used as reference for DataPoint projection.
    static std::vector<DataPoint> parseFileToVector(const std::string& path);
};

} // namespace core

#endif // JSON_SIGNAL_PARSER_H
