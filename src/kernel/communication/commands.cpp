#include "commands.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/commands_pool.h"

namespace communication {
namespace command {

#define REGISTRY_COMMAND(COMMAND, UUID) \
    const QUuidEx COMMAND = CommandsPool::Registry{UUID, #COMMAND};

REGISTRY_COMMAND(ToxPhoneInfo,             "936920c9-f5eb-43f5-8b4b-b00ed260f9d6")
REGISTRY_COMMAND(ApplShutdown,             "ade7ce1a-564f-4436-940d-c0899adac616")
REGISTRY_COMMAND(IncomingConfigConnection, "917b1e18-4a5e-4432-b6d6-457a666ef2b1")
REGISTRY_COMMAND(ToxProfile,               "400c2df2-fba2-4dc7-a80b-78cbcec61ac4")
REGISTRY_COMMAND(RequestFriendship,        "836aeccc-8ec5-44ff-93f5-3dcb92e87a04")
REGISTRY_COMMAND(FriendRequest,            "85ebe56d-68ec-4d5e-9dbf-11b3a66d0ea3")
REGISTRY_COMMAND(FriendRequests,           "541f07b3-5af1-427e-9eba-69aa561ec22a")
REGISTRY_COMMAND(FriendItem,               "af4115ca-7563-4386-b137-cd6bb46ad5de")
REGISTRY_COMMAND(FriendList,               "922f4190-da98-4a0e-a30e-daac62180e62")
REGISTRY_COMMAND(RemoveFriend,             "e6935f6b-3064-4bdb-91e8-37f6b83fc4b6")
REGISTRY_COMMAND(DhtConnectStatus,         "b17a2b17-fbae-4be6-a28e-1693aff51eb8")

#undef REGISTRY_COMMAND
} // namespace command

namespace data {

bserial::RawVector ToxPhoneInfo::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << info;
    stream << applId;
    stream << isPointToPoint;
    B_SERIALIZE_RETURN
}

void ToxPhoneInfo::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> info;
    stream >> applId;
    stream >> isPointToPoint;
    B_DESERIALIZE_END
}

bserial::RawVector ApplShutdown::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << applId;
    B_SERIALIZE_RETURN
}

void ApplShutdown::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> applId;
    B_DESERIALIZE_END
}

bserial::RawVector ToxProfile::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << toxId;
    stream << name;
    stream << status;
    B_SERIALIZE_RETURN
}

void ToxProfile::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> toxId;
    stream >> name;
    stream >> status;
    B_DESERIALIZE_END
}

bserial::RawVector RequestFriendship::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << toxId;
    stream << message;
    B_SERIALIZE_RETURN
}

void RequestFriendship::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> toxId;
    stream >> message;
    B_DESERIALIZE_END
}

bserial::RawVector FriendRequest::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << publicKey;
    stream << message;
    stream << dateTime;
    stream << accept;
    B_SERIALIZE_RETURN
}

void FriendRequest::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> publicKey;
    stream >> message;
    stream >> dateTime;
    stream >> accept;
    B_DESERIALIZE_END
}

bserial::RawVector FriendRequests::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << list;
    B_SERIALIZE_RETURN
}

void FriendRequests::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> list;
    B_DESERIALIZE_END
}

bserial::RawVector FriendItem::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << publicKey;
    stream << changeFlaf;
    stream << name;
    stream << statusMessage;
    stream << isConnecnted;
    B_SERIALIZE_RETURN
}

void FriendItem::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> publicKey;
    stream >> changeFlaf;
    stream >> name;
    stream >> statusMessage;
    stream >> isConnecnted;
    B_DESERIALIZE_END
}

bserial::RawVector FriendList::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << list;
    B_SERIALIZE_RETURN
}

void FriendList::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> list;
    B_DESERIALIZE_END
}

bserial::RawVector RemoveFriend::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << publicKey;
    stream << name;
    B_SERIALIZE_RETURN
}

void RemoveFriend::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> publicKey;
    stream >> name;
    B_DESERIALIZE_END
}

bserial::RawVector DhtConnectStatus::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << active;
    B_SERIALIZE_RETURN
}

void DhtConnectStatus::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> active;
    B_DESERIALIZE_END
}


} // namespace data
} // namespace communication
