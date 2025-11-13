#ifndef TRIANGULATION_SERVICE_H
#define TRIANGULATION_SERVICE_H

#include "DataPoint.h"
#include <vector>
#include <memory>
#include <functional>

namespace core {

/**
 * @class ITriangulationAlgorithm
 * @brief Interface for implementing different triangulation algorithms
 */
class ITriangulationAlgorithm {
public:
    virtual ~ITriangulationAlgorithm() = default;
    virtual bool processDataPoint(const DataPoint& point) = 0;
    virtual bool calculatePosition(double& out_latitude, double& out_longitude) = 0;
    virtual void reset() = 0;
};

/**
 * @class TriangulationService
 * @brief Manages signal source location estimation using configurable algorithms
 */
class TriangulationService {
public:
    using PositionCallback = std::function<void(double latitude, double longitude)>;
    
    TriangulationService();
    
    /**
     * @brief Sets the triangulation algorithm to use
     * @param algorithm Pointer to the algorithm implementation
     */
    void setAlgorithm(std::unique_ptr<ITriangulationAlgorithm> algorithm);
    
    /**
     * @brief Sets the callback for position updates
     * @param callback Function to call when new position is calculated
     */
    void setPositionCallback(PositionCallback callback);
    
    /**
     * @brief Processes a new data point
     * @param dataPoint The measurement data to process
     * @return true if processing was successful
     */
    bool addDataPoint(const DataPoint& dataPoint);
    
    /**
     * @brief Forces a position calculation
     * @return true if calculation was successful
     */
    bool calculatePosition();
    
    /**
     * @brief Clears all stored data points
     */
    void reset();

private:
    std::unique_ptr<ITriangulationAlgorithm> m_algorithm;
    PositionCallback m_positionCallback;
};

} // namespace core

#endif // TRIANGULATION_SERVICE_H