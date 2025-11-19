// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <logging.h>
#include <util/strencodings.h>
#include <node/miner.h>
#include <oracle/bundle_manager.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <primitives/block.h>
#include <script/script.h>
#include <policy/policy.h>
#include <test/util/setup_common.h>
#include <test/util/random.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <key.h>
#include <pubkey.h>
#include <streams.h>
#include <util/time.h>
#include <validation.h>

using node::BlockAssembler;
using node::CBlockTemplate;

/**
 * ORACLE MINER INTEGRATION TESTS
 * Week 4: Miner Integration - RED PHASE (TDD)
 *
 * Tests for integrating oracle bundles into mined blocks.
 *
 * NOTE: Implementation DOES exist (AddOracleBundleToBlock in bundle_manager.cpp)
 * These tests verify the implementation matches the specification.
 *
 * Tests verify:
 * - OracleBundleManager::AddOracleBundleToBlock() correctly adds bundles to coinbase
 * - CreateNewBlock() integration with oracle bundle system
 * - OP_RETURN serialization for oracle bundles
 * - Size limits (83 bytes MAX_OP_RETURN_RELAY)
 * - Graceful degradation when no oracle data available
 * - Phase One consensus (1-of-1)
 */

BOOST_FIXTURE_TEST_SUITE(oracle_miner_tests, TestChain100Setup)

//
// CATEGORY 1: AddOracleBundleToBlock() TESTS (3 tests)
//

/**
 * TEST: Oracle bundle added to coinbase OP_RETURN
 *
 * Verifies that AddOracleBundleToBlock() correctly:
 * - Adds oracle bundle as OP_RETURN output to coinbase
 * - Creates unspendable output (value = 0)
 * - Appends to existing coinbase outputs
 *
 * SPEC REFERENCE: Section 5.5.1
 * - Get latest bundle from OracleBundleManager
 * - Serialize bundle to OP_RETURN format
 * - Add as second output in coinbase transaction
 */
BOOST_AUTO_TEST_CASE(add_oracle_bundle_to_coinbase)
{
    // Create oracle bundle with valid message
    OracleBundleManager& manager = OracleBundleManager::GetInstance();

    // Generate oracle keypair
    CKey oracle_key;
    oracle_key.MakeNewKey(true);
    XOnlyPubKey oracle_pubkey(oracle_key.GetPubKey());

    // Create oracle price message
    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;  // $0.05 in micro-USD
    msg.timestamp = GetTime();
    msg.block_height = 101;
    msg.nonce = FastRandomContext().rand64();
    msg.oracle_pubkey = oracle_pubkey;

    // Sign the message
    BOOST_REQUIRE(msg.Sign(oracle_key));

    // Add message to bundle manager
    manager.AddOracleMessage(msg);

    // Create a simple block with coinbase
    CBlock block;
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vout.resize(1);  // Standard miner payout
    coinbase.vout[0].nValue = 72000 * COIN;  // DigiByte subsidy
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    block.vtx.push_back(MakeTransactionRef(coinbase));

    // Call AddOracleBundleToBlock
    // WILL FAIL: OracleBundleManager::AddOracleBundleToBlock() doesn't exist
    BOOST_CHECK(manager.AddOracleBundleToBlock(block, 101));

    // Verify oracle bundle was added to coinbase
    BOOST_REQUIRE(!block.vtx.empty());
    const CTransaction& updated_coinbase = *block.vtx[0];

    // Should have 2 outputs: miner payout + OP_RETURN oracle bundle
    BOOST_CHECK_EQUAL(updated_coinbase.vout.size(), 2);

    // Second output should be OP_RETURN (unspendable)
    BOOST_CHECK(updated_coinbase.vout[1].scriptPubKey.IsUnspendable());
    BOOST_CHECK_EQUAL(updated_coinbase.vout[1].nValue, 0);

    // OP_RETURN output should start with OP_RETURN opcode
    BOOST_REQUIRE(!updated_coinbase.vout[1].scriptPubKey.empty());
    BOOST_CHECK_EQUAL(updated_coinbase.vout[1].scriptPubKey[0], OP_RETURN);
}

/**
 * RED TEST: Oracle bundle serialization format validation
 *
 * EXPECTED TO FAIL:
 * - OracleBundleManager::CreateOracleScript() method may not exist
 * - Bundle serialization format may not match spec
 *
 * SPEC REFERENCE: Section 5.5.1
 * - Format: OP_RETURN <serialized_bundle>
 * - Size limit: 83 bytes (MAX_OP_RETURN_RELAY)
 * - Must be deserializable
 */
BOOST_AUTO_TEST_CASE(oracle_bundle_serialization_format)
{
    // Create oracle message and bundle
    CKey oracle_key;
    oracle_key.MakeNewKey(true);
    XOnlyPubKey oracle_pubkey(oracle_key.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 123456;  // $0.123456 in micro-USD
    msg.timestamp = GetTime();
    msg.block_height = 101;
    msg.nonce = FastRandomContext().rand64();
    msg.oracle_pubkey = oracle_pubkey;

    // Sign the message
    BOOST_REQUIRE(msg.Sign(oracle_key));

    // Create bundle with message
    COracleBundle bundle;
    bundle.epoch = 1;
    bundle.AddMessage(msg);

    // Serialize bundle using standard serialization
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << bundle;

    // Verify size is within OP_RETURN limit (83 bytes)
    // WILL FAIL if bundle is too large
    BOOST_CHECK_LT(ss.size(), MAX_OP_RETURN_RELAY);

    // Create OP_RETURN script with serialized bundle
    std::vector<unsigned char> bundle_data;
    bundle_data.reserve(ss.size());
    for (auto it = ss.begin(); it != ss.end(); ++it) {
        bundle_data.push_back(static_cast<unsigned char>(*it));
    }
    CScript oracle_script;
    oracle_script << OP_RETURN << bundle_data;

    // Verify script is unspendable
    BOOST_CHECK(oracle_script.IsUnspendable());

    // Verify total script size is within limits
    BOOST_CHECK_LE(oracle_script.size(), MAX_OP_RETURN_RELAY);

    // Test deserialization to ensure round-trip works
    // Extract bundle data from script
    CScript::const_iterator pc = oracle_script.begin();
    opcodetype opcode;
    std::vector<unsigned char> extracted_data;

    // Skip OP_RETURN
    BOOST_REQUIRE(oracle_script.GetOp(pc, opcode));
    BOOST_CHECK_EQUAL(opcode, OP_RETURN);

    // Extract bundle data
    BOOST_REQUIRE(oracle_script.GetOp(pc, opcode, extracted_data));

    // Deserialize bundle
    CDataStream ss_deserialize(extracted_data, SER_NETWORK, PROTOCOL_VERSION);
    COracleBundle deserialized_bundle;
    ss_deserialize >> deserialized_bundle;

    // Verify bundle data matches
    BOOST_CHECK_EQUAL(deserialized_bundle.epoch, bundle.epoch);
    BOOST_CHECK_EQUAL(deserialized_bundle.messages.size(), bundle.messages.size());
    if (!deserialized_bundle.messages.empty()) {
        BOOST_CHECK_EQUAL(deserialized_bundle.messages[0].price_micro_usd, msg.price_micro_usd);
        BOOST_CHECK_EQUAL(deserialized_bundle.messages[0].oracle_id, msg.oracle_id);
    }
}

/**
 * RED TEST: Oracle bundle size limit validation
 *
 * EXPECTED TO FAIL:
 * - Bundle size validation may not be implemented
 * - AddOracleBundleToBlock() may not check size limits
 *
 * SPEC REFERENCE: Section 5.5.1
 * - Maximum size: 83 bytes (MAX_OP_RETURN_RELAY)
 * - Must reject oversized bundles gracefully
 */
BOOST_AUTO_TEST_CASE(oracle_bundle_size_limit)
{
    // Create oracle message
    CKey oracle_key;
    oracle_key.MakeNewKey(true);
    XOnlyPubKey oracle_pubkey(oracle_key.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;
    msg.timestamp = GetTime();
    msg.block_height = 101;
    msg.nonce = FastRandomContext().rand64();
    msg.oracle_pubkey = oracle_pubkey;

    BOOST_REQUIRE(msg.Sign(oracle_key));

    // Create bundle
    COracleBundle bundle;
    bundle.epoch = 1;
    bundle.AddMessage(msg);

    // Serialize and check size
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << bundle;

    // Phase One with single oracle should fit in OP_RETURN
    // Single message bundle should be well under 83 bytes
    BOOST_CHECK_LT(ss.size(), MAX_OP_RETURN_RELAY);

    // Create OP_RETURN script
    std::vector<unsigned char> bundle_data;
    bundle_data.reserve(ss.size());
    for (auto it = ss.begin(); it != ss.end(); ++it) {
        bundle_data.push_back(static_cast<unsigned char>(*it));
    }
    CScript oracle_script;
    oracle_script << OP_RETURN << bundle_data;

    // Verify complete script (including OP_RETURN opcode) fits in limit
    BOOST_CHECK_LE(oracle_script.size(), MAX_OP_RETURN_RELAY);

    // Test that the bundle can be added to a block
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.AddOracleMessage(msg);

    CBlock block;
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 72000 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    block.vtx.push_back(MakeTransactionRef(coinbase));

    // Should succeed for Phase One single oracle
    // WILL FAIL: AddOracleBundleToBlock() doesn't exist
    BOOST_CHECK(manager.AddOracleBundleToBlock(block, 101));

    // Verify added OP_RETURN output is within size limits
    const CTransaction& updated_coinbase = *block.vtx[0];
    if (updated_coinbase.vout.size() > 1) {
        BOOST_CHECK_LE(updated_coinbase.vout[1].scriptPubKey.size(), MAX_OP_RETURN_RELAY);
    }
}

//
// CATEGORY 2: CreateNewBlock() INTEGRATION TESTS (3 tests)
//

/**
 * RED TEST: CreateNewBlock includes oracle bundle in coinbase
 *
 * EXPECTED TO FAIL:
 * - CreateNewBlock() may not call AddOracleBundleToBlock()
 * - Integration point at line ~170 in miner.cpp may not be implemented
 *
 * SPEC REFERENCE: Section 5.5.1
 * - CreateNewBlock() should call AddOracleBundleToBlock() before returning
 * - Oracle bundle should be in coinbase OP_RETURN
 * - Only when DigiDollar is enabled
 */
BOOST_AUTO_TEST_CASE(create_new_block_includes_oracle_bundle)
{
    // Add oracle message to bundle manager
    OracleBundleManager& manager = OracleBundleManager::GetInstance();

    CKey oracle_key;
    oracle_key.MakeNewKey(true);
    XOnlyPubKey oracle_pubkey(oracle_key.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;  // $0.05
    msg.timestamp = GetTime();
    msg.block_height = m_node.chainman->ActiveHeight() + 1;
    msg.nonce = FastRandomContext().rand64();
    msg.oracle_pubkey = oracle_pubkey;

    BOOST_REQUIRE(msg.Sign(oracle_key));
    manager.AddOracleMessage(msg);

    // Create new block using BlockAssembler
    CScript scriptPubKey = CScript() << OP_TRUE;
    std::unique_ptr<CBlockTemplate> pblocktemplate =
        BlockAssembler(m_node.chainman->ActiveChainstate(), m_node.mempool.get())
            .CreateNewBlock(scriptPubKey, ALGO_SHA256D);

    BOOST_REQUIRE(pblocktemplate);
    CBlock& block = pblocktemplate->block;

    // Verify block has at least coinbase transaction
    BOOST_REQUIRE(!block.vtx.empty());
    const CTransaction& coinbase = *block.vtx[0];

    // Check coinbase has oracle bundle output
    // WILL FAIL if CreateNewBlock() doesn't integrate oracle bundle
    BOOST_CHECK_GE(coinbase.vout.size(), 2);  // Payout + OP_RETURN (+ optional witness commitment)

    // Find OP_RETURN output (should be vout[1])
    bool found_oracle_opreturn = false;
    for (size_t i = 1; i < coinbase.vout.size(); ++i) {
        if (coinbase.vout[i].scriptPubKey.IsUnspendable() &&
            !coinbase.vout[i].scriptPubKey.empty() &&
            coinbase.vout[i].scriptPubKey[0] == OP_RETURN) {
            found_oracle_opreturn = true;

            // Verify it's not the witness commitment (which is also OP_RETURN)
            // Oracle OP_RETURN should have bundle data
            BOOST_CHECK_GT(coinbase.vout[i].scriptPubKey.size(), 2);
            break;
        }
    }

    // WILL FAIL: Oracle bundle not added by CreateNewBlock()
    BOOST_CHECK(found_oracle_opreturn);
}

/**
 * RED TEST: CreateNewBlock continues without oracle bundle if unavailable
 *
 * EXPECTED TO FAIL:
 * - Graceful degradation may not be implemented
 * - Block creation should succeed even if no oracle bundle available
 *
 * SPEC REFERENCE: Section 5.5.1
 * - Continue with block creation even if oracle bundle fails
 * - Graceful degradation for missing oracle data
 */
BOOST_AUTO_TEST_CASE(create_new_block_no_oracle_if_unavailable)
{
    // Ensure no oracle messages are pending
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    // Clear any existing messages (if Clear() method exists)
    // manager.Clear();  // May not exist yet

    // Create new block without any oracle data
    CScript scriptPubKey = CScript() << OP_TRUE;
    std::unique_ptr<CBlockTemplate> pblocktemplate =
        BlockAssembler(m_node.chainman->ActiveChainstate(), m_node.mempool.get())
            .CreateNewBlock(scriptPubKey, ALGO_SHA256D);

    // Block creation should still succeed
    BOOST_REQUIRE(pblocktemplate);
    CBlock& block = pblocktemplate->block;

    // Verify block is valid (has coinbase, merkle root, etc.)
    BOOST_REQUIRE(!block.vtx.empty());
    const CTransaction& coinbase = *block.vtx[0];

    // Coinbase should have at least miner payout
    BOOST_CHECK_GE(coinbase.vout.size(), 1);

    // Block should have valid merkle root
    BOOST_CHECK_EQUAL(block.hashMerkleRoot, BlockMerkleRoot(block));

    // Test should pass even without oracle bundle (graceful degradation)
    BOOST_CHECK(true);
}

/**
 * RED TEST: Phase One uses 1-of-1 oracle consensus in blocks
 *
 * EXPECTED TO FAIL:
 * - Phase One consensus validation may not be implemented
 * - Bundle should contain exactly 1 oracle message in Phase One
 *
 * SPEC REFERENCE: Section 5.5.1
 * - Phase One: Single oracle (testnet only)
 * - Bundle should have exactly 1 message
 * - Future phases: 8-of-15 consensus
 */
BOOST_AUTO_TEST_CASE(create_new_block_phase_one_single_oracle)
{
    // Add EXACTLY ONE oracle message (Phase One requirement)
    OracleBundleManager& manager = OracleBundleManager::GetInstance();

    CKey oracle_key;
    oracle_key.MakeNewKey(true);
    XOnlyPubKey oracle_pubkey(oracle_key.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_id = 0;  // First oracle
    msg.price_micro_usd = 50000;
    msg.timestamp = GetTime();
    msg.block_height = m_node.chainman->ActiveHeight() + 1;
    msg.nonce = FastRandomContext().rand64();
    msg.oracle_pubkey = oracle_pubkey;

    BOOST_REQUIRE(msg.Sign(oracle_key));
    manager.AddOracleMessage(msg);

    // Do NOT add additional oracle messages (Phase One = single oracle)

    // Create block
    CScript scriptPubKey = CScript() << OP_TRUE;
    std::unique_ptr<CBlockTemplate> pblocktemplate =
        BlockAssembler(m_node.chainman->ActiveChainstate(), m_node.mempool.get())
            .CreateNewBlock(scriptPubKey, ALGO_SHA256D);

    BOOST_REQUIRE(pblocktemplate);
    CBlock& block = pblocktemplate->block;
    BOOST_REQUIRE(!block.vtx.empty());

    const CTransaction& coinbase = *block.vtx[0];

    // Extract oracle bundle from OP_RETURN
    bool found_bundle = false;
    for (size_t i = 1; i < coinbase.vout.size(); ++i) {
        if (coinbase.vout[i].scriptPubKey.IsUnspendable() &&
            !coinbase.vout[i].scriptPubKey.empty() &&
            coinbase.vout[i].scriptPubKey[0] == OP_RETURN) {

            // Try to extract and deserialize bundle
            CScript::const_iterator pc = coinbase.vout[i].scriptPubKey.begin();
            opcodetype opcode;
            std::vector<unsigned char> bundle_data;

            // Skip OP_RETURN
            coinbase.vout[i].scriptPubKey.GetOp(pc, opcode);

            // Extract data
            if (coinbase.vout[i].scriptPubKey.GetOp(pc, opcode, bundle_data)) {
                try {
                    CDataStream ss(bundle_data, SER_NETWORK, PROTOCOL_VERSION);
                    COracleBundle bundle;
                    ss >> bundle;

                    // Phase One: Should have exactly 1 message
                    // WILL FAIL if bundle has != 1 message
                    BOOST_CHECK_EQUAL(bundle.messages.size(), 1);

                    if (!bundle.messages.empty()) {
                        BOOST_CHECK_EQUAL(bundle.messages[0].oracle_id, 0);
                        BOOST_CHECK_EQUAL(bundle.messages[0].price_micro_usd, 50000);
                    }

                    found_bundle = true;
                } catch (...) {
                    // Deserialization failed
                }
            }
            break;
        }
    }

    // WILL FAIL if no oracle bundle found
    BOOST_CHECK(found_bundle);
}

BOOST_AUTO_TEST_SUITE_END()
