#ifndef DATABASE_HANDLER_H
#define DATABASE_HANDLER_H

#include <sqlite3.h>
#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include "DataPoint.h"

namespace core {

class DatabaseHandler {
public:
    DatabaseHandler(const std::string& dbPath);
    ~DatabaseHandler();

    /**
     * @brief Initializes the database, creating tables if they don't exist
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Stores a data point in the database
     * @param point The data point to store
     * @return true if storage was successful
     */
    bool storeDataPoint(const DataPoint& point);

    /**
     * @brief Retrieves data points within a time range
     * @param startTime Start of time range in milliseconds
     * @param endTime End of time range in milliseconds
     * @return Vector of data points
     */
    std::vector<DataPoint> getDataPointsInRange(int64_t startTime, int64_t endTime);

    /**
     * @brief Stores a run JSON blob and returns the new run id, or -1 on failure
     * @param jsonBlob The run contents as a JSON array string
     * @param createdAt Timestamp in milliseconds for when the run was created
     * @return newly created run id, or -1 on error
     */
    int storeRun(const std::string& jsonBlob, int64_t createdAt);

    /**
     * @brief Retrieves the stored JSON blob for a specific run id
     * @param runId The id of the run to retrieve
     * @return JSON string if found, empty string if not found or on error
     */
    std::string getRunJsonById(int runId);

private:
    sqlite3* m_db;
    std::string m_dbPath;
    
    // Prepared statements for common operations
    sqlite3_stmt* m_insertStmt;
    sqlite3_stmt* m_insertRunStmt;
    
    bool prepareStatements();
    void finalizeStatements();
};

} // namespace core

#endif // DATABASE_HANDLER_H