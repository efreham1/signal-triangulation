#include "TriangulationService.h"
#include <filesystem>

namespace core {

TriangulationService::TriangulationService() {
    // Create database in a data subdirectory
    std::string dataDir = "data";
    if (!std::filesystem::exists(dataDir)) {
        std::filesystem::create_directory(dataDir);
    }
    
    std::string dbPath = dataDir + "/signals.db";
    m_db = std::make_unique<DatabaseHandler>(dbPath);
    m_db->initialize();
}

void TriangulationService::setAlgorithm(std::unique_ptr<ITriangulationAlgorithm> algorithm) {
    m_algorithm = std::move(algorithm);
}

void TriangulationService::setPositionCallback(PositionCallback callback) {
    m_positionCallback = std::move(callback);
}

bool TriangulationService::addDataPoint(const DataPoint& dataPoint) {
    // Store in database
    if (!m_db->storeDataPoint(dataPoint)) {
        return false;
    }

    // Process with algorithm if available
    if (m_algorithm) {
        if (!m_algorithm->processDataPoint(dataPoint)) {
            return false;
        }
    }

    return true;
}

bool TriangulationService::calculatePosition() {
    if (!m_algorithm || !m_positionCallback) {
        return false;
    }

    double latitude, longitude;
    if (!m_algorithm->calculatePosition(latitude, longitude)) {
        return false;
    }

    m_positionCallback(latitude, longitude);
    return true;
}

void TriangulationService::reset() {
    if (m_algorithm) {
        m_algorithm->reset();
    }
}

} // namespace core