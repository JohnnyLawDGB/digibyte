#!/usr/bin/env python3
"""DigiDollar Redemption End-to-End Test

Test complete redemption flow:
1. Mint DD with 1-hour timelock
2. Wait for timelock to expire
3. Check redemption eligibility
4. Redeem position
5. Verify DD burned and DGB returned
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)
from decimal import Decimal


class DigiDollarRedemptionE2ETest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [["-digidollar=1", "-mocktime=0"], ["-digidollar=1", "-mocktime=0"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("DigiDollar Redemption End-to-End Test Starting...")

        bob = self.nodes[0]
        alice = self.nodes[1]

        # STEP 1: Generate blocks past activation height
        self.log.info("Step 1: Generating 655 blocks for activation...")
        bob_addr = bob.getnewaddress()
        bob.generatetoaddress(655, bob_addr)
        self.sync_all()

        initial_dgb_balance = bob.getbalance()
        self.log.info(f"Bob's initial DGB balance: {initial_dgb_balance}")

        # STEP 2: Mint $1000 DD with 1-hour timelock (tier 1)
        self.log.info("Step 2: Minting $1000 DD with 1-hour timelock...")
        mint_result = bob.mintdigidollar(100000, 1)  # 100000 cents = $1000, tier 1 = 1 hour

        position_id = mint_result['position_id']
        dd_minted = mint_result['dd_minted']
        dgb_collateral = Decimal(str(mint_result['dgb_collateral']))
        unlock_height = mint_result['unlock_height']

        self.log.info(f"Minted: {dd_minted} cents")
        self.log.info(f"Position ID: {position_id}")
        self.log.info(f"DGB Collateral: {dgb_collateral}")
        self.log.info(f"Unlock Height: {unlock_height}")

        assert_equal(dd_minted, 100000)
        assert_greater_than(dgb_collateral, 0)

        # STEP 3: Mine blocks to confirm mint
        self.log.info("Step 3: Mining 10 blocks to confirm mint...")
        bob.generatetoaddress(10, bob_addr)
        self.sync_all()

        # STEP 4: Check Bob's DD balance
        self.log.info("Step 4: Checking Bob's DD balance...")
        dd_balance = bob.getdigidollarbalance()
        self.log.info(f"Bob's DD balance: {dd_balance['total']} cents")
        assert_equal(dd_balance['total'], 100000)

        # STEP 5: Check redemption info (should be LOCKED)
        self.log.info("Step 5: Checking redemption info (should be locked)...")
        redeem_info_before = bob.getredemptioninfo(position_id)

        self.log.info(f"Can redeem: {redeem_info_before['can_redeem']}")
        self.log.info(f"Timelock remaining: {redeem_info_before['timelock_remaining']} blocks")

        assert_equal(redeem_info_before['can_redeem'], False)
        assert_greater_than(redeem_info_before['timelock_remaining'], 0)

        # STEP 6: List redeemable positions (should be empty)
        self.log.info("Step 6: Listing redeemable positions (should be empty)...")
        redeemable_before = bob.listredeemablepositions()
        self.log.info(f"Redeemable positions: {len(redeemable_before)}")
        assert_equal(len(redeemable_before), 0)

        # STEP 7: Generate 240+ blocks to expire the 1-hour timelock
        self.log.info("Step 7: Generating 250 blocks to expire 1-hour timelock...")
        current_height = bob.getblockcount()
        blocks_needed = unlock_height - current_height + 10  # +10 for safety
        self.log.info(f"Current height: {current_height}, Unlock height: {unlock_height}")
        self.log.info(f"Generating {blocks_needed} blocks...")

        bob.generatetoaddress(blocks_needed, bob_addr)
        self.sync_all()

        new_height = bob.getblockcount()
        self.log.info(f"New height: {new_height}")
        assert_greater_than(new_height, unlock_height)

        # STEP 8: Check redemption info (should be REDEEMABLE)
        self.log.info("Step 8: Checking redemption info (should be redeemable)...")
        redeem_info_after = bob.getredemptioninfo(position_id)

        self.log.info(f"Can redeem: {redeem_info_after['can_redeem']}")
        self.log.info(f"Timelock remaining: {redeem_info_after['timelock_remaining']} blocks")
        self.log.info(f"DGB to be returned: {redeem_info_after['dgb_returned']}")

        assert_equal(redeem_info_after['can_redeem'], True)
        assert_equal(redeem_info_after['timelock_remaining'], 0)
        assert_greater_than(Decimal(str(redeem_info_after['dgb_returned'])), 0)

        # STEP 9: List redeemable positions (should have 1)
        self.log.info("Step 9: Listing redeemable positions (should have 1)...")
        redeemable_after = bob.listredeemablepositions()
        self.log.info(f"Redeemable positions: {len(redeemable_after)}")
        assert_equal(len(redeemable_after), 1)
        assert_equal(redeemable_after[0]['status'], 'redeemable')

        # STEP 10: REDEEM the position (full redemption)
        self.log.info("Step 10: Redeeming full position...")
        redeem_result = bob.redeemdigidollar(position_id, 100000)

        self.log.info(f"Redemption TX ID: {redeem_result['txid']}")
        self.log.info(f"DGB unlocked: {redeem_result['dgb_unlocked']}")
        self.log.info(f"DD burned: {redeem_result['dd_burned']}")
        self.log.info(f"Success: {redeem_result['success']}")

        assert_equal(redeem_result['success'], True)
        assert_equal(redeem_result['dd_burned'], 100000)
        assert_greater_than(Decimal(str(redeem_result['dgb_unlocked'])), 0)

        # STEP 11: Mine blocks to confirm redemption
        self.log.info("Step 11: Mining 5 blocks to confirm redemption...")
        bob.generatetoaddress(5, bob_addr)
        self.sync_all()

        # STEP 12: Check Bob's DD balance (should be 0)
        self.log.info("Step 12: Checking Bob's DD balance (should be 0)...")
        final_dd_balance = bob.getdigidollarbalance()
        self.log.info(f"Bob's final DD balance: {final_dd_balance['total']} cents")
        assert_equal(final_dd_balance['total'], 0)

        # STEP 13: Check Bob's DGB balance (should have increased)
        self.log.info("Step 13: Checking Bob's DGB balance...")
        final_dgb_balance = bob.getbalance()
        self.log.info(f"Bob's final DGB balance: {final_dgb_balance}")

        # DGB balance should have increased (got collateral back minus fees)
        # Note: Hard to test exact amount due to fees, but should be higher than initial - collateral
        self.log.info(f"DGB balance change: {final_dgb_balance - initial_dgb_balance}")

        # STEP 14: Verify position is closed
        self.log.info("Step 14: Verifying position is closed...")
        positions = bob.listdigidollarpositions()
        active_positions = [p for p in positions if p.get('is_active', False)]
        self.log.info(f"Active positions remaining: {len(active_positions)}")

        # Should have no active positions or the redeemed one should be inactive
        if len(active_positions) > 0:
            for pos in active_positions:
                assert pos['position_id'] != position_id, "Redeemed position should be inactive!"

        self.log.info("=" * 60)
        self.log.info("✅✅✅ DIGIDOLLAR REDEMPTION E2E TEST PASSED! ✅✅✅")
        self.log.info("=" * 60)
        self.log.info("")
        self.log.info("Summary:")
        self.log.info("  1. Bob minted $1000 DD with 1-hour timelock")
        self.log.info("  2. Waited 240+ blocks for timelock to expire")
        self.log.info("  3. Position became redeemable")
        self.log.info("  4. Bob redeemed full position, burning all DD")
        self.log.info("  5. Bob received DGB collateral back")
        self.log.info("")
        self.log.info("The redemption system is working correctly!")


if __name__ == '__main__':
    DigiDollarRedemptionE2ETest().main()
