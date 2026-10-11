#include "Application/ServerApplication.h"

int main(int argc, char* argv[])
{
    const BubbleFighter::Server::Application::ServerApplication application;
    return application.Run(argc, argv);
}
