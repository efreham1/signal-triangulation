#include "JsonSignalParser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <iostream>
#include <limits>

namespace core {

static inline std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a])) ++a;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b-a);
}

// Find a key like "latitude" inside object text and return the value text after the ':' up to ',' or '}'
static bool extractRawValue(const std::string& obj, const std::string& key, std::string& out) {
    std::string keyQuoted = '"' + key + '"';
    size_t pos = obj.find(keyQuoted);
    if (pos == std::string::npos) return false;
    pos = obj.find(':', pos + keyQuoted.size());
    if (pos == std::string::npos) return false;
    ++pos; // move past ':'
    // capture until comma or end of object
    size_t end = pos;
    bool inString = false;
    for (; end < obj.size(); ++end) {
        char c = obj[end];
        if (c == '"') {
            inString = !inString;
            // continue scanning so string value may contain commas
            continue;
        }
        if (!inString && (c == ',' || c == '}')) break;
    }
    out = trim(obj.substr(pos, end - pos));
    return true;
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        // naive unescape: remove surrounding quotes and replace common escapes
        std::string inner = s.substr(1, s.size()-2);
        std::string out; out.reserve(inner.size());
        for (size_t i = 0; i < inner.size(); ++i) {
            if (inner[i] == '\\' && i+1 < inner.size()) {
                char next = inner[i+1];
                switch (next) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default: out.push_back(next); break;
                }
                ++i;
            } else {
                out.push_back(inner[i]);
            }
        }
        return out;
    }
    return s;
}

std::vector<DataPoint> JsonSignalParser::parseFileToVector(const std::string& path)
{
    std::vector<DataPoint> out;
    std::ifstream ifs(path);
    if (!ifs) throw std::runtime_error("JsonSignalParser: cannot open file " + path);

    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();
    size_t pos = 0;

    // find first '['
    pos = content.find('[');
    if (pos == std::string::npos)
    {
        std::cout << "JsonSignalParser: no array found in file " << path << std::endl;
        return out;
    }
    ++pos;

    double zero_lat = 1000.0;
    double zero_lon = 1000.0;

    while (true) {
        // find next '{'
        size_t objStart = content.find('{', pos);
        if (objStart == std::string::npos) break;
        // find matching '}' (naive: find first '}' after objStart)
        size_t objEnd = content.find('}', objStart);
        if (objEnd == std::string::npos) break;
        std::string obj = content.substr(objStart, objEnd - objStart + 1);

        std::string rawLat, rawLon, rawRssi, rawSsid, rawTimestamp;
        bool ok = true;
        if (!extractRawValue(obj, "latitude", rawLat)) ok = false;
        if (!extractRawValue(obj, "longitude", rawLon)) ok = false;
        if (!extractRawValue(obj, "rssi", rawRssi)) ok = false;
        if (!extractRawValue(obj, "ssid", rawSsid)) ok = false;
        // timestamp field name may be "timestamp" or "timestamp_ms"; handle both
        if (!extractRawValue(obj, "timestamp", rawTimestamp)) {
            extractRawValue(obj, "timestamp_ms", rawTimestamp);
        }

        if (ok) {
            try {
                double lat = 0.0;
                double lon = 0.0;
                double rssi = 0;
                int64_t timestamp = 0;
                std::string ssid = "";

                if (!rawLat.empty()) lat = std::stod(unquote(rawLat));
                if (!rawLon.empty()) lon = std::stod(unquote(rawLon));
                if (!rawRssi.empty()) rssi = std::stoi(unquote(rawRssi));
                if (!rawTimestamp.empty()) timestamp = std::stoll(unquote(rawTimestamp));
                if (!rawSsid.empty()) ssid = unquote(rawSsid);

                if (zero_lat == 1000.0 && zero_lon == 1000.0) {
                    zero_lat = lat;
                    zero_lon = lon;
                }

                DataPoint dp(lat, lon, zero_lat, zero_lon, rssi, timestamp, ssid);
                out.push_back(dp);
            } catch (const std::exception& e) {
                std::cout << "Error parsing record: " << e.what() << std::endl;
                std::cout << "  Record content: " << obj << std::endl;
                std::cout << "  Raw values: latitude=" << rawLat << ", longitude=" << rawLon
                          << ", rssi=" << rawRssi << ", ssid=" << rawSsid << ", timestamp=" << rawTimestamp << std::endl;
                // skip malformed record
            }
        }

        pos = objEnd + 1;
    }
    return out;
}
} // namespace core
