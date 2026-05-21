# DigiByte Core v9.26.0-rc41 Release Notes

RC41 is a DigiDollar follow-up hardening and usability release candidate on top of RC40.

This release keeps the public DigiDollar testnet unchanged and focuses on the last RC40 feedback items: mainnet mint-floor enforcement, stricter mint metadata validation, no-wallet build safety, receive-request amount validation, Dandelion final-reject wallet cleanup, and small Qt usability fixes.

Development branch: `feature/digidollar-v1`

Release: https://github.com/DigiByte-Core/digibyte/releases/tag/v9.26.0-rc41

---

## Summary

RC41 does not reset the public DigiDollar testnet.

It keeps:

- Testnet: `testnet24`
- DigiDollar activation height: `600`
- Oracle activation height: `600`
- Oracle quorum: `9-of-17`
- Oracle bundle format: `v0x03` MuSig2 aggregate bundles
- Existing DigiDollar economic rules

RC41 is not a protocol redesign. It is a focused correctness, wallet-state, documentation, and Qt cleanup release.

---

## What Changed

### Mainnet minimum mint enforcement

RC41 fixes the mainnet DigiDollar mint-floor parameter.

Wallet and RPC paths already enforced the documented $100 minimum mint amount. Mainnet consensus parameters now enforce the same floor for raw DigiDollar mint transactions as soon as DigiDollar activates on mainnet.

### Stricter mint lock-height validation

RC41 rejects malformed mint OP_RETURN metadata with a missing, non-minimal, zero, or negative lock height.

Before this fix, validation could normalize a bad lock height into a default testing value. The mint path now fails closed instead of inventing consensus-critical metadata.

### Stale redemption metadata repair

RC41 includes the post-RC40 wallet repair for stale DigiDollar mint metadata before redemption.

The wallet now uses the original mint transaction as the authority for amount, collateral, lock tier, and unlock height before reconciliation, redeem construction, and signing. If mint metadata cannot be verified, redemption fails closed and tells the user to rescan or restore the wallet.

### No-wallet build boundary

RC41 fixes `--disable-wallet` builds.

Node-level DigiDollar RPC code remains available, while wallet-only DigiDollar RPC helpers compile only when wallet support is enabled. Wallet aggregate health has a no-wallet stub instead of linking against missing wallet symbols.

### DigiDollar receive-request amount validation

RC41 fixes malformed amount handling in the Qt DigiDollar Receive panel.

Malformed amounts such as `12.bad` are rejected before address generation. Valid request amounts are parsed as fixed-point DigiDollar cents, stored consistently, and emitted in payment URIs in canonical form.

### Qt amount label width

RC41 widens the DigiDollar overview amount label area so larger displayed values fit cleanly without crowding nearby text.

### Dandelion final-reject wallet cleanup

RC41 fixes a wallet-state bug when a Dandelion stem transaction is accepted locally but later rejected during final mempool promotion.

Rejected stem transactions are now removed from the stempool and wallet-owned DigiDollar transactions are marked abandoned when they are no longer confirmed or in mempool. This prevents failed mints or sends from staying visible as live pending transactions.

### DigiDollar transaction sorting

RC41 preserves the user's selected sort order in the DigiDollar Transactions table.

The first load still defaults to Date descending, but later refreshes no longer force the table back to Date descending every few seconds.

### Mint collateral ratio bar

RC41 changes the Qt mint collateral-ratio progress bar to show the practical 200%-500% safety band.

The exact collateral ratio text is unchanged. Ratios above 500% still display their real value in the label, while the visual bar clamps to full so it does not imply that 500% is only a partial safety level.

### Pending DigiDollar balance styling

RC41 adds explicit light and dark theme rules for the Pending DD row on the Overview page.

The pending balance calculation did not change. The row now uses the same spacing, alignment, font sizing, and theme colors as the adjacent balance rows.

### Documentation audit

RC41 includes a full documentation audit against current code.

Core navigation, DigiDollar architecture, oracle, activation, wallet integration, exchange integration, setup, and repo-map documents were updated so future developers see the current implementation instead of older plans or stale assumptions.

---

## What Did Not Change

RC41 does not change:

- Mainnet activation status.
- Testnet network identity.
- Testnet genesis.
- Default testnet ports.
- Oracle roster.
- Oracle quorum.
- Oracle epoch length.
- On-chain oracle bundle format.
- DigiDollar address formats.
- DigiDollar wallet database format.
- ERR policy.
- DCA policy.
- P2P message formats in a breaking way.

RC41 keeps the RC40 network and economic model intact.

---

## Testnet24 Network Details

| Item | RC41 value |
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

Older operator notes that mention `testnet23` or P2P port `12030` are stale for RC41. Use the values above.

---

## Validation Status

Focused RC41 validation completed on May 21, 2026 from `feature/digidollar-v1`.

| Gate | Status |
| --- | --- |
| Build: `make -j"$(nproc)"` | PASS |
| Dandelion unit tests: `./src/test/test_digibyte --run_test=dandelion_tests --log_level=error --report_level=short` | PASS |
| DigiDollar dropped-pending-mint wallet regression | PASS |
| DigiDollar pending-redeem cleanup regression | PASS |
| Qt tests: `./src/qt/test/test_digibyte-qt -platform offscreen` | PASS |
| Whitespace check: `git diff --check` | PASS |

Validation logs:

- Dandelion unit tests: `/tmp/rc40_final_dandelion.log`
- Dropped pending mint wallet regression: `/tmp/rc40_final_wallet_mint_abandon.log`
- Pending redeem cleanup regression: `/tmp/rc40_final_wallet_redeem_cleanup.log`
- Qt tests: `/tmp/rc40_final_qt.log`

---

## Commit Summary Since RC40

- `852be68e4f` digidollar wallet: repair stale mint metadata before redemption
- `d5f1e6c575` doc: update RC40 notes for redemption cache fix
- `16f6b77c8e` Update .gitignore
- `f466b7413b` docs: audit DigiByte and DigiDollar docs against current code
- `0607f081da` digidollar mainnet: fix DD-RHF-009 minimum mint enforcement
- `68fe7a2836` digidollar qt: fix DD-RHF-010 amount label width
- `3c28172cae` digidollar mint: fix DD-RHF-011 lock height validation
- `3b6f61d4bb` digidollar tests: accept DD-RHF-011 metadata rejection
- `98563f1940` digidollar wallet: fix DD-RHF-012 no-wallet build boundary
- `af69648808` digidollar qt: fix DD-RHF-013 receive amount validation
- `eaa79030e6` release: bump version to v9.26.0-rc41
- `645125c040` dandelion: drop rejected stem transactions
- `0a3a6b8434` digidollar qt: preserve transaction sort order
- `c8a62bc036` digidollar qt: clamp mint ratio bar scale
- `d473616a55` digidollar qt: style pending balance row

---

## Notes For Testers

Please focus RC41 testing on:

- Mainnet/testnet mint amount limits after activation.
- Raw or manually constructed mint transactions with bad OP_RETURN lock-height metadata.
- Redemption of older positions whose wallet metadata may have been stale.
- `--disable-wallet` build behavior.
- DigiDollar Receive requests with valid, blank, and malformed amount fields.
- Dandelion-enabled DigiDollar mints and sends that later leave stem relay.
- DD Transactions table sorting across refreshes.
- Mint collateral-ratio display for 200%, 500%, and 1000% tiers.
- Pending DD row appearance in light and dark themes.

Oracle operators should continue using `testnet24` and keep assigned oracle slots online.

---

## Known Risks

- RC41 does not include mainnet activation. Mainnet launch still requires the explicit release and activation decision.
- If fewer than 9 valid oracle operators are online and fresh, new oracle bundles should fail closed.
- Mixed older RC oracle nodes may not reliably complete the current MuSig2 signing flow.
- Windows tooltip reports still need exact affected controls or screenshots before a safe theme fix can be made.

---

## Bottom Line

RC41 is a follow-up hardening release candidate after RC40.

It keeps `testnet24`, keeps the DigiDollar oracle and economic model unchanged, and fixes the last validated RC40 feedback items without adding new product behavior.
