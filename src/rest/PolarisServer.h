#pragma once

#include <string>
#include <memory>
#include <httplib.h>

namespace rest
{

    class PolarisServer
    {
    public:
        PolarisServer(uint16_t port, const std::string &upload_dir);
        void start();
        void stop();

    private:
        uint16_t port_;
        std::string upload_dir_;
        std::unique_ptr<httplib::Server> server_;
    };

} // namespace rest