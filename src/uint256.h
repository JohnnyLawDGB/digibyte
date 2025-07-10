// Copyright (c) 2009-2010 Satoshi Nakamoto
<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2009-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_UINT256_H
#define DIGIBYTE_UINT256_H

#include <crypto/common.h>
#include <span.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <stdint.h>
#include <string>
<<<<<<< HEAD
#include <vector>
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

/** Template base class for fixed-sized opaque blobs. */
template<unsigned int BITS>
class base_blob
{
protected:
    static constexpr int WIDTH = BITS / 8;
<<<<<<< HEAD
    uint8_t m_data[WIDTH];
=======
    static_assert(BITS % 8 == 0, "base_blob currently only supports whole bytes.");
    std::array<uint8_t, WIDTH> m_data;
    static_assert(WIDTH == sizeof(m_data), "Sanity check");

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
public:
    /* construct 0 value by default */
    constexpr base_blob() : m_data() {}

    /* constructor for constants between 1 and 255 */
    constexpr explicit base_blob(uint8_t v) : m_data{v} {}
<<<<<<< HEAD
=======

    constexpr explicit base_blob(Span<const unsigned char> vch)
    {
        assert(vch.size() == WIDTH);
        std::copy(vch.begin(), vch.end(), m_data.begin());
    }
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    constexpr bool IsNull() const
    {
<<<<<<< HEAD
        for (int i = 0; i < WIDTH; i++)
            if (m_data[i] != 0)
                return false;
        return true;
=======
        return std::all_of(m_data.begin(), m_data.end(), [](uint8_t val) {
            return val == 0;
        });
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    constexpr void SetNull()
    {
<<<<<<< HEAD
        memset(m_data, 0, sizeof(m_data));
    }

    inline int Compare(const base_blob& other) const { return memcmp(m_data, other.m_data, sizeof(m_data)); }
=======
        std::fill(m_data.begin(), m_data.end(), 0);
    }

    constexpr int Compare(const base_blob& other) const { return std::memcmp(m_data.data(), other.m_data.data(), WIDTH); }
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    friend constexpr bool operator==(const base_blob& a, const base_blob& b) { return a.Compare(b) == 0; }
    friend constexpr bool operator!=(const base_blob& a, const base_blob& b) { return a.Compare(b) != 0; }
    friend constexpr bool operator<(const base_blob& a, const base_blob& b) { return a.Compare(b) < 0; }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);
    std::string ToString() const;

<<<<<<< HEAD
    const unsigned char* data() const { return m_data; }
    unsigned char* data() { return m_data; }

    unsigned char* begin()
    {
        return &m_data[0];
    }

    unsigned char* end()
    {
        return &m_data[WIDTH];
    }

    const unsigned char* begin() const
    {
        return &m_data[0];
    }

    const unsigned char* end() const
    {
        return &m_data[WIDTH];
    }

    static constexpr unsigned int size()
    {
        return sizeof(m_data);
    }

    uint64_t GetUint64(int pos) const
    {
        const uint8_t* ptr = m_data + pos * 8;
        return ((uint64_t)ptr[0]) | \
               ((uint64_t)ptr[1]) << 8 | \
               ((uint64_t)ptr[2]) << 16 | \
               ((uint64_t)ptr[3]) << 24 | \
               ((uint64_t)ptr[4]) << 32 | \
               ((uint64_t)ptr[5]) << 40 | \
               ((uint64_t)ptr[6]) << 48 | \
               ((uint64_t)ptr[7]) << 56;
    }
=======
    constexpr const unsigned char* data() const { return m_data.data(); }
    constexpr unsigned char* data() { return m_data.data(); }

    constexpr unsigned char* begin() { return m_data.data(); }
    constexpr unsigned char* end() { return m_data.data() + WIDTH; }

    constexpr const unsigned char* begin() const { return m_data.data(); }
    constexpr const unsigned char* end() const { return m_data.data() + WIDTH; }

    static constexpr unsigned int size() { return WIDTH; }

    constexpr uint64_t GetUint64(int pos) const { return ReadLE64(m_data.data() + pos * 8); }
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    template<typename Stream>
    void Serialize(Stream& s) const
    {
<<<<<<< HEAD
        s.write((char*)m_data, sizeof(m_data));
=======
        s << Span(m_data);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
<<<<<<< HEAD
        s.read((char*)m_data, sizeof(m_data));
=======
        s.read(MakeWritableByteSpan(m_data));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
};

/** 160-bit opaque blob.
 * @note This type is called uint160 for historical reasons only. It is an opaque
 * blob of 160 bits and has no integer operations.
 */
class uint160 : public base_blob<160> {
public:
<<<<<<< HEAD
    constexpr uint160() {}
    explicit uint160(const std::vector<unsigned char>& vch) : base_blob<160>(vch) {}
=======
    constexpr uint160() = default;
    constexpr explicit uint160(Span<const unsigned char> vch) : base_blob<160>(vch) {}
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
};

/** 256-bit opaque blob.
 * @note This type is called uint256 for historical reasons only. It is an
 * opaque blob of 256 bits and has no integer operations. Use arith_uint256 if
 * those are required.
 */
class uint256 : public base_blob<256> {
public:
<<<<<<< HEAD
    constexpr uint256() {}
    constexpr explicit uint256(uint8_t v) : base_blob<256>(v) {}
    explicit uint256(const std::vector<unsigned char>& vch) : base_blob<256>(vch) {}
    static const uint256 ZERO;
    static const uint256 ONE;
};

/** 512-bit opaque blob.
 * @note This type is called uint512 for historical reasons only. It is an opaque
 * blob of 512 bits and has no integer operations.
 */
class uint512 : public base_blob<512> {
public:
    uint512() {}
    uint512(const base_blob<512>& b) : base_blob<512>(b) {}
    explicit uint512(const std::vector<unsigned char>& vch) : base_blob<512>(vch) {}
    uint256 trim256() const
    {
        uint256 ret;
        unsigned char* data = ret.data();
        
        for (unsigned int i = 0; i < ret.size(); i++){
            data[i] = m_data[i];
        }
        return ret;
    }	
    
=======
    constexpr uint256() = default;
    constexpr explicit uint256(uint8_t v) : base_blob<256>(v) {}
    constexpr explicit uint256(Span<const unsigned char> vch) : base_blob<256>(vch) {}
    static const uint256 ZERO;
    static const uint256 ONE;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
};

/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching uint256(0).
 */
inline uint256 uint256S(const char *str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}
/* uint256 from std::string.
 * This is a separate function because the constructor uint256(const std::string &str) can result
 * in dangerously catching uint256(0) via std::string(const char*).
 */
inline uint256 uint256S(const std::string& str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}

#endif // DIGIBYTE_UINT256_H
