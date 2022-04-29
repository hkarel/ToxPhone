#include "commands.h"
#include "pproto/commands/pool.h"

namespace pproto {
namespace command {

#define REGISTRY_COMMAND_SINGLPROC(COMMAND, UUID) \
    const QUuidEx COMMAND = command::Pool::Registry{UUID, #COMMAND, false};

#define REGISTRY_COMMAND_MULTIPROC(COMMAND, UUID) \
    const QUuidEx COMMAND = command::Pool::Registry{UUID, #COMMAND, true};

REGISTRY_COMMAND_MULTIPROC(ToxPhoneInfo,               "936920c9-f5eb-43f5-8b4b-b00ed260f9d6")
REGISTRY_COMMAND_MULTIPROC(IncomingConfigConnection,   "917b1e18-4a5e-4432-b6d6-457a666ef2b1")
REGISTRY_COMMAND_SINGLPROC(ToxPhoneAbout,              "fbf1ff18-54d8-4fd4-9374-5d6d50767bdc")
REGISTRY_COMMAND_SINGLPROC(ApplShutdown,               "ade7ce1a-564f-4436-940d-c0899adac616")
REGISTRY_COMMAND_SINGLPROC(ToxProfile,                 "400c2df2-fba2-4dc7-a80b-78cbcec61ac4")
REGISTRY_COMMAND_SINGLPROC(RequestFriendship,          "836aeccc-8ec5-44ff-93f5-3dcb92e87a04")
REGISTRY_COMMAND_SINGLPROC(FriendRequest,              "85ebe56d-68ec-4d5e-9dbf-11b3a66d0ea3")
REGISTRY_COMMAND_SINGLPROC(FriendRequests,             "541f07b3-5af1-427e-9eba-69aa561ec22a")
REGISTRY_COMMAND_SINGLPROC(FriendItem,                 "af4115ca-7563-4386-b137-cd6bb46ad5de")
REGISTRY_COMMAND_SINGLPROC(FriendAudioChange,          "6f0428c4-ccc1-4244-b927-a184aed4a6e0")
REGISTRY_COMMAND_SINGLPROC(FriendList,                 "922f4190-da98-4a0e-a30e-daac62180e62")
REGISTRY_COMMAND_SINGLPROC(RemoveFriend,               "e6935f6b-3064-4bdb-91e8-37f6b83fc4b6")
REGISTRY_COMMAND_SINGLPROC(DhtConnectStatus,           "b17a2b17-fbae-4be6-a28e-1693aff51eb8")
REGISTRY_COMMAND_SINGLPROC(AudioDevInfo,               "1560a37b-529c-4233-8596-5f4e5076a359")
REGISTRY_COMMAND_SINGLPROC(AudioDevChange,             "f305679b-ba1d-47d6-9769-f16c30dec6bf")
REGISTRY_COMMAND_SINGLPROC(AudioStreamInfo,            "e8a04218-3d9e-4bd1-b25f-0543829d85f0")
REGISTRY_COMMAND_SINGLPROC(AudioNoise,                 "496fc84b-3ac2-40f0-8e61-e34cd2008c04")
REGISTRY_COMMAND_SINGLPROC(AudioTest,                  "eadfcffd-c78e-4320-bd6b-8e3fcd300edb")
REGISTRY_COMMAND_SINGLPROC(AudioRecordLevel,           "5accc4e1-e489-42aa-b016-2532e3cbd471")
REGISTRY_COMMAND_SINGLPROC(ToxCallAction,              "d29c17b2-ff6d-4ea9-bc5c-dcfb0ee55162")
REGISTRY_COMMAND_MULTIPROC(ToxCallState,               "283895bf-500d-465d-9b29-8284d3e17a99")
REGISTRY_COMMAND_SINGLPROC(FriendCallEndCause,         "287fcb22-d4d6-44e2-8401-0e5be28b444d")
REGISTRY_COMMAND_SINGLPROC(ToxMessage,                 "839f2186-cb3f-4f15-9edc-caf69bdb3e49")
REGISTRY_COMMAND_SINGLPROC(DiverterInfo,               "7bfe1a78-1012-48ac-9b09-bf728eb6b9bb")
REGISTRY_COMMAND_SINGLPROC(DiverterChange,             "f09db7e2-b3e7-410e-8131-7473c35c05bd")
REGISTRY_COMMAND_SINGLPROC(DiverterTest,               "ad1dda11-2065-4bd1-9cc0-352188f1f9f1")
REGISTRY_COMMAND_MULTIPROC(PhoneFriendInfo,            "a5d53b3e-397b-47b1-a2ca-e22d02071122")
REGISTRY_COMMAND_MULTIPROC(ConfigAuthorizationRequest, "9997fcc5-e13f-4643-a7a7-091630203968")
REGISTRY_COMMAND_MULTIPROC(ConfigAuthorization,        "b0552370-b87b-4fce-b3e3-b2d0506c995c")
REGISTRY_COMMAND_SINGLPROC(ConfigSavePassword,         "ab728622-221e-4690-b74c-2d3c60b82e32")
REGISTRY_COMMAND_MULTIPROC(PlaybackFinish,             "cd6b68b6-2cda-4291-b3c8-365cf0e84828")
REGISTRY_COMMAND_SINGLPROC(DiverterHandset,            "6650b9f5-a7e1-4255-8d6d-498e03e3dcdb")

#undef REGISTRY_COMMAND_SINGLPROC
#undef REGISTRY_COMMAND_MULTIPROC
} // namespace command

namespace data {

bserial::RawVector ToxPhoneInfo::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << info;
    stream << applId;
    stream << isPointToPoint;
    stream << configConnectCount;
    B_SERIALIZE_V2(stream)
    stream << hostPoint;
    B_SERIALIZE_RETURN
}

void ToxPhoneInfo::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> info;
    stream >> applId;
    stream >> isPointToPoint;
    stream >> configConnectCount;
    B_DESERIALIZE_V2(vect, stream)
    stream >> hostPoint;
    B_DESERIALIZE_END
}

bserial::RawVector ToxPhoneAbout::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << version;
    stream << toxcore;
    stream << gitrev;
    stream << qtvers;
    stream << sodium;
    B_SERIALIZE_RETURN
}

void ToxPhoneAbout::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> version;
    stream >> toxcore;
    stream >> gitrev;
    stream >> qtvers;
    stream >> sodium;
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
    B_SERIALIZE_V2(stream)
    stream << avatar;
    B_SERIALIZE_RETURN
}

void ToxProfile::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> toxId;
    stream >> name;
    stream >> status;
    B_DESERIALIZE_V2(vect, stream)
    stream >> avatar;
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
    stream << changeFlag;
    stream << publicKey;
    stream << number;
    stream << name;
    stream << statusMessage;
    stream << isConnecnted;
    /** Дополнительные параметры для ToxPhone **/
    stream << nameAlias;
    stream << phoneNumber;
    /** Персональные аудио-установки **/
    B_SERIALIZE_V2(stream)
    stream << personalVolumes;
    stream << echoCancel;
    B_SERIALIZE_V3(stream)
    stream << avatar;
    B_SERIALIZE_RETURN
}

void FriendItem::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> changeFlag;
    stream >> publicKey;
    stream >> number;
    stream >> name;
    stream >> statusMessage;
    stream >> isConnecnted;
    /** Дополнительные параметры для ToxPhone **/
    stream >> nameAlias;
    stream >> phoneNumber;
    /** Персональные аудио-установки **/
    B_DESERIALIZE_V2(vect, stream)
    stream >> personalVolumes;
    stream >> echoCancel;
    B_DESERIALIZE_V3(vect, stream)
    stream >> avatar;
    B_DESERIALIZE_END
}

bserial::RawVector FriendAudioChange::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << changeFlag;
    stream << publicKey;
    stream << number;
    stream << value;
    B_SERIALIZE_RETURN
}

void FriendAudioChange::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> changeFlag;
    stream >> publicKey;
    stream >> number;
    stream >> value;
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

int AudioDevInfo::Find::operator()(const char* devName, const AudioDevInfo* item2) const
{
    return strcmp(devName, item2->name.constData());
}

int AudioDevInfo::Find::operator()(const QByteArray* devName, const AudioDevInfo* item2) const
{
    return strcmp(devName->constData(), item2->name.constData());
}

int AudioDevInfo::Find::operator()(const quint32* devIndex, const AudioDevInfo* item2) const
{
    return LIST_COMPARE_ITEM(*devIndex, item2->index);
}

bserial::RawVector AudioDevInfo::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << cardIndex;
    stream << type;
    stream << index;
    stream << name;
    stream << description;
    stream << channels;
    stream << baseVolume;
    stream << volume;
    stream << volumeSteps;
    stream << isCurrent;
    stream << isDefault;
    //stream << isRingtone;
    B_SERIALIZE_RETURN
}

void AudioDevInfo::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> cardIndex;
    stream >> type;
    stream >> index;
    stream >> name;
    stream >> description;
    stream >> channels;
    stream >> baseVolume;
    stream >> volume;
    stream >> volumeSteps;
    stream >> isCurrent;
    stream >> isDefault;
    //stream >> isRingtone;
    B_DESERIALIZE_END
}

AudioDevChange::AudioDevChange(const AudioDevInfo& audioDevInfo)
{
    cardIndex = audioDevInfo.cardIndex;
    type      = audioDevInfo.type;
    index     = audioDevInfo.index;
}

bserial::RawVector AudioDevChange::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << changeFlag;
    stream << cardIndex;
    stream << type;
    stream << index;
    stream << value;
    //stream << isRingtone;
    B_SERIALIZE_RETURN
}

void AudioDevChange::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> changeFlag;
    stream >> cardIndex;
    stream >> type;
    stream >> index;
    stream >> value;
    //stream >> isRingtone;
    B_DESERIALIZE_END
}

bserial::RawVector AudioStreamInfo::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << type;
    stream << state;
    stream << devIndex;
    stream << index;
    stream << name;
    stream << hasVolume;
    stream << volumeWritable;
    stream << channels;
    stream << volume;
    stream << volumeSteps;
    B_SERIALIZE_RETURN
}

void AudioStreamInfo::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> type;
    stream >> state;
    stream >> devIndex;
    stream >> index;
    stream >> name;
    stream >> hasVolume;
    stream >> volumeWritable;
    stream >> channels;
    stream >> volume;
    stream >> volumeSteps;
    B_DESERIALIZE_END
}

bserial::RawVector AudioNoise::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << filterType;
    stream << publicKey;
    stream << number;
    B_SERIALIZE_RETURN
}

void AudioNoise::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> filterType;
    stream >> publicKey;
    stream >> number;
    B_DESERIALIZE_END
}

bserial::RawVector AudioTest::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << begin;
    stream << playback;
    stream << record;
    B_SERIALIZE_RETURN
}

void AudioTest::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> begin;
    stream >> playback;
    stream >> record;
    B_DESERIALIZE_END
}

bserial::RawVector AudioRecordLevel::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << max;
    stream << time;
    B_SERIALIZE_RETURN
}

void AudioRecordLevel::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> max;
    stream >> time;
    B_DESERIALIZE_END
}

bserial::RawVector ToxCallAction::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << action;
    stream << friendNumber;
    B_SERIALIZE_RETURN
}

void ToxCallAction::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> action;
    stream >> friendNumber;
    B_DESERIALIZE_END
}

bserial::RawVector ToxCallState::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << direction;
    stream << callState;
    stream << callEnd;
    stream << friendNumber;
    B_SERIALIZE_V2(stream)
    stream << friendPublicKey;
    B_SERIALIZE_RETURN
}

void ToxCallState::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> direction;
    stream >> callState;
    stream >> callEnd;
    stream >> friendNumber;
    B_DESERIALIZE_V2(vect, stream)
    stream >> friendPublicKey;
    B_DESERIALIZE_END
}

bserial::RawVector FriendCallEndCause::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << callEnd;
    B_SERIALIZE_RETURN
}

void FriendCallEndCause::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> callEnd;
    B_DESERIALIZE_END
}

bserial::RawVector ToxMessage::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << friendNumber;
    stream << text;
    B_SERIALIZE_RETURN
}

void ToxMessage::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> friendNumber;
    stream >> text;
    B_DESERIALIZE_END
}

bserial::RawVector DiverterInfo::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << active;
    stream << attached;
    stream << defaultMode;
    stream << ringTone;
    stream << deviceUsbBus;
    stream << deviceName;
    stream << deviceVersion;
    stream << deviceSerial;
    B_SERIALIZE_V2(stream)
    stream << currentMode;
    B_SERIALIZE_RETURN
}

void DiverterInfo::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> active;
    stream >> attached;
    stream >> defaultMode;
    stream >> ringTone;
    stream >> deviceUsbBus;
    stream >> deviceName;
    stream >> deviceVersion;
    stream >> deviceSerial;
    B_DESERIALIZE_V2(vect, stream)
    stream >> currentMode;
    B_DESERIALIZE_END
}

bserial::RawVector DiverterChange::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << changeFlag;
    stream << active;
    stream << defaultMode;
    stream << ringTone;
    B_SERIALIZE_RETURN
}

void DiverterChange::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> changeFlag;
    stream >> active;
    stream >> defaultMode;
    stream >> ringTone;
    B_DESERIALIZE_END
}

bserial::RawVector DiverterTest::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << begin;
    stream << ringTone;
    B_SERIALIZE_RETURN
}

void DiverterTest::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> begin;
    stream >> ringTone;
    B_DESERIALIZE_END
}

bserial::RawVector PhoneFriendInfo::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << publicKey;
    stream << number;
    stream << name;
    stream << nameAlias;
    stream << phoneNumber;
    B_SERIALIZE_RETURN
}

void PhoneFriendInfo::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> publicKey;
    stream >> number;
    stream >> name;
    stream >> nameAlias;
    stream >> phoneNumber;
    B_DESERIALIZE_END
}

bserial::RawVector ConfigAuthorizationRequest::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << needPassword;
    B_SERIALIZE_RETURN
}

void ConfigAuthorizationRequest::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> needPassword;
    B_DESERIALIZE_END
}

bserial::RawVector ConfigAuthorization::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << password;
    B_SERIALIZE_RETURN
}

void ConfigAuthorization::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> password;
    B_DESERIALIZE_END
}

bserial::RawVector ConfigSavePassword::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << password;
    B_SERIALIZE_RETURN
}

void ConfigSavePassword::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> password;
    B_DESERIALIZE_END
}

bserial::RawVector PlaybackFinish::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << code;
    B_SERIALIZE_RETURN
}

void PlaybackFinish::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> code;
    B_DESERIALIZE_END
}

bserial::RawVector DiverterHandset::toRaw() const
{
    B_SERIALIZE_V1(stream)
    stream << on;
    B_SERIALIZE_RETURN
}

void DiverterHandset::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect, stream)
    stream >> on;
    B_DESERIALIZE_END
}

} // namespace data
} // namespace pproto
