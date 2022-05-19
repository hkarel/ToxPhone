#pragma once

#include "pproto/commands/base.h"

namespace pproto {
namespace error {

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
