#pragma once

#include "toxcore/tox.h"
#include "toxav/toxav.h"
#include "pproto/commands/base.h"

bool toxError(TOX_ERR_NEW                  , pproto::data::MessageError&);
bool toxError(TOXAV_ERR_CALL               , pproto::data::MessageError&);
bool toxError(TOXAV_ERR_ANSWER             , pproto::data::MessageError&);
bool toxError(TOXAV_ERR_CALL_CONTROL       , pproto::data::MessageError&);
bool toxError(TOXAV_ERR_SEND_FRAME         , pproto::data::MessageError&);
bool toxError(TOX_ERR_FRIEND_CUSTOM_PACKET , pproto::data::MessageError&);
bool toxError(TOX_ERR_FILE_SEND            , pproto::data::MessageError&);
bool toxError(TOX_ERR_FILE_SEND_CHUNK      , pproto::data::MessageError&);
bool toxError(TOX_ERR_FRIEND_ADD           , pproto::data::MessageError&);

