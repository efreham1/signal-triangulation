#include "FileReceiver.h"

#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <httplib.h>

namespace utils
{

FileReceiver::FileReceiver(uint16_t port, const std::string& output_dir)
    : port_(port), output_dir_(output_dir)
{
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_dir_);
}

void FileReceiver::start()
{
    server_ = std::make_unique<httplib::Server>();

    server_->Post("/", [this](const httplib::Request& req, httplib::Response& res) {
        // Resolve filename
        std::string filename = "upload.bin";
        auto it = req.headers.find("X-Filename");
        if (it != req.headers.end() && !it->second.empty())
        {
            filename = it->second;
        }

        // Ensure output directory exists
        std::filesystem::create_directories(output_dir_);
        std::filesystem::path target_path = std::filesystem::path(output_dir_) / filename;

        try
        {
            std::ofstream outfile(target_path, std::ios::binary);
            if (!outfile)
            {
                spdlog::error("Failed to open file for writing: {}", target_path.string());
                res.status = 500;
                res.set_content("Failed to create file", "text/plain");
                return;
            }

            outfile.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
            outfile.close();
        }
        catch (const std::exception& ex)
        {
            spdlog::error("Exception while saving file: {}", ex.what());
            res.status = 500;
            res.set_content("Internal server error", "text/plain");
            return;
        }

        spdlog::info("Saved file: {} ({} bytes)", target_path.string(), req.body.size());
        res.status = 200;
        res.set_content("saved " + target_path.string() + "\n", "text/plain");
    });

    spdlog::info("Listening on 0.0.0.0:{}, saving to {}", port_, output_dir_);
    server_->listen("0.0.0.0", port_);
}

void FileReceiver::stop()
{
    if (server_)
    {
        server_->stop();
    }
}

} // namespace utils
