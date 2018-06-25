#pragma once
#include "toxcore/tox.h"
#include "toxav/toxav.h"

const char* toxError(TOX_ERR_NEW);
const char* toxError(TOXAV_ERR_CALL);
const char* toxError(TOXAV_ERR_ANSWER);
const char* toxError(TOXAV_ERR_CALL_CONTROL);
const char* toxError(TOXAV_ERR_SEND_FRAME);
const char* toxError(TOX_ERR_FRIEND_CUSTOM_PACKET);

