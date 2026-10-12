#include "ChatRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>
namespace BubbleFighter::Server::Protocol
{
    TcpPacket ChatRequest::Encode() const
    {
        PacketWriter w; w.WriteUInt32(Channel); w.WriteUtf16(Text); w.WriteUtf16(Expression);
        return {MessageId, w.TakeBytes()};
    }
    ChatRequest ChatRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid chat request."); }
        PacketReader r(packet.Payload); ChatRequest m;
        m.Channel = r.ReadUInt32(); m.Text = r.ReadUtf16(); m.Expression = r.ReadUtf16(); r.RequireEnd(); return m;
    }
}
