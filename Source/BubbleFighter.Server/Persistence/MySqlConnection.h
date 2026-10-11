#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>

struct MYSQL;

namespace BubbleFighter::Server::Persistence
{
    class MySqlError : public std::runtime_error
    {
    public:
        explicit MySqlError(unsigned int code);
        unsigned int Code() const;
    private:
        unsigned int code_;
    };

    // A connection is owned and used by one thread. Configuration is explicit;
    // no global MySQL defaults or login-path credentials are loaded.
    class MySqlConnection
    {
    public:
        explicit MySqlConnection(const std::filesystem::path& configurationFile);
        ~MySqlConnection();
        MySqlConnection(const MySqlConnection&) = delete;
        MySqlConnection& operator=(const MySqlConnection&) = delete;
        MYSQL* Handle() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
