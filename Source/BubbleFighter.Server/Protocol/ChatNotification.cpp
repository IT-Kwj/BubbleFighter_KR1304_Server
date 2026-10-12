#include "ChatNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <bit>
#include <stdexcept>
namespace BubbleFighter::Server::Protocol
{
    TcpPacket ChatNotification::Encode() const
    {
        PacketWriter w; w.WriteUInt16(std::bit_cast<std::uint16_t>(Slot)); w.WriteUtf16(Sender);
        w.WriteUInt32(Channel); w.WriteUInt32(ValueAt32); w.WriteUtf16(Expression); w.WriteUtf16(Text);
        w.WriteUInt32(TextColor); w.WriteUInt32(NameColor); w.WriteUInt32(FlagAt92 ? 1U : 0U);
        w.WriteUtf16(StringAt96); w.WriteUInt32(FlagAt120 ? 1U : 0U); return {MessageId, w.TakeBytes()};
    }
    ChatNotification ChatNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid chat notification."); }
        PacketReader r(packet.Payload); ChatNotification m;
        m.Slot = std::bit_cast<std::int16_t>(r.ReadUInt16()); m.Sender = r.ReadUtf16();
        m.Channel = r.ReadUInt32(); m.ValueAt32 = r.ReadUInt32(); m.Expression = r.ReadUtf16(); m.Text = r.ReadUtf16();
        m.TextColor = r.ReadUInt32(); m.NameColor = r.ReadUInt32(); m.FlagAt92 = r.ReadUInt32() != 0;
        m.StringAt96 = r.ReadUtf16(); m.FlagAt120 = r.ReadUInt32() != 0; r.RequireEnd(); return m;
    }
}
