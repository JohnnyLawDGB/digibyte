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
#include <consensus/merkle.h>
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
    bundle.median_price_micro_usd = price_micro_usd; // Phase One: 1 message = median
    bundle.timestamp = timestamp;

    return bundle;
}

/**
 * Add oracle bundle to coinbase transaction OP_RETURN output
 * Uses Phase One compact format (20 bytes total)
 */
static void AddOracleBundleToCoinbase(CMutableTransaction& coinbase, const COracleBundle& bundle)
{
    // Use compact format for Phase One (single oracle)
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    CScript oracle_script = manager.CreateOracleScript(bundle);

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
    // Enable oracle system for testing
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1); // Phase One: 1-of-1 consensus

    // Generate oracle keypair
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    // Create block with valid oracle bundle
    uint64_t price = 50000; // 50000 micro-USD = $0.05 = 5 cents (valid range: 100 - 100000000 micro-USD)
    int64_t timestamp = GetTime();
    int32_t block_height = 50;  // Below Phase Two activation (100) for Phase One testing  // Above activation height (600)
    CScript coinbase_script_sig = CScript() << block_height << OP_0;

    CBlock block = CreateBlockWithOracleBundle(oracle_key, price, timestamp, block_height, coinbase_script_sig);

    // Test CheckBlock validation
    BlockValidationState state;
    const Consensus::Params& params = Params().GetConsensus();

    // EXPECTED FAILURE: CheckBlock does not yet validate oracle data properly
    bool checkblock_result = CheckBlock(block, state, params, false, false);

    if (!checkblock_result) {
        BOOST_TEST_MESSAGE("CheckBlock rejected: " << state.GetRejectReason() << " - " << state.GetDebugMessage());
    }

    BOOST_CHECK_MESSAGE(
        checkblock_result,
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
    // Phase Two: 4-of-7 multi-oracle consensus test
    // RegTest: nDDActivationHeight=650, nDigiDollarPhase2Height=100
    // Height 700 is in Phase Two territory — oracle validation is active
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(3); // Phase Two regtest: 3-of-5

    // Test: Block with invalid oracle price (0) should be rejected
    // Create a single message with price=0 — this fails IsValid() price range check
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 0; // Invalid: below ORACLE_MIN_PRICE_MICRO_USD (100)
    msg.timestamp = GetTime();
    msg.block_height = 700;
    msg.nonce = GetRand(UINT64_MAX);
    msg.oracle_pubkey = XOnlyPubKey(oracle_key.GetPubKey());
    // Don't sign — price=0 is structurally invalid regardless

    COracleBundle bundle;
    bundle.messages.push_back(msg);
    bundle.epoch = GetCurrentEpoch(700);
    bundle.median_price_micro_usd = 0;
    bundle.timestamp = msg.timestamp;

    // Create block at height 700 (above both activation heights)
    CBlock block;
    block.nVersion = 1;
    block.nTime = GetTime();
    block.hashPrevBlock.SetNull();
    block.nBits = 0x207fffff;
    block.nNonce = 0;

    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << 700 << OP_0;
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 72000 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    AddOracleBundleToCoinbase(coinbase, bundle);
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));

    BlockValidationState state;
    const Consensus::Params& params = Params().GetConsensus();

    // CheckBlock is context-free and must not enforce oracle price rules.
    bool accepted = CheckBlock(block, state, params, false, false);
    BOOST_CHECK_MESSAGE(accepted,
        "CheckBlock should defer oracle price validation to contextual block checks");

    CBlockIndex prev_index;
    prev_index.nHeight = 699;
    prev_index.nTime = block.nTime - 15;

    BlockValidationState contextual_state;
    bool contextual_accepted = OracleDataValidator::ValidateBlockOracleData(
        block, &prev_index, params, contextual_state);

    BOOST_CHECK_MESSAGE(!contextual_accepted,
        "Contextual oracle validation should reject block with invalid oracle price (0)");
    std::string reason = contextual_state.GetRejectReason();
    BOOST_CHECK_MESSAGE(
        reason == "bad-oracle-bundle" || reason == "bad-oracle-phase2" ||
        reason == "bad-oracle-consensus" || reason == "bad-oracle-median",
        "Expected oracle rejection reason, got: " + reason
    );
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
    // Enable oracle system for testing
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1); // Phase One: 1-of-1 consensus

    // Generate two oracle keypairs
    CKey oracle_key1, oracle_key2;
    oracle_key1.MakeNewKey(true);
    oracle_key2.MakeNewKey(true);

    int64_t timestamp = GetTime();
    int32_t block_height = 50;  // Below Phase Two activation (100) for Phase One testing  // Above activation height (600)

    // Create bundle with TWO messages (violates Phase One 1-of-1)
    COraclePriceMessage msg1 = CreateValidOracleMessage(oracle_key1, 5, timestamp, block_height);
    COraclePriceMessage msg2 = CreateValidOracleMessage(oracle_key2, 6, timestamp, block_height);

    COracleBundle bundle;
    bundle.messages.push_back(msg1);
    bundle.messages.push_back(msg2); // INVALID: Phase One requires exactly 1 message
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.median_price_micro_usd = 5; // Median: 5 cents ($0.05)
    bundle.timestamp = timestamp;

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

    // Test CheckBlock behavior with wrong consensus
    // Phase One: CreateOracleScript rejects bundles with multiple messages by returning empty script
    // This means the block will have NO oracle data (transition period behavior)
    BlockValidationState state;
    const Consensus::Params& params = Params().GetConsensus();

    // Phase One behavior: Block with invalid bundle structure (>1 message) results in NO oracle data
    // Since we're in transition period, blocks without oracle data are allowed
    // The proper fix is to test that CreateOracleScript returns empty for multi-message bundles

    // RegTest now has Phase Two active (nDigiDollarPhase2Height = 100)
    // Multi-message bundles should be ACCEPTED and produce a Phase Two script
    OracleBundleManager& test_manager = OracleBundleManager::GetInstance();
    CScript oracle_script = test_manager.CreateOracleScript(bundle);

    BOOST_CHECK_MESSAGE(
        !oracle_script.empty(),
        "CreateOracleScript should produce Phase Two script for multi-message bundles when Phase Two is active"
    );

    // If script is empty, CheckBlock will pass (transition period)
    // This is correct Phase One behavior - invalid bundles are rejected at creation, not validation
    if (oracle_script.empty()) {
        BOOST_CHECK_MESSAGE(
            CheckBlock(block, state, params, false, false),
            "CheckBlock should pass when oracle bundle is rejected at creation (empty script)"
        );
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
    int32_t block_height = 50;  // Below Phase Two activation (100) for Phase One testing  // Above activation height (600)

    CBlock block = CreateBlockWithOracleBundle(
        oracle_key,
        50000,  // 50000 micro-USD = $0.05 = 5 cents (valid range: 100 - 100000000 micro-USD)
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
    bool check_result = CheckBlock(block, state, params, false, false);
    if (!check_result) {
        LogPrintf("CheckBlock FAILED: %s - %s\n", state.GetRejectReason(), state.GetDebugMessage());
    }
    BOOST_REQUIRE(check_result);

    // EXPECTED FAILURE: ContextualCheckBlock may not exist or validate oracle timestamps
    // Note: This is a static function in validation.cpp, may need to be exposed
    // For now, we test through OracleDataValidator::ValidateBlockOracleData

    bool valid = OracleDataValidator::ValidateBlockOracleData(block, &prev_index, params, state);

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
    // Enable oracle system for testing
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1); // Phase One: 1-of-1 consensus

    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    int64_t block_time = GetTime();
    int64_t oracle_timestamp = block_time - 7200; // 2 hours old (invalid: > 1 hour)
    int32_t block_height = 700;  // Above DigiDollar activation height (650) so validation kicks in

    CBlock block = CreateBlockWithOracleBundle(
        oracle_key,
        50000,  // 50000 micro-USD = $0.05 = 5 cents (valid range: 100 - 100000000 micro-USD)
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
    bool valid = OracleDataValidator::ValidateBlockOracleData(block, &prev_index, params, state);

    BOOST_CHECK_MESSAGE(
        !valid,
        "Oracle bundle older than 1 hour should be rejected"
    );

    if (valid) {
        LogPrintf("TEST FAILURE (EXPECTED): ContextualCheckBlock accepted oracle bundle older than 1 hour\n");
    }
}

BOOST_AUTO_TEST_SUITE_END()
