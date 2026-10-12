#include "PlazaMovement.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        void Write(PacketWriter& w, const PlazaMovementRequest& m)
        {
            m.Validate(); w.WriteUInt32(m.Sequence);
            for (auto v : m.PositionBits) { w.WriteUInt32(v); }
            for (auto v : m.Axes) { w.WriteUInt8(v); }
            w.WriteUInt32(m.YawBits);
        }
        PlazaMovementRequest Read(PacketReader& r)
        {
            PlazaMovementRequest m; m.Sequence = r.ReadUInt32();
            for (auto& v : m.PositionBits) { v = r.ReadUInt32(); }
            for (auto& v : m.Axes) { v = r.ReadUInt8(); }
            m.YawBits = r.ReadUInt32(); m.Validate(); return m;
        }
    }
    void PlazaMovementRequest::Validate() const
    {
        // Local input bounds, not recovered map collision or original policy.
        const auto valid = [](std::uint32_t v)
        { const auto f = std::bit_cast<float>(v); return std::isfinite(f) && std::abs(f) <= 10000000.0F; };
        for (auto v : PositionBits) { if (!valid(v)) { throw std::invalid_argument("Invalid plaza position."); } }
        if (!valid(YawBits)) { throw std::invalid_argument("Invalid plaza yaw."); }
        for (auto v : Axes) { if (v != 0 && v != 1 && v != 255) { throw std::invalid_argument("Invalid plaza axis."); } }
    }
    TcpPacket PlazaMovementRequest::Encode() const
    { PacketWriter w; Write(w, *this); return {MessageId, w.TakeBytes()}; }
    PlazaMovementRequest PlazaMovementRequest::Decode(const TcpPacket& p)
    {
        if (p.MessageId != MessageId) { throw std::invalid_argument("Invalid plaza movement request."); }
        PacketReader r(p.Payload); auto m = Read(r); r.RequireEnd(); return m;
    }
    TcpPacket PlazaMovementNotification::Encode() const
    { PacketWriter w; w.WriteUInt32(PlayerId); Write(w, Movement); return {MessageId, w.TakeBytes()}; }
    PlazaMovementNotification PlazaMovementNotification::Decode(const TcpPacket& p)
    {
        if (p.MessageId != MessageId) { throw std::invalid_argument("Invalid plaza movement notification."); }
        PacketReader r(p.Payload); PlazaMovementNotification m; m.PlayerId = r.ReadUInt32();
        m.Movement = Read(r); r.RequireEnd(); return m;
    }
}
