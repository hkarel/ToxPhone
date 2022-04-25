#pragma once

#include "pproto/commands/base.h"

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
DECL_ERROR_CODE(tox_err_friend_add,            0, "d850be4b-c8d4-44f1-b99e-e1dc0657be31", "TOX_ERR_FRIEND_ADD")

DECL_ERROR_CODE(save_tox_state,               10, "378ff889-099b-485b-aed9-340c9d902b80", QT_TRANSLATE_NOOP("ToxNet", "An error occurred when saved tox state"))
DECL_ERROR_CODE(save_tox_profile,             10, "486cd166-f6af-49c9-8247-a2f3b41750f4", QT_TRANSLATE_NOOP("ToxNet", "An error occurred when saved tox profile"))
DECL_ERROR_CODE(save_tox_avatar,              10, "5ca769a6-2263-4ed3-bab3-1e66a44115c7", QT_TRANSLATE_NOOP("ToxNet", "An error occurred when saved tox avatar"))
DECL_ERROR_CODE(tox_id_length,                10, "4ccc0fd2-5741-497a-aae3-408f14a36e56", QT_TRANSLATE_NOOP("ToxNet", "Invalid Tox ID length, should be %1 symbols"))
DECL_ERROR_CODE(tox_id_format,                10, "0afdd1c3-3444-4b9f-ae98-78fbbb51eb43", QT_TRANSLATE_NOOP("ToxNet", "Invalid Tox ID format"))
DECL_ERROR_CODE(tox_id_checksumm,             10, "f4e76704-9842-4b0d-b3bc-09381bd99c55", QT_TRANSLATE_NOOP("ToxNet", "Invalid Tox ID checksumm"))
DECL_ERROR_CODE(tox_message_request,          10, "41bb9ecb-f44b-4c50-a585-f4783390e463", QT_TRANSLATE_NOOP("ToxNet", "You need to write a message with your request"))
DECL_ERROR_CODE(tox_message_too_long,         10, "d4b84abf-108a-4c40-8523-6c9defa4cdd9", QT_TRANSLATE_NOOP("ToxNet", "Your message is too long! Maximum length %1 symbols"))
DECL_ERROR_CODE(tox_friend_already_addedg,    10, "23593c3b-6368-41b1-9c69-84c17f44a28b", QT_TRANSLATE_NOOP("ToxNet", "Friend is already added"))
DECL_ERROR_CODE(tox_request_friendship,       10, "74afb540-6c9b-42cb-9223-fcf62b5be2ba", QT_TRANSLATE_NOOP("ToxNet", "Failed to request friendship. Friend id: %1"))
DECL_ERROR_CODE(tox_add_friend,               10, "96589658-ea9e-4e2b-b4b8-7a474ae7047b", QT_TRANSLATE_NOOP("ToxNet", "Failed add friend"))
DECL_ERROR_CODE(tox_remove_friend,            10, "d0737547-9a1b-4bff-970b-c1325c849e64", QT_TRANSLATE_NOOP("ToxNet", "Failed remove friend %1"))
DECL_ERROR_CODE(tox_friend_found,             10, "4d322162-ec06-4567-9249-0ab8cdc9409a", QT_TRANSLATE_NOOP("ToxNet", "Friend %1 not found"))

DECL_ERROR_CODE(save_diverter_state,          20, "8d1b2f14-4330-4ed3-8ac4-303e689db6df", QT_TRANSLATE_NOOP("ToxPhoneAppl", "Failed save a diverter state"))
DECL_ERROR_CODE(change_settings_diverter,     20, "bb76ae22-d123-465d-bcd5-b85f47ec98df", QT_TRANSLATE_NOOP("ToxPhoneAppl", "Impossible to change settings of a diverter when handset is on"))
DECL_ERROR_CODE(change_settings_diverter2,    20, "f392cd66-1c5c-48ff-b58f-192a10276fd7", QT_TRANSLATE_NOOP("ToxPhoneAppl", "Impossible to change settings of a diverter during a call"))

DECL_ERROR_CODE(ringtone_test_impossible,     20, "ba71e5e2-578c-4cb1-b06e-695b445165d3", QT_TRANSLATE_NOOP("ToxPhoneAppl", "Ringtone test is impossible when handset is on"))
DECL_ERROR_CODE(decrypt_password,             20, "17ebf8eb-d7cc-4960-9161-48b15521aed0", QT_TRANSLATE_NOOP("ToxPhoneAppl", "Failed decrypt password"))
DECL_ERROR_CODE(generate_password_hash,       20, "15fb33fd-539f-4de0-96c2-35068eba8179", QT_TRANSLATE_NOOP("ToxPhoneAppl", "Failed generate password-hash"))
DECL_ERROR_CODE(mismatch_passwords,           20, "006c8bf5-540f-4c82-b38e-91d54fe525bc", QT_TRANSLATE_NOOP("ToxPhoneAppl", "Authorization failed. Mismatch of passwords. Code error: 1"))
DECL_ERROR_CODE(mismatch_passwords2,          20, "dfb6dcba-e4fe-4dfe-aeb0-7ba0142cfa64", QT_TRANSLATE_NOOP("ToxPhoneAppl", "Authorization failed. Mismatch of passwords. Code error: 2"))

} // namespace error
} // namespace pproto
