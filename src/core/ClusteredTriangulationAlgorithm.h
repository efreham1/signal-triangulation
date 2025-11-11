#ifndef CLUSTERED_TRIANGULATION_ALGORITHM_H
#define CLUSTERED_TRIANGULATION_ALGORITHM_H

#include "TriangulationService.h"
#include "DataPoint.h"

#include <vector>

namespace core {

/**
 * @class ClusteredTriangulationAlgorithm
 * @brief Skeleton for the cluster-based triangulation algorithm described in the
 * design notes. This file provides method stubs and a minimal data model so the
 * class can be compiled and integrated; algorithmic details should be implemented
 * in subsequent iterations.
 */
class ClusteredTriangulationAlgorithm : public ITriangulationAlgorithm {
public:
    ClusteredTriangulationAlgorithm();
    ~ClusteredTriangulationAlgorithm() override;

    // ITriangulationAlgorithm interface
    bool processDataPoint(const DataPoint& point) override;
    bool calculatePosition(double& out_latitude, double& out_longitude) override;
    void reset() override;

private:
    // Storage for received measurements. Implement clustering and additional
    // state in later iterations.
    std::vector<DataPoint> m_points;

    // Internal helpers (stubs)
    void clusterData();
    void estimateAoAForCluster();
    void buildCostFunction();
};

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM_H
