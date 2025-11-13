#include "TriangulationService.h"
#include "DataPoint.h"

#include <utility>
#include <stdexcept>

namespace core {

TriangulationService::TriangulationService() = default;

void TriangulationService::setAlgorithm(std::unique_ptr<ITriangulationAlgorithm> algorithm)
{
    m_algorithm = std::move(algorithm);
}

void TriangulationService::setPositionCallback(PositionCallback callback)
{
    m_positionCallback = std::move(callback);
}

bool TriangulationService::addDataPoint(const DataPoint& dataPoint)
{
    if (!m_algorithm) {
        throw std::runtime_error("TriangulationService: no algorithm set");
    }

    // Delegate processing to the configured algorithm; let exceptions propagate
    return m_algorithm->processDataPoint(dataPoint);
}

bool TriangulationService::calculatePosition()
{
    if (!m_algorithm) {
        throw std::runtime_error("TriangulationService: no algorithm set");
    }

    double latitude = 0.0;
    double longitude = 0.0;

    // Delegate to algorithm; let exceptions propagate to caller
    bool ok = m_algorithm->calculatePosition(latitude, longitude);

    if (ok && m_positionCallback) {
        // Notify listener with the computed position
        m_positionCallback(latitude, longitude);
    }

    return ok;
}

void TriangulationService::reset()
{
    if (m_algorithm) {
        // Let algorithm::reset() propagate exceptions if it fails
        m_algorithm->reset();
    }
}

} // namespace core
