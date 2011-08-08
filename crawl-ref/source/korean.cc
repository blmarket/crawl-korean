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


