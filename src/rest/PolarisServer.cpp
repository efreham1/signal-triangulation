#include "PolarisServer.h"
#include "AlgorithmRunner.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace rest
{

    PolarisServer::PolarisServer(uint16_t port, const std::string &upload_dir)
        : port_(port), upload_dir_(upload_dir)
    {
        std::filesystem::create_directories(upload_dir_);
    }

    void PolarisServer::start()
    {
        server_ = std::make_unique<httplib::Server>();

        server_->set_read_timeout(3600, 0);  // 1 hour
        server_->set_write_timeout(3600, 0); // 1 hour

        // POST /upload
        server_->Post("/upload", [this](const httplib::Request &req, httplib::Response &res)
                      {
            std::string filename = "data.json";
            auto it = req.headers.find("X-Filename");
            if (it != req.headers.end() && !it->second.empty()) {
                filename = it->second;
            }
            std::filesystem::path target_path = std::filesystem::path(upload_dir_) / filename;
            try {
                std::ofstream outfile(target_path, std::ios::binary);
                if (!outfile) {
                    std::cerr << "Failed to open file for writing: " << target_path.string() << std::endl;
                    nlohmann::json err_json;
                    err_json["error"] = "Failed to create file";
                    res.status = 500;
                    res.set_content(err_json.dump(), "application/json");
                    return;
                }
                outfile.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
                outfile.close();
            } catch (const std::exception& ex) {
                std::cerr << "Exception while saving file: " << ex.what() << std::endl;
                nlohmann::json err_json;
                err_json["error"] = ex.what();
                res.status = 500;
                res.set_content(err_json.dump(), "application/json");
                return;
            }
            std::cout << "Saved file: " << target_path.string() << " (" << req.body.size() << " bytes)" << std::endl;
            res.status = 200;
            res.set_content("saved " + target_path.string() + "\n", "text/plain"); });

        server_->Get("/files", [this](const httplib::Request &, httplib::Response &res)
                     {
            std::cout << "Listing files in upload directory: " << upload_dir_ << std::endl;
            nlohmann::json file_list = nlohmann::json::array();
            try {
                for (const auto& entry : std::filesystem::directory_iterator(upload_dir_)) {
                    if (entry.is_regular_file()) {
                        file_list.push_back(entry.path().filename().string());
                    }
                }
                res.status = 200;
                res.set_content(file_list.dump(), "application/json");
            } catch (const std::exception& ex) {
                nlohmann::json err_json;
                err_json["error"] = ex.what();
                res.status = 500;
                res.set_content(err_json.dump(), "application/json");
            } });

        // GET /run-algorithm
        server_->Get("/run-algorithm", [this](const httplib::Request &req, httplib::Response &res)
                     {
            std::cout << "Running algorithm on uploaded files" << std::endl;
            // Get 'files' query parameter
            auto files_param = req.get_param_value("files");
            if (files_param.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing 'files' query parameter\"}", "application/json");
                return;
            }

            // Split filenames by comma
            std::vector<std::string> filenames;
            size_t start = 0, end = 0;
            while ((end = files_param.find(',', start)) != std::string::npos) {
                filenames.push_back(files_param.substr(start, end - start));
                start = end + 1;
            }
            filenames.push_back(files_param.substr(start));

            std::vector<std::string> json_inputs;
            for (const auto &fname : filenames)
            {
                // Reject dangerous filenames
                if (fname.find("..") != std::string::npos || fname.find('/') != std::string::npos || fname.find('\\') != std::string::npos)
                {
                    nlohmann::json err_json;
                    err_json["error"] = "Invalid filename: " + fname;
                    res.status = 400;
                    res.set_content(err_json.dump(), "application/json");
                    return;
                }
                std::filesystem::path data_path = std::filesystem::path(upload_dir_) / fname;
                // Ensure the resolved path is within upload_dir_
                auto canonical_upload_dir = std::filesystem::canonical(upload_dir_);
                std::error_code ec;
                auto canonical_data_path = std::filesystem::weakly_canonical(data_path, ec);
                if (ec || canonical_data_path.string().find(canonical_upload_dir.string()) != 0)
                {
                    nlohmann::json err_json;
                    err_json["error"] = "Access denied for file: " + fname;
                    res.status = 403;
                    res.set_content(err_json.dump(), "application/json");
                    return;
                }
                if (!std::filesystem::exists(data_path))
                {
                    nlohmann::json err_json;
                    err_json["error"] = std::string("File not found: ") + fname;
                    res.status = 404;
                    res.set_content(err_json.dump(), "application/json");
                    return;
                }
                std::ifstream infile(data_path, std::ios::binary);
                std::string json_input((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
                json_inputs.push_back(json_input);
            }

            try {
                // Pass all JSONs to the algorithm runner
                std::string result_json = AlgorithmRunner::runFromJsons(json_inputs);
                res.status = 200;
                res.set_content(result_json, "application/json");
            } catch (const std::exception& ex) {
                nlohmann::json err_json;
                err_json["error"] = ex.what();
                res.status = 500;
                res.set_content(err_json.dump(), "application/json");
            } });

        std::cout << "===========================================" << std::endl;
        std::cout << "  Polaris REST API Server" << std::endl;
        std::cout << "===========================================" << std::endl;
        std::cout << "  POST /upload" << std::endl;
        std::cout << "  GET  /files" << std::endl;
        std::cout << "  GET  /run-algorithm" << std::endl;
        std::cout << "  Port:   " << port_ << std::endl;
        std::cout << "  Upload: " << upload_dir_ << std::endl;
        std::cout << "===========================================" << std::endl;

        server_->listen("0.0.0.0", port_);
    }

    void PolarisServer::stop()
    {
        if (server_)
        {
            server_->stop();
        }
    }

} // namespace rest