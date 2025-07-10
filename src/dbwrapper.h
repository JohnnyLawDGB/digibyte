<<<<<<< HEAD
// Copyright (c) 2012-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2012-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_DBWRAPPER_H
#define DIGIBYTE_DBWRAPPER_H

#include <attributes.h>
#include <serialize.h>
#include <span.h>
#include <streams.h>
<<<<<<< HEAD
#include <util/strencodings.h>
#include <util/system.h>
=======
#include <util/check.h>
#include <util/fs.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

static const size_t DBWRAPPER_PREALLOC_KEY_SIZE = 64;
static const size_t DBWRAPPER_PREALLOC_VALUE_SIZE = 1024;

//! User-controlled performance and debug options.
struct DBOptions {
    //! Compact database on startup.
    bool force_compact = false;
};

//! Application-specific storage settings.
struct DBParams {
    //! Location in the filesystem where leveldb data will be stored.
    fs::path path;
    //! Configures various leveldb cache settings.
    size_t cache_bytes;
    //! If true, use leveldb's memory environment.
    bool memory_only = false;
    //! If true, remove all existing data.
    bool wipe_data = false;
    //! If true, store data obfuscated via simple XOR. If false, XOR with a
    //! zero'd byte array.
    bool obfuscate = false;
    //! Passed-through options.
    DBOptions options{};
};

class dbwrapper_error : public std::runtime_error
{
public:
    explicit dbwrapper_error(const std::string& msg) : std::runtime_error(msg) {}
};

class CDBWrapper;

/** These should be considered an implementation detail of the specific database.
 */
namespace dbwrapper_private {

/** Work around circular dependency, as well as for testing in dbwrapper_tests.
 * Database obfuscation should be considered an implementation detail of the
 * specific database.
 */
const std::vector<unsigned char>& GetObfuscateKey(const CDBWrapper &w);

}; // namespace dbwrapper_private

bool DestroyDB(const std::string& path_str);

/** Batch of changes queued to be written to a CDBWrapper */
class CDBBatch
{
    friend class CDBWrapper;

private:
    const CDBWrapper &parent;

    struct WriteBatchImpl;
    const std::unique_ptr<WriteBatchImpl> m_impl_batch;

    DataStream ssKey{};
    DataStream ssValue{};

    size_t size_estimate{0};

    void WriteImpl(Span<const std::byte> key, DataStream& ssValue);
    void EraseImpl(Span<const std::byte> key);

public:
    /**
     * @param[in] _parent   CDBWrapper that this batch is to be submitted to
     */
    explicit CDBBatch(const CDBWrapper& _parent);
    ~CDBBatch();
    void Clear();

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
<<<<<<< HEAD
        ssKey << key;
        leveldb::Slice slKey((const char*)ssKey.data(), ssKey.size());

=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        ssValue.reserve(DBWRAPPER_PREALLOC_VALUE_SIZE);
        ssKey << key;
        ssValue << value;
<<<<<<< HEAD
        ssValue.Xor(dbwrapper_private::GetObfuscateKey(parent));
        leveldb::Slice slValue((const char*)ssValue.data(), ssValue.size());

        batch.Put(slKey, slValue);
        // LevelDB serializes writes as:
        // - byte: header
        // - varint: key length (1 byte up to 127B, 2 bytes up to 16383B, ...)
        // - byte[]: key
        // - varint: value length
        // - byte[]: value
        // The formula below assumes the key and value are both less than 16k.
        size_estimate += 3 + (slKey.size() > 127) + slKey.size() + (slValue.size() > 127) + slValue.size();
=======
        WriteImpl(ssKey, ssValue);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        ssKey.clear();
        ssValue.clear();
    }

    template <typename K>
    void Erase(const K& key)
    {
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
<<<<<<< HEAD
        leveldb::Slice slKey((const char*)ssKey.data(), ssKey.size());

        batch.Delete(slKey);
        // LevelDB serializes erases as:
        // - byte: header
        // - varint: key length
        // - byte[]: key
        // The formula below assumes the key is less than 16kB.
        size_estimate += 2 + (slKey.size() > 127) + slKey.size();
=======
        EraseImpl(ssKey);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        ssKey.clear();
    }

    size_t SizeEstimate() const { return size_estimate; }
};

class CDBIterator
{
public:
    struct IteratorImpl;

private:
    const CDBWrapper &parent;
    const std::unique_ptr<IteratorImpl> m_impl_iter;

    void SeekImpl(Span<const std::byte> key);
    Span<const std::byte> GetKeyImpl() const;
    Span<const std::byte> GetValueImpl() const;

public:

    /**
     * @param[in] _parent          Parent CDBWrapper instance.
     * @param[in] _piter           The original leveldb iterator.
     */
    CDBIterator(const CDBWrapper& _parent, std::unique_ptr<IteratorImpl> _piter);
    ~CDBIterator();

    bool Valid() const;

    void SeekToFirst();

    template<typename K> void Seek(const K& key) {
        DataStream ssKey{};
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
<<<<<<< HEAD
        leveldb::Slice slKey((const char*)ssKey.data(), ssKey.size());
        piter->Seek(slKey);
=======
        SeekImpl(ssKey);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    void Next();

    template<typename K> bool GetKey(K& key) {
        try {
<<<<<<< HEAD
            CDataStream ssKey(MakeUCharSpan(slKey), SER_DISK, CLIENT_VERSION);
=======
            DataStream ssKey{GetKeyImpl()};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            ssKey >> key;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template<typename V> bool GetValue(V& value) {
        try {
<<<<<<< HEAD
            CDataStream ssValue(MakeUCharSpan(slValue), SER_DISK, CLIENT_VERSION);
=======
            DataStream ssValue{GetValueImpl()};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            ssValue.Xor(dbwrapper_private::GetObfuscateKey(parent));
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }
};

struct LevelDBContext;

class CDBWrapper
{
    friend const std::vector<unsigned char>& dbwrapper_private::GetObfuscateKey(const CDBWrapper &w);
private:
    //! holds all leveldb-specific fields of this class
    std::unique_ptr<LevelDBContext> m_db_context;

    //! the name of this database
    std::string m_name;

    //! a key used for optional XOR-obfuscation of the database
    std::vector<unsigned char> obfuscate_key;

    //! the key under which the obfuscation key is stored
    static const std::string OBFUSCATE_KEY_KEY;

    //! the length of the obfuscate key in number of bytes
    static const unsigned int OBFUSCATE_KEY_NUM_BYTES;

    std::vector<unsigned char> CreateObfuscateKey() const;

    //! path to filesystem storage
    const fs::path m_path;

    //! whether or not the database resides in memory
    bool m_is_memory;

    std::optional<std::string> ReadImpl(Span<const std::byte> key) const;
    bool ExistsImpl(Span<const std::byte> key) const;
    size_t EstimateSizeImpl(Span<const std::byte> key1, Span<const std::byte> key2) const;
    auto& DBContext() const LIFETIMEBOUND { return *Assert(m_db_context); }

public:
    CDBWrapper(const DBParams& params);
    ~CDBWrapper();

    CDBWrapper(const CDBWrapper&) = delete;
    CDBWrapper& operator=(const CDBWrapper&) = delete;

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        DataStream ssKey{};
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
<<<<<<< HEAD
        leveldb::Slice slKey((const char*)ssKey.data(), ssKey.size());

        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            dbwrapper_private::HandleError(status);
        }
        try {
            CDataStream ssValue(MakeUCharSpan(strValue), SER_DISK, CLIENT_VERSION);
=======
        std::optional<std::string> strValue{ReadImpl(ssKey)};
        if (!strValue) {
            return false;
        }
        try {
            DataStream ssValue{MakeByteSpan(*strValue)};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            ssValue.Xor(obfuscate_key);
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template <typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        CDBBatch batch(*this);
        batch.Write(key, value);
        return WriteBatch(batch, fSync);
    }

    //! @returns filesystem path to the on-disk data.
    std::optional<fs::path> StoragePath() {
        if (m_is_memory) {
            return {};
        }
        return m_path;
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        DataStream ssKey{};
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
<<<<<<< HEAD
        leveldb::Slice slKey((const char*)ssKey.data(), ssKey.size());

        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            dbwrapper_private::HandleError(status);
        }
        return true;
=======
        return ExistsImpl(ssKey);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    template <typename K>
    bool Erase(const K& key, bool fSync = false)
    {
        CDBBatch batch(*this);
        batch.Erase(key);
        return WriteBatch(batch, fSync);
    }

    bool WriteBatch(CDBBatch& batch, bool fSync = false);

    // Get an estimate of LevelDB memory usage (in bytes).
    size_t DynamicMemoryUsage() const;

<<<<<<< HEAD
    CDBIterator *NewIterator()
    {
        return new CDBIterator(*this, pdb->NewIterator(iteroptions));
    }
=======
    CDBIterator* NewIterator();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    /**
     * Return true if the database managed by this class contains no entries.
     */
    bool IsEmpty();

    template<typename K>
    size_t EstimateSize(const K& key_begin, const K& key_end) const
    {
        DataStream ssKey1{}, ssKey2{};
        ssKey1.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey2.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey1 << key_begin;
        ssKey2 << key_end;
<<<<<<< HEAD
        leveldb::Slice slKey1((const char*)ssKey1.data(), ssKey1.size());
        leveldb::Slice slKey2((const char*)ssKey2.data(), ssKey2.size());
        uint64_t size = 0;
        leveldb::Range range(slKey1, slKey2);
        pdb->GetApproximateSizes(&range, 1, &size);
        return size;
    }

    /**
     * Compact a certain range of keys in the database.
     */
    template<typename K>
    void CompactRange(const K& key_begin, const K& key_end) const
    {
        CDataStream ssKey1(SER_DISK, CLIENT_VERSION), ssKey2(SER_DISK, CLIENT_VERSION);
        ssKey1.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey2.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey1 << key_begin;
        ssKey2 << key_end;
        leveldb::Slice slKey1((const char*)ssKey1.data(), ssKey1.size());
        leveldb::Slice slKey2((const char*)ssKey2.data(), ssKey2.size());
        pdb->CompactRange(&slKey1, &slKey2);
    }
=======
        return EstimateSizeImpl(ssKey1, ssKey2);
    }
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
};

#endif // DIGIBYTE_DBWRAPPER_H
