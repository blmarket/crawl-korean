/**
 * @file
 * @brief Conversions between Unicode and local charsets, string
 *        manipulation functions that act on character types.
**/
#ifndef UNICODE_H
#define UNICODE_H


int wctoutf8(char *d, ucs_t s);
int utf8towc(ucs_t *d, const char *s);
#ifdef TARGET_OS_WINDOWS
typedef wchar_t utf16_t;
wstring utf8_to_16(const char *s);
string utf16_to_8(const wchar_t *s);

static inline wstring utf8_to_16(const string &s)
{
    return utf8_to_16(s.c_str());
}
static inline string utf16_to_8(const wstring &s)
{
    return utf16_to_8(s.c_str());
}
#else
typedef uint16_t utf16_t;
#endif
string utf8_to_mb(const char *s);
string mb_to_utf8(const char *s);

static inline string utf8_to_mb(const string &s)
{
    return utf8_to_mb(s.c_str());
}
static inline string mb_to_utf8(const string &s)
{
    return mb_to_utf8(s.c_str());
}

int wclen(ucs_t c);

#ifndef UNIX
int wcwidth(ucs_t c);
#endif

char *prev_glyph(char *s, char *start);
char *next_glyph(char *s);

#define OUTS(x) utf8_to_mb(x).c_str()
#define OUTW(x) utf8_to_16(x).c_str()

class LineInput
{
public:
    virtual ~LineInput() {}
    virtual bool eof() = 0;
    virtual bool error() { return false; };
    virtual string get_line() = 0;
};

class FileLineInput : public LineInput
{
    enum bom_type
    {
        BOM_NORMAL, // system locale
        BOM_UTF8,
        BOM_UTF16LE,
        BOM_UTF16BE,
        BOM_UTF32LE,
        BOM_UTF32BE,
    };
    FILE *f;
    bom_type bom;
    bool seen_eof;
public:
    FileLineInput(const char *name);
    ~FileLineInput();
    bool eof() { return seen_eof || !f; };
    bool error() { return !f; };
    string get_line();
};

// The file is always UTF-8, no BOM.
// Just read it as-is, merely validating for a well-formed stream.
class UTF8FileLineInput : public LineInput
{
    FILE *f;
    bool seen_eof;
public:
    UTF8FileLineInput(const char *name);
    ~UTF8FileLineInput();
    bool eof() { return seen_eof || !f; };
    bool error() { return !f; };
    string get_line();
};

extern unsigned short charset_vt100[128];
extern unsigned short charset_cp437[256];
#endif
