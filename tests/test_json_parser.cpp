#include <gtest/gtest.h>
#include "../src/core/JsonSignalParser.h"
#include <fstream>
#include <cstdio>

// Helper class to create temporary JSON files for testing
class TempJsonFile
{
public:
    std::string path;

    TempJsonFile(const std::string &content)
    {
        char tmp_name[L_tmpnam];
        std::tmpnam(tmp_name);
        path = std::string(tmp_name);
        std::ofstream f(path);
        f << content;
        f.close();
    }

    ~TempJsonFile()
    {
        std::remove(path.c_str());
    }
};

// ====================
// parseFileToVector Tests
// ====================

TEST(JsonSignalParser, ParseFileToVector_ValidFile)
{
    TempJsonFile file(R"({
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9,
                "rssi": -50,
                "timestamp": 1234567890,
                "ssid": "TestNetwork",
                "deviceID": "device1"
            },
            {
                "latitude": 57.71,
                "longitude": 11.91,
                "rssi": -60,
                "timestamp": 1234567900,
                "ssid": "TestNetwork",
                "deviceID": "device1"
            }
        ]
    })");

    auto points = core::JsonSignalParser::parseFileToVector(file.path);

    ASSERT_EQ(points.size(), 2u);

    // First point should be at origin (zero_lat/lon set from first entry)
    EXPECT_NEAR(points[0].getX(), 0.0, 1e-9);
    EXPECT_NEAR(points[0].getY(), 0.0, 1e-9);
    EXPECT_EQ(points[0].rssi, -50);
    EXPECT_EQ(points[0].timestamp_ms, 1234567890);
    EXPECT_EQ(points[0].ssid, "TestNetwork");
    EXPECT_EQ(points[0].dev_id, "device1");

    // Second point should be offset
    EXPECT_NE(points[1].getX(), 0.0);
    EXPECT_NE(points[1].getY(), 0.0);
    EXPECT_EQ(points[1].rssi, -60);
}

TEST(JsonSignalParser, ParseFileToVector_SingleMeasurement)
{
    TempJsonFile file(R"({
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9,
                "rssi": -45,
                "timestamp": 1000
            }
        ]
    })");

    auto points = core::JsonSignalParser::parseFileToVector(file.path);

    ASSERT_EQ(points.size(), 1u);
    EXPECT_EQ(points[0].rssi, -45);
    EXPECT_EQ(points[0].timestamp_ms, 1000);
}

TEST(JsonSignalParser, ParseFileToVector_MissingOptionalFields)
{
    TempJsonFile file(R"({
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9
            }
        ]
    })");

    auto points = core::JsonSignalParser::parseFileToVector(file.path);

    ASSERT_EQ(points.size(), 1u);
    EXPECT_EQ(points[0].rssi, 0);          // Default value
    EXPECT_EQ(points[0].timestamp_ms, 0);  // Default value
    EXPECT_TRUE(points[0].ssid.empty());   // Default empty
    EXPECT_TRUE(points[0].dev_id.empty()); // Default empty
}

TEST(JsonSignalParser, ParseFileToVector_FileNotFound)
{
    EXPECT_THROW(
        core::JsonSignalParser::parseFileToVector("/nonexistent/path/file.json"),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToVector_InvalidJson)
{
    TempJsonFile file("{ not valid json }}}");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToVector(file.path),
        std::exception); // nlohmann::json throws parse_error
}

TEST(JsonSignalParser, ParseFileToVector_MissingMeasurementsArray)
{
    TempJsonFile file(R"({
        "source_pos": { "x": 57.0, "y": 11.0 }
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToVector(file.path),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToVector_EmptyMeasurementsArray)
{
    TempJsonFile file(R"({
        "measurements": []
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToVector(file.path),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToVector_MeasurementsNotArray)
{
    TempJsonFile file(R"({
        "measurements": "not an array"
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToVector(file.path),
        std::runtime_error);
}

// ====================
// parseFileToSourcePos Tests
// ====================

TEST(JsonSignalParser, ParseFileToSourcePos_ValidFile)
{
    TempJsonFile file(R"({
        "source_pos": {
            "x": 57.701,
            "y": 11.901
        },
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9,
                "rssi": -50,
                "timestamp": 1000
            }
        ]
    })");

    auto [x, y] = core::JsonSignalParser::parseFileToSourcePos(file.path);

    // Source position should be converted to x/y coordinates
    // relative to the first measurement's lat/lon as zero point
    EXPECT_NE(x, 0.0);
    EXPECT_NE(y, 0.0);
}

TEST(JsonSignalParser, ParseFileToSourcePos_SourceAtZeroPoint)
{
    TempJsonFile file(R"({
        "source_pos": {
            "x": 57.7,
            "y": 11.9
        },
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9,
                "rssi": -50,
                "timestamp": 1000
            }
        ]
    })");

    auto [x, y] = core::JsonSignalParser::parseFileToSourcePos(file.path);

    // Source at same location as first measurement should be at origin
    EXPECT_NEAR(x, 0.0, 1e-9);
    EXPECT_NEAR(y, 0.0, 1e-9);
}

TEST(JsonSignalParser, ParseFileToSourcePos_FileNotFound)
{
    EXPECT_THROW(
        core::JsonSignalParser::parseFileToSourcePos("/nonexistent/path/file.json"),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToSourcePos_MissingSourcePos)
{
    TempJsonFile file(R"({
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9,
                "rssi": -50,
                "timestamp": 1000
            }
        ]
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToSourcePos(file.path),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToSourcePos_SourcePosNotObject)
{
    TempJsonFile file(R"({
        "source_pos": [57.7, 11.9],
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9,
                "rssi": -50,
                "timestamp": 1000
            }
        ]
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToSourcePos(file.path),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToSourcePos_MissingXField)
{
    TempJsonFile file(R"({
        "source_pos": {
            "y": 11.9
        },
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9,
                "rssi": -50,
                "timestamp": 1000
            }
        ]
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToSourcePos(file.path),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToSourcePos_MissingYField)
{
    TempJsonFile file(R"({
        "source_pos": {
            "x": 57.7
        },
        "measurements": [
            {
                "latitude": 57.7,
                "longitude": 11.9,
                "rssi": -50,
                "timestamp": 1000
            }
        ]
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToSourcePos(file.path),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToSourcePos_MissingMeasurements)
{
    TempJsonFile file(R"({
        "source_pos": {
            "x": 57.7,
            "y": 11.9
        }
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToSourcePos(file.path),
        std::runtime_error);
}

TEST(JsonSignalParser, ParseFileToSourcePos_EmptyMeasurements)
{
    TempJsonFile file(R"({
        "source_pos": {
            "x": 57.7,
            "y": 11.9
        },
        "measurements": []
    })");

    EXPECT_THROW(
        core::JsonSignalParser::parseFileToSourcePos(file.path),
        std::runtime_error);
}

// ====================
// Integration Test with Real Recording
// ====================

#ifdef RECORDINGS_DIR
TEST(JsonSignalParser, ParseRealRecording)
{
    std::string path = std::string(RECORDINGS_DIR) + "/football.json";
    
    auto points = core::JsonSignalParser::parseFileToVector(path);
    EXPECT_GT(points.size(), 0u);
    
    // All points should have valid coordinates
    for (const auto &p : points)
    {
        EXPECT_TRUE(p.validCoordinates());
    }
    
    // First point should be at origin
    EXPECT_NEAR(points[0].getX(), 0.0, 1e-9);
    EXPECT_NEAR(points[0].getY(), 0.0, 1e-9);
    
    auto [src_x, src_y] = core::JsonSignalParser::parseFileToSourcePos(path);
    EXPECT_TRUE(std::isfinite(src_x));
    EXPECT_TRUE(std::isfinite(src_y));
}
#endif
