// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2014-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <node/miner.h>

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <common/args.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <deploymentstatus.h>
#include <logging.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <timedata.h>
#include <util/moneystr.h>
#include <validation.h>
#include <oracle/bundle_manager.h>
#include <consensus/digidollar.h>
#include <digidollar/digidollar.h>
#include <digidollar/validation.h>

#include <algorithm>
#include <string_view>
#include <utility>

namespace node {
int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev, int algo)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime{std::max<int64_t>(pindexPrev->GetMedianTimePast() + 1, TicksSinceEpoch<std::chrono::seconds>(GetAdjustedTime()))};

    if (nOldTime < nNewTime) {
        pblock->nTime = nNewTime;
    }

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks) {
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams, algo);
    }

    return nNewTime - nOldTime;
}

void RegenerateCommitments(CBlock& block, ChainstateManager& chainman)
{
    CMutableTransaction tx{*block.vtx.at(0)};
    tx.vout.erase(
        std::remove_if(tx.vout.begin(), tx.vout.end(),
            [](const CTxOut& txout) { return txout.scriptPubKey.IsUnspendable(); }),
        tx.vout.end());
    block.vtx.at(0) = MakeTransactionRef(tx);

    const CBlockIndex* prev_block = WITH_LOCK(::cs_main, return chainman.m_blockman.LookupBlockIndex(block.hashPrevBlock));
    chainman.GenerateCoinbaseCommitment(block, prev_block);

    // Re-add oracle bundle (was stripped with other OP_RETURN outputs above)
    if (prev_block) {
        if (DigiDollar::IsDigiDollarEnabled(prev_block, chainman)) {
            OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
            int32_t nHeight = prev_block->nHeight + 1;
            oracle_manager.AddOracleBundleToBlock(block, nHeight);
        }
    }

    block.hashMerkleRoot = BlockMerkleRoot(block);
}

static BlockAssembler::Options ClampOptions(BlockAssembler::Options options)
{
    // Limit weight to between 4K and DEFAULT_BLOCK_MAX_WEIGHT for sanity:
    options.nBlockMaxWeight = std::clamp<size_t>(options.nBlockMaxWeight, 4000, DEFAULT_BLOCK_MAX_WEIGHT);
    return options;
}

BlockAssembler::BlockAssembler(Chainstate& chainstate, const CTxMemPool* mempool, const Options& options)
    : chainparams{chainstate.m_chainman.GetParams()},
      m_mempool{mempool},
      m_chainstate{chainstate},
      m_options{ClampOptions(options)}
{
}

void ApplyArgsManOptions(const ArgsManager& args, BlockAssembler::Options& options)
{
    // Block resource limits
    options.nBlockMaxWeight = args.GetIntArg("-blockmaxweight", options.nBlockMaxWeight);
    if (const auto blockmintxfee{args.GetArg("-blockmintxfee")}) {
        if (const auto parsed{ParseMoney(*blockmintxfee)}) options.blockMinFeeRate = CFeeRate{*parsed};
    }
}
static BlockAssembler::Options ConfiguredOptions()
{
    BlockAssembler::Options options;
    ApplyArgsManOptions(gArgs, options);
    return options;
}

BlockAssembler::BlockAssembler(Chainstate& chainstate, const CTxMemPool* mempool)
    : BlockAssembler(chainstate, mempool, ConfiguredOptions()) {}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}

static bool IsInsufficientCollateralFailure(const BlockValidationState& state, std::string_view what = {})
{
    if (state.GetRejectReason() == "insufficient-collateral") {
        return true;
    }
    return !what.empty() && what.find("insufficient-collateral") != std::string_view::npos;
}

bool BlockAssembler::IsDDTransactionForMiner(const CTransaction& tx) const
{
    if (DigiDollar::HasDigiDollarMarker(tx)) {
        return true;
    }

    // Backstop: if a tx carries DD OP_RETURN metadata but marker bits are missing,
    // skip it from templates instead of risking a miner-side validity failure.
    for (const CTxOut& output : tx.vout) {
        if (output.scriptPubKey.empty() || output.scriptPubKey[0] != OP_RETURN) {
            continue;
        }

        CScript::const_iterator pc = output.scriptPubKey.begin();
        opcodetype opcode;
        std::vector<unsigned char> data;
        if (!output.scriptPubKey.GetOp(pc, opcode) || opcode != OP_RETURN) {
            continue;
        }
        if (output.scriptPubKey.GetOp(pc, opcode, data) &&
            data.size() == 2 && data[0] == 'D' && data[1] == 'D') {
            return true;
        }
    }

    return false;
}

bool BlockAssembler::ValidateDDForBlockInclusion(const CTransaction& tx, const CBlockIndex* pindexPrev)
{
    AssertLockHeld(::cs_main);
    if (!IsDDTransactionForMiner(tx)) {
        return true;
    }

    // If the tx has DD-looking metadata but not the canonical marker bits,
    // keep miner behavior conservative and skip it.
    if (!DigiDollar::HasDigiDollarMarker(tx)) {
        return false;
    }

    if (!DigiDollar::IsDigiDollarEnabled(pindexPrev, m_chainstate.m_chainman)) {
        return false;
    }

    auto txLookup = [this](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        AssertLockHeld(::cs_main);
        const CBlockIndex* pblockindex = m_chainstate.m_chain[coinHeight];
        if (!pblockindex) return false;
        CBlock block;
        if (!m_chainstate.m_blockman.ReadBlockFromDisk(block, *pblockindex)) return false;
        for (const auto& btx : block.vtx) {
            if (btx->GetHash() == txid) {
                tx_out = btx;
                return true;
            }
        }
        return false;
    };

    DigiDollar::ValidationContext dd_context(
        pindexPrev->nHeight + 1,
        GetOraclePriceForTransaction(tx),
        DigiDollar::GetSystemCollateralRatio(),
        chainparams,
        &m_chainstate.CoinsTip(),
        false,
        txLookup,
        m_mempool
    );

    TxValidationState tx_state;
    if (!DigiDollar::ValidateDigiDollarTransaction(tx, dd_context, tx_state)) {
        LogPrint(BCLog::DIGIDOLLAR,
                 "CreateNewBlock(): skipping DD tx %s during package selection: %s\n",
                 tx.GetHash().ToString(), tx_state.GetRejectReason());
        return false;
    }

    return true;
}

bool BlockAssembler::RemoveDDTransactionsFromBlock(const CBlockIndex* pindexPrev)
{
    CBlock& block = pblocktemplate->block;
    if (block.vtx.size() <= 1) {
        return false;
    }

    std::vector<CTransactionRef> kept_vtx;
    std::vector<CAmount> kept_fees;
    std::vector<int64_t> kept_sigops;
    kept_vtx.reserve(block.vtx.size());
    kept_fees.reserve(pblocktemplate->vTxFees.size());
    kept_sigops.reserve(pblocktemplate->vTxSigOpsCost.size());

    kept_vtx.push_back(block.vtx[0]);
    kept_fees.push_back(pblocktemplate->vTxFees[0]);
    kept_sigops.push_back(pblocktemplate->vTxSigOpsCost[0]);

    CAmount kept_fee_total{0};
    size_t removed{0};
    for (size_t i{1}; i < block.vtx.size(); ++i) {
        if (IsDDTransactionForMiner(*block.vtx[i])) {
            ++removed;
            continue;
        }
        kept_vtx.push_back(block.vtx[i]);
        kept_fees.push_back(pblocktemplate->vTxFees[i]);
        kept_sigops.push_back(pblocktemplate->vTxSigOpsCost[i]);
        kept_fee_total += pblocktemplate->vTxFees[i];
    }

    if (removed == 0) {
        return false;
    }

    block.vtx = std::move(kept_vtx);
    pblocktemplate->vTxFees = std::move(kept_fees);
    pblocktemplate->vTxSigOpsCost = std::move(kept_sigops);

    nBlockTx = block.vtx.size() - 1;
    nFees = kept_fee_total;

    CMutableTransaction coinbase_tx{*block.vtx[0]};
    if (coinbase_tx.vout.empty()) {
        return false;
    }
    coinbase_tx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    block.vtx[0] = MakeTransactionRef(std::move(coinbase_tx));

    // Rebuild witness commitment/oracle bundle and merkle root after removing txs.
    CMutableTransaction stripped_coinbase{*block.vtx[0]};
    stripped_coinbase.vout.erase(
        std::remove_if(stripped_coinbase.vout.begin(), stripped_coinbase.vout.end(),
            [](const CTxOut& txout) { return txout.scriptPubKey.IsUnspendable(); }),
        stripped_coinbase.vout.end());
    block.vtx[0] = MakeTransactionRef(std::move(stripped_coinbase));
    pblocktemplate->vchCoinbaseCommitment = m_chainstate.m_chainman.GenerateCoinbaseCommitment(block, pindexPrev);

    if (DigiDollar::IsDigiDollarEnabled(pindexPrev, m_chainstate.m_chainman)) {
        OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
        if (!oracle_manager.AddOracleBundleToBlock(block, nHeight)) {
            LogPrintf("CreateNewBlock(): Warning - Failed to add oracle bundle during DD retry for block %d\n", nHeight);
        }
    }

    block.hashMerkleRoot = BlockMerkleRoot(block);

    pblocktemplate->vTxFees[0] = -nFees;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*block.vtx[0]);

    nBlockWeight = GetBlockWeight(block);
    nBlockSigOpsCost = 0;
    for (int64_t sigops : pblocktemplate->vTxSigOpsCost) {
        if (sigops > 0) {
            nBlockSigOpsCost += sigops;
        }
    }

    m_last_block_num_txs = nBlockTx;
    m_last_block_weight = nBlockWeight;

    return true;
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, int algo)
{
    const auto time_start{SteadyClock::now()};

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if (!pblocktemplate.get()) {
        return nullptr;
    }
    CBlock* const pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK(::cs_main);
    CBlockIndex* pindexPrev = m_chainstate.m_chain.Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;
    
    // DigiByte: Check if algorithm is active before creating block
    if (!IsAlgoActive(pindexPrev, chainparams.GetConsensus(), algo))
        throw std::runtime_error(strprintf("Algorithm '%s' is not currently active.", GetAlgoName(algo).c_str()));
    
    pblock->nVersion = m_chainstate.m_chainman.m_versionbitscache.ComputeBlockVersion(pindexPrev, chainparams.GetConsensus(), algo);
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand()) {
        pblock->nVersion = gArgs.GetIntArg("-blockversion", pblock->nVersion);
    }

    pblock->nTime = TicksSinceEpoch<std::chrono::seconds>(GetAdjustedTime());
    m_lock_time_cutoff = pindexPrev->GetMedianTimePast();

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    if (m_mempool) {
        LOCK(m_mempool->cs);
        addPackageTxs(*m_mempool, nPackagesSelected, nDescendantsUpdated);
    }

    const auto time_1{SteadyClock::now()};

    m_last_block_num_txs = nBlockTx;
    m_last_block_weight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
    coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;
    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    pblocktemplate->vchCoinbaseCommitment = m_chainstate.m_chainman.GenerateCoinbaseCommitment(*pblock, pindexPrev);
    pblocktemplate->vTxFees[0] = -nFees;

    // Add oracle bundle to block (after DigiDollar activation)
    if (DigiDollar::IsDigiDollarEnabled(pindexPrev, m_chainstate.m_chainman)) {
        OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
        if (!oracle_manager.AddOracleBundleToBlock(*pblock, nHeight)) {
            LogPrintf("CreateNewBlock(): Warning - Failed to add oracle bundle to block %d\n", nHeight);
            // Continue with block creation even if oracle bundle fails (graceful degradation)
        }
    }

    LogPrintf("CreateNewBlock(): block weight: %u txs: %u fees: %ld sigops %d\n", GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost);

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev, algo);
    pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus(), algo);
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    BlockValidationState state;
    if (m_options.test_block_validity) {
        if (m_options.on_before_test_block_validity) {
            m_options.on_before_test_block_validity();
        }

        auto run_block_validity = [&](BlockValidationState& check_state) {
            return TestBlockValidity(check_state, chainparams, m_chainstate, *pblock, pindexPrev,
                                     GetAdjustedTime, /*fCheckPOW=*/false, /*fCheckMerkleRoot=*/false);
        };

        bool needs_dd_retry{false};
        try {
            if (!run_block_validity(state)) {
                needs_dd_retry = IsInsufficientCollateralFailure(state);
                if (!needs_dd_retry) {
                    throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, state.ToString()));
                }
            }
        } catch (const std::exception& e) {
            needs_dd_retry = IsInsufficientCollateralFailure(state, e.what());
            if (!needs_dd_retry) {
                throw;
            }
        }

        if (needs_dd_retry) {
            LogPrintf("CreateNewBlock(): DD collateral failure detected, retrying block assembly without DD transactions at height %d\n", nHeight);
            if (!RemoveDDTransactionsFromBlock(pindexPrev)) {
                throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, state.ToString()));
            }

            BlockValidationState retry_state;
            if (!run_block_validity(retry_state)) {
                throw std::runtime_error(strprintf("%s: TestBlockValidity failed after DD retry: %s", __func__, retry_state.ToString()));
            }
        }
    }
    const auto time_2{SteadyClock::now()};

    LogPrint(BCLog::BENCH, "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), validity: %.2fms (total %.2fms)\n",
             Ticks<MillisecondsDouble>(time_1 - time_start), nPackagesSelected, nDescendantsUpdated,
             Ticks<MillisecondsDouble>(time_2 - time_1),
             Ticks<MillisecondsDouble>(time_2 - time_start));

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        } else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= m_options.nBlockMaxWeight) {
        return false;
    }
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST) {
        return false;
    }
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package) const
{
    for (CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, m_lock_time_cutoff)) {
            return false;
        }
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblocktemplate->block.vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee rate %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

/** Add descendants of given transactions to mapModifiedTx with ancestor
 * state updated assuming given transactions are inBlock. Returns number
 * of updated descendants. */
static int UpdatePackagesForAdded(const CTxMemPool& mempool,
                                  const CTxMemPool::setEntries& alreadyAdded,
                                  indexed_modified_transaction_set& mapModifiedTx) EXCLUSIVE_LOCKS_REQUIRED(mempool.cs)
{
    AssertLockHeld(mempool.cs);

    int nDescendantsUpdated = 0;
    for (CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc)) {
                continue;
            }
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                mit = mapModifiedTx.insert(modEntry).first;
            }
            mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
        }
    }
    return nDescendantsUpdated;
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(const CTxMemPool& mempool, int& nPackagesSelected, int& nDescendantsUpdated)
{
    AssertLockHeld(mempool.cs);

    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty()) {
        // First try to find a new transaction in mapTx to evaluate.
        //
        // Skip entries in mapTx that are already in a block or are present
        // in mapModifiedTx (which implies that the mapTx ancestor state is
        // stale due to ancestor inclusion in the block)
        // Also skip transactions that we've already failed to add. This can happen if
        // we consider a transaction in mapModifiedTx and it fails: we can then
        // potentially consider it again while walking mapTx.  It's currently
        // guaranteed to fail again, but as a belt-and-suspenders check we put it in
        // failedTx and avoid re-evaluation, since the re-evaluation would be using
        // cached size/sigops/fee values that are not actually correct.
        /** Return true if given transaction from mapTx has already been evaluated,
         * or if the transaction's cached data in mapTx is incorrect. */
        if (mi != mempool.mapTx.get<ancestor_score>().end()) {
            auto it = mempool.mapTx.project<0>(mi);
            assert(it != mempool.mapTx.end());
            if (mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it)) {
                ++mi;
                continue;
            }
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == m_mempool->mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = m_mempool->mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < m_options.blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    m_options.nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        auto ancestors{mempool.AssumeCalculateMemPoolAncestors(__func__, *iter, CTxMemPool::Limits::NoLimits(), /*fSearchForParents=*/false)};

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, sortedEntries);
        const CBlockIndex* const pindexPrev = m_chainstate.m_chain.Tip();
        assert(pindexPrev != nullptr);
        CTxMemPool::setEntries added_ancestors;

        for (size_t i = 0; i < sortedEntries.size(); ++i) {
            const CTransaction& tx = sortedEntries[i]->GetTx();
            if (IsDDTransactionForMiner(tx) && !ValidateDDForBlockInclusion(tx, pindexPrev)) {
                failedTx.insert(sortedEntries[i]);
                continue;
            }
            AddToBlock(sortedEntries[i]);
            added_ancestors.insert(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        // If the trigger tx came from mapModifiedTx but was not added (DD
        // validation failure), erase modit so the outer loop progresses.
        // Without this, the same mapModifiedTx entry is reselected forever.
        if (fUsingModified && !added_ancestors.count(iter)) {
            mapModifiedTx.get<ancestor_score>().erase(modit);
        }

        if (added_ancestors.empty()) {
            continue;
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(mempool, added_ancestors, mapModifiedTx);
    }
}
void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce));
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}
} // namespace node
