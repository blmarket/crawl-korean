/**
 * @file
 * @brief Auxilary functions to make savefile versioning simpler.
**/

/*
   The marshalling and unmarshalling of data is done in big endian and
   is meant to keep savefiles cross-platform.  Note also that the marshalling
   sizes are 1, 2, and 4 for byte, short, and int.  If a strange platform
   with different sizes of these basic types pops up, please sed it to fixed-
   width ones.  For now, that wasn't done in order to keep things convenient.
*/

#include "AppHdr.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>            // for memcpy
#include <iterator>
#include <algorithm>
#include <vector>

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "abyss.h"
#include "artefact.h"
#include "branch.h"
#include "coord.h"
#include "coordit.h"
#include "describe.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "enum.h"
#include "errors.h"
#include "map_knowledge.h"
#include "externs.h"
#include "files.h"
#include "ghost.h"
#include "itemname.h"
#include "libutil.h"
#include "mapmark.h"
#include "misc.h"
#include "mon-info.h"
#include "mon-util.h"
#include "mon-transit.h"
#include "place.h"
#include "quiver.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "state.h"
#include "env.h"
#include "syscalls.h"
#include "tags.h"
#include "tiledef-dngn.h"
#include "tiledef-player.h"
#include "tileview.h"
#ifdef USE_TILE
 #include "tilemcache.h"
 #include "tilepick.h"
#endif
#include "travel.h"

// defined in dgn-overview.cc
extern std::map<branch_type, std::set<level_id> > stair_level;
extern std::map<level_pos, shop_type> shops_present;
extern std::map<level_pos, god_type> altars_present;
extern std::map<level_pos, branch_type> portals_present;
extern std::map<level_pos, std::string> portal_notes;
extern std::map<level_id, std::string> level_annotations;
extern std::map<level_id, std::string> level_exclusions;
extern std::map<level_id, std::string> level_uniques;
extern std::set<std::pair<std::string, level_id> > auto_unique_annotations;

// defined in abyss.cc
extern abyss_state abyssal_state;

reader::reader(const std::string &_read_filename, int minorVersion)
    : _filename(_read_filename), _chunk(0), _pbuf(NULL), _read_offset(0),
      _minorVersion(minorVersion), seen_enums()
{
    _file       = fopen_u(_filename.c_str(), "rb");
    opened_file = !!_file;
}

reader::reader(package *save, const std::string &chunkname, int minorVersion)
    : _file(0), _chunk(0), opened_file(false), _pbuf(0), _read_offset(0),
     _minorVersion(minorVersion)
{
    ASSERT(save);
    _chunk = new chunk_reader(save, chunkname);
}

reader::~reader()
{
    if (_chunk)
        delete _chunk;
    close();
}

void reader::close()
{
    if (opened_file && _file)
        fclose(_file);
    _file = NULL;
}

void reader::advance(size_t offset)
{
    char junk[128];

    while (offset)
    {
        const size_t junklen = std::min(sizeof(junk), offset);
        offset -= junklen;
        read(junk, junklen);
    }
}

bool reader::valid() const
{
    return ((_file && !feof(_file)) ||
            (_pbuf && _read_offset < _pbuf->size()));
}

// Reads input in network byte order, from a file or buffer.
unsigned char reader::readByte()
{
    if (_file)
    {
        int b = fgetc(_file);
        if (b == EOF)
            throw short_read_exception();
        return b;
    }
    else if (_chunk)
    {
        unsigned char buf;
        if (_chunk->read(&buf, 1) != 1)
            throw short_read_exception();
        return buf;
    }
    else
    {
        if (_read_offset >= _pbuf->size())
            throw short_read_exception();
        return (*_pbuf)[_read_offset++];
    }
}

void reader::read(void *data, size_t size)
{
    if (_file)
    {
        if (data)
        {
            if (fread(data, 1, size, _file) != size)
                throw short_read_exception();
        }
        else
            fseek(_file, (long)size, SEEK_CUR);
    }
    else if (_chunk)
    {
        if (_chunk->read(data, size) != size)
            throw short_read_exception();
    }
    else
    {
        if (_read_offset+size > _pbuf->size())
            throw short_read_exception();
        if (data)
            memcpy(data, &(*_pbuf)[_read_offset], size);

        _read_offset += size;
    }
}

int reader::getMinorVersion() const
{
    ASSERT(_minorVersion != TAG_MINOR_INVALID);
    return _minorVersion;
}

void reader::setMinorVersion(int minorVersion)
{
    _minorVersion = minorVersion;
}

void reader::fail_if_not_eof(const std::string &name)
{
    char dummy;
    if (_chunk ? _chunk->read(&dummy, 1) :
        _file ? (fgetc(_file) != EOF) :
        _read_offset >= _pbuf->size())
    {
        fail("Incomplete read of \"%s\" - aborting.", name.c_str());
    }
}

void writer::check_ok(bool ok)
{
    if (!ok && !failed)
    {
        failed = true;
        if (!_ignore_errors)
            end(1, true, "Error writing to %s", _filename.c_str());
    }
}

void writer::writeByte(unsigned char ch)
{
    if (failed)
        return;

    if (_chunk)
        _chunk->write(&ch, 1);
    else if (_file)
        check_ok(fputc(ch, _file) != EOF);
    else
        _pbuf->push_back(ch);
}

void writer::write(const void *data, size_t size)
{
    if (failed)
        return;

    if (_chunk)
        _chunk->write(data, size);
    else if (_file)
        check_ok(fwrite(data, 1, size, _file) == size);
    else
    {
        const unsigned char* cdata = static_cast<const unsigned char*>(data);
        _pbuf->insert(_pbuf->end(), cdata, cdata+size);
    }
}

long writer::tell()
{
    ASSERT(!_chunk);
    return _file? ftell(_file) : _pbuf->size();
}


#ifdef DEBUG_GLOBALS
// Force a conditional jump valgrind may pick up, no matter the optimizations.
static volatile uint32_t hashroll;
static void CHECK_INITIALIZED(uint32_t x)
{
    hashroll = 0;
    if ((hashroll += x) & 1)
        hashroll += 2;
}
#else
#define CHECK_INITIALIZED(x)
#endif

// static helpers
static void tag_construct_char(writer &th);
static void tag_construct_you(writer &th);
static void tag_construct_you_items(writer &th);
static void tag_construct_you_dungeon(writer &th);
static void tag_construct_lost_monsters(writer &th);
static void tag_construct_lost_items(writer &th);
static void tag_read_you(reader &th);
static void tag_read_you_items(reader &th);
static void tag_read_you_dungeon(reader &th);
static void tag_read_lost_monsters(reader &th);
static void tag_read_lost_items(reader &th);

static void tag_construct_level(writer &th);
static void tag_construct_level_items(writer &th);
static void tag_construct_level_monsters(writer &th);
static void tag_construct_level_tiles(writer &th);
static void tag_read_level(reader &th);
static void tag_read_level_items(reader &th);
static void tag_read_level_monsters(reader &th);
static void tag_read_level_tiles(reader &th);
static void tag_missing_level_tiles();
static void tag_init_tile_bk();

static void tag_construct_ghost(writer &th);
static void tag_read_ghost(reader &th);

static void marshallGhost(writer &th, const ghost_demon &ghost);
static ghost_demon unmarshallGhost(reader &th);

static void marshallSpells(writer &, const monster_spells &);
static void unmarshallSpells(reader &, monster_spells &);

template<typename T, typename T_iter, typename T_marshal>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshal marshal);
template<typename T>
static void unmarshall_vector(reader& th, std::vector<T>& vec,
                              T (*T_unmarshall)(reader&));

template<int SIZE>
void marshallFixedBitVector(writer& th, const FixedBitVector<SIZE>& arr);
template<int SIZE>
void unmarshallFixedBitVector(reader& th, FixedBitVector<SIZE>& arr);

void marshallByte(writer &th, int8_t data)
{
    CHECK_INITIALIZED(data);
    th.writeByte(data);
}

int8_t unmarshallByte(reader &th)
{
    return th.readByte();
}

void marshallUByte(writer &th, uint8_t data)
{
    CHECK_INITIALIZED(data);
    th.writeByte(data);
}

uint8_t unmarshallUByte(reader &th)
{
    return th.readByte();
}

void marshallShort(std::vector<unsigned char>& buf, short data)
{
    CHECK_INITIALIZED(data);
    COMPILE_CHECK(sizeof(data) == 2);
    buf.push_back((unsigned char) ((data & 0xFF00) >> 8));
    buf.push_back((unsigned char) ((data & 0x00FF)   ));
}

// Marshall 2 byte short in network order.
void marshallShort(writer &th, short data)
{
    CHECK_INITIALIZED(data);
    const char b2 = (char)(data & 0x00FF);
    const char b1 = (char)((data & 0xFF00) >> 8);
    th.writeByte(b1);
    th.writeByte(b2);
}

// Unmarshall 2 byte short in network order.
int16_t unmarshallShort(reader &th)
{
    int16_t b1 = th.readByte();
    int16_t b2 = th.readByte();
    int16_t data = (b1 << 8) | (b2 & 0x00FF);
    return data;
}

void marshallInt(std::vector<unsigned char>& buf, int32_t data)
{
    CHECK_INITIALIZED(data);
    buf.push_back((unsigned char) ((data & 0xFF000000) >> 24));
    buf.push_back((unsigned char) ((data & 0x00FF0000) >> 16));
    buf.push_back((unsigned char) ((data & 0x0000FF00) >>  8));
    buf.push_back((unsigned char) ((data & 0x000000FF)    ));
}

// Marshall 4 byte int in network order.
void marshallInt(writer &th, int32_t data)
{
    CHECK_INITIALIZED(data);
    char b4 = (char) (data & 0x000000FF);
    char b3 = (char)((data & 0x0000FF00) >> 8);
    char b2 = (char)((data & 0x00FF0000) >> 16);
    char b1 = (char)((data & 0xFF000000) >> 24);

    th.writeByte(b1);
    th.writeByte(b2);
    th.writeByte(b3);
    th.writeByte(b4);
}

// Unmarshall 4 byte signed int in network order.
int32_t unmarshallInt(reader &th)
{
    int32_t b1 = th.readByte();
    int32_t b2 = th.readByte();
    int32_t b3 = th.readByte();
    int32_t b4 = th.readByte();

    int32_t data = (b1 << 24) | ((b2 & 0x000000FF) << 16);
    data |= ((b3 & 0x000000FF) << 8) | (b4 & 0x000000FF);
    return data;
}

void marshallUnsigned(writer& th, uint64_t v)
{
    do
    {
        unsigned char b = (unsigned char)(v & 0x7f);
        v >>= 7;
        if (v)
            b |= 0x80;
        th.writeByte(b);
    }
    while (v);
}

uint64_t unmarshallUnsigned(reader& th)
{
    unsigned i = 0;
    uint64_t v = 0;
    for (;;)
    {
        unsigned char b = th.readByte();
        v |= (uint64_t)(b & 0x7f) << i;
        i += 7;
        if (!(b & 0x80))
            break;
    }
    return v;
}

void marshallSigned(writer& th, int64_t v)
{
    if (v < 0)
        marshallUnsigned(th, (uint64_t)((-v - 1) << 1) | 1);
    else
        marshallUnsigned(th, (uint64_t)(v << 1));
}

int64_t unmarshallSigned(reader& th)
{
    uint64_t u;
    unmarshallUnsigned(th, u);
    if (u & 1)
        return (int64_t)(-(u >> 1) - 1);
    else
        return (int64_t)(u >> 1);
}

// Optimized for short vectors that have only the first few bits set, and
// can have invalid length.  For long ones you might want to do this
// differently to not lose 1/8 bits and speed.
template<int SIZE>
void marshallFixedBitVector(writer& th, const FixedBitVector<SIZE>& arr)
{
    int last_bit;
    for (last_bit = SIZE - 1; last_bit > 0; last_bit--)
        if (arr[last_bit])
            break;

    int i = 0;
    while (1)
    {
        uint8_t byte = 0;
        for (int j = 0; j < 7; j++)
            if (i < SIZE && arr[i++])
                byte |= 1 << j;
        if (i <= last_bit)
            marshallUByte(th, byte);
        else
        {
            marshallUByte(th, byte | 0x80);
            break;
        }
    }
}

template<int SIZE>
void unmarshallFixedBitVector(reader& th, FixedBitVector<SIZE>& arr)
{
    arr.reset();

    int i = 0;
    while (1)
    {
        uint8_t byte = unmarshallUByte(th);
        for (int j = 0; j < 7; j++)
            if (i < SIZE)
                arr.set(i++, !!(byte & (1 << j)));
        if (byte & 0x80)
            break;
    }
}

// FIXME: Kill this abomination - it will break!
template<typename T>
static void _marshall_as_int(writer& th, const T& t)
{
    marshallInt(th, static_cast<int>(t));
}

template <typename data>
void marshallSet(writer &th, const std::set<data> &s,
                 void (*marshall)(writer &, const data &))
{
    marshallInt(th, s.size());
    typename std::set<data>::const_iterator i = s.begin();
    for (; i != s.end(); ++i)
        marshall(th, *i);
}

template<typename key, typename value>
void marshallMap(writer &th, const std::map<key,value>& data,
                 void (*key_marshall)(writer&, const key&),
                 void (*value_marshall)(writer&, const value&))
{
    marshallInt(th, data.size());
    typename std::map<key,value>::const_iterator ci;
    for (ci = data.begin(); ci != data.end(); ++ci)
    {
        key_marshall(th, ci->first);
        value_marshall(th, ci->second);
    }
}

template<typename T_iter, typename T_marshall_t>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshall_t T_marshall)
{
    marshallInt(th, std::distance(beg, end));
    while (beg != end)
    {
        T_marshall(th, *beg);
        ++beg;
    }
}

template<typename T>
static void unmarshall_vector(reader& th, std::vector<T>& vec,
                              T (*T_unmarshall)(reader&))
{
    vec.clear();
    const int num_to_read = unmarshallInt(th);
    for (int i = 0; i < num_to_read; ++i)
        vec.push_back(T_unmarshall(th));
}

template <typename T_container, typename T_inserter, typename T_unmarshall>
static void unmarshall_container(reader &th, T_container &container,
                                 T_inserter inserter, T_unmarshall unmarshal)
{
    container.clear();
    const int num_to_read = unmarshallInt(th);
    for (int i = 0; i < num_to_read; ++i)
        (container.*inserter)(unmarshal(th));
}

void marshall_level_id(writer& th, const level_id& id)
{
    marshallShort(th, id.packed_place());
}

void marshall_level_id_set(writer& th, const std::set<level_id>& id)
{
    marshallSet(th, id, marshall_level_id);
}

// XXX: Redundant with level_pos.save()/load().
void marshall_level_pos(writer& th, const level_pos& lpos)
{
    marshallInt(th, lpos.pos.x);
    marshallInt(th, lpos.pos.y);
    marshall_level_id(th, lpos.id);
}

template <typename data, typename set>
void unmarshallSet(reader &th, set &dset,
                   data (*data_unmarshall)(reader &))
{
    dset.clear();
    int len = unmarshallInt(th);
    for (int i = 0; i < len; ++i)
        dset.insert(data_unmarshall(th));
}

template<typename key, typename value, typename map>
void unmarshallMap(reader& th, map& data,
                   key   (*key_unmarshall)  (reader&),
                   value (*value_unmarshall)(reader&))
{
    int i, len = unmarshallInt(th);
    key k;
    for (i = 0; i < len; ++i)
    {
        k = key_unmarshall(th);
        std::pair<key, value> p(k, value_unmarshall(th));
        data.insert(p);
    }
}

template<typename T>
static T unmarshall_int_as(reader& th)
{
    return static_cast<T>(unmarshallInt(th));
}

level_id unmarshall_level_id(reader& th)
{
    return level_id::from_packed_place(unmarshallShort(th));
}

std::set<level_id> unmarshall_level_id_set(reader& th)
{
    std::set<level_id> id;
    unmarshallSet(th, id, unmarshall_level_id);
    return id;
}

level_pos unmarshall_level_pos(reader& th)
{
    level_pos lpos;
    lpos.pos.x = unmarshallInt(th);
    lpos.pos.y = unmarshallInt(th);
    lpos.id    = unmarshall_level_id(th);
    return lpos;
}

void marshallCoord(writer &th, const coord_def &c)
{
    marshallShort(th, c.x);
    marshallShort(th, c.y);
}

coord_def unmarshallCoord(reader &th)
{
    coord_def c;
    c.x = unmarshallShort(th);
    c.y = unmarshallShort(th);
    return c;
}

static void _marshall_constriction(writer &th, const actor *who)
{
    _marshall_as_int(th, who->held);
    marshallInt(th, who->constricted_by);
    marshallInt(th, who->escape_attempts);

    // Assumes an empty map is marshalled as just the int 0.
    const actor::constricting_t * const cmap = who->constricting;
    if (cmap)
        marshallMap(th, *cmap, _marshall_as_int<mid_t>, _marshall_as_int<int>);
    else
        marshallInt(th, 0);
}

#if TAG_MAJOR_VERSION <= 33
// Temporarily store constriction information here when reading from
// an old (pre-TAG_MINOR_CONSTRICT_MID) save, so we can convert mindex
// to mid in a separate pass after all monsters are loaded.
struct old_constrict_t {
    old_constrict_t(short who_midx, short whom_midx, int dur)
        : constrictor(who_midx), constrictee(whom_midx), duration(dur)
    { }

    short constrictor, constrictee;
    int duration;
};

static std::vector<old_constrict_t> *temp_constrict = 0;

static bool _ok_mindex(short midx)
{
    return (midx == MHITYOU || !invalid_monster_index(midx));
}

static actor *_mindex_to_actor(short mindex)
{
    if (mindex == MHITYOU)
        return &you;
    else if (invalid_monster_index(mindex))
        return 0;
    else
        return &env.mons[mindex];
}

static void _fixup_constriction()
{
    if (!temp_constrict)
        return;

    std::vector<old_constrict_t>::iterator i;

    for (i = temp_constrict->begin(); i != temp_constrict->end(); ++i)
    {
        actor * const who  = _mindex_to_actor(i->constrictor);
        actor * const whom = _mindex_to_actor(i->constrictee);

        // Also sets up whom->constricted_by and whom->held
        if (who && whom)
            who->start_constricting(*whom, i->duration);
    }

    // Reset temp_constrict in preparation for the next load.
    delete temp_constrict;
    temp_constrict = 0;
}
#endif

static void _unmarshall_constriction(reader &th, actor *who)
{
#if TAG_MAJOR_VERSION <= 33
    const int MAX_CONSTRICT = 8;

    if (th.getMinorVersion() < TAG_MINOR_CONSTRICT_MID)
    {
        // Not setting constricted_by or held here; they will be set later,
        // based on the forward-direction 'constricting' data.  We don't
        // have to worry about HELD_MONSTER vs HELD_CONSTRICTED because old
        // versions didn't have that distinction.

        (void) unmarshallShort(th); // was constricted_by
        who->escape_attempts = unmarshallInt(th);
        (void) unmarshallInt(th); // was dur_been_constricted

        who->constricted_by = 0;
        who->held = HELD_NONE;
        who->constricting = 0;

        for (int k = 0; k < MAX_CONSTRICT; k++)
        {
            short ctee = unmarshallShort(th);
            int dur = unmarshallInt(th);

            // Skip bad constrictions; not that they are known to have existed
            // in the versions just before TAG_MINOR_CONSTRICT_MID, but if they
            // did, there's no point asserting about them now.
            if (!_ok_mindex(ctee))
                continue;

            if (!temp_constrict)
                temp_constrict = new std::vector<old_constrict_t>();

            temp_constrict->push_back(old_constrict_t(who->mindex(), ctee, dur));
        }

        return;
    }

#endif
    who->held = unmarshall_int_as<held_type>(th);
    who->constricted_by = unmarshallInt(th);
    who->escape_attempts = unmarshallInt(th);

    actor::constricting_t cmap;
    unmarshallMap(th, cmap, unmarshall_int_as<mid_t>, unmarshallInt);

    if (cmap.size() == 0)
        who->constricting = 0;
    else
        who->constricting = new actor::constricting_t(cmap);
}

template <typename marshall, typename grid>
static void _run_length_encode(writer &th, marshall m, const grid &g,
                               int width, int height)
{
    int last = 0, nlast = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            if (!nlast)
                last = g[x][y];
            if (last == g[x][y] && nlast < 255)
            {
                nlast++;
                continue;
            }

            marshallByte(th, nlast);
            m(th, last);

            last = g[x][y];
            nlast = 1;
        }

    marshallByte(th, nlast);
    m(th, last);
}

template <typename unmarshall, typename grid>
static void _run_length_decode(reader &th, unmarshall um, grid &g,
                               int width, int height)
{
    const int end = width * height;
    int offset = 0;
    while (offset < end)
    {
        const int run = unmarshallUByte(th);
        const int value = um(th);

        for (int i = 0; i < run; ++i)
        {
            const int y = offset / width;
            const int x = offset % width;
            g[x][y] = value;
            ++offset;
        }
    }
}

union float_marshall_kludge
{
    // [ds] Does ANSI C guarantee that sizeof(float) == sizeof(long)?
    // [haranp] no, unfortunately
    // [1KB] on 64 bit arches, long is 64 bits, while float is 32 bits.
    float    f_num;
    int32_t  l_num;
};

// single precision float -- marshall in network order.
void marshallFloat(writer &th, float data)
{
    float_marshall_kludge k;
    k.f_num = data;
    marshallInt(th, k.l_num);
}

// single precision float -- unmarshall in network order.
float unmarshallFloat(reader &th)
{
    float_marshall_kludge k;
    k.l_num = unmarshallInt(th);
    return k.f_num;
}

// string -- 2 byte length, string data
void marshallString(writer &th, const std::string &data, int maxSize)
{
    // allow for very long strings (well, up to 32K).
    int len = data.length();
    if (maxSize > 0 && len > maxSize)
        len = maxSize;
    marshallShort(th, len);

    // put in the actual string -- we'll null terminate on
    // unmarshall.
    th.write(data.c_str(), len);
}

// To pass to marshallMap
static void marshallStringNoMax(writer &th, const std::string &data)
{
    marshallString(th, data);
}

// string -- unmarshall length & string data
static int unmarshallCString(reader &th, char *data, int maxSize)
{
    ASSERT(maxSize > 0);

    // Get length.
    short len = unmarshallShort(th);
    int copylen = len;

    if (len >= maxSize)
        copylen = maxSize - 1;

    // Read the actual string and null terminate.
    th.read(data, copylen);
    data[copylen] = 0;

    th.advance(len - copylen);
    return copylen;
}

std::string unmarshallString(reader &th, int maxSize)
{
    if (maxSize <= 0)
        return "";
    char *buffer = new char [maxSize];
    if (!buffer)
        return "";
    *buffer = 0;
    const int slen = unmarshallCString(th, buffer, maxSize);
    ASSERT(slen >= 0 && slen < maxSize);
    const std::string res(buffer, slen);
    delete [] buffer;
    return res;
}

// To pass to unmarshallMap
static std::string unmarshallStringNoMax(reader &th)
{
    return unmarshallString(th);
}

// string -- 4 byte length, non-terminated string data.
void marshallString4(writer &th, const std::string &data)
{
    marshallInt(th, data.length());
    th.write(data.c_str(), data.length());
}
void unmarshallString4(reader &th, std::string& s)
{
    const int len = unmarshallInt(th);
    s.resize(len);
    if (len) th.read(&s.at(0), len);
}

// boolean (to avoid system-dependent bool implementations)
void marshallBoolean(writer &th, bool data)
{
    th.writeByte(data ? 1 : 0);
}

// boolean (to avoid system-dependent bool implementations)
bool unmarshallBoolean(reader &th)
{
    return (th.readByte() != 0);
}

// Saving the date as a string so we're not reliant on a particular epoch.
std::string make_date_string(time_t in_date)
{
    if (in_date <= 0)
        return "";

    struct tm *date = TIME_FN(&in_date);

    return make_stringf(
              "%4d%02d%02d%02d%02d%02d%s",
              date->tm_year + 1900, date->tm_mon, date->tm_mday,
              date->tm_hour, date->tm_min, date->tm_sec,
              ((date->tm_isdst > 0) ? "D" : "S"));
}

void marshallEnumVal(writer& wr, const enum_info *ei, int val)
{
    enum_write_state& ews = wr.used_enums[ei];

    if (!ews.store_type)
    {
        std::vector<std::pair<int, std::string> > values;

        ei->collect(values);

        for (unsigned i = 0; i < values.size(); ++i)
            ews.names.insert(values[i]);

        ews.store_type = 1;

        if (ews.names.begin() != ews.names.end()
            && (ews.names.rbegin()->first >= 128
                || ews.names.begin()->first <= -1))
        {
            ews.store_type = 2;
        }

        marshallByte(wr, ews.store_type);
    }

    if (ews.store_type == 2)
        marshallShort(wr, val);
    else
        marshallByte(wr, val);

    if (ews.used.find(val) == ews.used.end())
    {
        ASSERT(ews.names.find(val) != ews.names.end());
        marshallString(wr, ews.names[val]);

        ews.used.insert(val);
    }
}

int unmarshallEnumVal(reader& rd, const enum_info *ei)
{
    enum_read_state& ers = rd.seen_enums[ei];

    if (!ers.store_type)
    {
        std::vector<std::pair<int, std::string> > values;

        ei->collect(values);

        for (unsigned i = 0; i < values.size(); ++i)
            ers.names.insert(make_pair(values[i].second, values[i].first));

        if (rd.getMinorVersion() < ei->non_historical_first)
        {
            ers.store_type = ei->historic_bytes;

            const enum_info::enum_val *evi = ei->historical;

            for (; evi->name; ++evi)
            {
                if (ers.names.find(std::string(evi->name)) != ers.names.end())
                    ers.mapping[evi->value] = ers.names[std::string(evi->name)];
                else
                    ers.mapping[evi->value] = ei->replacement;
            }
        }
        else
            ers.store_type = unmarshallByte(rd);
    }

    int raw;

    if (ers.store_type == 2)
        raw = unmarshallShort(rd);
    else
        raw = unmarshallByte(rd);

    if (ers.mapping.find(raw) != ers.mapping.end())
        return ers.mapping[raw];

    ASSERT(rd.getMinorVersion() >= ei->non_historical_first);

    std::string name = unmarshallString(rd);

    if (ers.names.find(name) != ers.names.end())
        ers.mapping[raw] = ers.names[name];
    else
        ers.mapping[raw] = ei->replacement;

    return ers.mapping[raw];
}


// Write a tagged chunk of data to the FILE*.
// tagId specifies what to write.
void tag_write(tag_type tagID, writer &outf)
{
    std::vector<unsigned char> buf;
    writer th(&buf);
    switch (tagID)
    {
    case TAG_CHR:
        tag_construct_char(th);
        break;
    case TAG_YOU:
        tag_construct_you(th);
        tag_construct_you_items(th);
        tag_construct_you_dungeon(th);
        tag_construct_lost_monsters(th);
        tag_construct_lost_items(th);
        break;
    case TAG_LEVEL:
        tag_construct_level(th);
        tag_construct_level_items(th);
        tag_construct_level_monsters(th);
        tag_construct_level_tiles(th);
        break;
    case TAG_GHOST:
        tag_construct_ghost(th);
        break;
    default:
        // I don't know how to make that!
        break;
    }

    // make sure there is some data to write!
    if (buf.empty())
        return;

    // Write tag header.
    marshallInt(outf, buf.size());

    // Write tag data.
    outf.write(&buf[0], buf.size());
}

// Read a piece of data from inf into memory, then run the appropriate reader.
//
// minorVersion is available for any sub-readers that need it
void tag_read(reader &inf, tag_type tag_id)
{
    // Read header info and data
    std::vector<unsigned char> buf;
    const int data_size = unmarshallInt(inf);
    ASSERT(data_size >= 0);

    // Fetch data in one go
    buf.resize(data_size);
    inf.read(&buf[0], buf.size());

    // Ok, we have data now.
    reader th(buf, inf.getMinorVersion());
    switch (tag_id)
    {
    case TAG_YOU:
        tag_read_you(th);
        tag_read_you_items(th);
        tag_read_you_dungeon(th);
        tag_read_lost_monsters(th);
        tag_read_lost_items(th);

        // If somebody SIGHUP'ed out of the skill menu with all skills disabled.
        // Doing this here rather in tag_read_you() because you.can_train()
        // requires the player's equipment be loaded.
        init_can_train();
        check_selected_skills();
        break;
    case TAG_LEVEL:
        tag_read_level(th);
        tag_read_level_items(th);
        tag_read_level_monsters(th);
        tag_read_level_tiles(th);
        break;
    case TAG_GHOST:
        tag_read_ghost(th);
        break;
    default:
        // I don't know how to read that!
        die("unknown tag type");
    }
}

static void tag_construct_char(writer &th)
{
    marshallByte(th, TAG_CHR_FORMAT);
    // Important: you may never remove or alter a field without bumping
    // CHR_FORMAT.  Bumping it makes all saves invisible when browsed in an
    // older version.
    // Please keep this compatible even over major version breaks!

    // Appending fields is fine.

    marshallString(th, you.your_name);
    marshallString(th, Version::Long());

    marshallByte(th, you.species);
    marshallByte(th, you.char_class);
    marshallByte(th, you.experience_level);
    marshallString(th, you.class_name);
    marshallByte(th, you.religion);
    marshallString(th, you.jiyva_second_name);

    marshallByte(th, you.wizard);

    marshallByte(th, crawl_state.type);
    if (crawl_state.game_is_tutorial())
        marshallString(th, crawl_state.map);

    marshallString(th, species_name(you.species));
    marshallString(th, you.religion ? god_name(you.religion) : "");
}

static void tag_construct_you(writer &th)
{
    int i, j;

    marshallInt(th, you.last_mid);
    marshallByte(th, you.piety);
    marshallByte(th, you.rotting);
    marshallShort(th, you.pet_target);

    marshallByte(th, you.max_level);
    marshallByte(th, you.where_are_you);
    marshallByte(th, you.depth);
    marshallByte(th, you.char_direction);
    marshallByte(th, you.opened_zot);
    marshallByte(th, you.royal_jelly_dead);
    marshallByte(th, you.transform_uncancellable);
    marshallByte(th, you.is_undead);
    marshallShort(th, you.unrand_reacts);
    marshallByte(th, you.berserk_penalty);
    marshallShort(th, you.manual_skill);
    marshallInt(th, you.manual_index);
    marshallInt(th, you.abyss_speed);

    marshallInt(th, you.disease);
    marshallShort(th, you.dead ? 0 : you.hp);

    marshallShort(th, you.hunger);
    marshallBoolean(th, you.fishtail);
    marshallInt(th, you.earth_attunement);
    marshallInt(th, you.form);

    j = std::min<int>(you.sage_skills.size(), 32767);
    marshallShort(th, you.sage_skills.size());
    for (i = 0; i < (int)you.sage_skills.size(); ++i)
    {
        marshallByte(th, you.sage_skills[i]);
        marshallInt(th, you.sage_xp[i]);
        marshallInt(th, you.sage_bonus[i]);
    }

    // how many you.equip?
    marshallByte(th, NUM_EQUIP);
    for (i = 0; i < NUM_EQUIP; ++i)
        marshallByte(th, you.equip[i]);
    for (i = 0; i < NUM_EQUIP; ++i)
        marshallBoolean(th, you.melded[i]);

    marshallByte(th, you.magic_points);
    marshallByte(th, you.max_magic_points);

    COMPILE_CHECK(NUM_STATS == 3);
    for (i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.base_stats[i]);
    for (i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.stat_loss[i]);
    for (i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.stat_zero[i]);
    for (i = 0; i < NUM_STATS; ++i)
        marshallString(th, you.stat_zero_cause[i]);

    marshallByte(th, you.hit_points_regeneration);
    marshallByte(th, you.magic_points_regeneration);

    marshallShort(th, you.hit_points_regeneration * 100);
    marshallInt(th, you.experience);
    marshallInt(th, you.total_experience);
    marshallInt(th, you.gold);

    marshallInt(th, you.exp_available);
    marshallInt(th, you.zot_points);

    marshallInt(th, you.zigs_completed);
    marshallByte(th, you.zig_max);

    marshallShort(th, you.hp_max_temp);
    marshallShort(th, you.hp_max_perm);
    marshallShort(th, you.mp_max_temp);
    marshallShort(th, you.mp_max_perm);

    marshallShort(th, you.pos().x);
    marshallShort(th, you.pos().y);

    marshallShort(th, you.burden);

    // how many spells?
    marshallUByte(th, MAX_KNOWN_SPELLS);
    for (i = 0; i < MAX_KNOWN_SPELLS; ++i)
        marshallByte(th, you.spells[i]);

    marshallByte(th, 52);
    for (i = 0; i < 52; i++)
        marshallByte(th, you.spell_letter_table[i]);

    marshallByte(th, 52);
    for (i = 0; i < 52; i++)
        marshallShort(th, you.ability_letter_table[i]);

    // how many skills?
    marshallByte(th, NUM_SKILLS);
    for (j = 0; j < NUM_SKILLS; ++j)
    {
        marshallUByte(th, you.skills[j]);
        marshallByte(th, you.train[j]);
        marshallByte(th, you.train_alt[j]);
        marshallInt(th, you.training[j]);
        marshallInt(th, you.skill_points[j]);
        marshallInt(th, you.ct_skill_points[j]);
        marshallByte(th, you.skill_order[j]);   // skills ordering
    }

    marshallBoolean(th, you.auto_training);
    marshallByte(th, you.exercises.size());
    for (std::list<skill_type>::iterator it = you.exercises.begin();
         it != you.exercises.end(); ++it)
    {
        marshallInt(th, *it);
    }

    marshallByte(th, you.exercises_all.size());
    for (std::list<skill_type>::iterator it = you.exercises_all.begin();
         it != you.exercises_all.end(); ++it)
    {
        marshallInt(th, *it);
    }

    marshallByte(th, you.skill_menu_do);
    marshallByte(th, you.skill_menu_view);

    marshallInt(th, you.transfer_from_skill);
    marshallInt(th, you.transfer_to_skill);
    marshallInt(th, you.transfer_skill_points);
    marshallInt(th, you.transfer_total_skill_points);

    // how many durations?
    marshallByte(th, NUM_DURATIONS);
    for (j = 0; j < NUM_DURATIONS; ++j)
        marshallInt(th, you.duration[j]);

    // how many attributes?
    marshallByte(th, NUM_ATTRIBUTES);
    for (j = 0; j < NUM_ATTRIBUTES; ++j)
        marshallInt(th, you.attribute[j]);

    // Sacrifice values.
    marshallByte(th, NUM_OBJECT_CLASSES);
    for (j = 0; j < NUM_OBJECT_CLASSES; ++j)
        marshallInt(th, you.sacrifice_value[j]);

    // how many mutations/demon powers?
    marshallShort(th, NUM_MUTATIONS);
    for (j = 0; j < NUM_MUTATIONS; ++j)
    {
        marshallByte(th, you.mutation[j]);
        marshallByte(th, you.innate_mutations[j]);
    }

    marshallByte(th, you.demonic_traits.size());
    for (j = 0; j < int(you.demonic_traits.size()); ++j)
    {
        marshallByte(th, you.demonic_traits[j].level_gained);
        marshallShort(th, you.demonic_traits[j].mutation);
    }

    // how many penances?
    marshallByte(th, NUM_GODS);
    for (i = 0; i < NUM_GODS; i++)
        marshallByte(th, you.penance[i]);

    // which gods have been worshipped by this character?
    for (i = 0; i < NUM_GODS; i++)
        marshallByte(th, you.worshipped[i]);

    // what is the extent of divine generosity?
    for (i = 0; i < NUM_GODS; i++)
        marshallShort(th, you.num_current_gifts[i]);
    for (i = 0; i < NUM_GODS; i++)
        marshallShort(th, you.num_total_gifts[i]);
    for (i = 0; i < NUM_GODS; i++)
        marshallBoolean(th, you.one_time_ability_used[i]);

    // how much piety have you achieved at highest with each god?
    for (i = 0; i < NUM_GODS; i++)
        marshallByte(th, you.piety_max[i]);

    marshallByte(th, NUM_NEMELEX_GIFT_TYPES);
    for (i = 0; i < NUM_NEMELEX_GIFT_TYPES; ++i)
        marshallBoolean(th, you.nemelex_sacrificing[i]);

    marshallByte(th, you.gift_timeout);
    marshallByte(th, you.hell_exit);
    marshallByte(th, you.hell_branch);

#if TAG_MAJOR_VERSION <= 33
    // Fix a hell stair loop bug, present from 4f27c86 to d0ec4fc.
    if (is_hell_subbranch(you.hell_branch))
    {
        // root_branch hasn't been loaded yet
        you.hell_branch = BRANCH_MAIN_DUNGEON;
        you.hell_exit = 1;
    }
#endif

    marshallInt(th, you.exp_docked);
    marshallInt(th, you.exp_docked_total);

    // elapsed time
    marshallInt(th, you.elapsed_time);

    // time of game start
    marshallInt(th, you.birth_time);

    handle_real_time();

    marshallInt(th, you.real_time);
    marshallInt(th, you.num_turns);
    marshallInt(th, you.exploration);

    marshallShort(th, you.magic_contamination);

    marshallShort(th, you.transit_stair);
    marshallByte(th, you.entering_level);

    marshallByte(th, you.deaths);
    marshallByte(th, you.lives);

    marshallInt(th, you.dactions.size());
    for (unsigned int k = 0; k < you.dactions.size(); k++)
        marshallByte(th, you.dactions[k]);

    marshallInt(th, you.level_stack.size());
    for (unsigned int k = 0; k < you.level_stack.size(); k++)
        you.level_stack[k].save(th);

    // List of currently beholding monsters (usually empty).
    marshallShort(th, you.beholders.size());
    for (unsigned int k = 0; k < you.beholders.size(); k++)
         marshallShort(th, you.beholders[k]);

    marshallShort(th, you.fearmongers.size());
    for (unsigned int k = 0; k < you.fearmongers.size(); k++)
        marshallShort(th, you.fearmongers[k]);

    marshallByte(th, you.piety_hysteresis);

    you.m_quiver->save(th);

    marshallByte(th, you.friendly_pickup);

    marshallString(th, you.zotdef_wave_name);

#if TAG_MAJOR_VERSION <= 33
    for (unsigned int k = 0; k < ARRAYSZ(you.montiers); k++)
        marshallInt(th, you.montiers[k]);
#endif

    // Action counts.
    j = 0;
    for (std::map<std::pair<caction_type, int>, FixedVector<int, 27> >::const_iterator ac =
         you.action_count.begin(); ac != you.action_count.end(); ++ac)
    {
        j++;
    }
    marshallShort(th, j);
    for (std::map<std::pair<caction_type, int>, FixedVector<int, 27> >::const_iterator ac =
         you.action_count.begin(); ac != you.action_count.end(); ++ac)
    {
        marshallShort(th, ac->first.first);
        marshallInt(th, ac->first.second);
        for (int k = 0; k < 27; k++)
            marshallInt(th, ac->second[k]);
    }


    marshallCoord(th, abyssal_state.major_coord);
    marshallFloat(th, abyssal_state.depth);
    marshallFloat(th, abyssal_state.phase);

    _marshall_constriction(th, &you);

    marshallUByte(th, you.octopus_king_rings);

    if (!dlua.callfn("dgn_save_data", "u", &th))
        mprf(MSGCH_ERROR, "Failed to save Lua data: %s", dlua.error.c_str());

    // Write a human-readable string out on the off chance that
    // we fail to be able to read this file back in using some later version.
    std::string revision = "Git:";
    revision += Version::Long();
    marshallString(th, revision);

    you.props.write(th);
}

static void tag_construct_you_items(writer &th)
{
    int i,j;

    // how many inventory slots?
    marshallByte(th, ENDOFPACK);
    for (i = 0; i < ENDOFPACK; ++i)
        marshallItem(th, you.inv[i]);

    marshallFixedBitVector<NUM_RUNE_TYPES>(th, you.runes);
    marshallByte(th, you.obtainable_runes);

    // Item descrip for each type & subtype.
    // how many types?
    marshallUByte(th, NUM_IDESC);
    // how many subtypes?
    marshallUByte(th, MAX_SUBTYPES);
    for (i = 0; i < NUM_IDESC; ++i)
        for (j = 0; j < MAX_SUBTYPES; ++j)
            marshallByte(th, you.item_description[i][j]);

    marshallUByte(th, NUM_OBJECT_CLASSES);
    for (i = 0; i < NUM_OBJECT_CLASSES; ++i)
    {
        if (!item_type_has_ids((object_class_type)i))
            continue;
        for (j = 0; j < MAX_SUBTYPES; ++j)
            marshallUByte(th, you.type_ids[i][j]);
    }

    // Additional identification info
    you.type_id_props.write(th);

    // how many unique items?
    marshallByte(th, MAX_UNRANDARTS);
    for (j = 0; j < MAX_UNRANDARTS; ++j)
        marshallByte(th,you.unique_items[j]);

    marshallByte(th, NUM_FIXED_BOOKS);
    for (j = 0; j < NUM_FIXED_BOOKS; ++j)
        marshallByte(th,you.had_book[j]);

    marshallShort(th, NUM_SPELLS);
    for (j = 0; j < NUM_SPELLS; ++j)
        marshallByte(th,you.seen_spell[j]);

    marshallShort(th, NUM_WEAPONS);
    for (j = 0; j < NUM_WEAPONS; ++j)
        marshallInt(th,you.seen_weapon[j]);

    marshallShort(th, NUM_ARMOURS);
    for (j = 0; j < NUM_ARMOURS; ++j)
        marshallInt(th,you.seen_armour[j]);

    marshallFixedBitVector<NUM_MISCELLANY>(th, you.seen_misc);

    for (i = 0; i < NUM_OBJECT_CLASSES; i++)
        for (j = 0; j < MAX_SUBTYPES; j++)
            marshallInt(th, you.force_autopickup[i][j]);
}

static void marshallPlaceInfo(writer &th, PlaceInfo place_info)
{
    marshallInt(th, place_info.branch);

    marshallInt(th, place_info.num_visits);
    marshallInt(th, place_info.levels_seen);

    marshallInt(th, place_info.mon_kill_exp);

    for (int i = 0; i < KC_NCATEGORIES; i++)
        marshallInt(th, place_info.mon_kill_num[i]);

    marshallInt(th, place_info.turns_total);
    marshallInt(th, place_info.turns_explore);
    marshallInt(th, place_info.turns_travel);
    marshallInt(th, place_info.turns_interlevel);
    marshallInt(th, place_info.turns_resting);
    marshallInt(th, place_info.turns_other);

    marshallInt(th, place_info.elapsed_total);
    marshallInt(th, place_info.elapsed_explore);
    marshallInt(th, place_info.elapsed_travel);
    marshallInt(th, place_info.elapsed_interlevel);
    marshallInt(th, place_info.elapsed_resting);
    marshallInt(th, place_info.elapsed_other);
}

static void tag_construct_you_dungeon(writer &th)
{
    // how many unique creatures?
    marshallShort(th, NUM_MONSTERS);
    for (int j = 0; j < NUM_MONSTERS; ++j)
        marshallByte(th,you.unique_creatures[j]); // unique beasties

    // how many branches?
    marshallByte(th, NUM_BRANCHES);
    for (int j = 0; j < NUM_BRANCHES; ++j)
    {
        marshallInt(th, brdepth[j]);
        marshallInt(th, startdepth[j]);
    }

    // Root of the dungeon; usually BRANCH_MAIN_DUNGEON.
    marshallInt(th, root_branch);

    marshallMap(th, stair_level,
                _marshall_as_int<branch_type>, marshall_level_id_set);
    marshallMap(th, shops_present,
                marshall_level_pos, _marshall_as_int<shop_type>);
    marshallMap(th, altars_present,
                marshall_level_pos, _marshall_as_int<god_type>);
    marshallMap(th, portals_present,
                marshall_level_pos, _marshall_as_int<branch_type>);
    marshallMap(th, portal_notes,
                marshall_level_pos, marshallStringNoMax);
    marshallMap(th, level_annotations,
                marshall_level_id, marshallStringNoMax);
    marshallMap(th, level_exclusions,
                marshall_level_id, marshallStringNoMax);
    marshallMap(th, level_uniques,
            marshall_level_id, marshallStringNoMax);
    marshallUniqueAnnotations(th);

    marshallPlaceInfo(th, you.global_info);
    std::vector<PlaceInfo> list = you.get_all_place_info();
    // How many different places we have info on?
    marshallShort(th, list.size());

    for (unsigned int k = 0; k < list.size(); k++)
        marshallPlaceInfo(th, list[k]);

    marshall_iterator(th, you.uniq_map_tags.begin(), you.uniq_map_tags.end(),
                      marshallStringNoMax);
    marshall_iterator(th, you.uniq_map_names.begin(), you.uniq_map_names.end(),
                      marshallStringNoMax);

    write_level_connectivity(th);
}

static void marshall_follower(writer &th, const follower &f)
{
    ASSERT(!invalid_monster_type(f.mons.type));
    marshallMonster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        marshallItem(th, f.items[i]);
}

static void unmarshall_follower(reader &th, follower &f)
{
    unmarshallMonster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
#if TAG_MAJOR_VERSION == 33
        if ((th.getMinorVersion() < TAG_MINOR_MONSTER_JEWELLERY) &&
            (i == MSLOT_JEWELLERY))
        {
            f.items[i].clear();
            continue;
        }
#endif
        unmarshallItem(th, f.items[i]);
    }
}

static void marshall_follower_list(writer &th, const m_transit_list &mlist)
{
    marshallShort(th, mlist.size());

    for (m_transit_list::const_iterator mi = mlist.begin();
         mi != mlist.end(); ++mi)
    {
        marshall_follower(th, *mi);
    }
}

static void marshall_item_list(writer &th, const i_transit_list &ilist)
{
    marshallShort(th, ilist.size());

    for (i_transit_list::const_iterator ii = ilist.begin();
         ii != ilist.end(); ++ii)
    {
        marshallItem(th, *ii);
    }
}


static m_transit_list unmarshall_follower_list(reader &th)
{
    m_transit_list mlist;

    const int size = unmarshallShort(th);

    for (int i = 0; i < size; ++i)
    {
        follower f;
        unmarshall_follower(th, f);
        mlist.push_back(f);
    }

    return mlist;
}

static i_transit_list unmarshall_item_list(reader &th)
{
    i_transit_list ilist;

    const int size = unmarshallShort(th);

    for (int i = 0; i < size; ++i)
    {
        item_def item;
        unmarshallItem(th, item);
        ilist.push_back(item);
    }

    return ilist;
}

static void marshall_level_map_masks(writer &th)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        marshallInt(th, env.level_map_mask(*ri));
        marshallInt(th, env.level_map_ids(*ri));
    }
}

static void unmarshall_level_map_masks(reader &th)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        env.level_map_mask(*ri) = unmarshallInt(th);
        env.level_map_ids(*ri)  = unmarshallInt(th);
    }
}

static void marshall_level_map_unique_ids(writer &th)
{
    marshallSet(th, env.level_uniq_maps, marshallStringNoMax);
    marshallSet(th, env.level_uniq_map_tags, marshallStringNoMax);
}

static void unmarshall_level_map_unique_ids(reader &th)
{
    unmarshallSet(th, env.level_uniq_maps, unmarshallStringNoMax);
    unmarshallSet(th, env.level_uniq_map_tags, unmarshallStringNoMax);
}

static void marshall_mapdef(writer &th, const map_def &map)
{
    marshallString(th, map.name);
    map.write_index(th);
    map.write_maplines(th);
    marshallString(th, map.description);
    marshallMap(th, map.feat_renames,
                _marshall_as_int<dungeon_feature_type>, marshallStringNoMax);
}

static map_def unmarshall_mapdef(reader &th)
{
    map_def map;
    map.name = unmarshallString(th);
    map.read_index(th);
    map.read_maplines(th);
    map.description = unmarshallString(th);
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() >= TAG_MINOR_FEAT_RENAMES)
#endif
    unmarshallMap(th, map.feat_renames,
                  unmarshall_int_as<dungeon_feature_type>,
                  unmarshallStringNoMax);
    return map;
}

static void marshall_vault_placement(writer &th, const vault_placement &vp)
{
    marshallCoord(th, vp.pos);
    marshallCoord(th, vp.size);
    marshallShort(th, vp.orient);
    marshall_mapdef(th, vp.map);
    marshall_iterator(th, vp.exits.begin(), vp.exits.end(), marshallCoord);
    marshallShort(th, vp.level_number);
    marshallByte(th, vp.seen);
}

static vault_placement unmarshall_vault_placement(reader &th)
{
    vault_placement vp;
    vp.pos = unmarshallCoord(th);
    vp.size = unmarshallCoord(th);
    vp.orient = static_cast<map_section_type>(unmarshallShort(th));
    vp.map = unmarshall_mapdef(th);
    unmarshall_vector(th, vp.exits, unmarshallCoord);
    vp.level_number = unmarshallShort(th);
    vp.seen = !!unmarshallByte(th);

    return vp;
}

static void marshall_level_vault_placements(writer &th)
{
    marshallShort(th, env.level_vaults.size());
    for (int i = 0, size = env.level_vaults.size(); i < size; ++i)
        marshall_vault_placement(th, *env.level_vaults[i]);
}

static void unmarshall_level_vault_placements(reader &th)
{
    const int nvaults = unmarshallShort(th);
    ASSERT(nvaults >= 0);
    dgn_clear_vault_placements(env.level_vaults);
    for (int i = 0; i < nvaults; ++i)
    {
        env.level_vaults.push_back(
            new vault_placement(unmarshall_vault_placement(th)));
    }
}

static void marshall_level_vault_data(writer &th)
{
    marshallStringNoMax(th, env.level_build_method);
    marshallSet(th, env.level_layout_types, marshallStringNoMax);

    marshall_level_map_masks(th);
    marshall_level_map_unique_ids(th);
    marshall_level_vault_placements(th);
}

static void unmarshall_level_vault_data(reader &th)
{
    env.level_build_method = unmarshallStringNoMax(th);
    unmarshallSet(th, env.level_layout_types, unmarshallStringNoMax);

    unmarshall_level_map_masks(th);
    unmarshall_level_map_unique_ids(th);
    unmarshall_level_vault_placements(th);
}

static void tag_construct_lost_monsters(writer &th)
{
    marshallMap(th, the_lost_ones, marshall_level_id,
                 marshall_follower_list);
}

static void tag_construct_lost_items(writer &th)
{
    marshallMap(th, transiting_items, marshall_level_id,
                 marshall_item_list);
}

// Save versions 30-32.26 are readable but don't store the names.
static const char* old_species[]=
{
    "Human", "High Elf", "Deep Elf", "Sludge Elf", "Mountain Dwarf", "Halfling",
    "Hill Orc", "Kobold", "Mummy", "Naga", "Ogre", "Troll",
    "Red Draconian", "White Draconian", "Green Draconian", "Yellow Draconian",
    "Grey Draconian", "Black Draconian", "Purple Draconian", "Mottled Draconian",
    "Pale Draconian", "Draconian", "Centaur", "Demigod", "Spriggan", "Minotaur",
    "Demonspawn", "Ghoul", "Tengu", "Merfolk", "Vampire", "Deep Dwarf", "Felid",
    "Octopode",
};

static const char* old_gods[]=
{
    "", "Zin", "The Shining One", "Kikubaaqudgha", "Yredelemnul", "Xom",
    "Vehumet", "Okawaru", "Makhleb", "Sif Muna", "Trog", "Nemelex Xobeh",
    "Elyvilon", "Lugonu", "Beogh", "Jiyva", "Fedhas", "Cheibriados",
    "Ashenzari",
};

void tag_read_char(reader &th, uint8_t format, uint8_t major, uint8_t minor)
{
    // Important: values out of bounds are good here, the save browser needs to
    // be forward-compatible.  We validate them only on an actual restore.
    you.your_name         = unmarshallString(th);
    you.prev_save_version = unmarshallString(th);
    dprf("Last save Crawl version: %s", you.prev_save_version.c_str());

    you.species           = static_cast<species_type>(unmarshallUByte(th));
    you.char_class        = static_cast<job_type>(unmarshallUByte(th));
    you.experience_level  = unmarshallByte(th);
    you.class_name        = unmarshallString(th);
    you.religion          = static_cast<god_type>(unmarshallUByte(th));
    you.jiyva_second_name = unmarshallString(th);

    you.wizard            = unmarshallBoolean(th);

    crawl_state.type = (game_type) unmarshallUByte(th);
    if (crawl_state.game_is_tutorial())
        crawl_state.map = unmarshallString(th);

    if (major > 32 || major == 32 && minor > 26)
    {
        you.species_name = unmarshallString(th);
        you.god_name     = unmarshallString(th);
    }
    else
    {
        if (you.species >= 0 && you.species < (int)ARRAYSZ(old_species))
            you.species_name = old_species[you.species];
        else
            you.species_name = "Yak";
        if (you.religion >= 0 && you.religion < (int)ARRAYSZ(old_gods))
            you.god_name = old_gods[you.religion];
        else
            you.god_name = "Marduk";
    }
}

static void tag_read_you(reader &th)
{
    int i,j;
    int count;

    ASSERT(is_valid_species(you.species) && species_exp_modifier(you.species));
    ASSERT(you.char_class < NUM_JOBS);
    ASSERT(you.experience_level > 0);
    ASSERT(you.experience_level <= 27);
    ASSERT(you.religion < NUM_GODS);
    ASSERT(crawl_state.type > GAME_TYPE_UNSPECIFIED);
    ASSERT(crawl_state.type < NUM_GAME_TYPE);
    you.last_mid          = unmarshallInt(th);
    you.piety             = unmarshallUByte(th);
    ASSERT(you.piety <= MAX_PIETY);
    you.rotting           = unmarshallUByte(th);
    you.pet_target        = unmarshallShort(th);

    you.max_level         = unmarshallByte(th);
    you.where_are_you     = static_cast<branch_type>(unmarshallUByte(th));
    ASSERT(you.where_are_you < NUM_BRANCHES);
    you.depth             = unmarshallByte(th);
    ASSERT(you.depth > 0);
    you.char_direction    = static_cast<game_direction_type>(unmarshallUByte(th));
    ASSERT(you.char_direction <= GDT_ASCENDING);

    you.opened_zot = unmarshallBoolean(th);
    you.royal_jelly_dead = unmarshallBoolean(th);
    you.transform_uncancellable = unmarshallBoolean(th);

    you.is_undead         = static_cast<undead_state_type>(unmarshallUByte(th));
    ASSERT(you.is_undead <= US_SEMI_UNDEAD);
    you.unrand_reacts     = unmarshallShort(th);
    you.berserk_penalty   = unmarshallByte(th);
    you.manual_skill  = static_cast<skill_type>(unmarshallShort(th));
    you.manual_index  = unmarshallInt(th);

    you.abyss_speed = unmarshallInt(th);

    you.disease         = unmarshallInt(th);
    you.hp              = unmarshallShort(th);
    you.hunger          = unmarshallShort(th);
    you.fishtail        = unmarshallBoolean(th);
    you.earth_attunement= unmarshallInt(th);
    you.form            = static_cast<transformation_type>(unmarshallInt(th));
    ASSERT(you.form >= TRAN_NONE && you.form <= LAST_FORM);

    count = unmarshallShort(th);
    ASSERT(count >= 0 && count <= 32767);
    you.sage_skills.resize(count, SK_NONE);
    you.sage_xp.resize(count, 0);
    you.sage_bonus.resize(count, 0);
    for (i = 0; i < count; ++i)
    {
        you.sage_skills[i] = static_cast<skill_type>(unmarshallByte(th));
        ASSERT(!is_invalid_skill(you.sage_skills[i]));
        ASSERT(!is_useless_skill(you.sage_skills[i]));
        you.sage_xp[i] = unmarshallInt(th);
        you.sage_bonus[i] = unmarshallInt(th);
    }

    // How many you.equip?
    count = unmarshallByte(th);
    ASSERT(count <= NUM_EQUIP);
    for (i = 0; i < count; ++i)
    {
        you.equip[i] = unmarshallByte(th);
        ASSERT(you.equip[i] >= -1 && you.equip[i] < ENDOFPACK);
    }
    for (i = count; i < NUM_EQUIP; ++i)
        you.equip[i] = -1;
    for (i = 0; i < count; ++i)
        you.melded[i] = unmarshallBoolean(th);
    for (i = count; i < NUM_EQUIP; ++i)
        you.melded[i] = false;

    you.magic_points              = unmarshallByte(th);
    you.max_magic_points          = unmarshallByte(th);

    for (i = 0; i < NUM_STATS; ++i)
        you.base_stats[i] = unmarshallByte(th);
    for (i = 0; i < NUM_STATS; ++i)
        you.stat_loss[i] = unmarshallByte(th);
    for (i = 0; i < NUM_STATS; ++i)
        you.stat_zero[i] = unmarshallByte(th);
    for (i = 0; i < NUM_STATS; ++i)
        you.stat_zero_cause[i] = unmarshallString(th);

    you.hit_points_regeneration   = unmarshallByte(th);
    you.magic_points_regeneration = unmarshallByte(th);

    you.hit_points_regeneration   = unmarshallShort(th) / 100;
    you.experience                = unmarshallInt(th);
    you.total_experience = unmarshallInt(th);
    you.gold                      = unmarshallInt(th);
    you.exp_available             = unmarshallInt(th);
    you.zot_points                = unmarshallInt(th);
    you.zigs_completed            = unmarshallInt(th);
    you.zig_max                   = unmarshallByte(th);

    you.hp_max_temp               = unmarshallShort(th);
    you.hp_max_perm               = unmarshallShort(th);
    you.mp_max_temp               = unmarshallShort(th);
    you.mp_max_perm               = unmarshallShort(th);

    const int x = unmarshallShort(th);
    const int y = unmarshallShort(th);
    // SIGHUP during Step from Time/etc is ok.
    ASSERT(!x && !y || in_bounds(x, y));
    you.moveto(coord_def(x, y));

    you.burden = unmarshallShort(th);

    // how many spells?
    you.spell_no = 0;
    count = unmarshallUByte(th);
    ASSERT(count >= 0);
    for (i = 0; i < count && i < MAX_KNOWN_SPELLS; ++i)
    {
        you.spells[i] = static_cast<spell_type>(unmarshallUByte(th));
        if (you.spells[i] != SPELL_NO_SPELL)
            you.spell_no++;
    }
    for (; i < count; ++i)
        unmarshallUByte(th);

    count = unmarshallByte(th);
    ASSERT(count == (int)you.spell_letter_table.size());
    for (i = 0; i < count; i++)
    {
        int s = unmarshallByte(th);
        ASSERT(s >= -1 && s < MAX_KNOWN_SPELLS);
        you.spell_letter_table[i] = s;
    }

    count = unmarshallByte(th);
    ASSERT(count == (int)you.ability_letter_table.size());
    for (i = 0; i < count; i++)
    {
        int a = unmarshallShort(th);
        ASSERT(a >= -1 && a != 0 && a < NUM_ABILITIES);
        you.ability_letter_table[i] = static_cast<ability_type>(a);
    }

    // how many skills?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_SKILLS);
    for (j = 0; j < count; ++j)
    {
        you.skills[j]          = unmarshallUByte(th);
        ASSERT(you.skills[j] <= 27 || you.wizard);

        you.train[j]    = unmarshallByte(th);
        you.train_alt[j]    = unmarshallByte(th);
        you.training[j] = unmarshallInt(th);
        you.skill_points[j]    = unmarshallInt(th);
        you.ct_skill_points[j] = unmarshallInt(th);
        you.skill_order[j]     = unmarshallByte(th);
    }

    you.auto_training = unmarshallBoolean(th);

    count = unmarshallByte(th);
    for (i = 0; i < count; i++)
        you.exercises.push_back((skill_type)unmarshallInt(th));

    count = unmarshallByte(th);
    for (i = 0; i < count; i++)
        you.exercises_all.push_back((skill_type)unmarshallInt(th));

    you.skill_menu_do = static_cast<skill_menu_state>(unmarshallByte(th));
    you.skill_menu_view = static_cast<skill_menu_state>(unmarshallByte(th));
    you.transfer_from_skill = static_cast<skill_type>(unmarshallInt(th));
    ASSERT(you.transfer_from_skill == SK_NONE || you.transfer_from_skill < NUM_SKILLS);
    you.transfer_to_skill = static_cast<skill_type>(unmarshallInt(th));
    ASSERT(you.transfer_to_skill == SK_NONE || you.transfer_to_skill < NUM_SKILLS);
    you.transfer_skill_points = unmarshallInt(th);
    you.transfer_total_skill_points = unmarshallInt(th);

    // Set up you.skill_cost_level.
    you.skill_cost_level = 0;
    check_skill_cost_change();

    // how many durations?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_DURATIONS <= 256);
    for (j = 0; j < count && j < NUM_DURATIONS; ++j)
        you.duration[j] = unmarshallInt(th);
    for (j = NUM_DURATIONS; j < count; ++j)
        unmarshallInt(th);

    // how many attributes?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_ATTRIBUTES <= 256);
    for (j = 0; j < count && j < NUM_ATTRIBUTES; ++j)
        you.attribute[j] = unmarshallInt(th);
    for (j = count; j < NUM_ATTRIBUTES; ++j)
        you.attribute[j] = 0;
    for (j = NUM_ATTRIBUTES; j < count; ++j)
        unmarshallInt(th);

    count = unmarshallByte(th);
    ASSERT(count <= NUM_OBJECT_CLASSES);
    for (j = 0; j < count; ++j)
        you.sacrifice_value[j] = unmarshallInt(th);
    for (j = count; j < NUM_OBJECT_CLASSES; ++j)
        you.sacrifice_value[j] = 0;

    // how many mutations/demon powers?
    count = unmarshallShort(th);
    ASSERT(count >= 0 && count <= NUM_MUTATIONS);
    for (j = 0; j < count; ++j)
    {
        you.mutation[j]         = unmarshallUByte(th);
        you.innate_mutations[j] = unmarshallUByte(th);
    }
    for (j = count; j < NUM_MUTATIONS; ++j)
        you.mutation[j] = you.innate_mutations[j] = 0;

#if TAG_MAJOR_VERSION == 33
    if (you.species == SP_FELID
          && you.innate_mutations[MUT_SLOW_METABOLISM] == 2
        || you.species == SP_SPRIGGAN
          && you.innate_mutations[MUT_SLOW_METABOLISM] == 3)
    {
        you.mutation[MUT_SLOW_METABOLISM]--;
        you.innate_mutations[MUT_SLOW_METABOLISM]--;
    }
    if (you.mutation[MUT_SLOW_METABOLISM] == 3)
        you.mutation[MUT_SLOW_METABOLISM]--;
#endif

    count = unmarshallUByte(th);
    you.demonic_traits.clear();
    for (j = 0; j < count; ++j)
    {
        player::demon_trait dt;
        dt.level_gained = unmarshallByte(th);
        ASSERT(dt.level_gained > 1 && dt.level_gained <= 27);
        dt.mutation = static_cast<mutation_type>(unmarshallShort(th));
        ASSERT(dt.mutation >= 0 && dt.mutation < NUM_MUTATIONS);
        you.demonic_traits.push_back(dt);
    }

    // how many penances?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_GODS);
    for (i = 0; i < count; i++)
    {
        you.penance[i] = unmarshallUByte(th);
        ASSERT(you.penance[i] <= MAX_PENANCE);
    }

    for (i = 0; i < count; i++)
        you.worshipped[i] = unmarshallByte(th);

    for (i = 0; i < count; i++)
        you.num_current_gifts[i] = unmarshallShort(th);
    for (i = 0; i < count; i++)
        you.num_total_gifts[i] = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() >= TAG_MINOR_ONE_TIME_ABILITIES)
    {
#endif
    for (i = 0; i < count; i++)
        you.one_time_ability_used[i] = unmarshallBoolean(th);
#if TAG_MAJOR_VERSION == 33
    }
    else
    {
        you.one_time_ability_used[GOD_JIYVA] = (bool)you.num_total_gifts[GOD_JIYVA];
        you.one_time_ability_used[GOD_KIKUBAAQUDGHA] = (bool)you.num_total_gifts[GOD_KIKUBAAQUDGHA];
        you.num_total_gifts[GOD_KIKUBAAQUDGHA] *= 2;
        you.one_time_ability_used[GOD_LUGONU] = (bool)you.num_total_gifts[GOD_LUGONU];
        you.one_time_ability_used[GOD_SHINING_ONE] = (bool)you.num_total_gifts[GOD_SHINING_ONE];
        you.one_time_ability_used[GOD_ZIN] = (bool)you.num_total_gifts[GOD_ZIN];
    }
#endif
    for (i = 0; i < count; i++)
        you.piety_max[i] = unmarshallByte(th);
    count = unmarshallByte(th);
    ASSERT(count == NUM_NEMELEX_GIFT_TYPES);
    for (i = 0; i < count; i++)
        you.nemelex_sacrificing[i] = unmarshallBoolean(th);

    you.gift_timeout   = unmarshallByte(th);

    you.hell_exit      = unmarshallByte(th);
    you.hell_branch = static_cast<branch_type>(unmarshallByte(th));
    ASSERT(you.hell_branch < NUM_BRANCHES);

    you.exp_docked       = unmarshallInt(th);
    you.exp_docked_total = unmarshallInt(th);

    // elapsed time
    you.elapsed_time   = unmarshallInt(th);
    you.elapsed_time_at_last_input = you.elapsed_time;

    // time of character creation
    you.birth_time = unmarshallInt(th);

    you.real_time  = unmarshallInt(th);
    you.num_turns  = unmarshallInt(th);
    you.exploration = unmarshallInt(th);

    you.magic_contamination = unmarshallShort(th);

    you.transit_stair  = static_cast<dungeon_feature_type>(unmarshallShort(th));
    you.entering_level = unmarshallByte(th);

    you.deaths = unmarshallByte(th);
    you.lives = unmarshallByte(th);
    you.dead = !you.hp;

    int n_dact = unmarshallInt(th);
    ASSERT(n_dact >= 0);
    ASSERT(n_dact < 100000); // arbitrary, sanity check
    you.dactions.resize(n_dact, NUM_DACTIONS);
    for (i = 0; i < n_dact; i++)
    {
        int a = unmarshallUByte(th);
        ASSERT(a < NUM_DACTIONS);
        you.dactions[i] = static_cast<daction_type>(a);
    }

    you.level_stack.clear();
    int n_levs = unmarshallInt(th);
    for (int k = 0; k < n_levs; k++)
    {
        level_pos pos;
        pos.load(th);
        you.level_stack.push_back(pos);
    }

    // List of currently beholding monsters (usually empty).
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (i = 0; i < count; i++)
        you.beholders.push_back(unmarshallShort(th));

    // Also usually empty.
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (i = 0; i < count; i++)
        you.fearmongers.push_back(unmarshallShort(th));

    you.piety_hysteresis = unmarshallByte(th);

    you.m_quiver->load(th);

    you.friendly_pickup = unmarshallByte(th);

    you.zotdef_wave_name = unmarshallString(th);

#if TAG_MAJOR_VERSION <= 33
    for (unsigned int k = 0; k < ARRAYSZ(you.montiers); k++)
        you.montiers[k] = unmarshallInt(th);
#endif

    // Counts of actions made, by type.
    count = unmarshallShort(th);
    for (i = 0; i < count; i++)
    {
        caction_type caction = (caction_type)unmarshallShort(th);
        int subtype = unmarshallInt(th);
        for (j = 0; j < 27; j++)
            you.action_count[std::pair<caction_type, int>(caction, subtype)][j] = unmarshallInt(th);
    }
    abyssal_state.major_coord = unmarshallCoord(th);
    abyssal_state.depth = unmarshallFloat(th);
    abyssal_state.phase = unmarshallFloat(th);

    // Don't undo digging on the next tick after a load.
    if (you.where_are_you == BRANCH_ABYSS)
        recompute_saved_abyss_features();

    _unmarshall_constriction(th, &you);

    you.octopus_king_rings = unmarshallUByte(th);

    if (!dlua.callfn("dgn_load_data", "u", &th))
    {
        mprf(MSGCH_ERROR, "Failed to load Lua persist table: %s",
             dlua.error.c_str());
    }

    unmarshallString(th);

    you.props.clear();
    you.props.read(th);
}

static void tag_read_you_items(reader &th)
{
    int i,j;
    int count, count2;

    // how many inventory slots?
    count = unmarshallByte(th);
    ASSERT(count == ENDOFPACK); // not supposed to change
    for (i = 0; i < count; ++i)
        unmarshallItem(th, you.inv[i]);

    unmarshallFixedBitVector<NUM_RUNE_TYPES>(th, you.runes);
    you.obtainable_runes = unmarshallByte(th);

    // Item descrip for each type & subtype.
    // how many types?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_IDESC);
    // how many subtypes?
    count2 = unmarshallUByte(th);
    ASSERT(count2 <= MAX_SUBTYPES);
    for (i = 0; i < count; ++i)
        for (j = 0; j < count2; ++j)
            you.item_description[i][j] = unmarshallByte(th);
    for (i = 0; i < count; ++i)
        for (j = count2; j < MAX_SUBTYPES; ++j)
            you.item_description[i][j] = 0;
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() < TAG_MINOR_DETECT_CURSE_REMOVAL)
        for (j = SCR_REMOVE_CURSE + 1; j + 1 < count2; ++j)
        {
            you.item_description[IDESC_SCROLLS][j] = you.item_description[IDESC_SCROLLS][j+1];
            you.item_description[IDESC_SCROLLS_II][j] = you.item_description[IDESC_SCROLLS_II][j+1];
        }

    int iclasses = th.getMinorVersion() < TAG_MINOR_OBJ_RODS
        ? 14
        : unmarshallUByte(th);
#else
    int iclasses = unmarshallUByte(th);
#endif
    ASSERT(iclasses <= NUM_OBJECT_CLASSES);

    // Identification status.
    for (i = 0; i < iclasses; ++i)
    {
        if (!item_type_has_ids((object_class_type)i))
            continue;
        for (j = 0; j < count2; ++j)
        {
            uint8_t x = unmarshallUByte(th);
            ASSERT(x < NUM_ID_STATE_TYPES);
            you.type_ids[i][j] = static_cast<item_type_id_state_type>(x);
        }
        for (j = count2; j < MAX_SUBTYPES; ++j)
            you.type_ids[i][j] = ID_UNKNOWN_TYPE;
    }
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() < TAG_MINOR_DETECT_CURSE_REMOVAL)
        for (j = SCR_REMOVE_CURSE + 1; j + 1 < MAX_SUBTYPES; ++j)
            you.type_ids[OBJ_SCROLLS][j] = you.type_ids[OBJ_SCROLLS][j+1];
#endif

    // Additional identification info
    you.type_id_props.read(th);

#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() == TAG_MINOR_KNOWN_MISSILES)
    {
        count = unmarshallShort(th);
        for (i = 0; i < count; ++i)
            unmarshallInt(th);
    }
#endif

    // how many unique items?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NO_UNRANDARTS <= 256);
    for (j = 0; j < count && j < NO_UNRANDARTS; ++j)
    {
        you.unique_items[j] =
            static_cast<unique_item_status_type>(unmarshallByte(th));
    }
    // # of unrandarts could certainly change.
    // If it does, the new ones won't exist yet - zero them out.
    for (; j < NO_UNRANDARTS; j++)
        you.unique_items[j] = UNIQ_NOT_EXISTS;
    for (j = NO_UNRANDARTS; j < count; j++)
        unmarshallByte(th);

    // how many books?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_FIXED_BOOKS <= 256);
    for (j = 0; j < count && j < NUM_FIXED_BOOKS; ++j)
        you.had_book[j] = unmarshallByte(th);
    for (j = count; j < NUM_FIXED_BOOKS; ++j)
        you.had_book[j] = 0;
    for (j = NUM_FIXED_BOOKS; j < count; ++j)
        unmarshallByte(th);

    // how many spells?
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_SPELLS; ++j)
        you.seen_spell[j] = unmarshallByte(th);
    for (j = count; j < NUM_SPELLS; ++j)
        you.seen_spell[j] = 0;
    for (j = NUM_SPELLS; j < count; ++j)
        unmarshallByte(th);

    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_WEAPONS; ++j)
        you.seen_weapon[j] = unmarshallInt(th);
    for (j = count; j < NUM_WEAPONS; ++j)
        you.seen_weapon[j] = 0;
    for (j = NUM_WEAPONS; j < count; ++j)
        unmarshallInt(th);

    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_ARMOURS; ++j)
        you.seen_armour[j] = unmarshallInt(th);
    for (j = count; j < NUM_ARMOURS; ++j)
        you.seen_armour[j] = 0;
    for (j = NUM_ARMOURS; j < count; ++j)
        unmarshallInt(th);
    unmarshallFixedBitVector<NUM_MISCELLANY>(th, you.seen_misc);

#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() >= TAG_MINOR_AUTOPICKUP_TABLE)
#endif
        for (i = 0; i < iclasses; i++)
            for (j = 0; j < count2; j++)
                you.force_autopickup[i][j] = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 33
    // Reset old broken force_autopickup tables.
    if (th.getMinorVersion() < TAG_MINOR_CLEAR_APTABLE)
    {
        for (i = 0; i < iclasses; i++)
            for (j = 0; j < count2; j++)
                you.force_autopickup[i][j] = 0;
    }

    if (th.getMinorVersion() < TAG_MINOR_DETECT_CURSE_REMOVAL)
        for (j = SCR_REMOVE_CURSE + 1; j + 1 < MAX_SUBTYPES; ++j)
            you.force_autopickup[OBJ_SCROLLS][j] = you.force_autopickup[OBJ_SCROLLS][j+1];
#endif
}

static PlaceInfo unmarshallPlaceInfo(reader &th)
{
    PlaceInfo place_info;

    place_info.branch      = static_cast<branch_type>(unmarshallInt(th));

    place_info.num_visits  = unmarshallInt(th);
    place_info.levels_seen = unmarshallInt(th);

    place_info.mon_kill_exp       = unmarshallInt(th);

    for (int i = 0; i < KC_NCATEGORIES; i++)
        place_info.mon_kill_num[i] = unmarshallInt(th);

    place_info.turns_total      = unmarshallInt(th);
    place_info.turns_explore    = unmarshallInt(th);
    place_info.turns_travel     = unmarshallInt(th);
    place_info.turns_interlevel = unmarshallInt(th);
    place_info.turns_resting    = unmarshallInt(th);
    place_info.turns_other      = unmarshallInt(th);

    place_info.elapsed_total      = unmarshallInt(th);
    place_info.elapsed_explore    = unmarshallInt(th);
    place_info.elapsed_travel     = unmarshallInt(th);
    place_info.elapsed_interlevel = unmarshallInt(th);
    place_info.elapsed_resting    = unmarshallInt(th);
    place_info.elapsed_other      = unmarshallInt(th);

    return place_info;
}

static void tag_read_you_dungeon(reader &th)
{
    // how many unique creatures?
    int count = unmarshallShort(th);
    you.unique_creatures.init(false);
    for (int j = 0; j < count; ++j)
    {
        const bool created = unmarshallBoolean(th);

        if (j < NUM_MONSTERS)
            you.unique_creatures[j] = created;
    }

    // how many branches?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_BRANCHES);
    for (int j = 0; j < count; ++j)
    {
        brdepth[j]    = unmarshallInt(th);
        ASSERT(brdepth[j] >= -1);
        ASSERT(brdepth[j] <= MAX_BRANCH_DEPTH);
        startdepth[j] = unmarshallInt(th);
    }
    ASSERT(you.depth <= brdepth[you.where_are_you]);

    // Root of the dungeon; usually BRANCH_MAIN_DUNGEON.
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() >= TAG_MINOR_ROOT_BRANCH)
#endif
        root_branch = static_cast<branch_type>(unmarshallInt(th));
#if TAG_MAJOR_VERSION == 33
    else
        root_branch = BRANCH_MAIN_DUNGEON;
#endif

    unmarshallMap(th, stair_level,
                  unmarshall_int_as<branch_type>,
                  unmarshall_level_id_set);
    unmarshallMap(th, shops_present,
                  unmarshall_level_pos, unmarshall_int_as<shop_type>);
    unmarshallMap(th, altars_present,
                  unmarshall_level_pos, unmarshall_int_as<god_type>);
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() < TAG_MINOR_UNIFIED_PORTALS)
    {
        // std::map<level_pos, portal_type> portals_present
        count = unmarshallInt(th);
        for (int i = 0; i < count; ++i)
        {
            level_pos p = unmarshall_level_pos(th);
            switch (static_cast<oldportal_type>(unmarshallInt(th)))
            {
            case PORTAL_LABYRINTH:
                portals_present[p] = BRANCH_LABYRINTH;
                break;
            case PORTAL_HELL:
                portals_present[p] = BRANCH_VESTIBULE_OF_HELL;
                break;
            case PORTAL_ABYSS:
                portals_present[p] = BRANCH_ABYSS;
                break;
            case PORTAL_PANDEMONIUM:
                portals_present[p] = BRANCH_PANDEMONIUM;
                break;
            default: ; // buggy portals
            }
        }

        // std::map<level_pos, std::string> portal_vaults_present
        count = unmarshallInt(th);
        for (int i = 0; i < count; ++i)
        {
            unmarshall_level_pos(th);
            unmarshallStringNoMax(th);
        }
    }
    else
#endif
    unmarshallMap(th, portals_present,
                  unmarshall_level_pos, unmarshall_int_as<branch_type>);
    unmarshallMap(th, portal_notes,
                  unmarshall_level_pos, unmarshallStringNoMax);
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() < TAG_MINOR_UNIFIED_PORTALS)
    {
        // std::map<level_pos, uint8_t> portal_vault_colours
        count = unmarshallInt(th);
        for (int i = 0; i < count; ++i)
        {
            unmarshall_level_pos(th);
            unmarshallUByte(th);
        }
    }
#endif
    unmarshallMap(th, level_annotations,
                  unmarshall_level_id, unmarshallStringNoMax);
    unmarshallMap(th, level_exclusions,
                  unmarshall_level_id, unmarshallStringNoMax);
    unmarshallMap(th, level_uniques,
                  unmarshall_level_id, unmarshallStringNoMax);
    unmarshallUniqueAnnotations(th);

    PlaceInfo place_info = unmarshallPlaceInfo(th);
    ASSERT(place_info.is_global());
    you.set_place_info(place_info);

    std::vector<PlaceInfo> list = you.get_all_place_info();
    unsigned short count_p = (unsigned short) unmarshallShort(th);
    // Use "<=" so that adding more branches or non-dungeon places
    // won't break save-file compatibility.
    ASSERT(count_p <= list.size());

    for (int i = 0; i < count_p; i++)
    {
        place_info = unmarshallPlaceInfo(th);
        ASSERT(!place_info.is_global());
        you.set_place_info(place_info);
    }

    typedef std::pair<string_set::iterator, bool> ssipair;
    unmarshall_container(th, you.uniq_map_tags,
                         (ssipair (string_set::*)(const std::string &))
                         &string_set::insert,
                         unmarshallStringNoMax);
    unmarshall_container(th, you.uniq_map_names,
                         (ssipair (string_set::*)(const std::string &))
                         &string_set::insert,
                         unmarshallStringNoMax);

    read_level_connectivity(th);
}

static void tag_read_lost_monsters(reader &th)
{
    the_lost_ones.clear();
    unmarshallMap(th, the_lost_ones,
                  unmarshall_level_id, unmarshall_follower_list);
}

static void tag_read_lost_items(reader &th)
{
    transiting_items.clear();

    unmarshallMap(th, transiting_items,
                  unmarshall_level_id, unmarshall_item_list);
}

template <typename Z>
static int _last_used_index(const Z &thinglist, int max_things)
{
    for (int i = max_things - 1; i >= 0; --i)
        if (thinglist[i].defined())
            return (i + 1);
    return 0;
}

// ------------------------------- level tags ---------------------------- //

static void tag_construct_level(writer &th)
{
    marshallByte(th, env.floor_colour);
    marshallByte(th, env.rock_colour);

    marshallInt(th, env.level_flags);

    marshallInt(th, you.elapsed_time);
    marshallCoord(th, you.pos());

    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallInt(th, env.turns_on_level);

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallByte(th, grd[count_x][count_y]);
            marshallMapCell(th, env.map_knowledge[count_x][count_y]);
            marshallInt(th, env.pgrid[count_x][count_y]);
        }

    _run_length_encode(th, marshallByte, env.grid_colours, GXM, GYM);

    // how many clouds?
    const int nc = _last_used_index(env.cloud, MAX_CLOUDS);
    marshallShort(th, nc);
    for (int i = 0; i < nc; i++)
    {
        marshallByte(th, env.cloud[i].type);
        if (env.cloud[i].type == CLOUD_NONE)
            continue;
        marshallByte(th, env.cloud[i].pos.x);
        marshallByte(th, env.cloud[i].pos.y);
        marshallShort(th, env.cloud[i].decay);
        marshallByte(th, env.cloud[i].spread_rate);
        marshallByte(th, env.cloud[i].whose);
        marshallByte(th, env.cloud[i].killer);
        marshallInt(th, env.cloud[i].source);
        marshallShort(th, env.cloud[i].colour);
        marshallString(th, env.cloud[i].name);
        marshallString(th, env.cloud[i].tile);
        marshallInt(th, env.cloud[i].excl_rad);
    }

    // how many shops?
    const int ns = _last_used_index(env.shop, MAX_SHOPS);
    marshallShort(th, ns);
    for (int i = 0; i < ns; i++)
    {
        marshallByte(th, env.shop[i].type);
        if (env.shop[i].type == SHOP_UNASSIGNED)
            continue;
        marshallByte(th, env.shop[i].keeper_name[0]);
        marshallByte(th, env.shop[i].keeper_name[1]);
        marshallByte(th, env.shop[i].keeper_name[2]);
        marshallByte(th, env.shop[i].pos.x);
        marshallByte(th, env.shop[i].pos.y);
        marshallByte(th, env.shop[i].greed);
        marshallByte(th, env.shop[i].level);
        marshallString(th, env.shop[i].shop_name);
        marshallString(th, env.shop[i].shop_type_name);
        marshallString(th, env.shop[i].shop_suffix_name);
    }

    marshallCoord(th, env.sanctuary_pos);
    marshallByte(th, env.sanctuary_time);

    marshallInt(th, env.spawn_random_rate);

    env.markers.write(th);
    env.properties.write(th);

    marshallInt(th, you.dactions.size());

    // Save heightmap, if present.
    marshallByte(th, !!env.heightmap.get());
    if (env.heightmap.get())
    {
        grid_heightmap &heightmap(*env.heightmap);
        for (rectangle_iterator ri(0); ri; ++ri)
            marshallShort(th, heightmap(*ri));
    }

    marshallInt(th, env.forest_awoken_until);
    marshall_level_vault_data(th);
    marshallInt(th, env.density);

    marshallShort(th, env.sunlight.size());
    for (size_t i = 0; i < env.sunlight.size(); ++i)
    {
        marshallCoord(th, env.sunlight[i].first);
        marshallInt(th, env.sunlight[i].second);
    }
}

void marshallItem(writer &th, const item_def &item, bool iinfo)
{
    marshallByte(th, item.base_type);
    if (item.base_type == OBJ_UNASSIGNED)
        return;

    ASSERT(item.is_valid(iinfo));

    marshallByte(th, item.sub_type);
    marshallShort(th, item.plus);
    marshallShort(th, item.plus2);
    marshallInt(th, item.special);
    marshallShort(th, item.quantity);

    marshallByte(th, item.colour);
    marshallByte(th, item.rnd);
    marshallShort(th, item.pos.x);
    marshallShort(th, item.pos.y);
    marshallInt(th, item.flags);

    marshallShort(th, item.link);
    if (item.pos.x >= 0 && item.pos.y >= 0)
        marshallShort(th, igrd(item.pos));  //  unused
    else
        marshallShort(th, -1); // unused

    marshallByte(th, item.slot);

    marshallShort(th, item.orig_place);
    marshallShort(th, item.orig_monnum);
    marshallString(th, item.inscription.c_str(), 80);

    item.props.write(th);
}

void unmarshallItem(reader &th, item_def &item)
{
    item.base_type   = static_cast<object_class_type>(unmarshallByte(th));
    if (item.base_type == OBJ_UNASSIGNED)
        return;
    item.sub_type    = unmarshallUByte(th);
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() < TAG_MINOR_DETECT_CURSE_REMOVAL
        && item.base_type == OBJ_SCROLLS && item.sub_type > SCR_REMOVE_CURSE)
    {
        --item.sub_type;
    }
#endif
    item.plus        = unmarshallShort(th);
    item.plus2       = unmarshallShort(th);
    item.special     = unmarshallInt(th);
    item.quantity    = unmarshallShort(th);
    item.colour      = unmarshallUByte(th);
    item.rnd         = unmarshallUByte(th);

    item.pos.x       = unmarshallShort(th);
    item.pos.y       = unmarshallShort(th);
    item.flags       = unmarshallInt(th);
    item.link        = unmarshallShort(th);

    unmarshallShort(th);  // igrd[item.x][item.y] -- unused

    item.slot        = unmarshallByte(th);

    item.orig_place  = unmarshallShort(th);
    item.orig_monnum = unmarshallShort(th);
    item.inscription = unmarshallString(th, 80);

    item.props.clear();
    item.props.read(th);

#if TAG_MAJOR_VERSION == 33
    if (item.base_type == OBJ_MISSILES && item.sub_type != MI_THROWING_NET)
        item.plus = 0; // allow stacking

    if (th.getMinorVersion() < TAG_MINOR_OBJ_RODS && item.base_type == OBJ_STAVES)
    {
        const int STAFF_FIRST_ROD = 13;
        const int OLD_NUM_STAVES = 23;
        if (item.sub_type >= STAFF_FIRST_ROD && item.sub_type < OLD_NUM_STAVES)
        {
            item.base_type = OBJ_RODS;
            item.sub_type -= STAFF_FIRST_ROD;
            if (item.props.exists("rod_enchantment"))
            {
                item.special = item.props["rod_enchantment"].get_short();
                item.props.erase("rod_enchantment");
            }
            else
                item.special = 0; // paranoia
        }
        else if (item.sub_type >= OLD_NUM_STAVES) // recover from recent bugs
            item.sub_type = random2(NUM_STAVES);
    }
#endif

    // Fixup artefact props to handle reloading items when the new version
    // of Crawl has more artefact props.
    if (is_artefact(item))
        artefact_fixup_props(item);
}

#define MAP_SERIALIZE_FLAGS_MASK 3
#define MAP_SERIALIZE_FLAGS_8 1
#define MAP_SERIALIZE_FLAGS_16 2
#define MAP_SERIALIZE_FLAGS_32 3

#define MAP_SERIALIZE_FEATURE 4
#define MAP_SERIALIZE_FEATURE_COLOUR 8
#define MAP_SERIALIZE_ITEM 0x10
#define MAP_SERIALIZE_CLOUD 0x20
#define MAP_SERIALIZE_MONSTER 0x40

void marshallMapCell(writer &th, const map_cell &cell)
{
    unsigned flags = 0;

    if (cell.flags > 0xffff)
        flags |= MAP_SERIALIZE_FLAGS_32;
    else if (cell.flags > 0xff)
        flags |= MAP_SERIALIZE_FLAGS_16;
    else if (cell.flags)
        flags |= MAP_SERIALIZE_FLAGS_8;

    if (cell.feat() != DNGN_UNSEEN)
        flags |= MAP_SERIALIZE_FEATURE;

    if (cell.feat_colour())
        flags |= MAP_SERIALIZE_FEATURE_COLOUR;

    if (cell.cloud() != CLOUD_NONE)
        flags |= MAP_SERIALIZE_CLOUD;

    if (cell.item())
        flags |= MAP_SERIALIZE_ITEM;

    if (cell.monster() != MONS_NO_MONSTER)
        flags |= MAP_SERIALIZE_MONSTER;

    marshallUnsigned(th, flags);

    switch (flags & MAP_SERIALIZE_FLAGS_MASK)
    {
    case MAP_SERIALIZE_FLAGS_8:
        marshallByte(th, cell.flags);
        break;
    case MAP_SERIALIZE_FLAGS_16:
        marshallShort(th, cell.flags);
        break;
    case MAP_SERIALIZE_FLAGS_32:
        marshallInt(th, cell.flags);
        break;
    }

    if (flags & MAP_SERIALIZE_FEATURE)
        marshallUnsigned(th, cell.feat());

    if (flags & MAP_SERIALIZE_FEATURE_COLOUR)
        marshallUnsigned(th, cell.feat_colour());

    if (feat_is_trap(cell.feat()))
        marshallByte(th, cell.trap());

    if (flags & MAP_SERIALIZE_CLOUD)
    {
        cloud_info* ci = cell.cloudinfo();
        marshallUnsigned(th, ci->type);
        marshallUnsigned(th, ci->colour);
        marshallUnsigned(th, ci->duration);
        marshallShort(th, ci->tile);
    }

    if (flags & MAP_SERIALIZE_ITEM)
        marshallItem(th, *cell.item(), true);

    if (flags & MAP_SERIALIZE_MONSTER)
        marshallMonsterInfo(th, *cell.monsterinfo());
}

void unmarshallMapCell(reader &th, map_cell& cell)
{
    unsigned flags = unmarshallUnsigned(th);
    unsigned cell_flags = 0;
    trap_type trap = TRAP_UNASSIGNED;

    cell.clear();

    switch (flags & MAP_SERIALIZE_FLAGS_MASK)
    {
    case MAP_SERIALIZE_FLAGS_8:
        cell_flags = unmarshallByte(th);
        break;
    case MAP_SERIALIZE_FLAGS_16:
        cell_flags = unmarshallShort(th);
        break;
    case MAP_SERIALIZE_FLAGS_32:
        cell_flags = unmarshallInt(th);
        break;
    }

    dungeon_feature_type feature = DNGN_UNSEEN;
    unsigned feat_colour = 0;

    if (flags & MAP_SERIALIZE_FEATURE)
        feature = (dungeon_feature_type)unmarshallUnsigned(th);
#if TAG_MAJOR_VERSION == 33
    if (feature == DNGN_OLD_WAX_WALL)
        feature = DNGN_ROCK_WALL;
#endif

    if (flags & MAP_SERIALIZE_FEATURE_COLOUR)
        feat_colour = unmarshallUnsigned(th);

    if (feat_is_trap(feature))
    {
        trap = (trap_type)unmarshallByte(th);
#if TAG_MAJOR_VERSION == 33
        if (trap == TRAP_AXED)
            trap = TRAP_SPEAR;
#endif
    }

    cell.set_feature(feature, feat_colour, trap);

    if (flags & MAP_SERIALIZE_CLOUD)
    {
        cloud_info ci;
        ci.type = (cloud_type)unmarshallUnsigned(th);
        unmarshallUnsigned(th, ci.colour);
        unmarshallUnsigned(th, ci.duration);
        ci.tile = unmarshallShort(th);
        cell.set_cloud(ci);
    }

    if (flags & MAP_SERIALIZE_ITEM)
    {
        item_def item;
        unmarshallItem(th, item);
        cell.set_item(item, false);
    }

    if (flags & MAP_SERIALIZE_MONSTER)
    {
        monster_info mi;
        unmarshallMonsterInfo(th, mi);
        cell.set_monster(mi);
    }

    // set this last so the other sets don't override this
    cell.flags = cell_flags;
}

static void tag_construct_level_items(writer &th)
{
    // how many traps?
    const int nt = _last_used_index(env.trap, MAX_TRAPS);
    marshallShort(th, nt);
    for (int i = 0; i < nt; ++i)
    {
        marshallByte(th, env.trap[i].type);
        if (env.trap[i].type == TRAP_UNASSIGNED)
            continue;
        marshallCoord(th, env.trap[i].pos);
        marshallShort(th, env.trap[i].ammo_qty);
    }

    // how many items?
    const int ni = _last_used_index(mitm, MAX_ITEMS);
    marshallShort(th, ni);
    for (int i = 0; i < ni; ++i)
        marshallItem(th, mitm[i]);
}

static void marshall_mon_enchant(writer &th, const mon_enchant &me)
{
    marshallShort(th, me.ench);
    marshallShort(th, me.degree);
    marshallShort(th, me.who);
    marshallInt(th, me.source);
    marshallShort(th, std::min(me.duration, INFINITE_DURATION));
    marshallShort(th, std::min(me.maxduration, INFINITE_DURATION));
}

static mon_enchant unmarshall_mon_enchant(reader &th)
{
    mon_enchant me;
    me.ench        = static_cast<enchant_type>(unmarshallShort(th));
    me.degree      = unmarshallShort(th);
    me.who         = static_cast<kill_category>(unmarshallShort(th));
    me.source      = unmarshallInt(th);
    me.duration    = unmarshallShort(th);
    me.maxduration = unmarshallShort(th);
    return me;
}

void marshallMonster(writer &th, const monster& m)
{
    if (!m.alive())
    {
        marshallShort(th, MONS_NO_MONSTER);
        return;
    }

    marshallShort(th, m.type);
    ASSERT(m.mid > 0);
    marshallInt(th, m.mid);
    marshallString(th, m.mname);
    marshallByte(th, m.ac);
    marshallByte(th, m.ev);
    marshallByte(th, m.hit_dice);
    marshallByte(th, m.speed);
    marshallByte(th, m.speed_increment);
    marshallByte(th, m.behaviour);
    marshallByte(th, m.pos().x);
    marshallByte(th, m.pos().y);
    marshallByte(th, m.target.x);
    marshallByte(th, m.target.y);
    marshallCoord(th, m.firing_pos);
    marshallCoord(th, m.patrol_point);
    int help = m.travel_target;
    marshallByte(th, help);

    marshallShort(th, m.travel_path.size());
    for (unsigned int i = 0; i < m.travel_path.size(); i++)
        marshallCoord(th, m.travel_path[i]);

    marshallUnsigned(th, m.flags);
    marshallInt(th, m.experience);

    marshallShort(th, m.enchantments.size());
    for (mon_enchant_list::const_iterator i = m.enchantments.begin();
         i != m.enchantments.end(); ++i)
    {
        marshall_mon_enchant(th, i->second);
    }
    marshallByte(th, m.ench_countdown);

    marshallShort(th, std::min(m.hit_points, MAX_MONSTER_HP));
    marshallShort(th, std::min(m.max_hit_points, MAX_MONSTER_HP));
    marshallInt(th, m.number);
    marshallShort(th, m.base_monster);
    marshallShort(th, m.colour);

    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
        marshallShort(th, m.inv[j]);

    marshallSpells(th, m.spells);
    marshallByte(th, m.god);
    marshallByte(th, m.attitude);
    marshallShort(th, m.foe);
    marshallInt(th, m.foe_memory);
    marshallShort(th, m.damage_friendly);
    marshallShort(th, m.damage_total);

    if (mons_is_ghost_demon(m.type))
    {
        // *Must* have ghost field set.
        ASSERT(m.ghost.get());
        marshallGhost(th, *m.ghost);
    }

    _marshall_constriction(th, &m);

    m.props.write(th);
}

void marshallMonsterInfo(writer &th, const monster_info& mi)
{
    marshallFixedBitVector<NUM_MB_FLAGS>(th, mi.mb);
    marshallString(th, mi.mname);
    marshallUnsigned(th, mi.type);
    marshallUnsigned(th, mi.base_type);
    if (mons_genus(mi.type) == MONS_DRACONIAN)
        marshallUnsigned(th, mi.draco_type);
    marshallUnsigned(th, mi.number);
    marshallUnsigned(th, mi.colour);
    marshallUnsigned(th, mi.attitude);
    marshallUnsigned(th, mi.threat);
    marshallUnsigned(th, mi.dam);
    marshallUnsigned(th, mi.fire_blocker);
    marshallString(th, mi.description);
    marshallString(th, mi.quote);
    marshallUnsigned(th, mi.holi);
    marshallUnsigned(th, mi.mintel);
    marshallInt(th, mi.mresists);
    marshallUnsigned(th, mi.mitemuse);
    marshallByte(th, mi.mbase_speed);
    marshallUnsigned(th, mi.fly);
    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (mi.inv[i].get())
        {
            marshallBoolean(th, true);
            marshallItem(th, *mi.inv[i].get(), true);
        }
        else
            marshallBoolean(th, false);
    }
    if (mons_is_pghost(mi.type))
    {
        marshallUnsigned(th, mi.u.ghost.species);
        marshallUnsigned(th, mi.u.ghost.job);
        marshallUnsigned(th, mi.u.ghost.religion);
        marshallUnsigned(th, mi.u.ghost.best_skill);
        marshallShort(th, mi.u.ghost.best_skill_rank);
        marshallShort(th, mi.u.ghost.xl_rank);
        marshallShort(th, mi.u.ghost.damage);
        marshallShort(th, mi.u.ghost.ac);
    }

    mi.props.write(th);
}

void unmarshallMonsterInfo(reader &th, monster_info& mi)
{
    unmarshallFixedBitVector<NUM_MB_FLAGS>(th, mi.mb);
    mi.mname = unmarshallString(th);
    unmarshallUnsigned(th, mi.type);
    ASSERT(!invalid_monster_type(mi.type));
    unmarshallUnsigned(th, mi.base_type);
    if (mons_genus(mi.type) == MONS_DRACONIAN)
        unmarshallUnsigned(th, mi.draco_type);
    unmarshallUnsigned(th, mi.number);
    unmarshallUnsigned(th, mi.colour);
    unmarshallUnsigned(th, mi.attitude);
    unmarshallUnsigned(th, mi.threat);
    unmarshallUnsigned(th, mi.dam);
    unmarshallUnsigned(th, mi.fire_blocker);
    mi.description = unmarshallString(th);
    mi.quote = unmarshallString(th);

    unmarshallUnsigned(th, mi.holi);
    unmarshallUnsigned(th, mi.mintel);
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() < TAG_MINOR_BITFIELD_RESISTS)
    {
        for (int i = 0; i < 10; i++)
            unmarshallByte(th);
        mi.mresists = 0;
    }
    else
#endif
    mi.mresists = unmarshallInt(th);
    unmarshallUnsigned(th, mi.mitemuse);
    mi.mbase_speed = unmarshallByte(th);

    unmarshallUnsigned(th, mi.fly);

    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
#if TAG_MAJOR_VERSION == 33
        if ((th.getMinorVersion() < TAG_MINOR_MONSTER_JEWELLERY) &&
            (i == MSLOT_JEWELLERY))
        {
            continue;
        }
#endif
        if (unmarshallBoolean(th))
        {
            mi.inv[i].reset(new item_def());
            unmarshallItem(th, *mi.inv[i].get());
        }
    }

    if (mons_is_pghost(mi.type))
    {
        unmarshallUnsigned(th, mi.u.ghost.species);
        unmarshallUnsigned(th, mi.u.ghost.job);
        unmarshallUnsigned(th, mi.u.ghost.religion);
        unmarshallUnsigned(th, mi.u.ghost.best_skill);
        mi.u.ghost.best_skill_rank = unmarshallShort(th);
        mi.u.ghost.xl_rank = unmarshallShort(th);
        mi.u.ghost.damage = unmarshallShort(th);
        mi.u.ghost.ac = unmarshallShort(th);
    }

    mi.props.clear();
    mi.props.read(th);
}

static void tag_construct_level_monsters(writer &th)
{
    int nm = 0;
    for (int i = 0; i < MAX_MONS_ALLOC; ++i)
        if (env.mons_alloc[i] != MONS_NO_MONSTER)
            nm = i + 1;

    // how many mons_alloc?
    marshallByte(th, nm);
    for (int i = 0; i < nm; ++i)
        marshallShort(th, env.mons_alloc[i]);

    // how many monsters?
    nm = _last_used_index(menv, MAX_MONSTERS);
    marshallShort(th, nm);

    for (int i = 0; i < nm; i++)
    {
        monster& m(menv[i]);

#if defined(DEBUG) || defined(DEBUG_MONS_SCAN)
        if (m.type != MONS_NO_MONSTER)
        {
            if (invalid_monster_type(m.type))
            {
                mprf(MSGCH_ERROR, "Marshalled monster #%d %s",
                     i, m.name(DESC_PLAIN, true).c_str());
            }
            if (!in_bounds(m.pos()))
            {
                mprf(MSGCH_ERROR,
                     "Marshalled monster #%d %s out of bounds at (%d, %d)",
                     i, m.name(DESC_PLAIN, true).c_str(),
                     m.pos().x, m.pos().y);
            }
        }
#endif
        marshallMonster(th, m);
    }
}

void tag_construct_level_tiles(writer &th)
{
    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallShort(th, env.tile_names.size());
    for (unsigned int i = 0; i < env.tile_names.size(); ++i)
    {
        marshallString(th, env.tile_names[i]);
#ifdef DEBUG_TILE_NAMES
        mprf("Writing '%s' into save.", env.tile_names[i].c_str());
#endif
    }

    // flavour
    marshallShort(th, env.tile_default.wall_idx);
    marshallShort(th, env.tile_default.floor_idx);

    marshallShort(th, env.tile_default.wall);
    marshallShort(th, env.tile_default.floor);
    marshallShort(th, env.tile_default.special);

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallShort(th, env.tile_flv[count_x][count_y].wall_idx);
            marshallShort(th, env.tile_flv[count_x][count_y].floor_idx);
            marshallShort(th, env.tile_flv[count_x][count_y].feat_idx);

            marshallShort(th, env.tile_flv[count_x][count_y].wall);
            marshallShort(th, env.tile_flv[count_x][count_y].floor);
            marshallShort(th, env.tile_flv[count_x][count_y].feat);
            marshallShort(th, env.tile_flv[count_x][count_y].special);
        }

    marshallInt(th, TILE_WALL_MAX);
}

static void tag_read_level(reader &th)
{
    env.floor_colour = unmarshallUByte(th);
    env.rock_colour  = unmarshallUByte(th);

    env.level_flags  = unmarshallInt(th);

    env.elapsed_time = unmarshallInt(th);
    env.old_player_pos = unmarshallCoord(th);
    env.absdepth0 = absdungeon_depth(you.where_are_you, you.depth);

    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);
    ASSERT(gx == GXM);
    ASSERT(gy == GYM);

    env.turns_on_level = unmarshallInt(th);

    for (int i = 0; i < gx; i++)
        for (int j = 0; j < gy; j++)
        {
            dungeon_feature_type feat = static_cast<dungeon_feature_type>(unmarshallUByte(th));
            grd[i][j] = feat;
            ASSERT(feat < NUM_FEATURES);
#if TAG_MAJOR_VERSION == 33
            if (feat == DNGN_OLD_WAX_WALL)
                grd[i][j] = DNGN_ROCK_WALL;
            if (you.where_are_you == root_branch && you.depth == 1
                && (feat == DNGN_ESCAPE_HATCH_UP
                    || feat == DNGN_STONE_STAIRS_UP_I
                    || feat == DNGN_STONE_STAIRS_UP_II
                    || feat == DNGN_STONE_STAIRS_UP_III))
            {
                grd[i][j] = DNGN_EXIT_DUNGEON;
            }
#endif

            unmarshallMapCell(th, env.map_knowledge[i][j]);
            // Fixup positions
            if (env.map_knowledge[i][j].monsterinfo())
                env.map_knowledge[i][j].monsterinfo()->pos = coord_def(i, j);
            if (env.map_knowledge[i][j].cloudinfo())
                env.map_knowledge[i][j].cloudinfo()->pos = coord_def(i, j);

            env.map_knowledge[i][j].flags &=~ MAP_VISIBLE_FLAG;
            env.pgrid[i][j] = unmarshallInt(th);

            mgrd[i][j] = NON_MONSTER;
            env.cgrid[i][j] = EMPTY_CLOUD;
            env.tgrid[i][j] = NON_ENTITY;
        }

    env.grid_colours.init(BLACK);
    _run_length_decode(th, unmarshallByte, env.grid_colours, GXM, GYM);

    env.cloud_no = 0;

    // how many clouds?
    const int num_clouds = unmarshallShort(th);
    ASSERT(num_clouds >= 0 && num_clouds <= MAX_CLOUDS);
    for (int i = 0; i < num_clouds; i++)
    {
        env.cloud[i].type  = static_cast<cloud_type>(unmarshallByte(th));
        if (env.cloud[i].type == CLOUD_NONE)
            continue;
        env.cloud[i].pos.x = unmarshallByte(th);
        env.cloud[i].pos.y = unmarshallByte(th);
        env.cloud[i].decay = unmarshallShort(th);
        env.cloud[i].spread_rate = unmarshallUByte(th);
        env.cloud[i].whose = static_cast<kill_category>(unmarshallUByte(th));
        env.cloud[i].killer = static_cast<killer_type>(unmarshallUByte(th));
        env.cloud[i].source = unmarshallInt(th);
        env.cloud[i].colour = unmarshallShort(th);
        env.cloud[i].name   = unmarshallString(th);
        env.cloud[i].tile   = unmarshallString(th);
        env.cloud[i].excl_rad = unmarshallInt(th);
        ASSERT(in_bounds(env.cloud[i].pos));
        env.cgrid(env.cloud[i].pos) = i;
        env.cloud_no++;
    }
    for (int i = num_clouds; i < MAX_CLOUDS; i++)
        env.cloud[i].type = CLOUD_NONE;

    // how many shops?
    const int num_shops = unmarshallShort(th);
    ASSERT(num_shops >= 0 && num_shops <= MAX_SHOPS);
    for (int i = 0; i < num_shops; i++)
    {
        env.shop[i].type  = static_cast<shop_type>(unmarshallByte(th));
        if (env.shop[i].type == SHOP_UNASSIGNED)
            continue;
        env.shop[i].keeper_name[0] = unmarshallUByte(th);
        env.shop[i].keeper_name[1] = unmarshallUByte(th);
        env.shop[i].keeper_name[2] = unmarshallUByte(th);
        env.shop[i].pos.x = unmarshallByte(th);
        env.shop[i].pos.y = unmarshallByte(th);
        env.shop[i].greed = unmarshallByte(th);
        env.shop[i].level = unmarshallByte(th);
        env.shop[i].shop_name = unmarshallString(th);
        env.shop[i].shop_type_name = unmarshallString(th);
        env.shop[i].shop_suffix_name = unmarshallString(th);
        env.tgrid(env.shop[i].pos) = i;
    }
    for (int i = num_shops; i < MAX_SHOPS; ++i)
        env.shop[i].type = SHOP_UNASSIGNED;

    env.sanctuary_pos  = unmarshallCoord(th);
    env.sanctuary_time = unmarshallByte(th);

    env.spawn_random_rate = unmarshallInt(th);

    env.markers.read(th);

    env.properties.clear();
    env.properties.read(th);

    env.dactions_done = unmarshallInt(th);

    // Restore heightmap
    env.heightmap.reset(NULL);
    const bool have_heightmap = unmarshallBoolean(th);
    if (have_heightmap)
    {
        env.heightmap.reset(new grid_heightmap);
        grid_heightmap &heightmap(*env.heightmap);
        for (rectangle_iterator ri(0); ri; ++ri)
            heightmap(*ri) = unmarshallShort(th);
    }

    env.forest_awoken_until = unmarshallInt(th);
    unmarshall_level_vault_data(th);
    env.density = unmarshallInt(th);

#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() >= TAG_MINOR_SUNLIGHT)
    {
#endif
    int num_lights = unmarshallShort(th);
    env.sunlight.clear();
    while (num_lights-- > 0)
    {
        coord_def c = unmarshallCoord(th);
        env.sunlight.push_back(std::pair<coord_def, int>(c, unmarshallInt(th)));
    }
#if TAG_MAJOR_VERSION == 33
    }
#endif
}

static void tag_read_level_items(reader &th)
{
    // how many traps?
    const int trap_count = unmarshallShort(th);
    ASSERT(trap_count >= 0 && trap_count <= MAX_TRAPS);
    for (int i = 0; i < trap_count; ++i)
    {
        env.trap[i].type =
            static_cast<trap_type>(unmarshallUByte(th));
        if (env.trap[i].type == TRAP_UNASSIGNED)
            continue;
#if TAG_MAJOR_VERSION == 33
        if (env.trap[i].type == TRAP_AXED)
            env.trap[i].type = TRAP_SPEAR;
#endif
        env.trap[i].pos      = unmarshallCoord(th);
        env.trap[i].ammo_qty = unmarshallShort(th);
        env.tgrid(env.trap[i].pos) = i;
    }
    for (int i = trap_count; i < MAX_TRAPS; ++i)
        env.trap[i].type = TRAP_UNASSIGNED;

    // how many items?
    const int item_count = unmarshallShort(th);
    ASSERT(item_count >= 0 && item_count <= MAX_ITEMS);
    for (int i = 0; i < item_count; ++i)
        unmarshallItem(th, mitm[i]);
    for (int i = item_count; i < MAX_ITEMS; ++i)
        mitm[i].clear();

#ifdef DEBUG_ITEM_SCAN
    // There's no way to fix this, even with wizard commands, so get
    // rid of it when restoring the game.
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def &item(mitm[i]);

        if (item.pos.origin())
            item.clear();
    }
#endif
}

void unmarshallMonster(reader &th, monster& m)
{
    m.reset();

    m.type           = static_cast<monster_type>(unmarshallShort(th));
    if (m.type == MONS_NO_MONSTER)
        return;

    ASSERT(!invalid_monster_type(m.type));

    m.mid             = unmarshallInt(th);
    ASSERT(m.mid > 0);
    m.mname           = unmarshallString(th, 100);
    m.ac              = unmarshallByte(th);
    m.ev              = unmarshallByte(th);
    m.hit_dice        = unmarshallByte(th);
    m.speed           = unmarshallByte(th);
    // Avoid sign extension when loading files (Elethiomel's hang)
    m.speed_increment = unmarshallUByte(th);
    m.behaviour       = static_cast<beh_type>(unmarshallUByte(th));
    int x             = unmarshallByte(th);
    int y             = unmarshallByte(th);
    m.set_position(coord_def(x,y));
    m.target.x        = unmarshallByte(th);
    m.target.y        = unmarshallByte(th);

    m.firing_pos      = unmarshallCoord(th);
    m.patrol_point    = unmarshallCoord(th);

    int help = unmarshallByte(th);
    m.travel_target = static_cast<montravel_target_type>(help);

    const int len = unmarshallShort(th);
    for (int i = 0; i < len; ++i)
        m.travel_path.push_back(unmarshallCoord(th));

    m.flags      = unmarshallUnsigned(th);
    m.experience = unmarshallInt(th);

    m.enchantments.clear();
    const int nenchs = unmarshallShort(th);
    for (int i = 0; i < nenchs; ++i)
    {
        mon_enchant me = unmarshall_mon_enchant(th);
        m.enchantments[me.ench] = me;
        m.ench_cache.set(me.ench, true);
    }
    m.ench_countdown = unmarshallByte(th);

    m.hit_points     = unmarshallShort(th);
    m.max_hit_points = unmarshallShort(th);
    m.number         = unmarshallInt(th);
    m.base_monster   = static_cast<monster_type>(unmarshallShort(th));
    m.colour         = unmarshallShort(th);

    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
    {
#if TAG_MAJOR_VERSION == 33
        if ((th.getMinorVersion() < TAG_MINOR_MONSTER_JEWELLERY) &&
            (j == MSLOT_JEWELLERY))
        {
            m.inv[j] = NON_ITEM;
            continue;
        }
#endif
        m.inv[j] = unmarshallShort(th);
    }

    unmarshallSpells(th, m.spells);

    m.god      = static_cast<god_type>(unmarshallByte(th));
    m.attitude = static_cast<mon_attitude_type>(unmarshallByte(th));
    m.foe      = unmarshallShort(th);
    m.foe_memory = unmarshallInt(th);

    m.damage_friendly = unmarshallShort(th);
    m.damage_total = unmarshallShort(th);

    if (mons_is_ghost_demon(m.type))
        m.set_ghost(unmarshallGhost(th));

    _unmarshall_constriction(th, &m);

    m.props.clear();
    m.props.read(th);

    if (m.props.exists("monster_tile_name"))
    {
        std::string tile = m.props["monster_tile_name"].get_string();
        tileidx_t index;
        if (!tile_player_index(tile.c_str(), &index))
        {
            // If invalid tile name, complain and discard the props.
            dprf("bad tile name: \"%s\".", tile.c_str());
            m.props.erase("monster_tile_name");
            if (m.props.exists("monster_tile"))
                m.props.erase("monster_tile");
        }
        else // Update monster tile.
            m.props["monster_tile"] = short(index);
    }

    m.check_speed();
}

static void tag_read_level_monsters(reader &th)
{
    int i;
    int count;

    reset_all_monsters();

    // how many mons_alloc?
    count = unmarshallByte(th);
    ASSERT(count >= 0);
    for (i = 0; i < count && i < MAX_MONS_ALLOC; ++i)
        env.mons_alloc[i] = static_cast<monster_type>(unmarshallShort(th));
    for (i = MAX_MONS_ALLOC; i < count; ++i)
        unmarshallShort(th);
    for (i = count; i < MAX_MONS_ALLOC; ++i)
        env.mons_alloc[i] = MONS_NO_MONSTER;

    // how many monsters?
    count = unmarshallShort(th);
    ASSERT(count >= 0 && count <= MAX_MONSTERS);

    for (i = 0; i < count; i++)
    {
        monster& m = menv[i];
        unmarshallMonster(th, m);

        // place monster
        if (m.alive())
        {
            env.mid_cache[m.mid] = i;
#if defined(DEBUG) || defined(DEBUG_MONS_SCAN)
            if (invalid_monster_type(m.type))
            {
                mprf(MSGCH_ERROR, "Unmarshalled monster #%d %s",
                     i, m.name(DESC_PLAIN, true).c_str());
            }
            if (!in_bounds(m.pos()))
            {
                mprf(MSGCH_ERROR,
                     "Unmarshalled monster #%d %s out of bounds at (%d, %d)",
                     i, m.name(DESC_PLAIN, true).c_str(),
                     m.pos().x, m.pos().y);
            }
            int midx = mgrd(m.pos());
            if (midx != NON_MONSTER)
                mprf(MSGCH_ERROR, "(%d, %d) for %s already occupied by %s",
                     m.pos().x, m.pos().y,
                     m.name(DESC_PLAIN, true).c_str(),
                     menv[midx].name(DESC_PLAIN, true).c_str());
#endif
            mgrd(m.pos()) = i;
        }
    }
#if TAG_MAJOR_VERSION <= 33
    // This relies on TAG_YOU (including lost monsters) being unmarshalled
    // on game load before the initial level.
    if (th.getMinorVersion() < TAG_MINOR_CONSTRICT_MID)
        _fixup_constriction();
#endif
}

static void _debug_count_tiles()
{
#ifdef DEBUG_DIAGNOSTICS
# ifdef USE_TILE
    std::map<int,bool> found;
    int t, cnt = 0;
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            t = env.tile_bk_bg[i][j];
            if (found.find(t) == found.end())
                cnt++, found[t] = true;
            t = env.tile_bk_fg[i][j];
            if (found.find(t) == found.end())
                cnt++, found[t] = true;
        }
    dprf("Unique tiles found: %d", cnt);
# endif
#endif
}

void tag_read_level_tiles(reader &th)
{
    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);

    env.tile_names.clear();
    unsigned int num_tilenames = unmarshallShort(th);
    for (unsigned int i = 0; i < num_tilenames; ++i)
    {
#ifdef DEBUG_TILE_NAMES
        std::string temp = unmarshallString(th);
        mprf("Reading tile_names[%d] = %s", i, temp.c_str());
        env.tile_names.push_back(temp);
#else
        env.tile_names.push_back(unmarshallString(th));
#endif
    }

    // flavour
    bool need_reinit = false;

    env.tile_default.wall_idx  = unmarshallShort(th);
    env.tile_default.floor_idx = unmarshallShort(th);
    env.tile_default.wall      = unmarshallShort(th);
    env.tile_default.floor     = unmarshallShort(th);
    env.tile_default.special   = unmarshallShort(th);

    if (env.tile_default.wall == 0 || env.tile_default.floor == 0)
        need_reinit = true;

    for (int x = 0; x < gx; x++)
        for (int y = 0; y < gy; y++)
        {
            env.tile_flv[x][y].wall_idx  = unmarshallShort(th);
            env.tile_flv[x][y].floor_idx = unmarshallShort(th);
            env.tile_flv[x][y].feat_idx  = unmarshallShort(th);

            env.tile_flv[x][y].wall    = unmarshallShort(th);
            env.tile_flv[x][y].floor   = unmarshallShort(th);
            env.tile_flv[x][y].feat    = unmarshallShort(th);
            env.tile_flv[x][y].special = unmarshallShort(th);

            if (env.tile_flv[x][y].wall == 0
                || env.tile_flv[x][y].floor == 0)
            {
                need_reinit = true;
            }
        }
    _debug_count_tiles();

    if (unmarshallInt(th) != TILE_WALL_MAX || need_reinit)
    {
        dprf("DNGN tilecount has changed -- recreating tile data.");
        tag_missing_level_tiles();
    }

    // Draw remembered map
    tag_init_tile_bk();
}

static tileidx_t _get_tile_from_vector(const unsigned int idx)
{
    if (idx <= 0 || idx > env.tile_names.size())
    {
#ifdef DEBUG_TILE_NAMES
        mprf("Index out of bounds: idx = %d - 1, size(tile_names) = %d",
            idx, env.tile_names.size());
#endif
        return 0;
    }
    std::string tilename = env.tile_names[idx - 1];

    tileidx_t tile;
    if (!tile_dngn_index(tilename.c_str(), &tile))
    {
#ifdef DEBUG_TILE_NAMES
        mprf("tilename %s (index %d) not found",
             tilename.c_str(), idx - 1);
#endif
        return 0;
    }
#ifdef DEBUG_TILE_NAMES
    mprf("tilename %s (index %d) resolves to tile %d",
         tilename.c_str(), idx - 1, (int) tile);
#endif

    return tile;
}

static void _reinit_flavour_tiles()
{
    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1));
         ri; ++ri)
    {
        if (env.tile_flv(*ri).wall_idx)
        {
            tileidx_t new_wall
                = _get_tile_from_vector(env.tile_flv(*ri).wall_idx);
            if (!new_wall)
                env.tile_flv(*ri).wall_idx = 0;
            else
                env.tile_flv(*ri).wall = new_wall;
        }
        if (env.tile_flv(*ri).floor_idx)
        {
            tileidx_t new_floor
                = _get_tile_from_vector(env.tile_flv(*ri).floor_idx);
            if (!new_floor)
                env.tile_flv(*ri).floor_idx = 0;
            else
                env.tile_flv(*ri).floor = new_floor;
        }
        if (env.tile_flv(*ri).feat_idx)
        {
            tileidx_t new_feat
                = _get_tile_from_vector(env.tile_flv(*ri).feat_idx);
            if (!new_feat)
                env.tile_flv(*ri).feat_idx = 0;
            else
                env.tile_flv(*ri).feat = new_feat;
        }
   }
}

static void tag_missing_level_tiles()
{
    /* Remember the wall_idx and floor_idx; tile_init_default_flavour
       sets them to 0 */
    tileidx_t default_wall_idx = env.tile_default.wall_idx;
    tileidx_t default_floor_idx = env.tile_default.floor_idx;
    tile_init_default_flavour();
    if (default_wall_idx)
    {
        tileidx_t new_wall = _get_tile_from_vector(default_wall_idx);
        if (new_wall)
        {
            env.tile_default.wall_idx = default_wall_idx;
            env.tile_default.wall = new_wall;
        }
    }
    if (default_floor_idx)
    {
        tileidx_t new_floor = _get_tile_from_vector(default_floor_idx);
        if (new_floor)
        {
            env.tile_default.floor_idx = default_floor_idx;
            env.tile_default.floor = new_floor;
        }
    }
    _reinit_flavour_tiles();

    tile_new_level(true, false);
}

static void tag_init_tile_bk()
{
#ifdef USE_TILE
    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1));
         ri; ++ri)
    {
        tile_draw_map_cell(*ri);
    }
#endif
}
// ------------------------------- ghost tags ---------------------------- //

static void marshallSpells(writer &th, const monster_spells &spells)
{
    for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
        marshallShort(th, spells[j]);
}

static void unmarshallSpells(reader &th, monster_spells &spells)
{
    for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
        spells[j] = static_cast<spell_type>(unmarshallShort(th));
}

static void marshallGhost(writer &th, const ghost_demon &ghost)
{
    marshallString(th, ghost.name.c_str(), 20);

    marshallShort(th, ghost.species);
    marshallShort(th, ghost.job);
    marshallByte(th, ghost.religion);
    marshallShort(th, ghost.best_skill);
    marshallShort(th, ghost.best_skill_level);
    marshallShort(th, ghost.xl);
    marshallShort(th, ghost.max_hp);
    marshallShort(th, ghost.ev);
    marshallShort(th, ghost.ac);
    marshallShort(th, ghost.damage);
    marshallShort(th, ghost.speed);
    marshallByte(th, ghost.see_invis);
    marshallShort(th, ghost.brand);
    marshallShort(th, ghost.att_type);
    marshallShort(th, ghost.att_flav);
    marshallInt(th, ghost.resists);
    marshallByte(th, ghost.spellcaster);
    marshallByte(th, ghost.cycle_colours);
    marshallByte(th, ghost.colour);
    marshallShort(th, ghost.fly);

    marshallSpells(th, ghost.spells);
}

static ghost_demon unmarshallGhost(reader &th)
{
    ghost_demon ghost;

    ghost.name             = unmarshallString(th, 20);
    ghost.species          = static_cast<species_type>(unmarshallShort(th));
    ghost.job              = static_cast<job_type>(unmarshallShort(th));
    ghost.religion         = static_cast<god_type>(unmarshallByte(th));
    ghost.best_skill       = static_cast<skill_type>(unmarshallShort(th));
    ghost.best_skill_level = unmarshallShort(th);
    ghost.xl               = unmarshallShort(th);
    ghost.max_hp           = unmarshallShort(th);
    ghost.ev               = unmarshallShort(th);
    ghost.ac               = unmarshallShort(th);
    ghost.damage           = unmarshallShort(th);
    ghost.speed            = unmarshallShort(th);
    ghost.see_invis        = unmarshallByte(th);
    ghost.brand            = static_cast<brand_type>(unmarshallShort(th));
    ghost.att_type = static_cast<attack_type>(unmarshallShort(th));
    ghost.att_flav = static_cast<attack_flavour>(unmarshallShort(th));
#if TAG_MAJOR_VERSION == 33
    if (th.getMinorVersion() < TAG_MINOR_BITFIELD_RESISTS)
    {
        for (int i = 0; i < 10; i++)
            unmarshallByte(th);
        ghost.resists = 0;
    }
    else
#endif
    ghost.resists = unmarshallInt(th);
    ghost.spellcaster      = unmarshallByte(th);
    ghost.cycle_colours    = unmarshallByte(th);
    ghost.colour           = unmarshallByte(th);

    ghost.fly              = static_cast<flight_type>(unmarshallShort(th));

    unmarshallSpells(th, ghost.spells);

    return ghost;
}

static void tag_construct_ghost(writer &th)
{
    // How many ghosts?
    marshallShort(th, ghosts.size());

    for (int i = 0, size = ghosts.size(); i < size; ++i)
        marshallGhost(th, ghosts[i]);
}

static void tag_read_ghost(reader &th)
{
    int nghosts = unmarshallShort(th);

    if (nghosts < 1 || nghosts > MAX_GHOSTS)
        return;

    for (int i = 0; i < nghosts; ++i)
        ghosts.push_back(unmarshallGhost(th));
}
