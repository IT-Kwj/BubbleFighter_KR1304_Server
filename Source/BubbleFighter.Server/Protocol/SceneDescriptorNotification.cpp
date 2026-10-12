#include "SceneDescriptorNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>
namespace BubbleFighter::Server::Protocol
{
    TcpPacket SceneDescriptorNotification::Encode() const
    { PacketWriter w; Descriptor.Write(w); return {MessageId, w.TakeBytes()}; }
    SceneDescriptorNotification SceneDescriptorNotification::Decode(const TcpPacket& p)
    {
        if (p.MessageId != MessageId) { throw std::invalid_argument("Invalid scene descriptor message."); }
        PacketReader r(p.Payload); SceneDescriptorNotification m{SessionDescriptor::Read(r)}; r.RequireEnd(); return m;
    }
}
