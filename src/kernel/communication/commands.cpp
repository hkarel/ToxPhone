#include "commands.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/commands_pool.h"

namespace communication {
namespace command {

#define REGISTRY_COMMAND(COMMAND, UUID) \
    const QUuidEx COMMAND = CommandsPool::Registry{UUID, #COMMAND};

REGISTRY_COMMAND(ToxPhoneInfo,    "936920c9-f5eb-43f5-8b4b-b00ed260f9d6")
REGISTRY_COMMAND(ApplShutdown,    "ade7ce1a-564f-4436-940d-c0899adac616")

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

} // namespace data
} // namespace communication
