#include "AppHdr.h"

#include "korean.h"
#include "actor.h"

/// 상수 문자열이 아닌 argument로 pgettext를 호출하기 위해 실행하는 함수
const char * pgettext_expr(const char *msgctxt, const char *msgid)
{
    size_t msgctxt_len = strlen (msgctxt) + 1;
    size_t msgid_len = strlen (msgid) + 1;
    const char *translation;

    char msg_ctxt_id[msgctxt_len + msgid_len];

    memcpy (msg_ctxt_id, msgctxt, msgctxt_len - 1);
    msg_ctxt_id[msgctxt_len - 1] = '\004';
    memcpy (msg_ctxt_id + msgctxt_len, msgid, msgid_len);

    translation = gettext (msg_ctxt_id);

    if (translation != msg_ctxt_id)
        return translation;
    return msgid;
}



