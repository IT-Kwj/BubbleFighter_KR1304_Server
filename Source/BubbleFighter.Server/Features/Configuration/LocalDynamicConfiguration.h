#pragma once

#include "Protocol/DynamicConfigRequest.h"
#include "Protocol/DynamicConfigResponse.h"

namespace BubbleFighter::Server::Features::Configuration
{
    class LocalDynamicConfiguration
    {
    public:
        static Protocol::DynamicConfigResponse Respond(const Protocol::DynamicConfigRequest& request);
    };
}
