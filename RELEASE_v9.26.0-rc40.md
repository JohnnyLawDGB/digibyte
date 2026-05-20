# DigiByte Core v9.26.0-rc40 Release Notes

RC40 is the DigiDollar final hardening and launch-readiness release candidate on top of RC39.

This release focuses on real security findings from the final Red Hornet review: activation guards, mint validation, sendmany safety, redemption collateral release, MuSig2 attempt isolation, wallet locked-state behavior, Qt display safety, RPC coverage, the final DigiDollar Qt launch-readiness pass, and a post-RC40 redemption cache repair for existing wallets.

Development branch: `feature/digidollar-v1`

Release: https://github.com/DigiByte-Core/digibyte/releases/tag/v9.26.0-rc40

---

## Summary

RC40 does not reset the public DigiDollar testnet.

It keeps:

- Testnet: `testnet24`
- DigiDollar activation height: `600`
- Oracle activation height: `600`
- Oracle quorum: `9-of-17`
- Oracle bundle format: `v0x03` MuSig2 aggregate bundles
- Existing DigiDollar economic rules

RC40 is a security-hardening and readiness release. It does not activate mainnet by itself.

---

## What Changed

### DigiDollar RPC activation guards

RC40 fixes and proves the pre-activation RPC gate.

Operational DigiDollar and oracle RPC commands now reject before DigiDollar BIP9 activation. The only intentional exception is `getdigidollardeploymentinfo`, which must remain available so users can monitor deployment status before activation.

The final recheck covered the live node and wallet RPC tables:

- 32 registered DigiDollar/oracle RPC commands.
- 31 operational commands requiring BIP9 activation.
- 31/31 operational commands verified as BIP9-gated.
- `getdigidollardeploymentinfo` remains ungated by design.

### Mint amount validation

RC40 tightens mint input parsing so malformed or unsafe mint amounts cannot slip through RPC parsing into transaction construction.

This protects the mint path before wallet code builds collateral and token outputs.

### DigiDollar sendmany duplicate-recipient rejection

RC40 rejects duplicate recipients in `sendmanydigidollar`.

This prevents user-facing confusion where the same recipient could appear more than once in a batch and make the intended send amount unclear.

### Redemption collateral release validation

RC40 strengthens redemption checks so collateral release must match the correct DigiDollar position and burn requirement.

The important rule remains simple: redemption cannot release locked collateral unless the right position is redeemed with the required DigiDollar burn.

### MuSig2 attempt isolation

RC40 fixes a MuSig2 attempt-preemption issue.

Oracle signing attempts are isolated so stale or lower-priority attempt data cannot replace the active attempt in a way that breaks signing convergence.

### MuSig2 context proposal bounds

RC40 adds bounds checking for context proposal handling.

This prevents oversized or malformed context proposal data from becoming a practical resource or validation problem.

### Wallet locked-state and spendability safety

RC40 fixes DigiDollar wallet RPC spendability reporting for locked and encrypted wallet states.

Locked wallets must not make DigiDollar outputs look safely spendable when signing cannot actually proceed.

### Qt mint unlock-height display

RC40 fixes the Qt mint unlock-height display.

The UI now reports the intended unlock height consistently with the actual lock tier behavior, reducing the risk of users misunderstanding when collateral can be redeemed.

### Redemption unlock-height cache repair

RC40 repairs stale DigiDollar wallet metadata before redemption.

Some wallets created by an earlier Qt mint path could store a local unlock height that was 100 blocks lower than the mint transaction's OP_RETURN unlock height. The 100-block difference is the DigiDollar mint confirmation buffer. Consensus and mempool validation were correct and rejected those early redemption attempts, but the wallet UI could show the vault as expired while the transaction was still built with the stale local height.

The wallet now treats the original mint transaction as authoritative for the DigiDollar amount, collateral amount, lock tier, and unlock height. Wallet rescan, position reconciliation, wallet redeem, RPC redeem, and redemption signing now repair the cached row from the mint transaction before building or signing a redemption. If the mint metadata cannot be verified, redemption fails closed and tells the user to rescan or restore the wallet.

The redeem RPC also no longer falls back to collateral-only data. Collateral alone is not enough to safely choose the redemption locktime, burn amount, or signing path.

This does not change DigiDollar consensus rules, oracle rules, wallet database format, or valid mint transactions. It fixes stale local wallet metadata so existing positions use the same unlock height that validation already enforces.

### Final DigiDollar Qt UI/UX readiness pass

RC40 includes the final DigiDollar Qt UI pass after RC39.

The wallet UI now makes the first-use DigiDollar flows clearer:

- Overview USD values show an explicit `USD` suffix.
- Overview privacy masking hides digits and amount units.
- Overview network totals have more room and avoid awkward label wrapping.
- Recent DigiDollar transaction amounts are right-aligned with enough width for clean decimal alignment.
- Receive DD starts with a clear empty state and hides QR/address details until an address is generated or selected.
- The Receive DD `Generate New Address` button has an explicit enabled style before hover.
- Redeem DD and DD Vault buttons explain no vault selected, timelock active, insufficient DD, wallet locked, watch-only/private-key-disabled wallets, invalid amount, and ready-to-redeem states.

These are UI and test changes only. They do not change DigiDollar consensus, oracle behavior, wallet accounting, mining, RPC schemas, or P2P formats.

### Test isolation fixes

RC40 includes two test-only fixes for ERR state isolation.

These do not change product behavior. They make the test suite deterministic so Red Hornet validation results are reliable.

---

## What Did Not Change

RC40 does not change:

- Mainnet activation status.
- Testnet network identity.
- Testnet genesis.
- Default testnet ports.
- Oracle roster.
- Oracle quorum.
- Oracle epoch length.
- On-chain oracle bundle format.
- DigiDollar economics.
- Wallet database format.
- RPC schema in a breaking way.
- P2P message formats in a breaking way.

RC40 is not a consensus redesign. It is a focused hardening release.

---

## Testnet24 Network Details

| Item | RC40 value |
| --- | --- |
| Testnet name | `testnet24` |
| Data directory | `testnet24` |
| Genesis hash | `0xe42636c490059fafe7e0278acc6fb451b901b6a316b31e10d7ccff565baf23df` |
| Merkle root | `0x502bf477644933ced36281bbfdcc6755895b3d9f75262eb148d2c1c2c21d7e73` |
| Genesis time | `2026-05-11 13:53:00 UTC` |
| Genesis nonce | `57535` |
| Network magic | `fe c4 b7 e5` |
| Default P2P port | `12031` |
| Default RPC port | `14026` |
| DigiDollar activation height | `600` |
| Oracle activation height | `600` |
| Oracle epoch length | `40` blocks |
| Oracle quorum | `9-of-17` |
| Oracle bundle format | `v0x03` MuSig2 aggregate bundle |

Older operator notes that mention `testnet23` or P2P port `12030` are stale for RC40. Use the values above.

---

## Validation Status

Final Red Hornet validation completed on May 19, 2026 from `feature/digidollar-v1`.

Post-fix RC40 redemption-cache validation completed on May 20, 2026 from commit `852be68e4f78eaa520f9c7b0da2d788b12ed2f1f`.

| Gate | Status |
| --- | --- |
| Build: `make -j"$(nproc)"` | PASS |
| Unit tests: `./src/test/test_digibyte --show_progress` | PASS, 3,376 test cases |
| Qt tests: `./src/qt/test/test_digibyte-qt -platform offscreen` | PASS |
| Functional tests: `test/functional/test_runner.py --jobs=4` | PASS, 371 selected tests completed |
| Extended functional tests: `test/functional/test_runner.py --jobs=4 --extended` | PASS, 375 selected tests completed |
| Fuzz target enumeration | PASS, 247 targets |
| Fuzz corpus replay | PASS, all selected targets completed against the available corpus |
| Post-final RPC guard recheck: `test/functional/digidollar_rpc_gating.py` | PASS, all 31/31 gated RPCs blocked before activation |
| Redemption stale-cache regression | PASS, a cache 100 blocks below OP_RETURN is repaired before reconciliation and redemption paths |
| Final Qt UI focused recheck: `DigiDollarWidgetTests` | PASS, 60 DigiDollar widget tests and 13 Wave 19 widget tests |
| Final Qt wallet manual QA | PASS, live regtest wallet launched and inspected on `DISPLAY=:1` |

Validation logs:

- Build: `/tmp/red_hornet_final_make.log`
- Unit tests: `/tmp/red_hornet_final_unit.log`
- Qt tests: `/tmp/red_hornet_final_qt.log`
- Functional tests: `/tmp/red_hornet_final_functional.log`
- Extended functional tests: `/tmp/red_hornet_final_functional_extended.log`
- Fuzz target list: `/tmp/red_hornet_final_fuzz_targets.txt`
- Fuzz run: `/tmp/red_hornet_final_fuzz.log`
- RPC guard recheck: `/tmp/rc40_rpc_gating_recheck.log`
- Final Qt UI focused recheck: `/tmp/rc39_qt_digidollar_widget_rerun.log`
- Final Qt wallet manual QA screenshots: `/tmp/rc39_qt_rebuilt_digidollar_overview.png`, `/tmp/rc39_qt_receive_empty.png`, `/tmp/rc39_qt_receive_generated.png`, `/tmp/rc39_qt_redeem_tooltip.png`, `/tmp/rc39_qt_dd_transactions_empty.png`
- Post-fix Qt test: `/tmp/rc40_stale_cache_green_qt.log`
- Post-fix unit tests: `/tmp/rc40_stale_cache_unit.log`
- Post-fix functional tests: `/tmp/rc40_stale_cache_functional.log`
- Post-fix extended functional tests: `/tmp/rc40_stale_cache_functional_extended.log`
- Post-fix fuzz target list: `/tmp/rc40_stale_cache_fuzz_targets.txt`
- Post-fix fuzz existing-corpus replay: `/tmp/rc40_stale_cache_fuzz_existing_corpus.log`
- Post-fix libFuzzer availability proof: `/tmp/rc40_fuzz_probe.log`

Fuzz mode used:

- Final Red Hornet corpus replay used `/tmp/rc38_qa_assets/fuzz_corpora`.
- The post-fix replay used the available local corpus at `/tmp/rc37_fuzz_corpus`.
- The local build was not a libFuzzer build, so timed empty-corpus fuzzing could not run. The probe reported `Must be built with libFuzzer`.
- All registered targets selected by the local fuzz binary completed against the available corpus.

---

## Commit Summary Since RC39

- `634942c5bb` doc: streamline RC39 release notes
- `b47914414f` digidollar rpc: fix DD-RHF-001 pre-activation list gating
- `3187e358de` digidollar mint: fix DD-RHF-002 malformed amount validation
- `178f7116a0` Update .gitignore
- `e00a9a3c63` digidollar rpc: fix DD-RHF-003 duplicate sendmany recipients
- `4f40349043` digidollar redemption: fix DD-RHF-004 collateral return validation
- `23ddf9c94a` digidollar musig2: fix DD-RHF-005 attempt preemption
- `ba64275d93` digidollar wallet rpc: fix DD-RHF-006 locked spendability
- `c60ec6104c` digidollar qt: fix DD-RHF-007 mint unlock height
- `9bb5943f20` digidollar tests: fix W12-TG-001 ERR state isolation
- `05d9305edd` digidollar tests: fix W12-TG-002 RH44 ERR isolation
- `38a15698a8` digidollar musig2: fix DD-RHF-008 context proposal bounds
- `6c19156e9c` digidollar rpc: cover all BIP9-gated commands
- `9f987cc616` doc: add RC40 release notes
- `cb7b1a8899` digidollar qt receive: clarify address generation state
- `21f753efb9` digidollar qt overview: normalize USD and amount display
- `e14664aec9` digidollar qt redeem: explain vault unlock states
- `852be68e4f` digidollar wallet: repair stale mint metadata before redemption

---

## Notes For Testers

Please focus RC40 testing on the hardened paths:

- RPC behavior before and after DigiDollar BIP9 activation.
- Mint amount validation and rejected malformed amounts.
- `sendmanydigidollar` duplicate-recipient rejection.
- Redemption collateral release and required DigiDollar burn.
- Existing DigiDollar positions minted before the final cache repair; redemption should use the unlock height recorded in the mint transaction.
- Locked and encrypted wallet DigiDollar spendability reporting.
- Qt mint lock tier and unlock-height display.
- DigiDollar Overview, Receive DD, Redeem DD, DD Vault, and DD Transactions UI clarity.
- Oracle MuSig2 attempt behavior after restart or reconnect.
- Oracle context proposal handling under malformed or oversized input.

Oracle operators should continue using `testnet24` and keep assigned oracle slots online.

---

## Known Risks

- RC40 does not include mainnet activation. Mainnet launch still requires the explicit release and activation decision.
- If fewer than 9 valid oracle operators are online and fresh, new oracle bundles should fail closed.
- Mixed older RC oracle nodes may not reliably complete the current MuSig2 signing flow.
- Operator setup material outside these release notes should be checked for stale `testnet23` or `12030` references before public publication.

---

## Bottom Line

RC40 is the DigiDollar final hardening candidate after the Red Hornet review.

The code passed the final build, unit, Qt, functional, extended functional, fuzz, and RPC activation-guard checks. No unfixed DigiDollar code blocker remains in this candidate.
