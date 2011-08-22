#ifndef KOREAN_H
#define KOREAN_H

#include <string>

class actor;

std::string translate_verb(const actor *actor, const std::string &verb);
const char * pgettext_expr(const char *msgctxt, const char *msgid);

#endif
