#pragma once

#include "toxcore/tox.h"
#include "toxav/toxav.h"
#include "shared/qt/communication/commands_base.h"

communication::data::MessageError toxError(TOX_ERR_NEW);
communication::data::MessageError toxError(TOXAV_ERR_CALL);
communication::data::MessageError toxError(TOXAV_ERR_ANSWER);
communication::data::MessageError toxError(TOXAV_ERR_CALL_CONTROL);
communication::data::MessageError toxError(TOXAV_ERR_SEND_FRAME);
communication::data::MessageError toxError(TOX_ERR_FRIEND_CUSTOM_PACKET);
communication::data::MessageError toxError(TOX_ERR_FILE_SEND);
communication::data::MessageError toxError(TOX_ERR_FILE_SEND_CHUNK);

