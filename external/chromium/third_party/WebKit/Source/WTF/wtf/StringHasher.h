/*
 * Copyright (C) 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#ifndef WTF_StringHasher_h
#define WTF_StringHasher_h

#include <wtf/unicode/Unicode.h>

namespace WTF {

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
static const unsigned stringHashingStartValue = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
// char* data is interpreted as latin-encoded (zero extended to 16 bits).

// NOTE: This class must stay in sync with the create_hash_table script in
// JavaScriptCore and the CodeGeneratorJS.pm script in WebCore.
class StringHasher {
public:
    static const unsigned flagCount = 8; // Save 8 bits for StringImpl to use as flags.

    inline StringHasher()
        : m_hash(stringHashingStartValue)
        , m_hasPendingCharacter(false)
        , m_pendingCharacter(0)
    {
    }

    inline void addCharacters(UChar a, UChar b)
    {
        ASSERT(!m_hasPendingCharacter);
        addCharactersToHash(a, b);
    }

    inline void addCharacter(UChar ch)
    {
        if (m_hasPendingCharacter) {
            addCharactersToHash(m_pendingCharacter, ch);
            m_hasPendingCharacter = false;
            return;
        }

        m_pendingCharacter = ch;
        m_hasPendingCharacter = true;
    }

    template<typename T, UChar Converter(T)> inline void addCharacters(const T* data, unsigned length)
    {
        if (!length)
            return;

        if (m_hasPendingCharacter) {
            addCharactersToHash(m_pendingCharacter, Converter(*data++));
            m_hasPendingCharacter = false;
            --length;
        }

        bool rem = length & 1;
        length >>= 1;

        while (length--) {
            addCharactersToHash(Converter(data[0]), Converter(data[1]));
            data += 2;
        }

        if (rem)
            addCharacter(Converter(*data));
    }

    inline unsigned hashWithTop8BitsMasked() const
    {
        unsigned result = avalancheBits();

        // Reserving space from the high bits for flags preserves most of the hash's
        // value, since hash lookup typically masks out the high bits anyway.
        result &= (1u << (sizeof(result) * 8 - flagCount)) - 1;

        // This avoids ever returning a hash code of 0, since that is used to
        // signal "hash not computed yet". Setting the high bit maintains
        // reasonable fidelity to a hash code of 0 because it is likely to yield
        // exactly 0 when hash lookup masks out the high bits.
        if (!result)
            result = 0x80000000 >> flagCount;

        return result;
    }

    inline unsigned hash() const
    {
        unsigned result = avalancheBits();

        // This avoids ever returning a hash code of 0, since that is used to
        // signal "hash not computed yet". Setting the high bit maintains
        // reasonable fidelity to a hash code of 0 because it is likely to yield
        // exactly 0 when hash lookup masks out the high bits.
        if (!result)
            result = 0x80000000;

        return result;
    }

    template<typename T, UChar Converter(T)> static inline unsigned computeHashAndMaskTop8Bits(const T* data, unsigned length)
    {
        // On Android there is a compiler bug that makes the original code generate different hashes for the same piece of
        // data in non Debug build. Rewriting in the following way can make the compiler generate code correctly.
#if defined(__LB_ANDROID__)
        StringHasher hasher;
        int i = 0;
        while (i + 1 < length) {
            hasher.addCharacters(Converter(data[i]), Converter(data[i + 1]));
            i += 2;
        }

        if (i < length)
            hasher.addCharacter(Converter(data[i]));

        return hasher.hashWithTop8BitsMasked();
#else  // defined(__LB_ANDROID__)
        StringHasher hasher;
        bool rem = length & 1;
        length >>= 1;

        while (length--) {
            hasher.addCharacters(Converter(data[0]), Converter(data[1]));
            data += 2;
        }

        if (rem)
            hasher.addCharacter(Converter(*data));

        return hasher.hashWithTop8BitsMasked();
#endif  // defined(__LB_ANDROID__)
    }

    template<typename T, UChar Converter(T)> static inline unsigned computeHashAndMaskTop8Bits(const T* data)
    {
        StringHasher hasher;

        while (true) {
            UChar b0 = Converter(*data++);
            if (!b0)
                break;
            UChar b1 = Converter(*data++);
            if (!b1) {
                hasher.addCharacter(b0);
                break;
            }

            hasher.addCharacters(b0, b1);
        }

        return hasher.hashWithTop8BitsMasked();
    }

    template<typename T> static inline unsigned computeHashAndMaskTop8Bits(const T* data, unsigned length)
    {
        return computeHashAndMaskTop8Bits<T, defaultConverter>(data, length);
    }

    template<typename T> static inline unsigned computeHashAndMaskTop8Bits(const T* data)
    {
        return computeHashAndMaskTop8Bits<T, defaultConverter>(data);
    }

    template<typename T, UChar Converter(T)> static inline unsigned computeHash(const T* data, unsigned length)
    {
        StringHasher hasher;
        hasher.addCharacters<T, Converter>(data, length);
        return hasher.hash();
    }

    template<typename T, UChar Converter(T)> static inline unsigned computeHash(const T* data)
    {
        StringHasher hasher;

        while (true) {
            UChar b0 = Converter(*data++);
            if (!b0)
                break;
            UChar b1 = Converter(*data++);
            if (!b1) {
                hasher.addCharacter(b0);
                break;
            }

            hasher.addCharacters(b0, b1);
        }

        return hasher.hash();
    }

    template<typename T> static inline unsigned computeHash(const T* data, unsigned length)
    {
        return computeHash<T, defaultConverter>(data, length);
    }

    template<typename T> static inline unsigned computeHash(const T* data)
    {
        return computeHash<T, defaultConverter>(data);
    }

    template<size_t length> static inline unsigned hashMemory(const void* data)
    {
        COMPILE_ASSERT(!(length % 4), length_must_be_a_multible_of_four);
        return computeHashAndMaskTop8Bits<UChar>(static_cast<const UChar*>(data), length / sizeof(UChar));
    }

    static inline unsigned hashMemory(const void* data, unsigned size)
    {
        ASSERT(!(size % 2));
        return computeHashAndMaskTop8Bits<UChar>(static_cast<const UChar*>(data), size / sizeof(UChar));
    }

private:
    static inline UChar defaultConverter(UChar ch)
    {
        return ch;
    }

    static inline UChar defaultConverter(LChar ch)
    {
        return ch;
    }

    inline void addCharactersToHash(UChar a, UChar b)
    {
        m_hash += a;
        unsigned tmp = (b << 11) ^ m_hash;
        m_hash = (m_hash << 16) ^ tmp;
        m_hash += m_hash >> 11;
    }

    inline unsigned avalancheBits() const
    {
        unsigned result = m_hash;

        // Handle end case.
        if (m_hasPendingCharacter) {
            result += m_pendingCharacter;
            result ^= result << 11;
            result += result >> 17;
        }

        // Force "avalanching" of final 31 bits.
        result ^= result << 3;
        result += result >> 5;
        result ^= result << 2;
        result += result >> 15;
        result ^= result << 10;

        return result;
    }

    unsigned m_hash;
    bool m_hasPendingCharacter;
    UChar m_pendingCharacter;
};

} // namespace WTF

using WTF::StringHasher;

#endif // WTF_StringHasher_h
