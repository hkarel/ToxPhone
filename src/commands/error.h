#pragma once

#include "pproto/commands/base.h"

#ifndef QTEXT
#define QTEXT(MSG) QString::fromUtf8(u8##MSG)
#endif

namespace pproto {
namespace error {

//--- TOXAV_ERR_CALL ---
DECL_ERROR_CODE(tox_err_new,                   0, "ecb029ff-eea4-4d1d-98e4-23d8d8c79da8", "TOX_ERR_NEW")
DECL_ERROR_CODE(toxav_err_call,                0, "7eff9f91-9fd2-42f0-a642-88231f69beaa", "TOXAV_ERR_CALL")
DECL_ERROR_CODE(toxav_err_answer,              0, "e2b22368-386a-4f11-8d29-cfe5cfbd91df", "TOXAV_ERR_ANSWER")
DECL_ERROR_CODE(toxav_err_call_control,        0, "606c3137-b516-40f4-8509-704aa057bfd3", "TOXAV_ERR_CALL_CONTROL")
DECL_ERROR_CODE(toxav_err_send_frame,          0, "1b7480b4-a6c4-4c9c-9a3b-ccfc4bf3c266", "TOXAV_ERR_SEND_FRAME")
DECL_ERROR_CODE(tox_err_friend_custom_packet,  0, "5487edc4-472c-4af1-a643-6e9c5115b56f", "TOX_ERR_FRIEND_CUSTOM_PACKET")
DECL_ERROR_CODE(tox_err_file_send,             0, "064a71e1-88a5-4486-8570-c7e2062197b2", "TOX_ERR_FILE_SEND")
DECL_ERROR_CODE(tox_err_file_send_chunk,       0, "2df2a88e-b035-45aa-8404-060c19f11cec", "TOX_ERR_FILE_SEND_CHUNK")

} // namespace error
} // namespace pproto
