# RC41 Issues

Final validation and fix ledger for the RC41 work pass on May 26, 2026.

This document treats the reported issues as triage leads, not proof. Each item below was checked against code and covered with a focused regression test, guard test, or documented verification path before or during the fix. The two new documented issues added after the initial list are entries 17 and 18.

## Summary

- Total tracked issues: 18
- Confirmed defects or valid risks fixed: 15
- Not-a-current-bug / guard or hardening only: 2
- Enhancement implemented: 1
- Deferred issues: 0

## Issue Ledger

| # | Status | Validation and fix | Tests / verification | Commit |
|---|---|---|---|---|
| 1 | Guarded, not a current Core loss-of-change bug | Validated that mint change is wallet-owned and spendable; added regression coverage for pending DD state and DGB change visibility. Remaining risk is a separate conflict/restart UI path, not missing change ownership. | `test/functional/wallet_digidollar_rc33_regressions.py --descriptors` | `139e8ecb551c4c73bebc7edc9f002d2dfd953803` |
| 2 | Fixed | Confirmed duplicate broadcast-before-wallet-commit in RPC/Qt mint could leave wallet state inconsistent and hit `!wtx.InMempool()`. Mint now commits through the wallet relay path once. | Rapid mint regression in `wallet_digidollar_rc33_regressions.py`; full unit/Qt/fuzz coverage. | `f148f5b4e60e984f33e6a85c93b5a59eeb6a3d1d` |
| 3 | Fixed | Confirmed fragmented UTXO auto-consolidation could stop after an insufficient first pass. Consolidation now completes enough passes and retries mint from usable consolidation outputs. | High-fragmentation functional regression in `wallet_digidollar_rc33_regressions.py`. | `33ca60e94fb679b86f7744a775a9642e1032b22a` |
| 4 | Fixed | Confirmed DD send/status refresh could block or stale out on UI-thread RPC/backend work. DD send and status refresh now use async/queued refresh paths and explicit DD update signals. | Qt refresh/responsiveness tests in `src/qt/test/digidollarwidgettests.cpp`. | `2d8b0808deb47ad62f8b63114db3b1f7f016e1c1` |
| 5 | Hardened, not a startup bug | The startup message is expected before chainstate readiness, but stale position validation now retries after readiness. | Wallet security unit coverage in `src/wallet/test/digidollar_wallet_security_tests.cpp`. | `402c9cdd4504b68a958fe0543ce3c4be83c40258` |
| 6 | Fixed | Confirmed redeem UI could show redeemable vault state while wallet signing state was locked/stale. Redeem state now refreshes on wallet lock/unlock and explains locked-wallet signing. | Qt tests for locked wallet redeem state and unlock refresh. | `515ca6d8f42cc32a2e2f7abb95dad51734b495f0` |
| 7 | Fixed | Confirmed custom/native tooltip paths could bypass readable dark-mode styling. Tooltip rendering was normalized globally, including native `QEvent::ToolTip` handling, item-view tooltip handling, and rich-text envelope stripping. | Qt tooltip guard tests, normal Qt suite, and X11 visual screenshot `/tmp/digibyte_tooltip_qa.png` showing readable black-on-yellow tooltip text. | `64c72f2a4eb73cdce220e1059d7efffd2630b262` |
| 8 | Fixed | Confirmed shutdown window was not covered by dark dialog rules. Added explicit shutdown object/style coverage for dark mode. | `darkThemeShutdownWindowHasReadableSurface` Qt test. | `f1257101d017d40e1117d543ffcc0d4ae9bfa5a5` |
| 9 | Fixed | Confirmed `Total DD` used white-on-white styling in dark mode. Updated dark CSS contrast. | `darkThemeDigiDollarSendTotalLabelHasReadableContrast` Qt test. | `cb44ecde3dae084de97c6f6129bda98a2eeeb71c` |
| 10 | Fixed | Confirmed escaped custom overview tooltips could show literal `<qt>` tags. Normalized custom tooltip rendering and stripped Qt rich-text wrappers before escaping. | `customTooltipRenderersNormalizeQtRichTextEnvelope` Qt test plus X11 tooltip visual QA. | `64c72f2a4eb73cdce220e1059d7efffd2630b262` |
| 11 | Fixed | Confirmed below-minimum mint feedback needed to report the actual chain minimum. Mint validation copy now reports the configured minimum. | Qt mint minimum-copy guard test. | `b4152410f33d60a14abbe54c9417134885d2308e` |
| 12 | Fixed | Confirmed tier 0 was inconsistently validated and explained. Wallet validation accepts tier 0 and UI copy separates lock duration from effective redeem availability. | Unit tier-0 validation plus Qt mint confirmation-copy tests. | `8c82100b231b5a95085dda91f0cd6efc9ca469fb` |
| 13 | Fixed in DGBstats | Confirmed HomePage truncated fractional testnet algo difficulties with `parseInt`, and DifficultiesPage needed Core algo-name normalization. | DGBstats unit tests for fractional values below 1 and `odocrypt`/Core aliases. | `/home/jared/Code/dgbstats` `931984f3f48a473d1b15095d35858e150e56512e` |
| 14 | Fixed in DGBstats; server inspected | Confirmed stale oracle/testnet25 copy: generic GitHub onboarding, 7 exchanges/CoinMarketCap, v0x02 fallback, and 15-second price updates. Updated DGBstats pages to RC41/testnet25, six active exchanges, v0x03-only, 60-second exchange fetch/broadcast, and assigned-slot coordination. DGBstats Server was inspected and did not require a code change. | DGBstats page tests and `OracleCopyGuards.test.js`; DGBstats Server `npm test`. | `/home/jared/Code/dgbstats` `8e68bcebc1437c0a21677dcb0c145dee194315b3` |
| 15 | Fixed / enhancement implemented | DD Overview recent transactions now switch to the DD Transactions tab and focus the matching row on double-click/activation. DD Transactions rows now open a non-modal DigiDollar details window on double-click/activation, matching the normal DGB transaction workflow while using a separate `DDTransactionDescDialog` object and green DD-specific light/dark styling. Follow-up fixes apply that green style directly on the dialog widgets so DD transaction details, DD coin selection, DD payment request, and DD address book dialogs cannot inherit normal DGB blue `QDialog` styling even when the global stylesheet falls back to blue. During X11 QA this also exposed an RPC warmup `-28` table-load stall; the DD Transactions widget now falls back to direct wallet history while RPC is warming up. | `overviewRecentTransactionDoubleClickOpensTransactionsTab`, `transactionsWidgetDoubleClickShowsDetailsDialog`, `transactionsWidgetDetailsDialogOverridesDgbBlueDialogFallback`, `transactionsWidgetDetailsDialogHasDigiDollarThemeRules`, `digiDollarModalDialogsUseGreenThemeRules`, `digiDollarModalDialogsOverrideDgbBlueFallback`, normal Qt suite, and X11 dark/light screenshots for DD transaction details plus `/tmp/digibyte_dd_coin_control_dark_qa.png`, `/tmp/digibyte_dd_coin_control_light_qa.png`, `/tmp/digibyte_dd_receive_request_dark_qa.png`, `/tmp/digibyte_dd_receive_request_light_qa.png`, `/tmp/digibyte_dd_address_book_dark_qa.png`, and `/tmp/digibyte_dd_address_book_light_qa.png`. | `d0605a19ef92410d506a7bcafeb26fd9f2ae4083`, `a28c7e2d764402f2a3d9bb614e9200ef7b924f47`, `8dcf61c097168886b0be583165f761d6b08d8537`, `d57b7875e8e4418baa5d4442987b55c4dc352441` |
| 16 | Fixed | Confirmed DD Send field was labeled `Label` while only storing local note metadata. Renamed UI and tooltip to local `Note`. | `sendWidgetNoteFieldTests` Qt test. | `ead2b2e3f79a1fc31081079aee5818107ac0e8e2` |
| 17 | Fixed / clarified | New issue. Confirmed rapid batch mint state could be confusing and lock-tier rejects lacked enough context. DD history now distinguishes local/stempool/mempool/rejected/confirmed state more clearly and logs lock-tier validation windows with tx context. | Lock-tier unit guard and Qt transaction/overview state tests. | `65f15d4775dcff7b0d633919f6981bbe75571b16` |
| 18 | Fixed | New issue. Confirmed RPC redeem had the same broadcast-before-wallet-commit shape as the mint assertion. Redeem now commits through the wallet relay path once and rapid duplicate redeems fail cleanly. | Rapid redeem functional regression in `wallet_digidollar_rc33_regressions.py`. | `d7097b05f627255c5f7fed6635adaa040c0d2a8a` |

## Additional Commit

- `1aa0e43334d6f66273de809733dd6ce2d8d89c3c` - `test: align DigiDollar owner key commit guard`. Full unit testing found a stale source-text guard that still searched for the old lower-case helper name. The implementation already stored the owner key before `CommitTransaction(txRef, ...)`; the guard now checks the actual wallet-owned commit call.

## Qt Visual QA

Automated offscreen Qt QA and X11 screenshot QA were used. The full Qt binary passed with `QT_QPA_PLATFORM=offscreen`, including explicit dark-mode and visual-surface guard tests for:

- Global/custom/native tooltip normalization and dark-mode tooltip readability, visually captured in `/tmp/digibyte_tooltip_qa.png`.
- Transaction overview custom tooltip path, including rich-text `<qt>` envelope stripping.
- Shutdown window dark-mode surface contrast.
- DigiDollar Send `Total DD` label contrast.
- Redeem widget with redeemable vault but encrypted/locked wallet.
- DigiDollar Send `Note` label/copy.
- DigiDollar Overview recent transaction double-click navigation to DD Transactions.
- DigiDollar Transactions double-click details dialog in dark and light mode, visually captured in `/tmp/digibyte_dd_transaction_details_dark_qa.png` and `/tmp/digibyte_dd_transaction_details_light_qa.png` with DD green styling and readable text/buttons.
- DigiDollar coin selection, payment request, and address book dialogs in dark and light mode, visually captured in `/tmp/digibyte_dd_coin_control_dark_qa.png`, `/tmp/digibyte_dd_coin_control_light_qa.png`, `/tmp/digibyte_dd_receive_request_dark_qa.png`, `/tmp/digibyte_dd_receive_request_light_qa.png`, `/tmp/digibyte_dd_address_book_dark_qa.png`, and `/tmp/digibyte_dd_address_book_light_qa.png`.

Manual/X11 visual QA screenshots were captured after the headless guard tests so the affected tooltip, transaction-detail, and DD modal surfaces were inspected as rendered windows.

## Verification Matrix

Focused checks run during TDD:

- DigiByte Core: `python3 test/functional/wallet_digidollar_rc33_regressions.py --descriptors` - passed.
- DigiByte Core: `src/test/test_digibyte --run_test=digidollar_locktier_tests/wave7_locktier_duration_reject_log_includes_txid_and_window` - passed.
- DigiByte Core: `src/test/test_digibyte --run_test=digidollar_wallet_hd_tests/wave1_qt_persists_owner_key_before_broadcast` - passed after guard update.
- DigiByte Core: `env QT_QPA_PLATFORM=offscreen src/qt/test/test_digibyte-qt` - passed.
- DGBstats: focused page tests for HomePage, DifficultiesPage, DDStatsPage, OraclesPage, DigiDollarPage, RoadmapPage, and OracleCopyGuards - passed.

Full checks:

- DigiByte Core build: `make` - passed.
- DigiByte Core RC41 multi-oracle/DD end-to-end: `./test_multi_oracle_testnet.sh` - passed end to end; 223 tracked checks, 222 OK, 0 failed, with warning-only live-market observations. Live oracle consensus price during the run was about `$0.003555`/DGB, all DD mint/redeem/transfer/persistence checks passed, and the script log is `/tmp/digidollar_debug_logs/test_run_20260526_152328.log`.
- DigiByte Core unit suite: `src/test/test_digibyte` - passed, 3384 test cases.
- DigiByte Core Qt suite: `env QT_QPA_PLATFORM=offscreen src/qt/test/test_digibyte-qt` - passed after the DD modal styling fix; DigiDollar widget tests reported 78 passed / 0 failed / 3 skipped and Wave19 widget tests reported 14 passed / 0 failed.
- DigiByte Core X11 DD modal visual QA: `env QT_QPA_PLATFORM=xcb DIGIBYTE_QT_DD_MODAL_VISUAL_QA_ONLY=1 src/qt/test/test_digibyte-qt -eventdelay 0 -keydelay 0 -mousedelay 0 digiDollarModalDialogsVisualQaDarkAndLight` - passed with the six DD modal screenshots listed above. The env-gated runner hook was temporary and was removed before commit.
- DigiByte Core fuzz: `python3 test/fuzz/test_runner.py --par 8 /tmp/digibyte-fuzz-seed` - passed all 247 targets with one seed input per target. `--empty_min_time` was attempted first but is unsupported by this non-libFuzzer build.
- DigiByte Core functional suite: `python3 test/functional/test_runner.py` - passed, 371/371 scheduled jobs completed with environment-gated skips only; accumulated test duration 2491 s, wall runtime 673 s. Test runner also warned that `feature_assumeutxo.py` and `feature_assumevalid.py` are not in the configured test list.
- DGBstats unit/integration: `npm run test:run` - passed, 23 files / 523 tests.
- DGBstats build: `npm run build` - passed with pre-existing ESLint warnings.
- DGBstats E2E: `npm run test:e2e` - failed, 292 passed / 1270 failed / 102 skipped out of 1664. Failures were broad pre-existing Playwright data/browser-matrix issues: loading states that never hide, missing mocked API/WebSocket data, touch-target thresholds, route-specific visual/data expectations, and Firefox/WebKit/Mobile Safari project failures in this environment.
- DGBstats Server: `npm test` - passed, 8 files / 152 tests.

## Remaining Risk

- DGBstats Playwright E2E remains red independently of the RC41 issue fixes. The RC41 DGBstats changes are covered by passing Vitest tests and production build.
- Qt visual QA now includes X11 screenshot artifacts for the tooltip, DD transaction-details, and DD modal-dialog surfaces listed above.
