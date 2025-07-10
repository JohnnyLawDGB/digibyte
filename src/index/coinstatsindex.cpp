<<<<<<< HEAD
// Copyright (c) 2020-2021 The DigiByte Core developers
=======
// Copyright (c) 2020-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
<<<<<<< HEAD
#include <crypto/muhash.h>
#include <index/coinstatsindex.h>
=======
#include <common/args.h>
#include <crypto/muhash.h>
#include <index/coinstatsindex.h>
#include <kernel/coinstats.h>
#include <logging.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <node/blockstorage.h>
#include <serialize.h>
#include <txdb.h>
#include <undo.h>
#include <validation.h>

<<<<<<< HEAD
=======
using kernel::ApplyCoinHash;
using kernel::CCoinsStats;
using kernel::GetBogoSize;
using kernel::RemoveCoinHash;

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
static constexpr uint8_t DB_BLOCK_HASH{'s'};
static constexpr uint8_t DB_BLOCK_HEIGHT{'t'};
static constexpr uint8_t DB_MUHASH{'M'};

namespace {

struct DBVal {
    uint256 muhash;
    uint64_t transaction_output_count;
    uint64_t bogo_size;
    CAmount total_amount;
    CAmount total_subsidy;
<<<<<<< HEAD
    CAmount block_unspendable_amount;
    CAmount block_prevout_spent_amount;
    CAmount block_new_outputs_ex_coinbase_amount;
    CAmount block_coinbase_amount;
    CAmount unspendables_genesis_block;
    CAmount unspendables_bip30;
    CAmount unspendables_scripts;
    CAmount unspendables_unclaimed_rewards;
=======
    CAmount total_unspendable_amount;
    CAmount total_prevout_spent_amount;
    CAmount total_new_outputs_ex_coinbase_amount;
    CAmount total_coinbase_amount;
    CAmount total_unspendables_genesis_block;
    CAmount total_unspendables_bip30;
    CAmount total_unspendables_scripts;
    CAmount total_unspendables_unclaimed_rewards;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    SERIALIZE_METHODS(DBVal, obj)
    {
        READWRITE(obj.muhash);
        READWRITE(obj.transaction_output_count);
        READWRITE(obj.bogo_size);
        READWRITE(obj.total_amount);
        READWRITE(obj.total_subsidy);
<<<<<<< HEAD
        READWRITE(obj.block_unspendable_amount);
        READWRITE(obj.block_prevout_spent_amount);
        READWRITE(obj.block_new_outputs_ex_coinbase_amount);
        READWRITE(obj.block_coinbase_amount);
        READWRITE(obj.unspendables_genesis_block);
        READWRITE(obj.unspendables_bip30);
        READWRITE(obj.unspendables_scripts);
        READWRITE(obj.unspendables_unclaimed_rewards);
=======
        READWRITE(obj.total_unspendable_amount);
        READWRITE(obj.total_prevout_spent_amount);
        READWRITE(obj.total_new_outputs_ex_coinbase_amount);
        READWRITE(obj.total_coinbase_amount);
        READWRITE(obj.total_unspendables_genesis_block);
        READWRITE(obj.total_unspendables_bip30);
        READWRITE(obj.total_unspendables_scripts);
        READWRITE(obj.total_unspendables_unclaimed_rewards);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
};

struct DBHeightKey {
    int height;

    explicit DBHeightKey(int height_in) : height(height_in) {}

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        ser_writedata8(s, DB_BLOCK_HEIGHT);
        ser_writedata32be(s, height);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        const uint8_t prefix{ser_readdata8(s)};
        if (prefix != DB_BLOCK_HEIGHT) {
            throw std::ios_base::failure("Invalid format for coinstatsindex DB height key");
        }
        height = ser_readdata32be(s);
    }
};

struct DBHashKey {
    uint256 block_hash;

    explicit DBHashKey(const uint256& hash_in) : block_hash(hash_in) {}

    SERIALIZE_METHODS(DBHashKey, obj)
    {
        uint8_t prefix{DB_BLOCK_HASH};
        READWRITE(prefix);
        if (prefix != DB_BLOCK_HASH) {
            throw std::ios_base::failure("Invalid format for coinstatsindex DB hash key");
        }

        READWRITE(obj.block_hash);
    }
};

}; // namespace

std::unique_ptr<CoinStatsIndex> g_coin_stats_index;

<<<<<<< HEAD
CoinStatsIndex::CoinStatsIndex(size_t n_cache_size, bool f_memory, bool f_wipe)
=======
CoinStatsIndex::CoinStatsIndex(std::unique_ptr<interfaces::Chain> chain, size_t n_cache_size, bool f_memory, bool f_wipe)
    : BaseIndex(std::move(chain), "coinstatsindex")
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    fs::path path{gArgs.GetDataDirNet() / "indexes" / "coinstats"};
    fs::create_directories(path);

    m_db = std::make_unique<CoinStatsIndex::DB>(path / "db", n_cache_size, f_memory, f_wipe);
}

<<<<<<< HEAD
bool CoinStatsIndex::WriteBlock(const CBlock& block, const CBlockIndex* pindex)
{
    CBlockUndo block_undo;
    const CAmount block_subsidy{GetBlockSubsidy(pindex->nHeight, Params().GetConsensus())};
    m_total_subsidy += block_subsidy;

    // Ignore genesis block
    if (pindex->nHeight > 0) {
        if (!UndoReadFromDisk(block_undo, pindex)) {
=======
bool CoinStatsIndex::CustomAppend(const interfaces::BlockInfo& block)
{
    CBlockUndo block_undo;
    const CAmount block_subsidy{GetBlockSubsidy(block.height, Params().GetConsensus())};
    m_total_subsidy += block_subsidy;

    // Ignore genesis block
    if (block.height > 0) {
        // pindex variable gives indexing code access to node internals. It
        // will be removed in upcoming commit
        const CBlockIndex* pindex = WITH_LOCK(cs_main, return m_chainstate->m_blockman.LookupBlockIndex(block.hash));
        if (!m_chainstate->m_blockman.UndoReadFromDisk(block_undo, *pindex)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            return false;
        }

        std::pair<uint256, DBVal> read_out;
<<<<<<< HEAD
        if (!m_db->Read(DBHeightKey(pindex->nHeight - 1), read_out)) {
            return false;
        }

        uint256 expected_block_hash{pindex->pprev->GetBlockHash()};
        if (read_out.first != expected_block_hash) {
            if (!m_db->Read(DBHashKey(expected_block_hash), read_out)) {
                return error("%s: previous block header belongs to unexpected block %s; expected %s",
                             __func__, read_out.first.ToString(), expected_block_hash.ToString());
            }
        }

        // TODO: Deduplicate BIP30 related code
        bool is_bip30_block{(pindex->nHeight == 91722 && pindex->GetBlockHash() == uint256S("0x00000000000271a2dc26e7667f8419f2e15416dc6955e5a6c6cdf3f2574dd08e")) ||
                            (pindex->nHeight == 91812 && pindex->GetBlockHash() == uint256S("0x00000000000af0aed4792b1acee3d966af36cf5def14935db8de83d6f9306f2f"))};

        // Add the new utxos created from the block
        for (size_t i = 0; i < block.vtx.size(); ++i) {
            const auto& tx{block.vtx.at(i)};

            // Skip duplicate txid coinbase transactions (BIP30).
            if (is_bip30_block && tx->IsCoinBase()) {
                m_block_unspendable_amount += block_subsidy;
                m_unspendables_bip30 += block_subsidy;
                continue;
            }

            for (size_t j = 0; j < tx->vout.size(); ++j) {
                const CTxOut& out{tx->vout[j]};
                Coin coin{out, pindex->nHeight, tx->IsCoinBase()};
                COutPoint outpoint{tx->GetHash(), static_cast<uint32_t>(j)};

                // Skip unspendable coins
                if (coin.out.scriptPubKey.IsUnspendable()) {
                    m_block_unspendable_amount += coin.out.nValue;
                    m_unspendables_scripts += coin.out.nValue;
                    continue;
                }

                m_muhash.Insert(MakeUCharSpan(TxOutSer(outpoint, coin)));

                if (tx->IsCoinBase()) {
                    m_block_coinbase_amount += coin.out.nValue;
                } else {
                    m_block_new_outputs_ex_coinbase_amount += coin.out.nValue;
=======
        if (!m_db->Read(DBHeightKey(block.height - 1), read_out)) {
            return false;
        }

        uint256 expected_block_hash{*Assert(block.prev_hash)};
        if (read_out.first != expected_block_hash) {
            LogPrintf("WARNING: previous block header belongs to unexpected block %s; expected %s\n",
                      read_out.first.ToString(), expected_block_hash.ToString());

            if (!m_db->Read(DBHashKey(expected_block_hash), read_out)) {
                return error("%s: previous block header not found; expected %s",
                             __func__, expected_block_hash.ToString());
            }
        }

        // Add the new utxos created from the block
        assert(block.data);
        for (size_t i = 0; i < block.data->vtx.size(); ++i) {
            const auto& tx{block.data->vtx.at(i)};

            // Skip duplicate txid coinbase transactions (BIP30).
            if (IsBIP30Unspendable(*pindex) && tx->IsCoinBase()) {
                m_total_unspendable_amount += block_subsidy;
                m_total_unspendables_bip30 += block_subsidy;
                continue;
            }

            for (uint32_t j = 0; j < tx->vout.size(); ++j) {
                const CTxOut& out{tx->vout[j]};
                Coin coin{out, block.height, tx->IsCoinBase()};
                COutPoint outpoint{tx->GetHash(), j};

                // Skip unspendable coins
                if (coin.out.scriptPubKey.IsUnspendable()) {
                    m_total_unspendable_amount += coin.out.nValue;
                    m_total_unspendables_scripts += coin.out.nValue;
                    continue;
                }

                ApplyCoinHash(m_muhash, outpoint, coin);

                if (tx->IsCoinBase()) {
                    m_total_coinbase_amount += coin.out.nValue;
                } else {
                    m_total_new_outputs_ex_coinbase_amount += coin.out.nValue;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                }

                ++m_transaction_output_count;
                m_total_amount += coin.out.nValue;
                m_bogo_size += GetBogoSize(coin.out.scriptPubKey);
            }

            // The coinbase tx has no undo data since no former output is spent
            if (!tx->IsCoinBase()) {
                const auto& tx_undo{block_undo.vtxundo.at(i - 1)};

                for (size_t j = 0; j < tx_undo.vprevout.size(); ++j) {
                    Coin coin{tx_undo.vprevout[j]};
                    COutPoint outpoint{tx->vin[j].prevout.hash, tx->vin[j].prevout.n};

<<<<<<< HEAD
                    m_muhash.Remove(MakeUCharSpan(TxOutSer(outpoint, coin)));

                    m_block_prevout_spent_amount += coin.out.nValue;
=======
                    RemoveCoinHash(m_muhash, outpoint, coin);

                    m_total_prevout_spent_amount += coin.out.nValue;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

                    --m_transaction_output_count;
                    m_total_amount -= coin.out.nValue;
                    m_bogo_size -= GetBogoSize(coin.out.scriptPubKey);
                }
            }
        }
    } else {
        // genesis block
<<<<<<< HEAD
        m_block_unspendable_amount += block_subsidy;
        m_unspendables_genesis_block += block_subsidy;
=======
        m_total_unspendable_amount += block_subsidy;
        m_total_unspendables_genesis_block += block_subsidy;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    // If spent prevouts + block subsidy are still a higher amount than
    // new outputs + coinbase + current unspendable amount this means
    // the miner did not claim the full block reward. Unclaimed block
    // rewards are also unspendable.
<<<<<<< HEAD
    const CAmount unclaimed_rewards{(m_block_prevout_spent_amount + m_total_subsidy) - (m_block_new_outputs_ex_coinbase_amount + m_block_coinbase_amount + m_block_unspendable_amount)};
    m_block_unspendable_amount += unclaimed_rewards;
    m_unspendables_unclaimed_rewards += unclaimed_rewards;

    std::pair<uint256, DBVal> value;
    value.first = pindex->GetBlockHash();
=======
    const CAmount unclaimed_rewards{(m_total_prevout_spent_amount + m_total_subsidy) - (m_total_new_outputs_ex_coinbase_amount + m_total_coinbase_amount + m_total_unspendable_amount)};
    m_total_unspendable_amount += unclaimed_rewards;
    m_total_unspendables_unclaimed_rewards += unclaimed_rewards;

    std::pair<uint256, DBVal> value;
    value.first = block.hash;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    value.second.transaction_output_count = m_transaction_output_count;
    value.second.bogo_size = m_bogo_size;
    value.second.total_amount = m_total_amount;
    value.second.total_subsidy = m_total_subsidy;
<<<<<<< HEAD
    value.second.block_unspendable_amount = m_block_unspendable_amount;
    value.second.block_prevout_spent_amount = m_block_prevout_spent_amount;
    value.second.block_new_outputs_ex_coinbase_amount = m_block_new_outputs_ex_coinbase_amount;
    value.second.block_coinbase_amount = m_block_coinbase_amount;
    value.second.unspendables_genesis_block = m_unspendables_genesis_block;
    value.second.unspendables_bip30 = m_unspendables_bip30;
    value.second.unspendables_scripts = m_unspendables_scripts;
    value.second.unspendables_unclaimed_rewards = m_unspendables_unclaimed_rewards;
=======
    value.second.total_unspendable_amount = m_total_unspendable_amount;
    value.second.total_prevout_spent_amount = m_total_prevout_spent_amount;
    value.second.total_new_outputs_ex_coinbase_amount = m_total_new_outputs_ex_coinbase_amount;
    value.second.total_coinbase_amount = m_total_coinbase_amount;
    value.second.total_unspendables_genesis_block = m_total_unspendables_genesis_block;
    value.second.total_unspendables_bip30 = m_total_unspendables_bip30;
    value.second.total_unspendables_scripts = m_total_unspendables_scripts;
    value.second.total_unspendables_unclaimed_rewards = m_total_unspendables_unclaimed_rewards;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    uint256 out;
    m_muhash.Finalize(out);
    value.second.muhash = out;

<<<<<<< HEAD
    return m_db->Write(DBHeightKey(pindex->nHeight), value) && m_db->Write(DB_MUHASH, m_muhash);
}

static bool CopyHeightIndexToHashIndex(CDBIterator& db_it, CDBBatch& batch,
=======
    // Intentionally do not update DB_MUHASH here so it stays in sync with
    // DB_BEST_BLOCK, and the index is not corrupted if there is an unclean shutdown.
    return m_db->Write(DBHeightKey(block.height), value);
}

[[nodiscard]] static bool CopyHeightIndexToHashIndex(CDBIterator& db_it, CDBBatch& batch,
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                                       const std::string& index_name,
                                       int start_height, int stop_height)
{
    DBHeightKey key{start_height};
    db_it.Seek(key);

    for (int height = start_height; height <= stop_height; ++height) {
        if (!db_it.GetKey(key) || key.height != height) {
            return error("%s: unexpected key in %s: expected (%c, %d)",
                         __func__, index_name, DB_BLOCK_HEIGHT, height);
        }

        std::pair<uint256, DBVal> value;
        if (!db_it.GetValue(value)) {
            return error("%s: unable to read value in %s at key (%c, %d)",
                         __func__, index_name, DB_BLOCK_HEIGHT, height);
        }

        batch.Write(DBHashKey(value.first), std::move(value.second));

        db_it.Next();
    }
    return true;
}

<<<<<<< HEAD
bool CoinStatsIndex::Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip)
{
    assert(current_tip->GetAncestor(new_tip->nHeight) == new_tip);

=======
bool CoinStatsIndex::CustomRewind(const interfaces::BlockKey& current_tip, const interfaces::BlockKey& new_tip)
{
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    CDBBatch batch(*m_db);
    std::unique_ptr<CDBIterator> db_it(m_db->NewIterator());

    // During a reorg, we need to copy all hash digests for blocks that are
    // getting disconnected from the height index to the hash index so we can
    // still find them when the height index entries are overwritten.
<<<<<<< HEAD
    if (!CopyHeightIndexToHashIndex(*db_it, batch, m_name, new_tip->nHeight, current_tip->nHeight)) {
=======
    if (!CopyHeightIndexToHashIndex(*db_it, batch, m_name, new_tip.height, current_tip.height)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        return false;
    }

    if (!m_db->WriteBatch(batch)) return false;

    {
        LOCK(cs_main);
<<<<<<< HEAD
        CBlockIndex* iter_tip{m_chainstate->m_blockman.LookupBlockIndex(current_tip->GetBlockHash())};
        const auto& consensus_params{Params().GetConsensus()};
=======
        const CBlockIndex* iter_tip{m_chainstate->m_blockman.LookupBlockIndex(current_tip.hash)};
        const CBlockIndex* new_tip_index{m_chainstate->m_blockman.LookupBlockIndex(new_tip.hash)};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

        do {
            CBlock block;

<<<<<<< HEAD
            if (!ReadBlockFromDisk(block, iter_tip, consensus_params)) {
=======
            if (!m_chainstate->m_blockman.ReadBlockFromDisk(block, *iter_tip)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                return error("%s: Failed to read block %s from disk",
                             __func__, iter_tip->GetBlockHash().ToString());
            }

<<<<<<< HEAD
            ReverseBlock(block, iter_tip);

            iter_tip = iter_tip->GetAncestor(iter_tip->nHeight - 1);
        } while (new_tip != iter_tip);
    }

    return BaseIndex::Rewind(current_tip, new_tip);
}

static bool LookUpOne(const CDBWrapper& db, const CBlockIndex* block_index, DBVal& result)
=======
            if (!ReverseBlock(block, iter_tip)) {
                return false; // failure cause logged internally
            }

            iter_tip = iter_tip->GetAncestor(iter_tip->nHeight - 1);
        } while (new_tip_index != iter_tip);
    }

    return true;
}

static bool LookUpOne(const CDBWrapper& db, const interfaces::BlockKey& block, DBVal& result)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    // First check if the result is stored under the height index and the value
    // there matches the block hash. This should be the case if the block is on
    // the active chain.
    std::pair<uint256, DBVal> read_out;
<<<<<<< HEAD
    if (!db.Read(DBHeightKey(block_index->nHeight), read_out)) {
        return false;
    }
    if (read_out.first == block_index->GetBlockHash()) {
=======
    if (!db.Read(DBHeightKey(block.height), read_out)) {
        return false;
    }
    if (read_out.first == block.hash) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        result = std::move(read_out.second);
        return true;
    }

    // If value at the height index corresponds to an different block, the
    // result will be stored in the hash index.
<<<<<<< HEAD
    return db.Read(DBHashKey(block_index->GetBlockHash()), result);
}

bool CoinStatsIndex::LookUpStats(const CBlockIndex* block_index, CCoinsStats& coins_stats) const
{
    DBVal entry;
    if (!LookUpOne(*m_db, block_index, entry)) {
        return false;
    }

    coins_stats.hashSerialized = entry.muhash;
    coins_stats.nTransactionOutputs = entry.transaction_output_count;
    coins_stats.nBogoSize = entry.bogo_size;
    coins_stats.nTotalAmount = entry.total_amount;
    coins_stats.total_subsidy = entry.total_subsidy;
    coins_stats.block_unspendable_amount = entry.block_unspendable_amount;
    coins_stats.block_prevout_spent_amount = entry.block_prevout_spent_amount;
    coins_stats.block_new_outputs_ex_coinbase_amount = entry.block_new_outputs_ex_coinbase_amount;
    coins_stats.block_coinbase_amount = entry.block_coinbase_amount;
    coins_stats.unspendables_genesis_block = entry.unspendables_genesis_block;
    coins_stats.unspendables_bip30 = entry.unspendables_bip30;
    coins_stats.unspendables_scripts = entry.unspendables_scripts;
    coins_stats.unspendables_unclaimed_rewards = entry.unspendables_unclaimed_rewards;

    return true;
}

bool CoinStatsIndex::Init()
=======
    return db.Read(DBHashKey(block.hash), result);
}

std::optional<CCoinsStats> CoinStatsIndex::LookUpStats(const CBlockIndex& block_index) const
{
    CCoinsStats stats{block_index.nHeight, block_index.GetBlockHash()};
    stats.index_used = true;

    DBVal entry;
    if (!LookUpOne(*m_db, {block_index.GetBlockHash(), block_index.nHeight}, entry)) {
        return std::nullopt;
    }

    stats.hashSerialized = entry.muhash;
    stats.nTransactionOutputs = entry.transaction_output_count;
    stats.nBogoSize = entry.bogo_size;
    stats.total_amount = entry.total_amount;
    stats.total_subsidy = entry.total_subsidy;
    stats.total_unspendable_amount = entry.total_unspendable_amount;
    stats.total_prevout_spent_amount = entry.total_prevout_spent_amount;
    stats.total_new_outputs_ex_coinbase_amount = entry.total_new_outputs_ex_coinbase_amount;
    stats.total_coinbase_amount = entry.total_coinbase_amount;
    stats.total_unspendables_genesis_block = entry.total_unspendables_genesis_block;
    stats.total_unspendables_bip30 = entry.total_unspendables_bip30;
    stats.total_unspendables_scripts = entry.total_unspendables_scripts;
    stats.total_unspendables_unclaimed_rewards = entry.total_unspendables_unclaimed_rewards;

    return stats;
}

bool CoinStatsIndex::CustomInit(const std::optional<interfaces::BlockKey>& block)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    if (!m_db->Read(DB_MUHASH, m_muhash)) {
        // Check that the cause of the read failure is that the key does not
        // exist. Any other errors indicate database corruption or a disk
        // failure, and starting the index would cause further corruption.
        if (m_db->Exists(DB_MUHASH)) {
            return error("%s: Cannot read current %s state; index may be corrupted",
                         __func__, GetName());
        }
    }

<<<<<<< HEAD
    if (BaseIndex::Init()) {
        const CBlockIndex* pindex{CurrentIndex()};

        if (pindex) {
            DBVal entry;
            if (!LookUpOne(*m_db, pindex, entry)) {
                return false;
            }

            m_transaction_output_count = entry.transaction_output_count;
            m_bogo_size = entry.bogo_size;
            m_total_amount = entry.total_amount;
            m_total_subsidy = entry.total_subsidy;
            m_block_unspendable_amount = entry.block_unspendable_amount;
            m_block_prevout_spent_amount = entry.block_prevout_spent_amount;
            m_block_new_outputs_ex_coinbase_amount = entry.block_new_outputs_ex_coinbase_amount;
            m_block_coinbase_amount = entry.block_coinbase_amount;
            m_unspendables_genesis_block = entry.unspendables_genesis_block;
            m_unspendables_bip30 = entry.unspendables_bip30;
            m_unspendables_scripts = entry.unspendables_scripts;
            m_unspendables_unclaimed_rewards = entry.unspendables_unclaimed_rewards;
        }

        return true;
    }

    return false;
=======
    if (block) {
        DBVal entry;
        if (!LookUpOne(*m_db, *block, entry)) {
            return error("%s: Cannot read current %s state; index may be corrupted",
                         __func__, GetName());
        }

        uint256 out;
        m_muhash.Finalize(out);
        if (entry.muhash != out) {
            return error("%s: Cannot read current %s state; index may be corrupted",
                         __func__, GetName());
        }

        m_transaction_output_count = entry.transaction_output_count;
        m_bogo_size = entry.bogo_size;
        m_total_amount = entry.total_amount;
        m_total_subsidy = entry.total_subsidy;
        m_total_unspendable_amount = entry.total_unspendable_amount;
        m_total_prevout_spent_amount = entry.total_prevout_spent_amount;
        m_total_new_outputs_ex_coinbase_amount = entry.total_new_outputs_ex_coinbase_amount;
        m_total_coinbase_amount = entry.total_coinbase_amount;
        m_total_unspendables_genesis_block = entry.total_unspendables_genesis_block;
        m_total_unspendables_bip30 = entry.total_unspendables_bip30;
        m_total_unspendables_scripts = entry.total_unspendables_scripts;
        m_total_unspendables_unclaimed_rewards = entry.total_unspendables_unclaimed_rewards;
    }

    return true;
}

bool CoinStatsIndex::CustomCommit(CDBBatch& batch)
{
    // DB_MUHASH should always be committed in a batch together with DB_BEST_BLOCK
    // to prevent an inconsistent state of the DB.
    batch.Write(DB_MUHASH, m_muhash);
    return true;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

// Reverse a single block as part of a reorg
bool CoinStatsIndex::ReverseBlock(const CBlock& block, const CBlockIndex* pindex)
{
    CBlockUndo block_undo;
    std::pair<uint256, DBVal> read_out;

    const CAmount block_subsidy{GetBlockSubsidy(pindex->nHeight, Params().GetConsensus())};
    m_total_subsidy -= block_subsidy;

    // Ignore genesis block
    if (pindex->nHeight > 0) {
<<<<<<< HEAD
        if (!UndoReadFromDisk(block_undo, pindex)) {
=======
        if (!m_chainstate->m_blockman.UndoReadFromDisk(block_undo, *pindex)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            return false;
        }

        if (!m_db->Read(DBHeightKey(pindex->nHeight - 1), read_out)) {
            return false;
        }

        uint256 expected_block_hash{pindex->pprev->GetBlockHash()};
        if (read_out.first != expected_block_hash) {
<<<<<<< HEAD
            if (!m_db->Read(DBHashKey(expected_block_hash), read_out)) {
                return error("%s: previous block header belongs to unexpected block %s; expected %s",
                             __func__, read_out.first.ToString(), expected_block_hash.ToString());
=======
            LogPrintf("WARNING: previous block header belongs to unexpected block %s; expected %s\n",
                      read_out.first.ToString(), expected_block_hash.ToString());

            if (!m_db->Read(DBHashKey(expected_block_hash), read_out)) {
                return error("%s: previous block header not found; expected %s",
                             __func__, expected_block_hash.ToString());
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            }
        }
    }

    // Remove the new UTXOs that were created from the block
    for (size_t i = 0; i < block.vtx.size(); ++i) {
        const auto& tx{block.vtx.at(i)};

<<<<<<< HEAD
        for (size_t j = 0; j < tx->vout.size(); ++j) {
            const CTxOut& out{tx->vout[j]};
            COutPoint outpoint{tx->GetHash(), static_cast<uint32_t>(j)};
=======
        for (uint32_t j = 0; j < tx->vout.size(); ++j) {
            const CTxOut& out{tx->vout[j]};
            COutPoint outpoint{tx->GetHash(), j};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            Coin coin{out, pindex->nHeight, tx->IsCoinBase()};

            // Skip unspendable coins
            if (coin.out.scriptPubKey.IsUnspendable()) {
<<<<<<< HEAD
                m_block_unspendable_amount -= coin.out.nValue;
                m_unspendables_scripts -= coin.out.nValue;
                continue;
            }

            m_muhash.Remove(MakeUCharSpan(TxOutSer(outpoint, coin)));

            if (tx->IsCoinBase()) {
                m_block_coinbase_amount -= coin.out.nValue;
            } else {
                m_block_new_outputs_ex_coinbase_amount -= coin.out.nValue;
=======
                m_total_unspendable_amount -= coin.out.nValue;
                m_total_unspendables_scripts -= coin.out.nValue;
                continue;
            }

            RemoveCoinHash(m_muhash, outpoint, coin);

            if (tx->IsCoinBase()) {
                m_total_coinbase_amount -= coin.out.nValue;
            } else {
                m_total_new_outputs_ex_coinbase_amount -= coin.out.nValue;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            }

            --m_transaction_output_count;
            m_total_amount -= coin.out.nValue;
            m_bogo_size -= GetBogoSize(coin.out.scriptPubKey);
        }

        // The coinbase tx has no undo data since no former output is spent
        if (!tx->IsCoinBase()) {
            const auto& tx_undo{block_undo.vtxundo.at(i - 1)};

            for (size_t j = 0; j < tx_undo.vprevout.size(); ++j) {
                Coin coin{tx_undo.vprevout[j]};
                COutPoint outpoint{tx->vin[j].prevout.hash, tx->vin[j].prevout.n};

<<<<<<< HEAD
                m_muhash.Insert(MakeUCharSpan(TxOutSer(outpoint, coin)));

                m_block_prevout_spent_amount -= coin.out.nValue;
=======
                ApplyCoinHash(m_muhash, outpoint, coin);

                m_total_prevout_spent_amount -= coin.out.nValue;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

                m_transaction_output_count++;
                m_total_amount += coin.out.nValue;
                m_bogo_size += GetBogoSize(coin.out.scriptPubKey);
            }
        }
    }

<<<<<<< HEAD
    const CAmount unclaimed_rewards{(m_block_new_outputs_ex_coinbase_amount + m_block_coinbase_amount + m_block_unspendable_amount) - (m_block_prevout_spent_amount + m_total_subsidy)};
    m_block_unspendable_amount -= unclaimed_rewards;
    m_unspendables_unclaimed_rewards -= unclaimed_rewards;
=======
    const CAmount unclaimed_rewards{(m_total_new_outputs_ex_coinbase_amount + m_total_coinbase_amount + m_total_unspendable_amount) - (m_total_prevout_spent_amount + m_total_subsidy)};
    m_total_unspendable_amount -= unclaimed_rewards;
    m_total_unspendables_unclaimed_rewards -= unclaimed_rewards;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // Check that the rolled back internal values are consistent with the DB read out
    uint256 out;
    m_muhash.Finalize(out);
    Assert(read_out.second.muhash == out);

    Assert(m_transaction_output_count == read_out.second.transaction_output_count);
    Assert(m_total_amount == read_out.second.total_amount);
    Assert(m_bogo_size == read_out.second.bogo_size);
    Assert(m_total_subsidy == read_out.second.total_subsidy);
<<<<<<< HEAD
    Assert(m_block_unspendable_amount == read_out.second.block_unspendable_amount);
    Assert(m_block_prevout_spent_amount == read_out.second.block_prevout_spent_amount);
    Assert(m_block_new_outputs_ex_coinbase_amount == read_out.second.block_new_outputs_ex_coinbase_amount);
    Assert(m_block_coinbase_amount == read_out.second.block_coinbase_amount);
    Assert(m_unspendables_genesis_block == read_out.second.unspendables_genesis_block);
    Assert(m_unspendables_bip30 == read_out.second.unspendables_bip30);
    Assert(m_unspendables_scripts == read_out.second.unspendables_scripts);
    Assert(m_unspendables_unclaimed_rewards == read_out.second.unspendables_unclaimed_rewards);

    return m_db->Write(DB_MUHASH, m_muhash);
=======
    Assert(m_total_unspendable_amount == read_out.second.total_unspendable_amount);
    Assert(m_total_prevout_spent_amount == read_out.second.total_prevout_spent_amount);
    Assert(m_total_new_outputs_ex_coinbase_amount == read_out.second.total_new_outputs_ex_coinbase_amount);
    Assert(m_total_coinbase_amount == read_out.second.total_coinbase_amount);
    Assert(m_total_unspendables_genesis_block == read_out.second.total_unspendables_genesis_block);
    Assert(m_total_unspendables_bip30 == read_out.second.total_unspendables_bip30);
    Assert(m_total_unspendables_scripts == read_out.second.total_unspendables_scripts);
    Assert(m_total_unspendables_unclaimed_rewards == read_out.second.total_unspendables_unclaimed_rewards);

    return true;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}
