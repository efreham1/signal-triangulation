#pragma once

#include <string>
#include <cstdint>

namespace utils
{

/**
 * @brief Simple HTTP file upload receiver
 * 
 * Listens on a port and accepts POST requests with file uploads.
 * Files are saved to a specified output directory.
 */
class FileReceiver
{
public:
    /**
     * @brief Constructor
     * @param port Port to listen on
     * @param output_dir Directory where uploaded files will be saved
     */
    FileReceiver(uint16_t port, const std::string& output_dir);

    /**
     * @brief Start the server and block until stopped
     * This will listen for incoming connections and handle file uploads.
     */
    void start();

    /**
     * @brief Stop the server
     */
    void stop();

private:
    uint16_t port_;
    std::string output_dir_;
    bool running_;
};

} // namespace utils
