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
        throw std::runtime_error("Server not properly configured");
    }
    
    // Implementation to be provided by specific platform
    return false;
}

void Server::stop() {
    m_running = false;
    // Implementation to be provided by specific platform
}

} // namespace network