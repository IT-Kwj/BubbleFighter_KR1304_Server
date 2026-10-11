#include "InitialHandshake.h"

#include "Protocol/ClientToServerMessage0x0001.h"
#include "Protocol/CenterListNotification.h"
#include "Protocol/GamePatchResponse.h"

#include <algorithm>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Authentication
{
    std::vector<Protocol::TcpPacket> InitialHandshake::Process(const Protocol::TcpPacket& packet)
    {
        if (state_ == InitialHandshakeState::Rejected)
        {
            throw std::runtime_error("Handshake has already failed.");
        }
        if (packet.MessageId != Protocol::ClientToServerMessage0x0001::MessageId)
        {
            // Other connection variants and login messages are not handled yet.
            // No authentication state is granted by this transport observation.
            return {};
        }
        if (state_ != InitialHandshakeState::AwaitingInitialRecord)
        {
            state_ = InitialHandshakeState::Rejected;
            throw std::runtime_error("Duplicate initial record.");
        }
        try
        {
            const auto message = Protocol::ClientToServerMessage0x0001::Decode(packet);
            // E008: exact initial profile emitted by the analyzed normal branch.
            // The first field's business meaning is still unknown.
            const bool supported = message.InitialValue == 1256 && message.IntermediateValue == 0 &&
                std::all_of(message.StringFields.begin(), message.StringFields.end(),
                    [](const auto& field) { return field.empty(); });
            if (!supported)
            {
                state_ = InitialHandshakeState::Rejected;
                return {Protocol::GamePatchResponse{1, u"Unsupported initial record."}.Encode()};
            }
            // E033: an empty gspCenterList explicitly selects the client's
            // single-center branch (0x881500) and releases its startup wait.
            std::vector<Protocol::TcpPacket> responses{
                Protocol::GamePatchResponse{0, {}}.Encode(),
                Protocol::CenterListNotification{}.Encode()};
            state_ = InitialHandshakeState::InitialRecordAccepted;
            return responses;
        }
        catch (...)
        {
            state_ = InitialHandshakeState::Rejected;
            throw;
        }
    }

    InitialHandshakeState InitialHandshake::State() const
    {
        return state_;
    }
}
