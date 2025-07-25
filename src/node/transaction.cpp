// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <consensus/validation.h>
#include <index/txindex.h>
#include <net.h>
#include <net_processing.h>
#include <node/blockstorage.h>
#include <node/context.h>
#include <txmempool.h>
#include <validation.h>
#include <validationinterface.h>
#include <node/transaction.h>
#include <random.h>
#include <logging.h>
#include <common/args.h>

#include <future>

namespace node {
static TransactionError HandleATMPError(const TxValidationState& state, std::string& err_string_out)
{
    err_string_out = state.ToString();
    if (state.IsInvalid()) {
        if (state.GetResult() == TxValidationResult::TX_MISSING_INPUTS) {
            return TransactionError::MISSING_INPUTS;
        }
        return TransactionError::MEMPOOL_REJECTED;
    } else {
        return TransactionError::MEMPOOL_ERROR;
    }
}

TransactionError BroadcastTransaction(NodeContext& node, const CTransactionRef tx, std::string& err_string, const CAmount& max_tx_fee, bool relay, bool wait_callback)
{
    // BroadcastTransaction can be called by either sendrawtransaction RPC or the wallet.
    // chainman, mempool and peerman are initialized before the RPC server and wallet are started
    // and reset after the RPC sever and wallet are stopped.
    assert(node.chainman);
    assert(node.mempool);
    assert(node.peerman);

    std::promise<void> promise;
    uint256 txid = tx->GetHash();
    uint256 wtxid = tx->GetWitnessHash();
    bool callback_set = false;

    {
        LOCK(cs_main);

        // If the transaction is already confirmed in the chain, don't do anything
        // and return early.
        CCoinsViewCache &view = node.chainman->ActiveChainstate().CoinsTip();
        for (size_t o = 0; o < tx->vout.size(); o++) {
            const Coin& existingCoin = view.AccessCoin(COutPoint(txid, o));
            // IsSpent doesn't mean the coin is spent, it means the output doesn't exist.
            // So if the output does exist, then this transaction exists in the chain.
            if (!existingCoin.IsSpent()) return TransactionError::ALREADY_IN_CHAIN;
        }

        if (auto mempool_tx = node.mempool->get(txid); mempool_tx) {
            // There's already a transaction in the mempool with this txid. Don't
            // try to submit this transaction to the mempool (since it'll be
            // rejected as a TX_CONFLICT), but do attempt to reannounce the mempool
            // transaction if relay=true.
            //
            // The mempool transaction may have the same or different witness (and
            // wtxid) as this transaction. Use the mempool's wtxid for reannouncement.
            wtxid = mempool_tx->GetWitnessHash();
        } else {
            // Transaction is not already in the mempool.
            if (max_tx_fee > 0) {
                // First, call ATMP with test_accept and check the fee. If ATMP
                // fails here, return error immediately.
                const MempoolAcceptResult result = AcceptToMemoryPool(node.chainman->ActiveChainstate(), *node.mempool, tx, /*bypass_limits=*/false, /*test_accept=*/true);
                if (result.m_result_type != MempoolAcceptResult::ResultType::VALID) {
                    return HandleATMPError(result.m_state, err_string);
                } else if (result.m_base_fees.value() > max_tx_fee) {
                    return TransactionError::MAX_FEE_EXCEEDED;
                } else {
                    // Test acceptance to stempool for consistency with Dandelion routing
                    if (gArgs.GetBoolArg("-dandelion", DEFAULT_DANDELION)) {
                        AcceptToMemoryPool(node.chainman->ActiveChainstate(), *node.stempool, tx, /*bypass_limits=*/false, /*test_accept=*/true);
                    }
                }
            }
            // Try to submit the transaction to the stempool only (if dandelion is enabled);
            if (gArgs.GetBoolArg("-dandelion", DEFAULT_DANDELION)) {
                // Submit to stempool for Dandelion routing
                LogPrintf("BroadcastTransaction: Dandelion enabled, submitting transaction %s to stempool\n", txid.ToString());
                const MempoolAcceptResult result = AcceptToMemoryPool(node.chainman->ActiveChainstate(), *node.stempool, tx, /*bypass_limits=*/false);
                if (result.m_result_type != MempoolAcceptResult::ResultType::VALID) {
                    LogPrintf("BroadcastTransaction: Failed to accept transaction %s to stempool: %s\n", 
                             txid.ToString(), result.m_state.ToString());
                    return HandleATMPError(result.m_state, err_string);
                }
                LogPrintf("BroadcastTransaction: Successfully accepted transaction %s to stempool (poolsz %u txn, %u kB)\n",
                         txid.ToString(), node.stempool->size(), node.stempool->DynamicMemoryUsage() / 1000);
                // Don't notify wallet here - stempool transactions should remain separate until fluffed
            } else {
                const MempoolAcceptResult result = node.chainman->ProcessTransaction(tx, /*test_accept=*/ false);
                if (result.m_result_type != MempoolAcceptResult::ResultType::VALID) {
                    return HandleATMPError(result.m_state, err_string);
                }
            }

            // Transaction was accepted to the mempool.

            if (relay && !gArgs.GetBoolArg("-dandelion", DEFAULT_DANDELION)) {
                // the mempool tracks locally submitted transactions to make a
                // best-effort of initial broadcast
                // Only add to unbroadcast if not using Dandelion
                node.mempool->AddUnbroadcastTx(txid);
            }

            if (wait_callback) {
                // For transactions broadcast from outside the wallet, make sure
                // that the wallet has been notified of the transaction before
                // continuing.
                //
                // This prevents a race where a user might call sendrawtransaction
                // with a transaction to/from their wallet, immediately call some
                // wallet RPC, and get a stale result because callbacks have not
                // yet been processed.
                CallFunctionInValidationInterfaceQueue([&promise] {
                    promise.set_value();
                });
                callback_set = true;
            }
        }
    } // cs_main

    if (callback_set) {
        // Wait until Validation Interface clients have been notified of the
        // transaction entering the mempool.
        promise.get_future().wait();
    }

    if (relay) {
        if (gArgs.GetBoolArg("-dandelion", DEFAULT_DANDELION)) {
            auto current_time = GetTime<std::chrono::milliseconds>();
            std::chrono::microseconds nEmbargo = DANDELION_EMBARGO_MINIMUM + PoissonNextSend(current_time, DANDELION_EMBARGO_AVG_ADD);
            node.connman->insertDandelionEmbargo(txid, nEmbargo);
            auto embargo_timeout = std::chrono::duration_cast<std::chrono::seconds>(nEmbargo - current_time).count();
            LogPrint(BCLog::DANDELION, "dandeliontx %s embargoed for %d seconds\n", txid.ToString(), embargo_timeout);
            CInv embargoTx(MSG_DANDELION_TX, txid);
            node.connman->localDandelionDestinationPushInventory(embargoTx);
            
            // Push the transaction immediately to the Dandelion destination
            // This ensures it actually propagates during the stem phase
            LogPrintf("BroadcastTransaction: Calling PushDandelionTransaction for %s\n", txid.ToString());
            node.peerman->PushDandelionTransaction(txid);
            
            return TransactionError::OK;
        }
        node.peerman->RelayTransaction(txid, wtxid);
    } else if (gArgs.GetBoolArg("-dandelion", DEFAULT_DANDELION)) {
        // For Dandelion, we need to set up the embargo and routing even when relay=false
        // This happens during wallet startup when resubmitting transactions
        auto current_time = GetTime<std::chrono::milliseconds>();
        std::chrono::microseconds nEmbargo = DANDELION_EMBARGO_MINIMUM + PoissonNextSend(current_time, DANDELION_EMBARGO_AVG_ADD);
        node.connman->insertDandelionEmbargo(txid, nEmbargo);
        auto embargo_timeout = std::chrono::duration_cast<std::chrono::seconds>(nEmbargo - current_time).count();
        LogPrint(BCLog::DANDELION, "dandeliontx %s embargoed for %d seconds (relay=false)\n", txid.ToString(), embargo_timeout);
        CInv embargoTx(MSG_DANDELION_TX, txid);
        node.connman->localDandelionDestinationPushInventory(embargoTx);
        
        LogPrintf("BroadcastTransaction: Calling PushDandelionTransaction for %s (relay=false)\n", txid.ToString());
        node.peerman->PushDandelionTransaction(txid);
    }

    return TransactionError::OK;
}

CTransactionRef GetTransaction(const CBlockIndex* const block_index, const CTxMemPool* const mempool, const uint256& hash, uint256& hashBlock, const BlockManager& blockman)
{
    if (mempool && !block_index) {
        CTransactionRef ptx = mempool->get(hash);
        if (ptx) return ptx;
    }
    if (g_txindex) {
        CTransactionRef tx;
        uint256 block_hash;
        if (g_txindex->FindTx(hash, block_hash, tx)) {
            if (!block_index || block_index->GetBlockHash() == block_hash) {
                // Don't return the transaction if the provided block hash doesn't match.
                // The case where a transaction appears in multiple blocks (e.g. reorgs or
                // BIP30) is handled by the block lookup below.
                hashBlock = block_hash;
                return tx;
            }
        }
    }
    if (block_index) {
        CBlock block;
        if (blockman.ReadBlockFromDisk(block, *block_index)) {
            for (const auto& tx : block.vtx) {
                if (tx->GetHash() == hash) {
                    hashBlock = block_index->GetBlockHash();
                    return tx;
                }
            }
        }
    }
    return nullptr;
}
} // namespace node
