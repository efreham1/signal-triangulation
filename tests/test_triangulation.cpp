#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <regex>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <numeric>
#include <iomanip>
#include "../src/core/DataPoint.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

#ifndef APP_BIN_PATH
#error "APP_BIN_PATH not defined"
#endif

#define MAX_ALLOWED_ERROR_METERS 3.0

static std::string runAppAndCapture(const std::string &full_command)
{
    std::string cmd_with_stderr = full_command + " 2>&1";
    
    std::string output;
    FILE *pipe = popen(cmd_with_stderr.c_str(), "r");
    if (!pipe)
        return {};
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        output += buffer;
    int exitCode = pclose(pipe);
    
    if (exitCode != 0)
    {
        std::cerr << "[DEBUG] Command failed with exit code " << exitCode << ": " << full_command << std::endl;
        return {};
    }
    return output;
}

std::string g_single_file_path;

// Helper to calculate error for a single file
// Returns -1.0 on failure (parsing/running)
double calculateErrorForFile(const std::string &filePath)
{
    if (!fs::exists(filePath))
    {
        std::cerr << "[DEBUG] File does not exist: " << filePath << std::endl;
        return -1.0;
    }

    // 1. Get Ground Truth
    std::ifstream ifs(filePath);
    if (!ifs)
        return -1.0;
    json j;
    try
    {
        ifs >> j;
    }
    catch (const nlohmann::json::exception &e)
    {
        std::cerr << "[DEBUG] JSON parse failed: " << filePath << std::endl;
        return -1.0;
    }

    if (!j["source_pos"].contains("x") || !j["source_pos"].contains("y"))
    {
        std::cerr << "[DEBUG] Missing 'x' or 'y' in 'source_pos' in JSON: " << filePath << std::endl;
        return -1.0;
    }
    double srcLat = j["source_pos"]["x"].get<double>();
    double srcLon = j["source_pos"]["y"].get<double>();

    // Run app and capture stdout (expects the line: "New position calculated: Lat=..., Lon=...")
    std::string full_command = std::string(APP_BIN_PATH) + " --signals-file " + filePath;
    std::string out = runAppAndCapture(full_command);
    
    std::regex re(R"(Calculated Position: Latitude\s*=\s*([0-9\.\-eE]+)\s*,\s*Longitude\s*=\s*([0-9\.\-eE]+))");
    std::smatch m;
    if (out.empty())
    {
        std::cerr << "[DEBUG] No output from app for file: " << filePath << std::endl;
        return -1.0;
    }
    if (!std::regex_search(out, m, re) || m.size() < 3)
    {
        std::cerr << "[DEBUG] Regex failed to match output for: " << filePath << "\nOutput was:\n"
                  << out << std::endl;
        return -1.0;
    }

    double lat = std::stod(m[1].str());
    double lon = std::stod(m[2].str());

    return core::distanceBetween(srcLat, srcLon, lat, lon);
}

TEST(Triangulation, SingleFileErrorCheck)
{
    if (g_single_file_path.empty())
    {
        GTEST_SKIP() << "g_single_file_path not set";
    }

    double err = calculateErrorForFile(g_single_file_path);

    ASSERT_GE(err, 0.0) << "Failed to calculate error for file: " << g_single_file_path;

    std::cout << "Global Average Error: " << std::fixed << std::setprecision(4) << err << " m\n";

    EXPECT_LT(err, MAX_ALLOWED_ERROR_METERS) << "Error is too high!";
}

// New Global Summary Test
TEST(Triangulation, GlobalSummary)
{
#ifdef RECORDINGS_DIR
    std::string recDir = RECORDINGS_DIR;
#else
    GTEST_SKIP() << "RECORDINGS_DIR not defined";
#endif

    ASSERT_TRUE(fs::exists(recDir)) << "Recordings dir not found: " << recDir;

    struct Result
    {
        std::string name;
        double error;
    };
    std::vector<Result> results;
    std::vector<double> errors;

    // Iterate all JSON files
    for (const auto &entry : fs::directory_iterator(recDir))
    {
        if (entry.path().extension() == ".json")
        {
            double err = calculateErrorForFile(entry.path().string());
            if (err >= 0.0)
            {
                results.push_back({entry.path().filename().string(), err});
                errors.push_back(err);
            }
        }
    }

    if (results.empty())
    {
        std::cout << "[   WARN   ] No valid recordings found in " << recDir << std::endl;
        GTEST_SKIP() << "No valid recordings found" << recDir;
    }

    // Truncate long names for better table formatting
    for (auto &r : results)
    {
        if (r.name.length() > 34)
        {
            r.name = "..." + r.name.substr(r.name.length() - 31);
        }
    }

    // Print Summary Table
    std::cout << "\n"
              << std::string(60, '-') << "\n";
    std::cout << std::left << std::setw(35) << "File"
              << "| " << std::setw(10) << "Error (m)" << "| Status\n";
    std::cout << std::string(60, '-') << "\n";

    for (const auto &r : results)
    {
        std::cout << std::left << std::setw(35) << r.name
                  << "| " << std::fixed << std::setprecision(2) << std::setw(10) << r.error
                  << "| " << (r.error < MAX_ALLOWED_ERROR_METERS ? "OK" : "HIGH") << "\n";
    }
    std::cout << std::string(60, '-') << "\n";

    // Calculate Average
    double sum = std::accumulate(errors.begin(), errors.end(), 0.0);
    double avg = sum / (double)errors.size();

    std::cout << "Global Average Error: " << std::fixed << std::setprecision(2) << avg
              << " m (across " << errors.size() << " files)\n";
    std::cout << std::string(60, '-') << "\n";

    EXPECT_LT(avg, MAX_ALLOWED_ERROR_METERS) << "Global average error is too high!";
}

int main (int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--run-single-file") {
            if (i + 1 < argc) {
                g_single_file_path = argv[++i];
            }
        }
    }

    // Print full command for debugging
    std::cout << "[DEBUG] Running tests" << std::endl;

    return RUN_ALL_TESTS();
}