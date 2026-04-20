# DigiByte v9.26.0-rc31 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## No Testnet Reset

RC31 is a **bug-fix and oracle-rotation release** on the existing `testnet23` chain. No reset, no new genesis, no network-magic change. Oracle operators running RC30 can drop in RC31 binaries.

---

## What's New in RC31

RC31 addresses feedback and bugs reported against RC30 within the first 24 hours of release, rotates a single oracle key, and prepares RC31's MuSig2 bundle-signing fixes identified by community testers.

## RC31 fix summary since RC30

- **Oracle slot 11 key rotation** — hallvardo's original RC28 keypair was unrecoverable on his current Ubuntu testnet node; a fresh keypair generated on the node that actually runs the oracle now replaces the old chainparams entry. Serves as the first real-world test of DigiDollar's oracle key-replacement flow.
- _(MuSig2 bundle-signing fixes — forthcoming this RC)_
- _(Qt mint-dialog stale oracle price refresh — forthcoming this RC)_
- _(Qt dark-mode Peers table contrast — forthcoming this RC)_
- _(Mint "Invalid amount format" → "Minimum $100" error-message regression — forthcoming this RC)_

_This release notes document will be expanded as each fix lands. Final release notes published at tag time._

## Known Issues

- **Slot 12 (DaPunzy)** startoracle flow reports `"Oracle key already exists in wallet for oracle_id 12"` after an earlier `createoraclekey`. Workaround under discussion in Gitter.
- **`sendmanydigidollar` RPC** not yet implemented — tracked as a feature request from the PayDesk team for a future RC.

## Upgrade Path

1. Stop RC30 node.
2. Install RC31 binary.
3. Start. Data dir and wallet remain compatible — no migration step.
4. Oracle operator for slot 11 (hallvardo) must ensure the new keypair (`024ef063…744ca08`) is loaded in the wallet running on the oracle node.

## Downloads

Binaries for all five platforms attached to the GitHub release once Guix builds complete.

## Commits

_Populated at tag time via `git log --oneline v9.26.0-rc30..HEAD`._
