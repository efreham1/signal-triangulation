#ifndef SERVER_H
#define SERVER_H

#include "../core/TriangulationService.h"
#include <string>
#include <memory>
#include <functional>

namespace network {

/**
 * @class IConnectionHandler
 * @brief Interface for handling client connections
 */
class IConnectionHandler {
public:
    virtual ~IConnectionHandler() = default;
    virtual void handleClient(int clientSocket) = 0;
};

/**
 * @class IMessageParser
 * @brief Interface for parsing incoming messages
 */
class IMessageParser {
public:
    virtual ~IMessageParser() = default;
    virtual bool parse(const std::string& message, core::DataPoint& out_dataPoint) = 0;
};

/**
 * @class Server
 * @brief Network server managing client connections and data flow
 */
class Server {
public:
    Server(const std::string& address, int port);
    ~Server();

    /**
     * @brief Sets the connection handler implementation
     */
    void setConnectionHandler(std::unique_ptr<IConnectionHandler> handler);

    /**
     * @brief Sets the message parser implementation
     */
    void setMessageParser(std::unique_ptr<IMessageParser> parser);

    /**
     * @brief Sets the triangulation service
     */
    void setTriangulationService(std::shared_ptr<core::TriangulationService> service);

    /**
     * @brief Starts the server
     * @return true if server started successfully
     */
    bool start();

    /**
     * @brief Stops the server
     */
    void stop();

private:
    std::string m_address;
    int m_port;
    int m_serverSocket;
    bool m_running;
    
    std::unique_ptr<IConnectionHandler> m_connectionHandler;
    std::unique_ptr<IMessageParser> m_messageParser;
    std::shared_ptr<core::TriangulationService> m_triangulationService;
};

} // namespace network

#endif // SERVER_H