/**
 * @file
 * @brief Bit array data type.
 *
 * Just contains the operations required by los.cc
 * for the moment.
**/

#ifndef BITARY_H
#define BITARY_H

#include "defines.h"

#include <bitset>

class bit_vector
{
public:
    bit_vector(unsigned long size = 0);
    bit_vector(const bit_vector& other);
    ~bit_vector();

    void reset();

    bool get(unsigned long index) const;
    void set(unsigned long index, bool value = true);

    bit_vector& operator |= (const bit_vector& other);
    bit_vector& operator &= (const bit_vector& other);
    bit_vector  operator & (const bit_vector& other) const;

protected:
    unsigned long size;
    int nwords;
    unsigned long *data;
};

#define LONGSIZE (sizeof(unsigned long)*8)
#ifndef ULONG_MAX
#define ULONG_MAX ((unsigned long)(-1))
#endif

template <unsigned int SIZE> class FixedBitVector
{
protected:
    bitset<SIZE> data;
public:
    void reset()
    {
        data.reset();
    }

    FixedBitVector()
    {
    }

    inline bool get(unsigned int i) const
    {
#ifdef ASSERTS
        // printed as signed, as in FixedVector
        if (i >= SIZE)
            die("bit vector range error: %d / %u", (int)i, SIZE);
#endif
        return data[i];
    }

    inline bool operator[](unsigned int i) const
    {
        return get(i);
    }

    inline void set(unsigned int i, bool value = true)
    {
#ifdef ASSERTS
        if (i >= SIZE)
            die("bit vector range error: %d / %u", (int)i, SIZE);
#endif
        data[i] = value;
    }

    inline FixedBitVector<SIZE>& operator|=(const FixedBitVector<SIZE>&x)
    {
        data |= x.data;
        return *this;
    }

    inline FixedBitVector<SIZE>& operator&=(const FixedBitVector<SIZE>&x)
    {
        data &= x.data;
        return *this;
    }

    void init(bool value)
    {
        data.reset();
        if (value)
            data.flip();
    }
};

template <unsigned int SIZEX, unsigned int SIZEY> class FixedBitArray
{
protected:
    std::bitset<SIZEX*SIZEY> data;
public:
    void reset()
    {
        data.reset();
    }

    void init(bool def)
    {
        data.reset();
        if (def)
            data.flip();
    }

    FixedBitArray()
    {
        reset();
    }

    FixedBitArray(bool def)
    {
        init(def);
    }

    inline bool get(int x, int y) const
    {
#ifdef ASSERTS
        // printed as signed, as in FixedArray
        if (x < 0 || y < 0 || x >= (int)SIZEX || y >= (int)SIZEY)
            die("bit array range error: %d,%d / %u,%u", x, y, SIZEX, SIZEY);
#endif
        unsigned int i = y * SIZEX + x;
        return data[i];
    }

    template<class Indexer> inline bool get(const Indexer &i) const
    {
        return get(i.x, i.y);
    }

    inline bool operator () (int x, int y) const
    {
        return get(x, y);
    }

    template<class Indexer> inline bool operator () (const Indexer &i) const
    {
        return get(i.x, i.y);
    }

    inline void set(int x, int y, bool value = true)
    {
#ifdef ASSERTS
        if (x < 0 || y < 0 || x >= (int)SIZEX || y >= (int)SIZEY)
            die("bit array range error: %d,%d / %u,%u", x, y, SIZEX, SIZEY);
#endif
        unsigned int i = y * SIZEX + x;
        data[i] = value;
    }

    template<class Indexer> inline void set(const Indexer &i, bool value = true)
    {
        return set(i.x, i.y, value);
    }

    inline FixedBitArray<SIZEX, SIZEY>& operator|=(const FixedBitArray<SIZEX, SIZEY>&x)
    {
        data |= x.data;
        return *this;
    }

    inline FixedBitArray<SIZEX, SIZEY>& operator&=(const FixedBitArray<SIZEX, SIZEY>&x)
    {
        data &= x.data;
        return *this;
    }
};

#endif
