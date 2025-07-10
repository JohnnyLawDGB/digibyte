<<<<<<< HEAD
// Copyright (c) 2020-2021 The DigiByte Core developers
=======
// Copyright (c) 2020-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_INDEX_COINSTATSINDEX_H
#define DIGIBYTE_INDEX_COINSTATSINDEX_H

<<<<<<< HEAD
#include <chain.h>
#include <crypto/muhash.h>
#include <flatfile.h>
#include <index/base.h>
#include <node/coinstats.h>
=======
#include <crypto/muhash.h>
#include <index/base.h>

class CBlockIndex;
class CDBBatch;
namespace kernel {
struct CCoinsStats;
}

static constexpr bool DEFAULT_COINSTATSINDEX{false};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

/**
 * CoinStatsIndex maintains statistics on the UTXO set.
 */
class CoinStatsIndex final : public BaseIndex
{
private:
<<<<<<< HEAD
    std::string m_name;
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    std::unique_ptr<BaseIndex::DB> m_db;

    MuHash3072 m_muhash;
    uint64_t m_transaction_output_count{0};
    uint64_t m_bogo_size{0};
    CAmount m_total_amount{0};
    CAmount m_total_subsidy{0};
<<<<<<< HEAD
    CAmount m_block_unspendable_amount{0};
    CAmount m_block_prevout_spent_amount{0};
    CAmount m_block_new_outputs_ex_coinbase_amount{0};
    CAmount m_block_coinbase_amount{0};
    CAmount m_unspendables_genesis_block{0};
    CAmount m_unspendables_bip30{0};
    CAmount m_unspendables_scripts{0};
    CAmount m_unspendables_unclaimed_rewards{0};

    bool ReverseBlock(const CBlock& block, const CBlockIndex* pindex);

protected:
    bool Init() override;

    bool WriteBlock(const CBlock& block, const CBlockIndex* pindex) override;

    bool Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip) override;

    BaseIndex::DB& GetDB() const override { return *m_db; }

    const char* GetName() const override { return "coinstatsindex"; }

public:
    // Constructs the index, which becomes available to be queried.
    explicit CoinStatsIndex(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

    // Look up stats for a specific block using CBlockIndex
    bool LookUpStats(const CBlockIndex* block_index, CCoinsStats& coins_stats) const;
=======
    CAmount m_total_unspendable_amount{0};
    CAmount m_total_prevout_spent_amount{0};
    CAmount m_total_new_outputs_ex_coinbase_amount{0};
    CAmount m_total_coinbase_amount{0};
    CAmount m_total_unspendables_genesis_block{0};
    CAmount m_total_unspendables_bip30{0};
    CAmount m_total_unspendables_scripts{0};
    CAmount m_total_unspendables_unclaimed_rewards{0};

    [[nodiscard]] bool ReverseBlock(const CBlock& block, const CBlockIndex* pindex);

    bool AllowPrune() const override { return true; }

protected:
    bool CustomInit(const std::optional<interfaces::BlockKey>& block) override;

    bool CustomCommit(CDBBatch& batch) override;

    bool CustomAppend(const interfaces::BlockInfo& block) override;

    bool CustomRewind(const interfaces::BlockKey& current_tip, const interfaces::BlockKey& new_tip) override;

    BaseIndex::DB& GetDB() const override { return *m_db; }

public:
    // Constructs the index, which becomes available to be queried.
    explicit CoinStatsIndex(std::unique_ptr<interfaces::Chain> chain, size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

    // Look up stats for a specific block using CBlockIndex
    std::optional<kernel::CCoinsStats> LookUpStats(const CBlockIndex& block_index) const;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
};

/// The global UTXO set hash object.
extern std::unique_ptr<CoinStatsIndex> g_coin_stats_index;

#endif // DIGIBYTE_INDEX_COINSTATSINDEX_H
