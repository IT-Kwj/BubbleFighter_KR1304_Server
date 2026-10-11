#include "LocalDynamicConfiguration.h"
#include "Protocol/ContentConfiguration.h"
#include "Protocol/ZlibStoredStream.h"

namespace BubbleFighter::Server::Features::Configuration
{
    Protocol::DynamicConfigResponse LocalDynamicConfiguration::Respond(const Protocol::DynamicConfigRequest& request)
    {
        using namespace Protocol;
        // Local policy for this client/resource set, not recovered production
        // settings: four scene characters, no preview overrides, no extra time
        // offset, and a positive local gauge maximum of 100. See E037.
        static const auto compressed = ZlibStoredStream::Encode(ContentConfiguration{
            u"Ui/DlgCreateUser3.ui", {10, 19, 25, 27}, {}, 0, 100}.Encode());
        static const auto checksum = ZlibStoredStream::Adler32(compressed, 0);
        DynamicConfigResponse response;
        const auto& cached = request.Checksums[ContentConfiguration::Category];
        if (cached && *cached != checksum) { response.Records[ContentConfiguration::Category] = compressed; }
        return response;
    }
}
