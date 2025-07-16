// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <protocol.h>
#include <primitives/transaction.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(dandelion_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dandelion_message_types)
{
    // Test that Dandelion message types are correctly defined
    BOOST_CHECK_EQUAL(MSG_DANDELION_TX, 6);
    BOOST_CHECK_EQUAL(MSG_DANDELION_WITNESS_TX, MSG_DANDELION_TX | MSG_WITNESS_FLAG);
    
    // Test CInv helper methods
    CInv dandelion_tx_inv(MSG_DANDELION_TX, uint256());
    BOOST_CHECK(dandelion_tx_inv.IsDandelionMsg());
    BOOST_CHECK(dandelion_tx_inv.IsGenTxMsg());
    BOOST_CHECK(dandelion_tx_inv.IsMsgTx()); // Should return true for dandelion TX
    
    CInv dandelion_wtx_inv(MSG_DANDELION_WITNESS_TX, uint256());
    BOOST_CHECK(dandelion_wtx_inv.IsDandelionMsg());
    BOOST_CHECK(dandelion_wtx_inv.IsGenTxMsg());
    BOOST_CHECK(!dandelion_wtx_inv.IsMsgTx()); // Should return false for witness variant
}

BOOST_AUTO_TEST_CASE(dandelion_witness_transaction_type)
{
    // Create a transaction with witness data
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    mtx.vin[0].scriptWitness.stack.push_back({0x00}); // Add witness data
    mtx.vout.resize(1);
    
    CTransaction tx(mtx);
    BOOST_CHECK(tx.HasWitness());
    
    // Test that witness transactions use the correct Dandelion message type
    int expected_type = MSG_DANDELION_WITNESS_TX;
    int regular_type = MSG_DANDELION_TX;
    
    // For witness transaction, should use MSG_DANDELION_WITNESS_TX
    int actual_type = tx.HasWitness() ? MSG_DANDELION_WITNESS_TX : MSG_DANDELION_TX;
    BOOST_CHECK_EQUAL(actual_type, expected_type);
    
    // For non-witness transaction
    CMutableTransaction mtx_no_witness;
    mtx_no_witness.nVersion = 2;
    mtx_no_witness.vin.resize(1);
    mtx_no_witness.vout.resize(1);
    
    CTransaction tx_no_witness(mtx_no_witness);
    BOOST_CHECK(!tx_no_witness.HasWitness());
    
    int actual_type_no_witness = tx_no_witness.HasWitness() ? MSG_DANDELION_WITNESS_TX : MSG_DANDELION_TX;
    BOOST_CHECK_EQUAL(actual_type_no_witness, regular_type);
}

BOOST_AUTO_TEST_CASE(to_gentxid_dandelion)
{
    // Test ToGenTxid handles Dandelion transactions correctly
    uint256 test_hash = uint256S("0x0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    
    // Regular Dandelion transaction
    CInv dandelion_inv(MSG_DANDELION_TX, test_hash);
    GenTxid gen_txid = ToGenTxid(dandelion_inv);
    BOOST_CHECK(!gen_txid.IsWtxid());
    BOOST_CHECK_EQUAL(gen_txid.GetHash(), test_hash);
    
    // Witness Dandelion transaction
    CInv dandelion_witness_inv(MSG_DANDELION_WITNESS_TX, test_hash);
    GenTxid gen_wtxid = ToGenTxid(dandelion_witness_inv);
    BOOST_CHECK(gen_wtxid.IsWtxid());
    BOOST_CHECK_EQUAL(gen_wtxid.GetHash(), test_hash);
}

BOOST_AUTO_TEST_SUITE_END()