// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/digidollar.h>
#include <digidollar/digidollar.h>
#include <digidollar/scripts.h>
#include <digidollar/txbuilder.h>
#include <digidollar/validation.h>
#include <key.h>
#include <node/miner.h>
#include <oracle/bundle_manager.h>
#include <oracle/mock_oracle.h>
#include <policy/feerate.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <test/util/txmempool.h>
#include <validation.h>

#include <string>

using node::BlockAssembler;
using node::CBlockTemplate;

namespace {

bool BlockHasTx(const CBlock& block, const uint256& txid)
{
    for (const auto& tx : block.vtx) {
        if (tx->GetHash() == txid) {
            return true;
        }
    }
    return false;
}

struct MinerDDValidationSetup : public TestChain100Setup {
    size_t m_coinbase_spend_index{0};

    MinerDDValidationSetup()
    {
        MockOracleManager::GetInstance().Reset();
        MockOracleManager::GetInstance().SetEnabled(true);
        OracleBundleManager::GetInstance().Clear();
        OracleBundleManager::GetInstance().SetEnabled(false);
        EnsureDigiDollarActive();
    }

    ~MinerDDValidationSetup()
    {
        MockOracleManager::GetInstance().Reset();
        OracleBundleManager::GetInstance().Clear();
        OracleBundleManager::GetInstance().SetEnabled(true);
    }

    void EnsureDigiDollarActive()
    {
        for (int i = 0; i < 2500; ++i) {
            const bool active = WITH_LOCK(cs_main, return DigiDollar::IsDigiDollarEnabled(m_node.chainman->ActiveChain().Tip(), *m_node.chainman));
            if (active) {
                return;
            }
            mineBlocks(1);
        }
        BOOST_FAIL("DigiDollar deployment did not activate in time");
    }

    int NextBlockHeight() const
    {
        return WITH_LOCK(cs_main, return m_node.chainman->ActiveChain().Height() + 1);
    }

    COutPoint ConfirmOpTrueFunding(CAmount output_value)
    {
        BOOST_REQUIRE(m_coinbase_spend_index < m_coinbase_txns.size());
        const int input_height = static_cast<int>(m_coinbase_spend_index + 1);
        CMutableTransaction funding = CreateValidMempoolTransaction(
            m_coinbase_txns[m_coinbase_spend_index], 0, input_height, coinbaseKey,
            CScript() << OP_TRUE, output_value, /*submit=*/false);
        ++m_coinbase_spend_index;

        const CPubKey coinbase_pubkey = coinbaseKey.GetPubKey();
        const CScript coinbase_script = CScript()
                                        << std::vector<unsigned char>(coinbase_pubkey.begin(), coinbase_pubkey.end())
                                        << OP_CHECKSIG;
        CBlock block = CreateAndProcessBlock({funding}, coinbase_script);
        BOOST_REQUIRE_GE(block.vtx.size(), 2U);
        return COutPoint(block.vtx[1]->GetHash(), 0);
    }

    CAmount RequiredCollateralAt(CAmount dd_amount, int lock_days, int next_height, CAmount oracle_price_micro_usd) const
    {
        const int64_t lock_blocks = DigiDollar::LockDaysToBlocks(lock_days);
        DigiDollar::ValidationContext ctx(next_height, oracle_price_micro_usd, 300, Params());
        return DigiDollar::CalculateRequiredCollateral(dd_amount, lock_blocks, ctx);
    }

    CTransactionRef BuildDDMint(const COutPoint& prevout, CAmount input_value, CAmount collateral_value,
                                CAmount dd_amount, int next_height, CAmount fee, int lock_days = 30)
    {
        BOOST_REQUIRE(input_value >= collateral_value + fee);

        CKey owner_key;
        owner_key.MakeNewKey(true);
        XOnlyPubKey owner_xonly(owner_key.GetPubKey());

        const int64_t lock_blocks = DigiDollar::LockDaysToBlocks(lock_days);
        const int64_t lock_height = next_height + lock_blocks;

        DigiDollar::MintParams params;
        params.ddAmount = dd_amount;
        params.lockHeight = lock_height;
        params.ownerKey = owner_xonly;
        params.internalKey = DigiDollar::GetCollateralNUMSKey();
        params.oracleKeys = DigiDollar::GetOracleKeys(15);

        CMutableTransaction mint;
        mint.SetDigiDollarType(DD_TX_MINT);
        mint.vin.emplace_back(prevout);
        mint.vout.emplace_back(collateral_value, DigiDollar::CreateCollateralP2TR(params));
        mint.vout.emplace_back(0, DigiDollar::CreateDigiDollarP2TR(owner_xonly, dd_amount));

        CScript op_return = CScript() << OP_RETURN
                                      << std::vector<unsigned char>{'D', 'D'}
                                      << CScriptNum(1)
                                      << CScriptNum(dd_amount)
                                      << CScriptNum(lock_height)
                                      << CScriptNum(lock_days == 30 ? 1 : 0)
                                      << std::vector<unsigned char>(owner_xonly.begin(), owner_xonly.end());
        mint.vout.emplace_back(0, op_return);
        return MakeTransactionRef(mint);
    }

    CTransactionRef BuildOpTrueSpend(const COutPoint& prevout, CAmount input_value, CAmount fee)
    {
        BOOST_REQUIRE(input_value > fee);
        CMutableTransaction tx;
        tx.vin.emplace_back(prevout);
        tx.vout.emplace_back(input_value - fee, CScript() << OP_TRUE);
        return MakeTransactionRef(tx);
    }

    CTransactionRef BuildDDTransfer(const COutPoint& prevout, const XOnlyPubKey& recipient, CAmount dd_amount)
    {
        CMutableTransaction tx;
        tx.SetDigiDollarType(DD_TX_TRANSFER);
        tx.vin.emplace_back(prevout);
        tx.vout.emplace_back(0, DigiDollar::CreateDigiDollarP2TR(recipient, dd_amount));

        CScript op_return = CScript() << OP_RETURN
                                      << std::vector<unsigned char>{'D', 'D'}
                                      << CScriptNum(2)
                                      << CScriptNum(dd_amount);
        tx.vout.emplace_back(0, op_return);
        return MakeTransactionRef(tx);
    }

    bool ValidateMintAtPrice(const CTransaction& tx, int next_height, CAmount oracle_price_micro_usd, std::string* reject_reason = nullptr) const
    {
        DigiDollar::ValidationContext ctx(next_height, oracle_price_micro_usd, 300, Params());
        TxValidationState state;
        const bool valid = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
        if (reject_reason) {
            *reject_reason = state.GetRejectReason();
        }
        return valid;
    }

    void AddToMempool(const CTransactionRef& tx, CAmount fee)
    {
        LOCK2(cs_main, m_node.mempool->cs);
        TestMemPoolEntryHelper entry;
        m_node.mempool->addUnchecked(entry.Fee(fee).FromTx(tx));
    }

    std::unique_ptr<CBlockTemplate> BuildTemplate(const BlockAssembler::Options& options)
    {
        return BlockAssembler(m_node.chainman->ActiveChainstate(), m_node.mempool.get(), options)
            .CreateNewBlock(CScript() << OP_TRUE, ALGO_SHA256D);
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(miner_dd_validation_tests)

BOOST_FIXTURE_TEST_CASE(block_skips_stale_dd_mint, MinerDDValidationSetup)
{
    constexpr CAmount kHighPrice = 50000; // $0.05
    constexpr CAmount kLowPrice = 45000;  // $0.045
    constexpr CAmount kDDAmount = 10000;  // $100.00
    constexpr CAmount kFee = 1000;

    const int next_height = NextBlockHeight();
    const CAmount required_high = RequiredCollateralAt(kDDAmount, /*lock_days=*/30, next_height, kHighPrice);
    const COutPoint funding = ConfirmOpTrueFunding(required_high + kFee);
    const CTransactionRef stale_mint = BuildDDMint(funding, required_high + kFee, required_high, kDDAmount, next_height, kFee);

    std::string reject_low;
    BOOST_REQUIRE(ValidateMintAtPrice(*stale_mint, next_height, kHighPrice));
    BOOST_REQUIRE(!ValidateMintAtPrice(*stale_mint, next_height, kLowPrice, &reject_low));
    BOOST_CHECK_EQUAL(reject_low, "insufficient-collateral");

    MockOracleManager::GetInstance().SetMockPrice(kLowPrice);
    AddToMempool(stale_mint, kFee);

    BlockAssembler::Options options;
    options.blockMinFeeRate = CFeeRate(0);
    options.test_block_validity = false;

    auto block_template = BuildTemplate(options);
    BOOST_REQUIRE(block_template);
    BOOST_CHECK(!BlockHasTx(block_template->block, stale_mint->GetHash()));
}

BOOST_FIXTURE_TEST_CASE(block_succeeds_without_dd_after_failure, MinerDDValidationSetup)
{
    constexpr CAmount kHighPrice = 50000;
    constexpr CAmount kLowPrice = 45000;
    constexpr CAmount kDDAmount = 10000;
    constexpr CAmount kFee = 1000;
    constexpr CAmount kStdInput = 2 * COIN;

    MockOracleManager::GetInstance().SetMockPrice(kHighPrice);

    const int next_height = NextBlockHeight();
    const CAmount required_high = RequiredCollateralAt(kDDAmount, 30, next_height, kHighPrice);
    const COutPoint dd_funding = ConfirmOpTrueFunding(required_high + kFee);
    const COutPoint std_funding = ConfirmOpTrueFunding(kStdInput);

    const CTransactionRef dd_mint = BuildDDMint(dd_funding, required_high + kFee, required_high, kDDAmount, next_height, kFee);
    const CTransactionRef std_tx = BuildOpTrueSpend(std_funding, kStdInput, kFee);

    AddToMempool(dd_mint, kFee);
    AddToMempool(std_tx, kFee);

    bool hook_called{false};
    BlockAssembler::Options options;
    options.blockMinFeeRate = CFeeRate(0);
    options.test_block_validity = true;
    options.on_before_test_block_validity = [&]() {
        hook_called = true;
        MockOracleManager::GetInstance().SetMockPrice(kLowPrice);
    };

    std::unique_ptr<CBlockTemplate> block_template;
    BOOST_REQUIRE_NO_THROW(block_template = BuildTemplate(options));
    BOOST_REQUIRE(block_template);
    BOOST_CHECK(hook_called);
    BOOST_CHECK(BlockHasTx(block_template->block, std_tx->GetHash()));
    BOOST_CHECK(!BlockHasTx(block_template->block, dd_mint->GetHash()));
}

BOOST_FIXTURE_TEST_CASE(mint_includes_safety_margin, RegTestingSetup)
{
    constexpr CAmount kOraclePrice = 50000; // $0.05
    constexpr CAmount kDDAmount = 10000;    // $100
    constexpr int kLockDays = 30;

    DigiDollar::MintTxBuilder builder(Params(), /*height=*/1000, kOraclePrice);
    const CAmount with_margin = builder.CalculateRequiredCollateral(kDDAmount, kLockDays);

    const int64_t lock_blocks = DigiDollar::LockDaysToBlocks(kLockDays);
    const auto& dd_params = Params().GetDigiDollarParams();
    const int base_ratio = DigiDollar::GetCollateralRatioForLockTime(lock_blocks, dd_params);

    __int128 numerator = static_cast<__int128>(kDDAmount) *
                         static_cast<__int128>(COIN) *
                         static_cast<__int128>(base_ratio) * 100;
    const CAmount base_required = static_cast<CAmount>(numerator / static_cast<__int128>(kOraclePrice));
    const CAmount expected_with_margin = static_cast<CAmount>((static_cast<__int128>(base_required) * 101) / 100);

    BOOST_CHECK_EQUAL(with_margin, expected_with_margin);
    BOOST_CHECK(with_margin > base_required);
}

BOOST_FIXTURE_TEST_CASE(addPackageTxs_skips_invalid_dd, MinerDDValidationSetup)
{
    constexpr CAmount kPrice = 45000;   // $0.045
    constexpr CAmount kDDAmount = 10000;
    constexpr CAmount kFee = 1000;

    MockOracleManager::GetInstance().SetMockPrice(kPrice);

    const int next_height = NextBlockHeight();
    const CAmount required = RequiredCollateralAt(kDDAmount, 30, next_height, kPrice);
    BOOST_REQUIRE(required > COIN);
    const CAmount insufficient_collateral = required - COIN;

    const COutPoint funding = ConfirmOpTrueFunding(insufficient_collateral + kFee);
    const CTransactionRef invalid_mint = BuildDDMint(funding, insufficient_collateral + kFee, insufficient_collateral, kDDAmount, next_height, kFee);
    AddToMempool(invalid_mint, kFee);

    BlockAssembler::Options options;
    options.blockMinFeeRate = CFeeRate(0);
    options.test_block_validity = false;

    auto block_template = BuildTemplate(options);
    BOOST_REQUIRE(block_template);
    BOOST_CHECK(!BlockHasTx(block_template->block, invalid_mint->GetHash()));
}

BOOST_FIXTURE_TEST_CASE(test_block_validity_retry, MinerDDValidationSetup)
{
    constexpr CAmount kHighPrice = 50000;
    constexpr CAmount kLowPrice = 45000;
    constexpr CAmount kDDAmount = 10000;
    constexpr CAmount kFee = 1000;

    MockOracleManager::GetInstance().SetMockPrice(kHighPrice);

    const int next_height = NextBlockHeight();
    const CAmount required_high = RequiredCollateralAt(kDDAmount, 30, next_height, kHighPrice);
    const COutPoint funding = ConfirmOpTrueFunding(required_high + kFee);
    const CTransactionRef borderline_mint = BuildDDMint(funding, required_high + kFee, required_high, kDDAmount, next_height, kFee);

    std::string reject_low;
    BOOST_REQUIRE(ValidateMintAtPrice(*borderline_mint, next_height, kHighPrice));
    BOOST_REQUIRE(!ValidateMintAtPrice(*borderline_mint, next_height, kLowPrice, &reject_low));
    BOOST_CHECK_EQUAL(reject_low, "insufficient-collateral");

    AddToMempool(borderline_mint, kFee);

    bool hook_called{false};
    BlockAssembler::Options options;
    options.blockMinFeeRate = CFeeRate(0);
    options.test_block_validity = true;
    options.on_before_test_block_validity = [&]() {
        hook_called = true;
        MockOracleManager::GetInstance().SetMockPrice(kLowPrice);
    };

    std::unique_ptr<CBlockTemplate> block_template;
    BOOST_REQUIRE_NO_THROW(block_template = BuildTemplate(options));
    BOOST_REQUIRE(block_template);
    BOOST_CHECK(hook_called);
    BOOST_CHECK(!BlockHasTx(block_template->block, borderline_mint->GetHash()));
    BOOST_CHECK_EQUAL(block_template->block.vtx.size(), 1U);
}

BOOST_FIXTURE_TEST_CASE(block_includes_chained_dd_transfers_from_mempool, MinerDDValidationSetup)
{
    constexpr CAmount kPrice = 50000;
    constexpr CAmount kDDAmount = 10000;
    constexpr CAmount kFee = 1000;

    MockOracleManager::GetInstance().SetMockPrice(kPrice);

    const int next_height = NextBlockHeight();
    const CAmount required = RequiredCollateralAt(kDDAmount, 30, next_height, kPrice);
    const COutPoint funding = ConfirmOpTrueFunding(required + kFee);
    const CTransactionRef mint = BuildDDMint(funding, required + kFee, required, kDDAmount, next_height, kFee);

    std::string reject_reason;
    BOOST_REQUIRE(ValidateMintAtPrice(*mint, next_height, kPrice, &reject_reason));

    CKey transfer1_key;
    transfer1_key.MakeNewKey(true);
    const XOnlyPubKey transfer1_xonly(transfer1_key.GetPubKey());

    CKey transfer2_key;
    transfer2_key.MakeNewKey(true);
    const XOnlyPubKey transfer2_xonly(transfer2_key.GetPubKey());

    const COutPoint mint_dd_out(mint->GetHash(), 1);
    const CTransactionRef transfer1 = BuildDDTransfer(mint_dd_out, transfer1_xonly, kDDAmount);
    const COutPoint transfer1_dd_out(transfer1->GetHash(), 0);
    const CTransactionRef transfer2 = BuildDDTransfer(transfer1_dd_out, transfer2_xonly, kDDAmount);

    AddToMempool(mint, kFee);
    AddToMempool(transfer1, kFee);
    AddToMempool(transfer2, kFee);

    BlockAssembler::Options options;
    options.blockMinFeeRate = CFeeRate(0);
    options.test_block_validity = false;

    auto block_template = BuildTemplate(options);
    BOOST_REQUIRE(block_template);
    BOOST_CHECK(BlockHasTx(block_template->block, mint->GetHash()));
    BOOST_CHECK(BlockHasTx(block_template->block, transfer1->GetHash()));
    BOOST_CHECK(BlockHasTx(block_template->block, transfer2->GetHash()));
}

BOOST_AUTO_TEST_SUITE_END()
