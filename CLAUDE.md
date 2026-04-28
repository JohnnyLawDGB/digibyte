# CLAUDE.md — AI Agent Guide for DigiByte Core / DigiDollar

## What this repo is

DigiByte Core v9.26 (Bitcoin Core v26.2 lineage) on the `feature/digidollar-v1` branch, augmented with the **DigiDollar** native USD-pegged stablecoin and a multi-oracle DGB/USD price feed network with MuSig2 Schnorr threshold consensus.

Working directory: `/home/jared/Code/digibyte`. Main branch: `develop`.

## Required reading order before any DigiDollar / oracle work

1. `ARCHITECTURE.md` — Core DigiByte system design
2. `REPO_MAP.md` — Core DigiByte file index (excludes DigiDollar/oracle subsystem)
3. `DIGIDOLLAR_ARCHITECTURE.md` — DigiDollar mint/transfer/redeem/collateral/health/state
4. `DIGIDOLLAR_ORACLE_ARCHITECTURE.md` — Exchange aggregation, bundle lifecycle, MuSig2, P2P
5. `REPO_MAP_DIGIDOLLAR.md` — DigiDollar/oracle file index (production, wallet, RPC, Qt, tests, fuzz)
6. `DIGIDOLLAR_ACTIVATION_EXPLAINER.md` — BIP9 gating of DD/oracle surface
7. `ORACLE_DISCOVERY_ARCHITECTURE.md` — Oracle endpoint discovery design

For protocol questions: `DIGIDOLLAR_EXPLAINER.md`, `DIGIDOLLAR_ORACLE_EXPLAINER.md`.
For setup/operator: `DIGIDOLLAR_ORACLE_SETUP.md`, `docs/ORACLE_OPERATOR_GUIDE.md`.
For integrators: `DIGIDOLLAR_WALLET_INTEGRATION.md`, `DIGIDOLLAR_EXCHANGE_INTEGRATION.md`.

## Repo layout (DigiDollar / oracle surface)

```
src/digidollar/      # 5 modules: digidollar, health, scripts, txbuilder, validation
src/oracle/          # Oracle daemon + MuSig2: bundle_manager, exchange, mock_oracle,
                     # node, signing_orchestrator, musig2_{aggregator,session,
                     # session_manager,orchestrator,oracle_participation,messages,
                     # session_mining}
src/consensus/       # dca, err, digidollar, digidollar_transaction_validation,
                     # digidollar_tx, volatility (DigiDollar/oracle consensus rules)
src/index/           # digidollarstatsindex (DD supply/health index)
src/primitives/      # oracle.h (price message + bundle types)
src/rpc/             # digidollar.cpp (18 base RPCs); digidollar_transactions.cpp is
                     # legacy / unregistered
src/wallet/          # digidollarwallet.{cpp,h}, ddcoincontrol.h; wallet/rpc/wallet.cpp
                     # registers 13 wallet-context DD/oracle RPCs
src/qt/              # 10 widgets: digidollar{tab,mintwidget,sendwidget,receivewidget,
                     # redeemwidget,overviewwidget,positionswidget,transactionswidget,
                     # coincontroldialog,receiverequest}
src/test/            # ~150 DigiDollar/oracle/MuSig2/Red-Hornet unit tests + fuzz/
src/wallet/test/     # 3 DD wallet tests (persistence, security, rh59 lock-bypass)
src/qt/test/         # digidollarwidgettests
test/functional/     # ~50 DD/oracle Python tests
```

## DigiDollar opcode soft-fork additions

Defined in `src/script/script.h:210-220`:
- `OP_DIGIDOLLAR    = 0xbb` (OP_NOP11)
- `OP_DDVERIFY      = 0xbc` (OP_NOP12)
- `OP_CHECKPRICE    = 0xbd` (OP_NOP13) — wired to live oracle consensus price; no mock fallback
- `OP_CHECKCOLLATERAL = 0xbe` (OP_NOP14)
- `OP_ORACLE        = 0xbf` (OP_NOP15) — coinbase oracle bundle marker

These are NOPs until `SCRIPT_VERIFY_DIGIDOLLAR` is set, which only happens when BIP9 `DEPLOYMENT_DIGIDOLLAR` is ACTIVE (bit 23).

## Activation summary (BIP9 bit 23)

| Network | Start | Min activation height | Window | Threshold | Status |
|---------|-------|----------------------|--------|-----------|--------|
| Mainnet | 2026-05-01 | 22,014,720 | 40,320 blocks (~1 week) | 70% | Pending |
| Testnet (testnet23) | Genesis (already past) | 600 | 200 blocks | 70% | Active |
| Regtest | ALWAYS_ACTIVE | 0 | n/a | n/a | Active |

`nDDActivationHeight` and `nOracleActivationHeight` align with BIP9 `min_activation_height` per network. Mainnet `nOracleActivationHeight = 3000000` is currently below `nDDActivationHeight`; testnet/regtest activate together. `nDigiDollarPhase3Height = 0` on all networks (MuSig2 always available once DigiDollar is active).

## Oracle roster

| Network | Total slots | Active | Consensus |
|---------|-------------|--------|-----------|
| Mainnet | 30 (`vOracleNodes`) | 17 (`vOraclePublicKeys` slots 0–16) | 9-of-17 MuSig2 |
| Testnet | 30 | 17 (slots 0–16) | 9-of-17 MuSig2 |
| Regtest | 7 | 7 | 4-of-7 |

Slots 17–29 on mainnet/testnet are reserve `vOracleNodes` entries; they are *not* in `consensus.vOraclePublicKeys` and do *not* participate in consensus.

`primitives/oracle.h` legacy constants (`ORACLE_TOTAL_COUNT=30`, `ORACLE_ACTIVE_COUNT=15`, `ORACLE_CONSENSUS_REQUIRED=8`) are header defaults; chainparams overrides them per network.

## DigiDollar critical constants

```python
# DigiByte chain values (do not use Bitcoin defaults)
BLOCK_TIME            = 15            # seconds
COINBASE_MATURITY     = 8             # blocks (100 after height threshold = COINBASE_MATURITY_2)
SUBSIDY               = 72000         # DGB
MAX_MONEY             = 21_000_000_000

# Fees (DigiByte uses DGB/kB, not DGB/vB)
MIN_RELAY_TX_FEE      = 0.001         # DGB/kB
DEFAULT_TRANSACTION_FEE = 0.1         # DGB/kB

# Network
P2P_PORT_MAINNET      = 12024
P2P_PORT_TESTNET      = 12025  # testnet23 uses 12030 in chainparams (see DIGIDOLLAR_ORACLE_SETUP.md)

# Address formats
REGTEST_BECH32        = 'dgbrt'
TESTNET_BECH32        = 'dgbt'
DD_ADDR_PREFIX        = 'DD'  / 'TD' / 'RD' (mainnet/testnet/regtest)

# DigiDollar-specific
DD_AMOUNT_UNIT        = 1 cent (10000 = $100.00)
DD_TX_VERSION_MARKER  = 0x0770 (low 16 bits of nVersion)
DD_TX_TYPE_FIELD      = (nVersion >> 24) & 0xFF  # 1=MINT, 2=TRANSFER, 3=REDEEM
MINT_MIN              = 10_000   cents
MINT_MAX              = 10_000_000 cents
LOCK_TIERS            = 0..9 (1h .. 10y)
```

## Where DigiDollar/oracle is gated at runtime

| Layer | Gate | Reference |
|-------|------|-----------|
| RPC | `DigiDollar::IsDigiDollarEnabled(tip, chainman)` at the top of each DD/oracle RPC | `src/rpc/digidollar.cpp` (10+ callsites) |
| Mempool | `IsDigiDollarEnabled` + `HasDigiDollarMarker` | `src/validation.cpp` |
| Block | Same checks during `ConnectBlock` | `src/validation.cpp` |
| Script | `SCRIPT_VERIFY_DIGIDOLLAR` flag | `src/validation.cpp` |
| P2P | `Consensus::IsOracleActive(params, height)` for ORACLEPRICE/BUNDLE/CONSENSUS/ATTESTATION/MUSIGNONCE/MUSIGPARTIALSIG/GETORACLES | `src/net_processing.cpp` (handlers ~5440–6210) |
| Qt | `DigiDollarTab` activation overlay; widgets check `isVisible()` before polling | `src/qt/digidollartab.cpp` |
| Price cache | `UpdatePriceCache` gated on `DEPLOYMENT_DIGIDOLLAR` | `src/validation.cpp` (rh61 fix) |

## RPC surface (31 commands)

18 commands registered via `RegisterDigiDollarRPCCommands()` in `src/rpc/digidollar.cpp:4793`:
`getdigidollarstats`, `getdcamultiplier`, `calculatecollateralrequirement`, `getdigidollardeploymentinfo`, `importdigidollaraddress`, `estimatecollateral`, `getoracleprice`, `getalloracleprices`, `getprotectionstatus`, `getoracles`, `listoracle`, `stoporacle`, `getoraclepubkey`, `setmockoracleprice` (regtest), `getmockoracleprice` (regtest), `simulatepricevolatility` (regtest), `enablemockoracle` (regtest), `submitoracleprice` (regtest/Phase 2 testing).

13 wallet-context commands registered in `GetWalletRPCCommands()` at `src/wallet/rpc/wallet.cpp` ~line 962:
`mintdigidollar`, `senddigidollar`, `sendmanydigidollar`, `redeemdigidollar`, `listdigidollarpositions`, `listdigidollaraddresses`, `getredemptioninfo`, `getdigidollarbalance`, `getdigidollaraddress`, `listdigidollartxs`, `validateddaddress`, `createoraclekey`, `startoracle`.

`sendoracleprice` is intentionally **removed** as a security vulnerability (fake-price injection). Oracle prices come exclusively from live exchange aggregation.

`src/rpc/digidollar_transactions.cpp` declares `getdigidollarinfo`, `transferdigidollar`, `createrawddtransaction`, `listredeemablepositions` but is **not registered** anywhere; treat as legacy/dead code unless wired in by a future change.

## Build / test commands

```bash
# Build
./autogen.sh && ./configure && make -j$(nproc)

# Run a specific C++ test suite
./src/test/test_digibyte --run_test=digidollar_validation_tests
./src/test/test_digibyte --list_content | grep -Ei 'digidollar|oracle|musig|^rh'

# Run a functional test
./test/functional/digidollar_basic.py

# Find DigiDollar/oracle code
rg -n 'digidollar|DigiDollar|oracle|MuSig|musig' src/ test/
```

## Important notes

- **Confirmed-only DigiDollar transfers.** Unconfirmed DD chaining was removed (commit `0b4959f563`). Consensus refuses to resolve DD amounts from `MEMPOOL_HEIGHT` inputs for transfer/redeem; wallets must wait for confirmation between sends.
- **OP_CHECKPRICE has no mock fallback in production.** It consults the live oracle consensus price via `g_get_oracle_consensus_price` (commit `f77678cd0f`); zero/unavailable fails closed.
- **Mainnet/testnet validator parity.** The mainnet oracle-validation short-circuit was removed (commit `f0d9a7b2c7`); both networks honor the same Phase 3 gates.
- **Mining graceful degradation.** `CreateNewBlock` strips DD txs that fail validation in `mapModifiedTx` (rather than hanging) and continues with the rest of the block (commit `6b5ff516c3`).
- **BIP324 V2 P2P transport** is supported and enabled with `-v2transport=1` (off by default).
- **Three-way comparison still applies for non-DigiDollar work.** Compare v8.26 ↔ v8.22.2 ↔ Bitcoin v26.2 in `digibyte-v8.22.2/` and `bitcoin-v26.2-for-digibyte/` when making changes to inherited code.
- **Avoid widening doc claims beyond what code shows.** All material claims in the approved docs (16 listed in `Z_PROMPTS.md`) must match `src/`. Treat code as truth.

## Quick orientation for sub-agents

When spawning a sub-agent on DigiDollar/oracle work, point it at this file plus the four most relevant docs (`DIGIDOLLAR_ARCHITECTURE.md`, `DIGIDOLLAR_ORACLE_ARCHITECTURE.md`, `REPO_MAP_DIGIDOLLAR.md`, `DIGIDOLLAR_ACTIVATION_EXPLAINER.md`) and an explicit list of files it should read/modify. Do not ask sub-agents to redesign architecture; treat existing approved docs as the contract.
