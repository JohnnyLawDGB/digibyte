// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2014-2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrdb.h>

#include <addrman.h>
#include <chainparams.h>
#include <clientversion.h>
#include <common/args.h>
#include <common/settings.h>
#include <cstdint>
#include <hash.h>
#include <logging.h>
#include <logging/timer.h>
#include <netbase.h>
#include <netgroup.h>
#include <random.h>
#include <streams.h>
#include <tinyformat.h>
#include <univalue.h>
#include <util/fs.h>
#include <util/fs_helpers.h>
#include <util/translation.h>

CBanEntry::CBanEntry(const UniValue& json)
    : nVersion(json["version"].getInt<int>()), nCreateTime(json["ban_created"].getInt<int64_t>()),
      nBanUntil(json["banned_until"].getInt<int64_t>())
{
}

UniValue CBanEntry::ToJson() const
{
    UniValue json(UniValue::VOBJ);
    json.pushKV("version", nVersion);
    json.pushKV("ban_created", nCreateTime);
    json.pushKV("banned_until", nBanUntil);
    return json;
}

namespace {

class DbNotFoundError : public std::exception
{
    using std::exception::exception;
};


template <typename Stream, typename Data>
bool SerializeDB(Stream& stream, const Data& data)
{
    // Write and commit header, data
    try {
        HashedSourceWriter hashwriter{stream};
        hashwriter << Params().MessageStart() << data;
        stream << hashwriter.GetHash();
    } catch (const std::exception& e) {
        return error("%s: Serialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool SerializeFileDB(const std::string& prefix, const fs::path& path, const Data& data)
{
    // Generate random temporary filename
    const uint16_t randv{GetRand<uint16_t>()};
    std::string tmpfn = strprintf("%s.%04x", prefix, randv);

    // open temp output file, and associate with CAutoFile
    fs::path pathTmp = gArgs.GetDataDirNet() / fs::u8path(tmpfn);
    FILE *file = fsbridge::fopen(pathTmp, "wb");
    AutoFile fileout{file};
    if (fileout.IsNull()) {
        fileout.fclose();
        remove(pathTmp);
        return error("%s: Failed to open file %s", __func__, PathToString(pathTmp));
    }

    // Serialize
    if (!SerializeDB(fileout, data)) {
        fileout.fclose();
        remove(pathTmp);
        return false;
    }
    if (!FileCommit(fileout.Get())) {
        fileout.fclose();
        remove(pathTmp);
        return error("%s: Failed to flush file %s", __func__, PathToString(pathTmp));
    }
    fileout.fclose();

    // replace existing file, if any, with new file
    if (!RenameOver(pathTmp, path)) {
        remove(pathTmp);
        return error("%s: Rename-into-place failed", __func__);
    }

    return true;
}

template <typename Stream, typename Data>
void DeserializeDB(Stream& stream, Data&& data, bool fCheckSum = true)
{
    HashVerifier verifier{stream};
    // de-serialize file header (network specific magic number) and ..
    MessageStartChars pchMsgTmp;
    verifier >> pchMsgTmp;
    // ... verify the network matches ours
    if (pchMsgTmp != Params().MessageStart()) {
        throw std::runtime_error{"Invalid network magic number"};
    }

    // de-serialize data
    verifier >> data;

    // verify checksum
    if (fCheckSum) {
        uint256 hashTmp;
        stream >> hashTmp;
        if (hashTmp != verifier.GetHash()) {
            throw std::runtime_error{"Checksum mismatch, data corrupted"};
        }
    }
}

template <typename Data>
void DeserializeFileDB(const fs::path& path, Data&& data)
{
    FILE* file = fsbridge::fopen(path, "rb");
    AutoFile filein{file};
    if (filein.IsNull()) {
        throw DbNotFoundError{};
    }
    DeserializeDB(filein, data);
}
} // namespace

CBanDB::CBanDB(fs::path ban_list_path)
    : m_banlist_dat(ban_list_path + ".dat"),
      m_banlist_json(ban_list_path + ".json")
{
}

bool CBanDB::Write(const banmap_t& banSet)
{
    std::vector<std::string> errors;
    if (common::WriteSettings(m_banlist_json, {{JSON_KEY, BanMapToJson(banSet)}}, errors)) {
        return true;
    }

    for (const auto& err : errors) {
        error("%s", err);
    }
    return false;
}

bool CBanDB::Read(banmap_t& banSet, bool& dirty)
{
    // If the JSON banlist does not exist, then try to read the non-upgraded banlist.dat.
    if (!fs::exists(m_banlist_json)) {
        // If this succeeds then we need to flush to disk in order to create the JSON banlist.
        dirty = true;
        try {
            DeserializeFileDB(m_banlist_dat, banSet);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    dirty = false;

    std::map<std::string, common::SettingsValue> settings;
    std::vector<std::string> errors;

    if (!common::ReadSettings(m_banlist_json, settings, errors)) {
        for (const auto& err : errors) {
            LogPrintf("Cannot load banlist %s: %s\n", PathToString(m_banlist_json), err);
        }
        return false;
    }

    try {
        BanMapFromJson(settings[JSON_KEY], banSet);
    } catch (const std::runtime_error& e) {
        LogPrintf("Cannot parse banlist %s: %s\n", PathToString(m_banlist_json), e.what());
        return false;
    }

    return true;
}

CAddrDB::CAddrDB()
{
    pathAddr = gArgs.GetDataDirNet() / "peers.dat";
}

bool CAddrDB::Write(const CAddrMan& addr)
{
    return SerializeFileDB("peers", pathAddr, addr);
}

bool CAddrDB::Read(CAddrMan& addr)
{
    try {
        DeserializeFileDB(pathAddr, addr);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool CAddrDB::Read(CAddrMan& addr, CDataStream& ssPeers)
{
    try {
        DeserializeDB(ssPeers, addr, false);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void DumpAnchors(const fs::path& anchors_db_path, const std::vector<CAddress>& anchors)
{
    LOG_TIME_SECONDS(strprintf("Flush %d outbound block-relay-only peer addresses to anchors.dat", anchors.size()));
    SerializeFileDB("anchors", anchors_db_path, CAddress::V2_DISK(anchors));
}

std::vector<CAddress> ReadAnchors(const fs::path& anchors_db_path)
{
    std::vector<CAddress> anchors;
    try {
        DeserializeFileDB(anchors_db_path, CAddress::V2_DISK(anchors));
        LogPrintf("Loaded %i addresses from %s\n", anchors.size(), fs::quoted(PathToString(anchors_db_path.filename())));
    } catch (const std::exception&) {
        anchors.clear();
    }

    fs::remove(anchors_db_path);
    return anchors;
}
