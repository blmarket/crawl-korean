#ifndef KOREAN_H
#define KOREAN_H

#include <string>

extern const wchar_t *korcodes;

class actor;

const char * pgettext_expr(const char *msgctxt, const char *msgid);
ucs_t korean_getindex(ucs_t c);

#endif
