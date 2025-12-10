#include "FileReceiver.h"

#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <httplib.h>

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

namespace utils
{

    namespace
    {
        std::string getLocalIPAddress()
        {
            std::string result = "0.0.0.0";
            struct ifaddrs *ifAddrStruct = nullptr;

            if (getifaddrs(&ifAddrStruct) == -1)
                return result;

            for (struct ifaddrs *ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next)
            {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                    continue;

                void *tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

                std::string addr(addressBuffer);

                // Skip loopback
                if (addr == "127.0.0.1")
                    continue;

                result = addr;
                break;
            }

            if (ifAddrStruct)
                freeifaddrs(ifAddrStruct);

            return result;
        }
    }

    FileReceiver::FileReceiver(uint16_t port, const std::string &output_dir)
        : port_(port), output_dir_(output_dir)
    {
        std::filesystem::create_directories(output_dir_);
    }

    void FileReceiver::start()
    {
        server_ = std::make_unique<httplib::Server>();

        server_->Post("/", [this](const httplib::Request &req, httplib::Response &res)
                      {
            std::string filename = "upload.bin";
            auto it = req.headers.find("X-Filename");
            if (it != req.headers.end() && !it->second.empty())
            {
                filename = it->second;
            }

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
            res.set_content("saved " + target_path.string() + "\n", "text/plain"); });

        std::string localIP = getLocalIPAddress();

        spdlog::info("===========================================");
        spdlog::info("  Polaris File Receiver");
        spdlog::info("===========================================");
        spdlog::info("  IP:     http://{}:{}/", localIP, port_);
        spdlog::info("  Output: {}", output_dir_);
        spdlog::info("===========================================");

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