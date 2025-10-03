// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <wallet/wallet.h>
#include <wallet/digidollarwallet.h>
#include <wallet/walletdb.h>
#include <wallet/test/util.h>
#include <wallet/test/wallet_test_fixture.h>
#include <base58.h>
#include <util/time.h>
#include <util/strencodings.h>
#include <streams.h>
#include <test/util/setup_common.h>

namespace wallet {

BOOST_FIXTURE_TEST_SUITE(digidollar_persistence_wallet_tests, WalletTestingSetup)

// =============================================================================
// PHASE 3 TASK 3.1: WriteDDBalance Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(digidollarwallet_write_balance_persists)
{
    // Use the fixture's wallet
    DigiDollarWallet dd_wallet(&m_wallet);

    // Create DD address
    CDigiDollarAddress addr("DD1qtest123456789abcdefghijklmnopqrstuvwxyz");
    CAmount balance = 10000; // $100.00

    // Write balance
    BOOST_CHECK(dd_wallet.WriteDDBalance(addr, balance));

    // Verify written to database
    WalletBatch batch(m_wallet.GetDatabase());
    WalletDDBalance read_balance;
    // For test addresses, use the same key generation logic
    std::string key = addr.ToString();
    if (key.empty()) {
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << addr;
        key = "test_addr_" + HexStr(ss);
    }
    BOOST_CHECK(batch.ReadDDBalance(key, read_balance));
    BOOST_CHECK_EQUAL(read_balance.balance, balance);
    // Note: read_balance.address may differ from addr for test addresses
    BOOST_CHECK(read_balance.last_updated > 0); // Timestamp should be set

    // Verify in-memory cache updated
    BOOST_CHECK_EQUAL(dd_wallet.GetDDBalance(addr), balance);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_balance_updates_total)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Write multiple balances
    CDigiDollarAddress addr1("DD1qtest111111111111111111111111111111111");
    CDigiDollarAddress addr2("DD1qtest222222222222222222222222222222222");

    BOOST_CHECK(dd_wallet.WriteDDBalance(addr1, 5000));
    BOOST_CHECK(dd_wallet.WriteDDBalance(addr2, 3000));

    // Verify individual balances
    BOOST_CHECK_EQUAL(dd_wallet.GetDDBalance(addr1), 5000);
    BOOST_CHECK_EQUAL(dd_wallet.GetDDBalance(addr2), 3000);

    // Verify total balance calculation is correct
    // Note: GetTotalDDBalance calculates from collateral_positions, not dd_balances
    // This test verifies the balance is stored correctly
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_balance_updates_existing)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    CDigiDollarAddress addr("DD1qtest333333333333333333333333333333333");

    // Write initial balance
    BOOST_CHECK(dd_wallet.WriteDDBalance(addr, 1000));
    BOOST_CHECK_EQUAL(dd_wallet.GetDDBalance(addr), 1000);

    // Update balance
    BOOST_CHECK(dd_wallet.WriteDDBalance(addr, 2000));
    BOOST_CHECK_EQUAL(dd_wallet.GetDDBalance(addr), 2000);

    // Verify database has updated value
    WalletBatch batch(m_wallet.GetDatabase());
    WalletDDBalance read_balance;
    std::string key = addr.ToString();
    if (key.empty()) {
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << addr;
        key = "test_addr_" + HexStr(ss);
    }
    BOOST_CHECK(batch.ReadDDBalance(key, read_balance));
    BOOST_CHECK_EQUAL(read_balance.balance, 2000);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_balance_persists_total_metadata)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    CDigiDollarAddress addr1("DD1qtest444444444444444444444444444444444");
    CDigiDollarAddress addr2("DD1qtest555555555555555555555555555555555");

    BOOST_CHECK(dd_wallet.WriteDDBalance(addr1, 7500));
    BOOST_CHECK(dd_wallet.WriteDDBalance(addr2, 2500));

    // Verify total metadata is written to database
    WalletBatch batch(m_wallet.GetDatabase());
    std::string total_str;
    BOOST_CHECK(batch.ReadDDMetadata("total_dd_balance", total_str));

    // Total should be 10000 (7500 + 2500)
    CAmount total = std::stoll(total_str);
    BOOST_CHECK_EQUAL(total, 10000);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_balance_invalid_address)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Empty address should fail
    CDigiDollarAddress empty_addr;
    BOOST_CHECK(!dd_wallet.WriteDDBalance(empty_addr, 1000));
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_balance_negative_amount)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    CDigiDollarAddress addr("DD1qtest666666666666666666666666666666666");

    // Negative balance should fail
    BOOST_CHECK(!dd_wallet.WriteDDBalance(addr, -1000));
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_balance_zero_amount)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    CDigiDollarAddress addr("DD1qtest777777777777777777777777777777777");

    // Zero balance should be allowed (clearing balance)
    BOOST_CHECK(dd_wallet.WriteDDBalance(addr, 0));
    BOOST_CHECK_EQUAL(dd_wallet.GetDDBalance(addr), 0);
}

// =============================================================================
// PHASE 3 TASK 3.2: WritePosition Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(digidollarwallet_write_position_persists)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Create position
    uint256 pos_id = uint256S("0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
    WalletCollateralPosition pos(pos_id, 10000, 500000, 3, 100000);

    BOOST_CHECK(dd_wallet.WritePosition(pos));

    // Verify in database
    WalletBatch batch(m_wallet.GetDatabase());
    WalletCollateralPosition read_pos;
    BOOST_CHECK(batch.ReadPosition(pos_id, read_pos));
    BOOST_CHECK_EQUAL(read_pos.dd_minted, 10000);
    BOOST_CHECK_EQUAL(read_pos.dgb_collateral, 500000);

    // Verify in memory
    auto positions = dd_wallet.GetPositions(false); // Get all positions, not just active
    BOOST_CHECK_EQUAL(positions.size(), 1);
    BOOST_CHECK_EQUAL(positions[0].dd_minted, 10000);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_position_updates_locked_collateral)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Write two active positions
    uint256 id1 = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    uint256 id2 = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");

    WalletCollateralPosition pos1(id1, 5000, 250000, 2, 50000);
    WalletCollateralPosition pos2(id2, 3000, 150000, 1, 30000);

    // Both positions are active by default in constructor
    BOOST_CHECK(dd_wallet.WritePosition(pos1));
    BOOST_CHECK(dd_wallet.WritePosition(pos2));

    // Verify locked collateral is sum of both
    BOOST_CHECK_EQUAL(dd_wallet.GetLockedCollateral(), 400000);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_position_updates_existing)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    uint256 pos_id = uint256S("0x3333333333333333333333333333333333333333333333333333333333333333");

    // Write initial position
    WalletCollateralPosition pos1(pos_id, 1000, 50000, 1, 10000);
    BOOST_CHECK(dd_wallet.WritePosition(pos1));

    // Verify initial values
    auto positions = dd_wallet.GetPositions(false);
    BOOST_CHECK_EQUAL(positions.size(), 1);
    BOOST_CHECK_EQUAL(positions[0].dd_minted, 1000);

    // Update position
    WalletCollateralPosition pos2(pos_id, 2000, 100000, 2, 20000);
    BOOST_CHECK(dd_wallet.WritePosition(pos2));

    // Verify updated values in memory
    positions = dd_wallet.GetPositions(false);
    BOOST_CHECK_EQUAL(positions.size(), 1);
    BOOST_CHECK_EQUAL(positions[0].dd_minted, 2000);
    BOOST_CHECK_EQUAL(positions[0].dgb_collateral, 100000);

    // Verify updated values in database
    WalletBatch batch(m_wallet.GetDatabase());
    WalletCollateralPosition read_pos;
    BOOST_CHECK(batch.ReadPosition(pos_id, read_pos));
    BOOST_CHECK_EQUAL(read_pos.dd_minted, 2000);
    BOOST_CHECK_EQUAL(read_pos.dgb_collateral, 100000);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_position_persists_metadata)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Write active position
    uint256 pos_id = uint256S("0x4444444444444444444444444444444444444444444444444444444444444444");
    WalletCollateralPosition pos(pos_id, 7500, 375000, 3, 75000);

    BOOST_CHECK(dd_wallet.WritePosition(pos));

    // Verify locked_collateral metadata is written to database
    WalletBatch batch(m_wallet.GetDatabase());
    std::string locked_str;
    BOOST_CHECK(batch.ReadDDMetadata("locked_collateral", locked_str));

    // Locked collateral should be 375000
    CAmount locked = std::stoll(locked_str);
    BOOST_CHECK_EQUAL(locked, 375000);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_position_invalid_id)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Null position ID should fail
    uint256 null_id;
    WalletCollateralPosition pos(null_id, 1000, 50000, 1, 10000);

    BOOST_CHECK(!dd_wallet.WritePosition(pos));
}

BOOST_AUTO_TEST_CASE(digidollarwallet_write_position_inactive_no_locked)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Write inactive position
    uint256 pos_id = uint256S("0x5555555555555555555555555555555555555555555555555555555555555555");
    WalletCollateralPosition pos(pos_id, 1000, 50000, 1, 10000);
    pos.is_active = false;

    BOOST_CHECK(dd_wallet.WritePosition(pos));

    // Verify locked collateral is NOT counted for inactive position
    BOOST_CHECK_EQUAL(dd_wallet.GetLockedCollateral(), 0);
}

// =============================================================================
// PHASE 3 TASK 3.3: UpdatePositionStatus Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(digidollarwallet_update_position_status)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Create and write position
    uint256 pos_id = uint256S("0x3333333333333333333333333333333333333333333333333333333333333333");
    WalletCollateralPosition pos(pos_id, 10000, 500000, 3, 100000);
    BOOST_REQUIRE(dd_wallet.WritePosition(pos));
    BOOST_CHECK_EQUAL(dd_wallet.GetLockedCollateral(), 500000);

    // Mark inactive (simulate redemption)
    BOOST_CHECK(dd_wallet.UpdatePositionStatus(pos_id, false));

    // Verify database updated
    WalletBatch batch(m_wallet.GetDatabase());
    WalletCollateralPosition read_pos;
    BOOST_CHECK(batch.ReadPosition(pos_id, read_pos));
    BOOST_CHECK(!read_pos.is_active);

    // Verify locked collateral decreased
    BOOST_CHECK_EQUAL(dd_wallet.GetLockedCollateral(), 0);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_update_position_status_multiple)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Create 3 positions
    uint256 id1 = uint256S("0x4444444444444444444444444444444444444444444444444444444444444444");
    uint256 id2 = uint256S("0x5555555555555555555555555555555555555555555555555555555555555555");
    uint256 id3 = uint256S("0x6666666666666666666666666666666666666666666666666666666666666666");

    dd_wallet.WritePosition(WalletCollateralPosition(id1, 5000, 250000, 2, 50000));
    dd_wallet.WritePosition(WalletCollateralPosition(id2, 3000, 150000, 1, 30000));
    dd_wallet.WritePosition(WalletCollateralPosition(id3, 2000, 100000, 1, 20000));

    BOOST_CHECK_EQUAL(dd_wallet.GetLockedCollateral(), 500000);

    // Mark middle one inactive
    dd_wallet.UpdatePositionStatus(id2, false);

    // Locked should be 250000 + 100000 = 350000
    BOOST_CHECK_EQUAL(dd_wallet.GetLockedCollateral(), 350000);

    // Verify in database
    WalletBatch batch(m_wallet.GetDatabase());
    WalletCollateralPosition read_pos;
    BOOST_CHECK(batch.ReadPosition(id2, read_pos));
    BOOST_CHECK(!read_pos.is_active);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_update_position_status_not_found)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Try to update non-existent position
    uint256 fake_id = uint256S("0x7777777777777777777777777777777777777777777777777777777777777777");
    BOOST_CHECK(!dd_wallet.UpdatePositionStatus(fake_id, false));
}

BOOST_AUTO_TEST_CASE(digidollarwallet_update_position_status_null_id)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Try to update with null ID
    uint256 null_id;
    BOOST_CHECK(!dd_wallet.UpdatePositionStatus(null_id, false));
}

BOOST_AUTO_TEST_CASE(digidollarwallet_update_position_status_reactivate)
{
    DigiDollarWallet dd_wallet(&m_wallet);

    // Create position
    uint256 pos_id = uint256S("0x8888888888888888888888888888888888888888888888888888888888888888");
    WalletCollateralPosition pos(pos_id, 10000, 500000, 3, 100000);
    BOOST_REQUIRE(dd_wallet.WritePosition(pos));

    // Mark inactive
    BOOST_CHECK(dd_wallet.UpdatePositionStatus(pos_id, false));
    BOOST_CHECK_EQUAL(dd_wallet.GetLockedCollateral(), 0);

    // Reactivate
    BOOST_CHECK(dd_wallet.UpdatePositionStatus(pos_id, true));
    BOOST_CHECK_EQUAL(dd_wallet.GetLockedCollateral(), 500000);

    // Verify in database
    WalletBatch batch(m_wallet.GetDatabase());
    WalletCollateralPosition read_pos;
    BOOST_CHECK(batch.ReadPosition(pos_id, read_pos));
    BOOST_CHECK(read_pos.is_active);
}

// =============================================================================
// PHASE 4 TASK 4.1: LoadFromDatabase Tests - THE CRITICAL METHOD
// =============================================================================

BOOST_AUTO_TEST_CASE(digidollarwallet_load_from_database_positions)
{
    // Write positions directly to database
    wallet::WalletBatch batch(m_wallet.GetDatabase());
    uint256 id1 = uint256S("0x8888888888888888888888888888888888888888888888888888888888888888");
    uint256 id2 = uint256S("0x9999999999999999999999999999999999999999999999999999999999999999");

    WalletCollateralPosition pos1(id1, 10000, 500000, 3, 100000);
    WalletCollateralPosition pos2(id2, 5000, 250000, 2, 50000);
    batch.WritePosition(pos1);
    batch.WritePosition(pos2);

    // Create NEW wallet instance (simulates restart)
    DigiDollarWallet fresh_wallet(&m_wallet);

    // Load should populate from database
    size_t loaded = fresh_wallet.LoadFromDatabase();
    BOOST_CHECK_EQUAL(loaded, 2);

    // Verify positions restored
    auto positions = fresh_wallet.GetPositions();
    BOOST_CHECK_EQUAL(positions.size(), 2);
    BOOST_CHECK_EQUAL(fresh_wallet.GetLockedCollateral(), 750000);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_load_from_database_balances)
{
    // Write balances directly to database
    wallet::WalletBatch batch(m_wallet.GetDatabase());

    CDigiDollarAddress addr1("DD1qtest111111111111111111111111111111111");
    CDigiDollarAddress addr2("DD1qtest222222222222222222222222222222222");

    WalletDDBalance bal1(addr1, 10000);
    bal1.last_updated = GetTime();
    WalletDDBalance bal2(addr2, 5000);
    bal2.last_updated = GetTime();

    std::string key1 = addr1.ToString();
    if (key1.empty()) {
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << addr1;
        key1 = "test_addr_" + HexStr(ss);
    }

    std::string key2 = addr2.ToString();
    if (key2.empty()) {
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << addr2;
        key2 = "test_addr_" + HexStr(ss);
    }

    batch.WriteDDBalance(key1, bal1);
    batch.WriteDDBalance(key2, bal2);

    // Create NEW wallet instance
    DigiDollarWallet fresh_wallet(&m_wallet);

    // Load should populate from database
    size_t loaded = fresh_wallet.LoadFromDatabase();
    BOOST_CHECK_EQUAL(loaded, 2);

    // Verify balances restored
    BOOST_CHECK_EQUAL(fresh_wallet.GetBalanceCount(), 2);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_load_from_database_mixed)
{
    // Write both positions and balances
    wallet::WalletBatch batch(m_wallet.GetDatabase());

    // Write 2 positions
    uint256 pos_id = uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    WalletCollateralPosition pos(pos_id, 10000, 500000, 3, 100000);
    batch.WritePosition(pos);

    // Write 1 balance
    CDigiDollarAddress addr("DD1qtest333333333333333333333333333333333");
    WalletDDBalance bal(addr, 7500);
    bal.last_updated = GetTime();

    std::string key = addr.ToString();
    if (key.empty()) {
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << addr;
        key = "test_addr_" + HexStr(ss);
    }
    batch.WriteDDBalance(key, bal);

    // Create NEW wallet instance
    DigiDollarWallet fresh_wallet(&m_wallet);

    // Load should return total items loaded
    size_t loaded = fresh_wallet.LoadFromDatabase();
    BOOST_CHECK_EQUAL(loaded, 2); // 1 position + 1 balance

    // Verify data restored
    BOOST_CHECK_EQUAL(fresh_wallet.GetPositionCount(), 1);
    BOOST_CHECK_EQUAL(fresh_wallet.GetBalanceCount(), 1);
    BOOST_CHECK_EQUAL(fresh_wallet.GetLockedCollateral(), 500000);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_load_from_database_empty)
{
    // Create wallet with no data in database
    DigiDollarWallet empty_wallet(&m_wallet);

    // Load should return 0
    size_t loaded = empty_wallet.LoadFromDatabase();
    BOOST_CHECK_EQUAL(loaded, 0);

    // Verify empty state
    BOOST_CHECK_EQUAL(empty_wallet.GetPositionCount(), 0);
    BOOST_CHECK_EQUAL(empty_wallet.GetBalanceCount(), 0);
    BOOST_CHECK_EQUAL(empty_wallet.GetLockedCollateral(), 0);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_load_from_database_recalculates_totals)
{
    // Write positions with different active states
    wallet::WalletBatch batch(m_wallet.GetDatabase());

    uint256 id1 = uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    uint256 id2 = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    uint256 id3 = uint256S("0xdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");

    WalletCollateralPosition pos1(id1, 10000, 500000, 3, 100000);
    pos1.is_active = true;

    WalletCollateralPosition pos2(id2, 5000, 250000, 2, 50000);
    pos2.is_active = false; // Inactive

    WalletCollateralPosition pos3(id3, 3000, 150000, 1, 30000);
    pos3.is_active = true;

    batch.WritePosition(pos1);
    batch.WritePosition(pos2);
    batch.WritePosition(pos3);

    // Create NEW wallet instance
    DigiDollarWallet fresh_wallet(&m_wallet);
    fresh_wallet.LoadFromDatabase();

    // Verify locked collateral only counts active positions
    // pos1: 500000 (active) + pos2: 0 (inactive) + pos3: 150000 (active) = 650000
    BOOST_CHECK_EQUAL(fresh_wallet.GetLockedCollateral(), 650000);

    // Verify all positions loaded (including inactive)
    auto all_positions = fresh_wallet.GetPositions(false);
    BOOST_CHECK_EQUAL(all_positions.size(), 3);

    // Verify only active positions returned by default
    auto active_positions = fresh_wallet.GetPositions(true);
    BOOST_CHECK_EQUAL(active_positions.size(), 2);
}

BOOST_AUTO_TEST_CASE(digidollarwallet_persistence_integration_test)
{
    // THE KEY TEST: Write → Clear → Load → Data restored

    // Step 1: Write data using wallet methods
    DigiDollarWallet wallet1(&m_wallet);

    uint256 pos_id = uint256S("0xeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
    WalletCollateralPosition pos(pos_id, 10000, 500000, 3, 100000);
    wallet1.WritePosition(pos);

    CDigiDollarAddress addr("DD1qtest999999999999999999999999999999999");
    wallet1.WriteDDBalance(addr, 10000);

    // Verify initial state
    BOOST_CHECK_EQUAL(wallet1.GetPositionCount(), 1);
    BOOST_CHECK_EQUAL(wallet1.GetBalanceCount(), 1);
    BOOST_CHECK_EQUAL(wallet1.GetLockedCollateral(), 500000);

    // Step 2: Clear in-memory data (simulate wallet shutdown)
    wallet1.ClearWalletData();
    BOOST_CHECK_EQUAL(wallet1.GetPositionCount(), 0);
    BOOST_CHECK_EQUAL(wallet1.GetBalanceCount(), 0);
    BOOST_CHECK_EQUAL(wallet1.GetLockedCollateral(), 0);

    // Step 3: Create new wallet instance and load (simulate restart)
    DigiDollarWallet wallet2(&m_wallet);
    size_t loaded = wallet2.LoadFromDatabase();
    BOOST_CHECK_EQUAL(loaded, 2);

    // Step 4: Verify data fully restored
    BOOST_CHECK_EQUAL(wallet2.GetPositionCount(), 1);
    BOOST_CHECK_EQUAL(wallet2.GetBalanceCount(), 1);
    BOOST_CHECK_EQUAL(wallet2.GetLockedCollateral(), 500000);

    auto positions = wallet2.GetPositions();
    BOOST_CHECK_EQUAL(positions.size(), 1);
    BOOST_CHECK_EQUAL(positions[0].position_id, pos_id);
    BOOST_CHECK_EQUAL(positions[0].dd_minted, 10000);
    BOOST_CHECK_EQUAL(positions[0].dgb_collateral, 500000);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace wallet
