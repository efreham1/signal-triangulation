#include "DatabaseHandler.h"
#include <stdexcept>

namespace core {

DatabaseHandler::DatabaseHandler(const std::string& dbPath)
    : m_db(nullptr)
    , m_dbPath(dbPath)
    , m_insertStmt(nullptr) {
}

DatabaseHandler::~DatabaseHandler() {
    finalizeStatements();
    if (m_db) {
        sqlite3_close(m_db);
    }
}

bool DatabaseHandler::initialize() {
    int rc = sqlite3_open(m_dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        return false;
    }

    // Create tables if they don't exist
    const char* createTableSQL = 
        "CREATE TABLE IF NOT EXISTS data_points ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "latitude REAL NOT NULL,"
        "longitude REAL NOT NULL,"
        "rssi INTEGER NOT NULL,"
        "timestamp_ms INTEGER NOT NULL"
        ");";

    char* errMsg = nullptr;
    rc = sqlite3_exec(m_db, createTableSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return false;
    }

    // Create runs table to store each run as a JSON blob
    const char* createRunsSQL =
        "CREATE TABLE IF NOT EXISTS runs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "created_at INTEGER NOT NULL,"
        "data TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(m_db, createRunsSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return false;
    }

    return prepareStatements();
}

bool DatabaseHandler::prepareStatements() {
    const char* insertSQL = 
        "INSERT INTO data_points (latitude, longitude, rssi, timestamp_ms) "
        "VALUES (?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(m_db, insertSQL, -1, &m_insertStmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    const char* insertRunSQL =
        "INSERT INTO runs (created_at, data) VALUES (?, ?);";

    rc = sqlite3_prepare_v2(m_db, insertRunSQL, -1, &m_insertRunStmt, nullptr);
    return rc == SQLITE_OK;
}

void DatabaseHandler::finalizeStatements() {
    if (m_insertStmt) {
        sqlite3_finalize(m_insertStmt);
        m_insertStmt = nullptr;
    }
    if (m_insertRunStmt) {
        sqlite3_finalize(m_insertRunStmt);
        m_insertRunStmt = nullptr;
    }
}

bool DatabaseHandler::storeDataPoint(const DataPoint& point) {
    sqlite3_bind_double(m_insertStmt, 1, point.latitude);
    sqlite3_bind_double(m_insertStmt, 2, point.longitude);
    sqlite3_bind_int(m_insertStmt, 3, point.rssi);
    sqlite3_bind_int64(m_insertStmt, 4, point.timestamp_ms);

    int rc = sqlite3_step(m_insertStmt);
    sqlite3_reset(m_insertStmt);
    
    return rc == SQLITE_DONE;
}

int DatabaseHandler::storeRun(const std::string& jsonBlob, int64_t createdAt) {
    if (!m_insertRunStmt) return -1;

    sqlite3_bind_int64(m_insertRunStmt, 1, createdAt);
    sqlite3_bind_text(m_insertRunStmt, 2, jsonBlob.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(m_insertRunStmt);
    if (rc != SQLITE_DONE) {
        sqlite3_reset(m_insertRunStmt);
        return -1;
    }

    sqlite3_reset(m_insertRunStmt);
    // return last inserted row id
    sqlite3_int64 last = sqlite3_last_insert_rowid(m_db);
    return static_cast<int>(last);
}

std::string DatabaseHandler::getRunJsonById(int runId) {
    std::string result;
    const char* queryRunSQL = "SELECT data FROM runs WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, queryRunSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_int(stmt, 1, runId);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) result = reinterpret_cast<const char*>(text);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<DataPoint> DatabaseHandler::getDataPointsInRange(int64_t startTime, int64_t endTime) {
    std::vector<DataPoint> results;
    
    const char* querySQL = 
        "SELECT latitude, longitude, rssi, timestamp_ms "
        "FROM data_points "
        "WHERE timestamp_ms BETWEEN ? AND ? "
        "ORDER BY timestamp_ms;";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, querySQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return results;
    }

    sqlite3_bind_int64(stmt, 1, startTime);
    sqlite3_bind_int64(stmt, 2, endTime);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        double lat = sqlite3_column_double(stmt, 0);
        double lon = sqlite3_column_double(stmt, 1);
        int rssi = sqlite3_column_int(stmt, 2);
        int64_t ts = sqlite3_column_int64(stmt, 3);

        results.emplace_back(lat, lon, rssi, ts);
    }

    sqlite3_finalize(stmt);
    return results;
}

} // namespace core