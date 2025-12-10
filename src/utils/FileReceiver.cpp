#include "FileReceiver.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <spdlog/spdlog.h>

namespace utils
{

FileReceiver::FileReceiver(uint16_t port, const std::string& output_dir)
    : port_(port), output_dir_(output_dir), running_(false)
{
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_dir_);
}

void FileReceiver::start()
{
    running_ = true;

    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        spdlog::error("Failed to create socket");
        return;
    }

    // Allow reuse of address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        spdlog::error("Failed to set socket options");
        close(server_fd);
        return;
    }

    // Bind to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        spdlog::error("Failed to bind to port {}", port_);
        close(server_fd);
        return;
    }

    // Listen
    if (listen(server_fd, 10) < 0)
    {
        spdlog::error("Failed to listen on port {}", port_);
        close(server_fd);
        return;
    }

    spdlog::info("Listening on 0.0.0.0:{}, saving to {}", port_, output_dir_);

    // Accept connections
    while (running_)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0)
        {
            if (running_)
            {
                spdlog::error("Failed to accept connection");
            }
            continue;
        }

        // Read request
        char buffer[4096];
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read <= 0)
        {
            close(client_fd);
            continue;
        }

        buffer[bytes_read] = '\0';
        std::string request(buffer);

        // Parse HTTP request
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        if (method != "POST")
        {
            // Send 405 Method Not Allowed
            const char* response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
            write(client_fd, response, strlen(response));
            close(client_fd);
            continue;
        }

        // Parse headers
        std::string line;
        size_t content_length = 0;
        std::string filename = "upload.bin";
        size_t header_end = request.find("\r\n\r\n");

        if (header_end != std::string::npos)
        {
            std::string headers = request.substr(0, header_end);
            std::istringstream header_stream(headers);
            
            while (std::getline(header_stream, line))
            {
                if (line.find("Content-Length:") == 0)
                {
                    content_length = std::stoull(line.substr(15));
                }
                else if (line.find("X-Filename:") == 0)
                {
                    filename = line.substr(11);
                    // Trim whitespace and \r
                    filename.erase(0, filename.find_first_not_of(" \t\r\n"));
                    filename.erase(filename.find_last_not_of(" \t\r\n") + 1);
                }
            }
        }

        if (content_length == 0)
        {
            // Send 400 Bad Request
            const char* response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 7\r\n\r\nNo body";
            write(client_fd, response, strlen(response));
            close(client_fd);
            continue;
        }

        // Get body (data after headers)
        size_t body_start = header_end + 4;
        size_t body_in_buffer = bytes_read - body_start;
        
        // Save file
        std::filesystem::path target_path = std::filesystem::path(output_dir_) / filename;
        std::ofstream outfile(target_path, std::ios::binary);
        
        if (!outfile)
        {
            spdlog::error("Failed to create file: {}", target_path.string());
            const char* response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
            write(client_fd, response, strlen(response));
            close(client_fd);
            continue;
        }

        // Write what we already have
        if (body_in_buffer > 0)
        {
            outfile.write(buffer + body_start, static_cast<std::streamsize>(body_in_buffer));
        }

        // Read remaining body
        size_t remaining = content_length - body_in_buffer;
        while (remaining > 0)
        {
            size_t to_read = std::min(remaining, sizeof(buffer));
            ssize_t n = read(client_fd, buffer, to_read);
            if (n <= 0)
            {
                break;
            }
            outfile.write(buffer, n);
            remaining -= static_cast<size_t>(n);
        }

        outfile.close();

        // Send success response
        std::string response_body = "saved " + target_path.string() + "\n";
        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: " + 
                              std::to_string(response_body.size()) + "\r\n\r\n" + 
                              response_body;
        write(client_fd, response.c_str(), response.size());
        
        spdlog::info("Saved file: {}", target_path.string());
        close(client_fd);
    }

    close(server_fd);
}

void FileReceiver::stop()
{
    running_ = false;
}

} // namespace utils
