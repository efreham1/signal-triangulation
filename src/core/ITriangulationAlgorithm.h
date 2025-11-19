#ifndef I_TRIANGULATION_ALGORITHM_H
#define I_TRIANGULATION_ALGORITHM_H

#include "DataPoint.h"

#include <vector>
#include <memory>
#include <functional>

namespace core
{

    /**
     * @class ITriangulationAlgorithm
     * @brief Interface for implementing different triangulation algorithms
     */
    class ITriangulationAlgorithm
    {
    public:
        virtual ~ITriangulationAlgorithm() = default;
        virtual bool processDataPoint(const DataPoint &point) = 0;
        virtual bool calculatePosition(double &out_latitude, double &out_longitude) = 0;
        virtual void reset() = 0;
    };

} // namespace core

#endif // I_TRIANGULATION_ALGORITHM_H