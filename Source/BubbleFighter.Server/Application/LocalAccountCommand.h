#pragma once

namespace BubbleFighter::Server::Application
{
    class LocalAccountCommand
    {
    public:
        static bool IsRequested(int argc, char* argv[]);
        int Run(int argc, char* argv[]) const;
    };
}
