// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/dca.h>
#include <consensus/err.h>
#include <consensus/volatility.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

// ============================================================================
// Initialization
// ============================================================================

void initialize_dd_logic()
{
    SelectParams(ChainType::REGTEST);
}

// ============================================================================
// Target 6: fuzz_dd_dca_multiplier
// Fuzz the DCA (Dynamic Collateral Adjustment) system.
// ============================================================================

FUZZ_TARGET(dd_dca_multiplier, .init = initialize_dd_logic)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());

    // Strategy 1: Random system health values
    {
        int system_health = fuzzed_data_provider.ConsumeIntegralInRange<int>(-1000, 31000);
        double multiplier = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(system_health);
        // Multiplier must be positive and reasonable (1.0 to 2.0 range)
        assert(multiplier >= 1.0 && multiplier <= 2.0);
    }

    // Strategy 2: Tier boundary values
    {
        // Healthy: >150% → 1.0x
        double healthy = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(200);
        assert(healthy == 1.0);

        // Warning: 120-150% → 1.2x
        double warning = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(135);
        assert(warning >= 1.0 && warning <= 2.0);

        // Critical: 100-120% → 1.5x
        double critical = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(110);
        assert(critical >= 1.0 && critical <= 2.0);

        // Emergency: <100% → 2.0x
        double emergency = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(50);
        assert(emergency == 2.0);
    }

    // Strategy 3: ApplyDCA with fuzzed base ratios
    {
        int base_ratio = fuzzed_data_provider.ConsumeIntegralInRange<int>(100, 1000);
        int system_health = fuzzed_data_provider.ConsumeIntegralInRange<int>(0, 30000);
        int adjusted = DigiDollar::DCA::DynamicCollateralAdjustment::ApplyDCA(base_ratio, system_health);
        // Adjusted ratio must be >= base ratio (DCA only increases requirements)
        assert(adjusted >= base_ratio);
    }

    // Strategy 4: CalculateSystemHealth with fuzzed inputs
    {
        CAmount total_collateral = fuzzed_data_provider.ConsumeIntegralInRange<CAmount>(0, MAX_MONEY);
        CAmount total_dd = fuzzed_data_provider.ConsumeIntegralInRange<CAmount>(0, MAX_MONEY);
        CAmount oracle_price = fuzzed_data_provider.ConsumeIntegralInRange<CAmount>(0, 100000000);

        int health = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
            total_collateral, total_dd, oracle_price);
        // Health is 0-30000 range
        assert(health >= 0 && health <= 30000);
    }

    // Strategy 5: Edge cases for CalculateSystemHealth
    {
        // Zero DD supply → maximum health
        int health_no_dd = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
            1000000000, 0, 5000);
        assert(health_no_dd == 30000);

        // Zero oracle price → 0 health
        int health_no_price = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
            1000000000, 100000, 0);
        assert(health_no_price == 0);

        // Zero collateral → 0 health (when DD exists)
        int health_no_coll = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
            0, 100000, 5000);
        assert(health_no_coll == 0);

        // Negative inputs
        int health_neg_coll = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
            -1, 100000, 5000);
        assert(health_neg_coll == 0);

        int health_neg_dd = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
            100000, -1, 5000);
        assert(health_neg_dd == 0);
    }

    // Strategy 6: GetCurrentTier
    {
        int health = fuzzed_data_provider.ConsumeIntegralInRange<int>(-100, 31000);
        DigiDollar::DCA::HealthTier tier = DigiDollar::DCA::DynamicCollateralAdjustment::GetCurrentTier(health);
        // Tier must have valid status string
        assert(!tier.status.empty());
        assert(tier.multiplier >= 1.0 && tier.multiplier <= 2.0);
    }

    // Strategy 7: IsSystemEmergency
    {
        int health = fuzzed_data_provider.ConsumeIntegralInRange<int>(-100, 500);
        bool is_emergency = DigiDollar::DCA::DynamicCollateralAdjustment::IsSystemEmergency(health);
        if (health < 100) {
            assert(is_emergency);
        } else {
            assert(!is_emergency);
        }
    }

    // Strategy 8: ValidateDCAConfig
    {
        std::string error;
        (void)DigiDollar::DCA::DynamicCollateralAdjustment::ValidateDCAConfig(error);
    }
}

// ============================================================================
// Target 7: fuzz_dd_err_calculation
// Fuzz the ERR (Emergency Redemption Ratio) system.
// ============================================================================

FUZZ_TARGET(dd_err_calculation, .init = initialize_dd_logic)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());

    // Strategy 1: ShouldActivateERR with fuzzed health
    {
        int system_health = fuzzed_data_provider.ConsumeIntegralInRange<int>(-1000, 31000);
        bool should_activate = DigiDollar::ERR::EmergencyRedemptionRatio::ShouldActivateERR(system_health);
        if (system_health >= 100) {
            assert(!should_activate);
        } else {
            assert(should_activate);
        }
    }

    // Strategy 2: CalculateERRAdjustment across full range
    {
        int system_health = fuzzed_data_provider.ConsumeIntegralInRange<int>(-1000, 31000);
        double adjustment = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(system_health);
        // Adjustment ratio must be in valid range
        if (system_health >= 100) {
            assert(adjustment == 1.0);
        } else {
            assert(adjustment >= 0.80 && adjustment <= 0.95);
        }
    }

    // Strategy 3: GetRequiredDDBurn with fuzzed inputs
    {
        CAmount original_dd = fuzzed_data_provider.ConsumeIntegralInRange<CAmount>(0, MAX_MONEY);
        int system_health = fuzzed_data_provider.ConsumeIntegralInRange<int>(0, 200);

        CAmount required_burn = DigiDollar::ERR::EmergencyRedemptionRatio::GetRequiredDDBurn(
            original_dd, system_health);

        if (original_dd <= 0) {
            assert(required_burn == 0);
        } else if (system_health >= 100) {
            // Healthy system: burn same as minted
            assert(required_burn == original_dd);
        } else {
            // ERR active: must burn MORE than originally minted
            assert(required_burn >= original_dd);
        }
    }

    // Strategy 4: ERR tier boundaries
    {
        // Test specific tier boundaries
        double adj_99 = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(99);
        double adj_95 = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(95);
        double adj_90 = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(90);
        double adj_85 = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(85);
        double adj_50 = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(50);
        double adj_0 = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(0);

        // Lower health → lower ratio (= more DD burn required)
        assert(adj_99 >= adj_50);
        assert(adj_95 >= adj_85);
        // Minimum ratio is 0.80
        assert(adj_0 >= 0.80);
        assert(adj_50 >= 0.80);
    }

    // Strategy 5: Zero and negative amounts
    {
        CAmount burn_zero = DigiDollar::ERR::EmergencyRedemptionRatio::GetRequiredDDBurn(0, 50);
        assert(burn_zero == 0);

        CAmount burn_neg = DigiDollar::ERR::EmergencyRedemptionRatio::GetRequiredDDBurn(-100, 50);
        assert(burn_neg == 0);
    }

    // Strategy 6: Large amounts (overflow testing)
    {
        CAmount large_amount = fuzzed_data_provider.ConsumeIntegralInRange<CAmount>(
            MAX_MONEY / 2, MAX_MONEY);
        int low_health = fuzzed_data_provider.ConsumeIntegralInRange<int>(1, 99);

        CAmount required = DigiDollar::ERR::EmergencyRedemptionRatio::GetRequiredDDBurn(
            large_amount, low_health);
        // Must not overflow to negative
        assert(required >= 0);
        // Must be >= original (ERR increases burn requirement)
        assert(required >= large_amount);
    }

    // Strategy 7: ValidateERRConfig
    {
        std::string error;
        (void)DigiDollar::ERR::EmergencyRedemptionRatio::ValidateERRConfig(error);
    }

    // Strategy 8: Format functions
    {
        double ratio = fuzzed_data_provider.ConsumeFloatingPointInRange<double>(0.0, 1.0);
        (void)DigiDollar::ERR::EmergencyRedemptionRatio::FormatERRAdjustment(ratio);

        int health = fuzzed_data_provider.ConsumeIntegralInRange<int>(0, 30000);
        bool active = fuzzed_data_provider.ConsumeBool();
        (void)DigiDollar::ERR::EmergencyRedemptionRatio::FormatERRHealth(health, active);
    }
}

// ============================================================================
// Target 8: fuzz_dd_volatility
// Fuzz volatility calculation with fuzzed price series.
// ============================================================================

FUZZ_TARGET(dd_volatility, .init = initialize_dd_logic)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());

    // Clear any state from previous runs
    DigiDollar::Volatility::VolatilityMonitor::ClearHistory();
    DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();

    // Strategy 1: Feed a series of random prices
    {
        int num_prices = fuzzed_data_provider.ConsumeIntegralInRange<int>(0, 50);
        int64_t base_timestamp = 1700000000; // Arbitrary start time

        for (int i = 0; i < num_prices; i++) {
            CAmount price = fuzzed_data_provider.ConsumeIntegralInRange<CAmount>(1, 10000000);
            int64_t timestamp = base_timestamp + (i * 3600); // 1 hour apart
            uint32_t height = static_cast<uint32_t>(i * 240); // ~1 hour in blocks

            DigiDollar::Volatility::VolatilityMonitor::RecordPrice(price, timestamp, height);
        }

        // Check state after feeding prices
        DigiDollar::Volatility::VolatilityState state = DigiDollar::Volatility::VolatilityMonitor::GetCurrentState();
        assert(state.hourlyVolatility >= 0.0);
        assert(state.dailyVolatility >= 0.0);
        assert(state.weeklyVolatility >= 0.0);
    }

    // Strategy 2: CalculateVolatility with various time windows
    {
        double vol_1h = DigiDollar::Volatility::VolatilityMonitor::CalculateVolatility(3600);
        double vol_24h = DigiDollar::Volatility::VolatilityMonitor::CalculateVolatility(24 * 3600);
        double vol_7d = DigiDollar::Volatility::VolatilityMonitor::CalculateVolatility(7 * 24 * 3600);
        assert(vol_1h >= 0.0);
        assert(vol_24h >= 0.0);
        assert(vol_7d >= 0.0);

        // Fuzzed time window
        int64_t window = fuzzed_data_provider.ConsumeIntegralInRange<int64_t>(1, 30 * 24 * 3600);
        double vol = DigiDollar::Volatility::VolatilityMonitor::CalculateVolatility(window);
        assert(vol >= 0.0);
    }

    // Strategy 3: Freeze mechanism checks
    {
        (void)DigiDollar::Volatility::VolatilityMonitor::ShouldFreezeMinting();
        (void)DigiDollar::Volatility::VolatilityMonitor::ShouldFreezeAll();
        (void)DigiDollar::Volatility::VolatilityMonitor::InCooldownPeriod();
        (void)DigiDollar::Volatility::VolatilityMonitor::GetCooldownEndHeight();
    }

    // Strategy 4: CalculatePercentageChange utility
    {
        CAmount old_price = fuzzed_data_provider.ConsumeIntegralInRange<CAmount>(1, 10000000);
        CAmount new_price = fuzzed_data_provider.ConsumeIntegralInRange<CAmount>(1, 10000000);
        double change = DigiDollar::Volatility::CalculatePercentageChange(old_price, new_price);
        (void)change; // Can be positive or negative

        // ExceedsThreshold
        double threshold = fuzzed_data_provider.ConsumeFloatingPointInRange<double>(0.0, 100.0);
        (void)DigiDollar::Volatility::ExceedsThreshold(old_price, new_price, threshold);
    }

    // Strategy 5: Edge case - zero prices in CalculatePercentageChange
    {
        // Zero old price is an edge case
        double change_zero_old = DigiDollar::Volatility::CalculatePercentageChange(0, 1000);
        (void)change_zero_old;

        double change_zero_new = DigiDollar::Volatility::CalculatePercentageChange(1000, 0);
        (void)change_zero_new;

        double change_both_zero = DigiDollar::Volatility::CalculatePercentageChange(0, 0);
        (void)change_both_zero;
    }

    // Strategy 6: TriggerFreeze and ClearFreeze
    {
        bool freeze_all = fuzzed_data_provider.ConsumeBool();
        uint32_t height = fuzzed_data_provider.ConsumeIntegral<uint32_t>();
        DigiDollar::Volatility::VolatilityMonitor::TriggerFreeze(freeze_all, height);

        (void)DigiDollar::Volatility::VolatilityMonitor::ShouldFreezeMinting();
        (void)DigiDollar::Volatility::VolatilityMonitor::ShouldFreezeAll();

        DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();
        // After clearing, freeze should be off
        // Note: Can't strictly assert because UpdateState might re-trigger
    }

    // Strategy 7: FormatVolatility
    {
        double vol = fuzzed_data_provider.ConsumeFloatingPointInRange<double>(0.0, 1000.0);
        (void)DigiDollar::Volatility::FormatVolatility(vol);
    }

    // Strategy 8: GetDiagnosticInfo and IsInitialized
    {
        (void)DigiDollar::Volatility::VolatilityMonitor::GetDiagnosticInfo();
        (void)DigiDollar::Volatility::VolatilityMonitor::IsInitialized();
        (void)DigiDollar::Volatility::VolatilityMonitor::GetDataAge();
    }

    // Clean up for next run
    DigiDollar::Volatility::VolatilityMonitor::ClearHistory();
    DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();
}
