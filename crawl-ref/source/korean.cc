#include "AppHdr.h"

#include "korean.h"
#include "actor.h"

std::string translate_verb(const actor *actor, const std::string &verb)
{
#ifndef KR
    return actor ? actor->conj_verb(verb).c_str() : verb;
#endif
    return gettext((verb + "-verb").c_str());
}

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



