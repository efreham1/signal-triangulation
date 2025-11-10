#include "Server.h"
#include <stdexcept>

namespace network {

Server::Server(const std::string& address, int port)
    : m_address(address)
    , m_port(port)
    , m_serverSocket(-1)
    , m_running(false)
{}

Server::~Server() {
    stop();
}

void Server::setConnectionHandler(std::unique_ptr<IConnectionHandler> handler) {
    m_connectionHandler = std::move(handler);
}

void Server::setMessageParser(std::unique_ptr<IMessageParser> parser) {
    m_messageParser = std::move(parser);
}

void Server::setTriangulationService(std::shared_ptr<core::TriangulationService> service) {
    m_triangulationService = service;
}

bool Server::start() {
    if (!m_connectionHandler || !m_messageParser || !m_triangulationService) {
        std::string missing;
        if (!m_connectionHandler) missing += "connection handler, ";
        if (!m_messageParser) missing += "message parser, ";
        if (!m_triangulationService) missing += "triangulation service, ";
        if (!missing.empty()) {
            // Remove trailing comma and space
            missing = missing.substr(0, missing.size() - 2);
        }
        throw std::runtime_error("Server not properly configured: missing " + missing);
    }
    
    // Implementation to be provided by specific platform
    return false;
}

void Server::stop() {
    m_running = false;
    // Implementation to be provided by specific platform
}

} // namespace network