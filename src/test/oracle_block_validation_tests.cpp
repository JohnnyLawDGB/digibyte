// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Oracle Block Validation Tests (Week 4: Block Validation Integration)
 *
 * RED PHASE (TDD) - These tests are EXPECTED TO FAIL until implementation is complete.
 *
 * Tests cover:
 * 1. CheckBlock() oracle bundle validation (3 tests)
 * 2. ContextualCheckBlock() timestamp validation (2 tests)
 * 3. ConnectBlock() oracle cache updates (3 tests)
 *
 * Specification: DIGIDOLLAR_ORACLE_PHASE_ONE_SPEC.md Section 5.4
 */

#include <boost/test/unit_test.hpp>
#include <logging.h>
#include <util/strencodings.h>

#include <chainparams.h>
#include <consensus/validation.h>
#include <key.h>
#include <node/miner.h>
#include <oracle/bundle_manager.h>
#include <primitives/block.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <random.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/time.h>
#include <validation.h>

#include <memory>

BOOST_FIXTURE_TEST_SUITE(oracle_block_validation_tests, TestChain100Setup)

//
// HELPER FUNCTIONS
//

/**
 * Create a valid oracle price message with Schnorr signature
 */
static COraclePriceMessage CreateValidOracleMessage(const CKey& oracle_key, uint64_t price_micro_usd, int64_t timestamp, int32_t block_height)
{
    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = price_micro_usd;
    msg.timestamp = timestamp;
    msg.block_height = block_height;
    msg.nonce = GetRand(UINT64_MAX);

    // Set Schnorr public key
    msg.oracle_pubkey = XOnlyPubKey(oracle_key.GetPubKey());

    // Sign with Schnorr signature (BIP-340)
    BOOST_REQUIRE(msg.Sign(oracle_key));

    return msg;
}

/**
 * Create a valid oracle bundle (Phase One: 1-of-1 consensus)
 */
static COracleBundle CreateValidOracleBundle(const CKey& oracle_key, uint64_t price_micro_usd, int64_t timestamp, int32_t block_height)
{
    COraclePriceMessage msg = CreateValidOracleMessage(oracle_key, price_micro_usd, timestamp, block_height);

    COracleBundle bundle;
    bundle.messages.push_back(msg);
    bundle.epoch = GetCurrentEpoch(block_height);

    return bundle;
}

/**
 * Add oracle bundle to coinbase transaction OP_RETURN output
 */
static void AddOracleBundleToCoinbase(CMutableTransaction& coinbase, const COracleBundle& bundle)
{
    // Serialize oracle bundle
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << bundle;
    std::vector<unsigned char> bundle_data;
    bundle_data.reserve(ss.size());
    for (auto it = ss.begin(); it != ss.end(); ++it) {
        bundle_data.push_back(static_cast<unsigned char>(*it));
    }

    // Create OP_RETURN script with oracle data
    CScript oracle_script;
    oracle_script << OP_RETURN << bundle_data;

    // Add as second output (after coinbase reward)
    CTxOut oracle_output;
    oracle_output.nValue = 0;
    oracle_output.scriptPubKey = oracle_script;

    coinbase.vout.push_back(oracle_output);
}

/**
 * Create a block with oracle bundle in coinbase
 */
static CBlock CreateBlockWithOracleBundle(const CKey& oracle_key, uint64_t price_micro_usd, int64_t timestamp, int32_t block_height, const CScript& coinbase_script_sig)
{
    CBlock block;
    block.nVersion = 1;
    block.nTime = timestamp;
    block.hashPrevBlock.SetNull();
    block.nBits = 0x207fffff; // Regtest difficulty
    block.nNonce = 0;

    // Create coinbase transaction
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = coinbase_script_sig;

    // Coinbase reward output
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 72000 * COIN; // DigiByte block reward
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    // Add oracle bundle to coinbase
    COracleBundle bundle = CreateValidOracleBundle(oracle_key, price_micro_usd, timestamp, block_height);
    AddOracleBundleToCoinbase(coinbase, bundle);

    // Add coinbase to block
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));

    return block;
}

//
// CATEGORY 1: CheckBlock() TESTS (3 tests)
//

/**
 * RED TEST 1: CheckBlock() accepts valid oracle bundle
 *
 * EXPECTED TO FAIL:
 * - CheckBlock() does not yet validate oracle bundles
 * - OracleDataValidator::ValidateBlockOracleData() may not be implemented
 * - Oracle bundle extraction from coinbase OP_RETURN may not work
 */
BOOST_AUTO_TEST_CASE(checkblock_accepts_valid_oracle_bundle)
{
    // Generate oracle keypair
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    // Create block with valid oracle bundle
    uint64_t price = 50000; // $0.05 in micro-USD
    int64_t timestamp = GetTime();
    int32_t block_height = 101;
    CScript coinbase_script_sig = CScript() << block_height << OP_0;

    CBlock block = CreateBlockWithOracleBundle(oracle_key, price, timestamp, block_height, coinbase_script_sig);

    // Test CheckBlock validation
    BlockValidationState state;
    const Consensus::Params& params = Params().GetConsensus();

    // EXPECTED FAILURE: CheckBlock does not yet validate oracle data properly
    BOOST_CHECK_MESSAGE(
        CheckBlock(block, state, params, false, false),
        "CheckBlock should accept block with valid oracle bundle"
    );

    if (!state.IsValid()) {
        LogPrintf("TEST FAILURE (EXPECTED): CheckBlock rejected valid oracle bundle: %s\n", state.GetRejectReason());
    }
}

/**
 * RED TEST 2: CheckBlock() rejects invalid Schnorr signature
 *
 * EXPECTED TO FAIL:
 * - CheckBlock() may not verify Schnorr signatures yet
 * - Signature validation may not be integrated
 * - Error handling for bad signatures may be incomplete
 */
BOOST_AUTO_TEST_CASE(checkblock_rejects_invalid_bundle_signature)
{
    // Generate oracle keypair
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    // Create oracle message with INVALID signature
    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;
    msg.timestamp = GetTime();
    msg.block_height = 101;
    msg.nonce = GetRand(UINT64_MAX);
    msg.oracle_pubkey = XOnlyPubKey(oracle_key.GetPubKey());

    // DON'T sign - leave signature invalid (or corrupt it)
    msg.schnorr_sig.resize(64, 0x00); // Invalid signature (all zeros)

    // Create bundle with invalid message
    COracleBundle bundle;
    bundle.messages.push_back(msg);
    bundle.epoch = GetCurrentEpoch(101);

    // Create block
    CBlock block;
    block.nVersion = 1;
    block.nTime = GetTime();
    block.hashPrevBlock.SetNull();
    block.nBits = 0x207fffff;
    block.nNonce = 0;

    // Create coinbase with bad oracle bundle
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << 101 << OP_0;
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 72000 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    AddOracleBundleToCoinbase(coinbase, bundle);
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));

    // Test CheckBlock REJECTS invalid signature
    BlockValidationState state;
    const Consensus::Params& params = Params().GetConsensus();

    // EXPECTED FAILURE: CheckBlock may not validate signatures yet
    BOOST_CHECK_MESSAGE(
        !CheckBlock(block, state, params, false, false),
        "CheckBlock should reject block with invalid oracle signature"
    );

    if (state.IsValid()) {
        LogPrintf("TEST FAILURE (EXPECTED): CheckBlock accepted invalid oracle signature\n");
    } else {
        // Check for expected rejection reason
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-oracle-bundle-signature");
    }
}

/**
 * RED TEST 3: CheckBlock() rejects bundle with wrong consensus (not 1-of-1)
 *
 * EXPECTED TO FAIL:
 * - CheckBlock() may not enforce Phase One 1-of-1 consensus requirement
 * - Bundle consensus validation may not be implemented
 */
BOOST_AUTO_TEST_CASE(checkblock_rejects_bundle_wrong_consensus)
{
    // Generate two oracle keypairs
    CKey oracle_key1, oracle_key2;
    oracle_key1.MakeNewKey(true);
    oracle_key2.MakeNewKey(true);

    int64_t timestamp = GetTime();
    int32_t block_height = 101;

    // Create bundle with TWO messages (violates Phase One 1-of-1)
    COraclePriceMessage msg1 = CreateValidOracleMessage(oracle_key1, 50000, timestamp, block_height);
    COraclePriceMessage msg2 = CreateValidOracleMessage(oracle_key2, 50100, timestamp, block_height);

    COracleBundle bundle;
    bundle.messages.push_back(msg1);
    bundle.messages.push_back(msg2); // INVALID: Phase One requires exactly 1 message
    bundle.epoch = GetCurrentEpoch(block_height);

    // Create block
    CBlock block;
    block.nVersion = 1;
    block.nTime = timestamp;
    block.hashPrevBlock.SetNull();
    block.nBits = 0x207fffff;
    block.nNonce = 0;

    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << block_height << OP_0;
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 72000 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    AddOracleBundleToCoinbase(coinbase, bundle);
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));

    // Test CheckBlock REJECTS wrong consensus
    BlockValidationState state;
    const Consensus::Params& params = Params().GetConsensus();

    // EXPECTED FAILURE: CheckBlock may not enforce 1-of-1 consensus yet
    BOOST_CHECK_MESSAGE(
        !CheckBlock(block, state, params, false, false),
        "CheckBlock should reject block with multiple oracle messages in Phase One"
    );

    if (state.IsValid()) {
        LogPrintf("TEST FAILURE (EXPECTED): CheckBlock accepted bundle with %d messages (should require exactly 1)\n", bundle.messages.size());
    } else {
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-oracle-consensus");
    }
}

//
// CATEGORY 2: ContextualCheckBlock() TESTS (2 tests)
//

/**
 * RED TEST 4: ContextualCheckBlock() validates timestamp within range
 *
 * EXPECTED TO FAIL:
 * - ContextualCheckBlock() may not exist or not check oracle timestamps
 * - Timestamp validation against block time may not be implemented
 */
BOOST_AUTO_TEST_CASE(contextual_checkblock_timestamp_validation)
{
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    int64_t block_time = GetTime();
    int64_t oracle_timestamp = block_time - 1800; // 30 minutes old (valid: < 1 hour)
    int32_t block_height = 101;

    CBlock block = CreateBlockWithOracleBundle(
        oracle_key,
        50000,
        oracle_timestamp,
        block_height,
        CScript() << block_height << OP_0
    );
    block.nTime = block_time;

    // Create mock previous block index
    CBlockIndex prev_index;
    prev_index.nHeight = block_height - 1;
    prev_index.nTime = block_time - 15; // DigiByte: 15 second blocks

    BlockValidationState state;
    const Consensus::Params& params = Params().GetConsensus();

    // First validate with CheckBlock
    BOOST_REQUIRE(CheckBlock(block, state, params, false, false));

    // EXPECTED FAILURE: ContextualCheckBlock may not exist or validate oracle timestamps
    // Note: This is a static function in validation.cpp, may need to be exposed
    // For now, we test through OracleDataValidator::ValidateBlockOracleData

    bool valid = OracleDataValidator::ValidateBlockOracleData(block, &prev_index, params);

    BOOST_CHECK_MESSAGE(
        valid,
        "Oracle bundle with timestamp within 1 hour of block time should be accepted"
    );

    if (!valid) {
        LogPrintf("TEST FAILURE (EXPECTED): ContextualCheckBlock rejected oracle timestamp within valid range\n");
    }
}

/**
 * RED TEST 5: ContextualCheckBlock() rejects old oracle bundle
 *
 * EXPECTED TO FAIL:
 * - Timestamp validation may not enforce 1-hour maximum age
 * - ContextualCheckBlock oracle integration may be incomplete
 */
BOOST_AUTO_TEST_CASE(contextual_checkblock_rejects_old_bundle)
{
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    int64_t block_time = GetTime();
    int64_t oracle_timestamp = block_time - 7200; // 2 hours old (invalid: > 1 hour)
    int32_t block_height = 101;

    CBlock block = CreateBlockWithOracleBundle(
        oracle_key,
        50000,
        oracle_timestamp,
        block_height,
        CScript() << block_height << OP_0
    );
    block.nTime = block_time;

    CBlockIndex prev_index;
    prev_index.nHeight = block_height - 1;
    prev_index.nTime = block_time - 15;

    BlockValidationState state;
    const Consensus::Params& params = Params().GetConsensus();

    // EXPECTED FAILURE: ContextualCheckBlock may not reject old bundles
    bool valid = OracleDataValidator::ValidateBlockOracleData(block, &prev_index, params);

    BOOST_CHECK_MESSAGE(
        !valid,
        "Oracle bundle older than 1 hour should be rejected"
    );

    if (valid) {
        LogPrintf("TEST FAILURE (EXPECTED): ContextualCheckBlock accepted oracle bundle older than 1 hour\n");
    }
}

//
// CATEGORY 3: ConnectBlock() TESTS (3 tests)
//

/**
 * RED TEST 6: ConnectBlock() updates oracle price cache
 *
 * EXPECTED TO FAIL:
 * - ConnectBlock() may not extract oracle bundles yet
 * - Oracle cache update integration may be missing
 * - OracleBundleManager cache may not be updated on block connect
 */
BOOST_AUTO_TEST_CASE(connectblock_updates_oracle_cache)
{
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    uint64_t price = 50000; // $0.05
    int64_t timestamp = GetTime();
    int32_t block_height = m_node.chainman->ActiveChain().Height() + 1;

    // Create valid block with oracle bundle
    CBlock block = CreateBlockWithOracleBundle(
        oracle_key,
        price,
        timestamp,
        block_height,
        CScript() << block_height << OP_0
    );
    block.nTime = timestamp;
    block.hashPrevBlock = m_node.chainman->ActiveChain().Tip()->GetBlockHash();

    // Mine the block (find valid nonce)
    const Consensus::Params& params = Params().GetConsensus();
    while (!CheckProofOfWork(block.GetHash(), block.nBits, params)) {
        ++block.nNonce;
    }

    // Get oracle price before connect
    CAmount price_before = OracleIntegration::GetCurrentOraclePrice();

    // Connect block
    BlockValidationState state;
    CBlockIndex* pindex = nullptr;

    // EXPECTED FAILURE: ConnectBlock may not update oracle cache
    {
        LOCK(cs_main);
        CBlockIndex indexDummy(block);
        indexDummy.nHeight = block_height;
        indexDummy.pprev = m_node.chainman->ActiveChain().Tip();
        pindex = &indexDummy;

        CCoinsViewCache view(&m_node.chainman->ActiveChainstate().CoinsTip());

        bool connected = m_node.chainman->ActiveChainstate().ConnectBlock(
            block, state, pindex, view, false
        );

        BOOST_CHECK_MESSAGE(connected, "ConnectBlock should succeed");
    }

    // Get oracle price after connect
    CAmount price_after = OracleIntegration::GetCurrentOraclePrice();

    // EXPECTED FAILURE: Price cache may not be updated
    BOOST_CHECK_MESSAGE(
        price_after == static_cast<CAmount>(price),
        strprintf("Oracle cache should be updated to %lld micro-USD, got %lld", price, price_after)
    );

    if (price_after != static_cast<CAmount>(price)) {
        LogPrintf("TEST FAILURE (EXPECTED): Oracle cache not updated on ConnectBlock (before=%lld, after=%lld, expected=%lld)\n",
                  price_before, price_after, price);
    }
}

/**
 * RED TEST 7: GetOraclePriceForHeight() works after ConnectBlock()
 *
 * EXPECTED TO FAIL:
 * - GetOraclePriceForHeight() may not be implemented
 * - Height-indexed price cache may not exist
 * - Oracle bundle may not be stored per height
 */
BOOST_AUTO_TEST_CASE(connectblock_oracle_price_available)
{
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    uint64_t price = 55000; // $0.055
    int64_t timestamp = GetTime();
    int32_t block_height = m_node.chainman->ActiveChain().Height() + 1;

    CBlock block = CreateBlockWithOracleBundle(
        oracle_key,
        price,
        timestamp,
        block_height,
        CScript() << block_height << OP_0
    );
    block.nTime = timestamp;
    block.hashPrevBlock = m_node.chainman->ActiveChain().Tip()->GetBlockHash();

    const Consensus::Params& params = Params().GetConsensus();
    while (!CheckProofOfWork(block.GetHash(), block.nBits, params)) {
        ++block.nNonce;
    }

    // Connect block
    BlockValidationState state;
    {
        LOCK(cs_main);
        CBlockIndex indexDummy(block);
        indexDummy.nHeight = block_height;
        indexDummy.pprev = m_node.chainman->ActiveChain().Tip();

        CCoinsViewCache view(&m_node.chainman->ActiveChainstate().CoinsTip());

        BOOST_REQUIRE(m_node.chainman->ActiveChainstate().ConnectBlock(
            block, state, &indexDummy, view, false
        ));
    }

    // EXPECTED FAILURE: GetOracleBundleForHeight may not be implemented
    COracleBundle retrieved_bundle = OracleIntegration::GetOracleBundleForHeight(block_height);

    BOOST_CHECK_MESSAGE(
        !retrieved_bundle.messages.empty(),
        "Oracle bundle should be retrievable by height after ConnectBlock"
    );

    if (!retrieved_bundle.messages.empty()) {
        uint64_t retrieved_price = retrieved_bundle.messages[0].price_micro_usd;
        BOOST_CHECK_EQUAL(retrieved_price, price);
    } else {
        LogPrintf("TEST FAILURE (EXPECTED): Oracle bundle not retrievable by height after ConnectBlock\n");
    }
}

/**
 * RED TEST 8: DisconnectBlock() reverts oracle cache
 *
 * EXPECTED TO FAIL:
 * - DisconnectBlock() may not revert oracle cache
 * - Oracle cache rollback may not be implemented
 * - Previous oracle state may not be restored on reorg
 */
BOOST_AUTO_TEST_CASE(connectblock_disconnect_reverts_cache)
{
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    // Get initial oracle price
    CAmount initial_price = OracleIntegration::GetCurrentOraclePrice();

    uint64_t new_price = 60000; // $0.06
    int64_t timestamp = GetTime();
    int32_t block_height = m_node.chainman->ActiveChain().Height() + 1;

    CBlock block = CreateBlockWithOracleBundle(
        oracle_key,
        new_price,
        timestamp,
        block_height,
        CScript() << block_height << OP_0
    );
    block.nTime = timestamp;
    block.hashPrevBlock = m_node.chainman->ActiveChain().Tip()->GetBlockHash();

    const Consensus::Params& params = Params().GetConsensus();
    while (!CheckProofOfWork(block.GetHash(), block.nBits, params)) {
        ++block.nNonce;
    }

    // Connect block
    BlockValidationState state;
    CBlockIndex* pindex = nullptr;
    {
        LOCK(cs_main);
        CBlockIndex indexDummy(block);
        indexDummy.nHeight = block_height;
        indexDummy.pprev = m_node.chainman->ActiveChain().Tip();
        pindex = &indexDummy;

        CCoinsViewCache view(&m_node.chainman->ActiveChainstate().CoinsTip());

        BOOST_REQUIRE(m_node.chainman->ActiveChainstate().ConnectBlock(
            block, state, pindex, view, false
        ));

        // Verify price updated
        CAmount after_connect = OracleIntegration::GetCurrentOraclePrice();
        BOOST_CHECK_EQUAL(after_connect, static_cast<CAmount>(new_price));

        // Now disconnect block
        DisconnectResult disconnect_result = m_node.chainman->ActiveChainstate().DisconnectBlock(
            block, pindex, view
        );
        BOOST_REQUIRE(disconnect_result == DISCONNECT_OK);
    }

    // EXPECTED FAILURE: DisconnectBlock may not revert oracle cache
    CAmount after_disconnect = OracleIntegration::GetCurrentOraclePrice();

    BOOST_CHECK_MESSAGE(
        after_disconnect == initial_price,
        strprintf("Oracle cache should revert to initial price %lld after disconnect, got %lld",
                  initial_price, after_disconnect)
    );

    if (after_disconnect != initial_price) {
        LogPrintf("TEST FAILURE (EXPECTED): Oracle cache not reverted on DisconnectBlock (initial=%lld, after_disconnect=%lld)\n",
                  initial_price, after_disconnect);
    }
}

BOOST_AUTO_TEST_SUITE_END()
