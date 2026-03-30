# DigiByte v9.26.0-rc27 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## ⚠️ TESTNET RESET — READ THIS FIRST

**RC27 resets DigiDollar testnet to `testnet20`.**

This is **not** the same chain as RC19–RC26.

- **New testnet data directory:** `testnet20`
- **New default P2P port:** **12034** (was 12033)
- **Same default RPC port:** **14025**
- **Oracle consensus:** **6-of-11**
- **Oracle bundle format:** **MuSig2 aggregate signing (`v0x03`)**

### What operators and testers need to do

1. **Stop your RC26 node**
2. **Back up your old `testnet19` directory**
3. **Install RC27**
4. **Start RC27 once** so it creates `testnet20`
5. If you want to keep your oracle wallet/keys, **copy your old oracle wallet into `testnet20/wallets/oracle/`**
6. Start the node again and verify your wallet/oracle loads correctly

### Fast migration example

```bash
digibyte-cli -testnet stop
mv ~/.digibyte/testnet19 ~/.digibyte/testnet19.backup
# replace binaries with RC27

digibyted -testnet -daemon
# stop once testnet20 is created if you need to copy wallets
digibyte-cli -testnet stop

cp -r ~/.digibyte/testnet19.backup/wallets/oracle ~/.digibyte/testnet20/wallets/

digibyted -testnet -daemon
```

If you are an oracle operator:

```bash
digibyte-cli -testnet loadwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

### 🔑 Oracle Operators

Oracle auto-start behavior from prior releases still applies:
- **Unencrypted wallets:** oracle starts automatically when wallet loads
- **Encrypted wallets:** oracle starts automatically after `walletpassphrase`

If auto-start does not trigger for any reason, use the manual `startoracle` command above.

---

## What's New in RC27

RC27 is the **testnet reset + MuSig2 rollout release** for DigiDollar oracle signing.

### 1. Testnet reset to `testnet20`

RC27 starts a fresh DigiDollar testnet on **`testnet20`** while keeping the existing testnet genesis/magic-byte family intact.

What changed:
- data directory moved from `testnet19` → `testnet20`
- default testnet P2P port moved from **12033** → **12034**
- testnet oracle peer endpoints were updated to match the new port

What did **not** change for this reset:
- no new genesis block for this RC27 reset path
- no new testnet magic bytes for this RC27 reset path

### 2. Oracle consensus updated to 6-of-11

Mainnet/testnet oracle configuration now uses the real 11-operator set with **6-of-11** consensus.

This replaces the prior smaller testnet quorum model and gives the network better operator redundancy while still being practical for coordinated signing.

### 3. MuSig2 aggregate signing (`v0x03`)

RC27 rolls DigiDollar oracle bundles onto **MuSig2 aggregate signatures**.

That means:
- participating oracle signers contribute nonces and partial signatures
- signatures are aggregated into a single compact Schnorr signature
- bundle validation follows the aggregate-signature path for `v0x03` oracle bundles

### 4. Oracle bundle / consensus hardening

The RC27 line also carries the broader MuSig2 implementation work:
- secp256k1 MuSig2 module integration
- MuSig2 signing session/orchestrator work
- oracle bundle serialization/deserialization support for `v0x03`
- expanded unit coverage for activation, bundle handling, aggregation, mining, net processing, and orchestration

### Test Suite

Final RC27 validation is being run on the combined `feature/digidollar-v1` branch state before release/tagging.

---

## Commits Since RC26

```text
3281f9a3f0 net: bump testnet directory and P2P port for reset
fa830acdf6 Remove phase3 gate for MuSig2 on main/test and update 6-of-11 oracle assumptions
a5cda93a73 Set main/test oracle consensus to 6-of-11 before Musig2 merge
d98a0028a0 fix: correct SignSchnorr API usage in musig2_basic_tests
867753de8c build: update secp256k1 subtree to v0.6.0 (adds MuSig2 module)
391a0481d4 build: add MuSig2SigningSession to build system
2828259a2e feat: add MuSig2SigningSession header (state machine interface)
b792f97090 feat: implement MuSig2SigningSession state machine
adcdc8789e oracle: add v0x03 MuSig2 script create/extract handling
1b0b00a59b test: add MuSig2 bundle manager v0x03 round-trip coverage
dadcfdace7 build: include musig2_bundle_manager_tests in unit test target
ef032561ad feat: implement MuSig2OracleAggregator
16f85ca6bc feat: add MuSig2OracleAggregator header (bitmap + key aggregation interface)
c2b058ed17 test: add MuSig2OracleAggregator tests (TDD — tests before implementation)
b14620a080 security: use memory_cleanse for secnonce zeroing in MuSig2SigningSession
be7dddfee9 feat: implement v0x03 serialization/deserialization in oracle.cpp
9ae376fb37 test: add v0x03 MuSig2 bundle format tests (TDD first)
4579902c7a build: add musig2_bundle_creation_tests to unit test target
70f25fb5c9 feat: add MuSig2 P2P message enums, NetMsgType strings, GetHash impls, aggregator cache ctor
648b937251 feat: wire MuSig2 v0x03 into bundle manager, session, and build system
```

---

## What is DigiDollar?

DigiDollar is a USD-pegged stablecoin built natively into DigiByte. It uses an over-collateralized model where users lock DGB to mint DUSD at the current oracle price of DGB.

The world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

DGB becomes the strategic reserve asset (21B max, only ~1.94 DGB per person on Earth). Everything happens inside DigiByte Core wallet. You never give up custody of your private keys. No centralized company, fund or pool. Pure decentralization.

**Learn more:** https://digibyte.io/digidollar

---

## Oracle Operator Setup

### Upgrading from RC26

```bash
digibyte-cli -testnet stop
# Replace binary
# Back up/migrate wallet data from testnet19 if needed

digibyted -testnet -daemon
digibyte-cli -testnet loadwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

### New Oracle Setup

```bash
digibyted -testnet -daemon
digibyte-cli -testnet createwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle createoraclekey <your_oracle_id>
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
# Future restarts will auto-start your oracle.
```

For the complete guide including earlier migration details, see **`DIGIDOLLAR_ORACLE_SETUP.md`**.

### Current Oracle Operators (Testnet)

| ID | Operator | Status |
|----|----------|--------|
| 0 | Jared | ✅ Active |
| 1 | Green Candle | ✅ Active |
| 2 | Bastian | ✅ Active |
| 3 | DanGB | ✅ Active |
| 4 | Shenger | ✅ Active |
| 5 | Ycagel | ✅ Active |
| 6 | Aussie Epic | ✅ Active |
| 7 | LookIntoMyEyes | ✅ Active |
| 8 | JohnnyLawDGB | ✅ Active |
| 9 | Ogilvie | ✅ Active |
| 10 | Brian Oakes | ✅ Active |

---

## Complete RPC Command Reference

### DigiDollar Commands (Wallet)

| Command | Description |
|---------|-------------|
| `mintdigidollar` | Mint DigiDollars by locking DGB as collateral |
| `senddigidollar` | Send DigiDollars to another address |
| `redeemdigidollar` | Redeem DigiDollars to unlock DGB collateral |
| `getdigidollarbalance` | Show your DigiDollar balance |
| `listdigidollarpositions` | List your active collateral positions |
| `listdigidollartxs` | List your DigiDollar transaction history |
| `getdigidollaraddress` | Get or create a DigiDollar receive address |
| `validateddaddress` | Validate a DigiDollar address |
| `listdigidollaraddresses` | List all DigiDollar addresses in your wallet |
| `importdigidollaraddress` | Import a DigiDollar address for watch-only |
| `getdigidollarstats` | Get network-wide DigiDollar statistics |
| `getdigidollardeploymentinfo` | Get DigiDollar activation/deployment status |
| `calculatecollateralrequirement` | Calculate DGB collateral needed for a DD mint |
| `estimatecollateral` | Estimate collateral requirement by tier |
| `getdcamultiplier` | Get the current DCA multiplier for collateral |
| `getredemptioninfo` | Get info about redeeming a specific position |
| `getprotectionstatus` | Check if liquidation protection is active |

### Oracle Commands

| Command | Description |
|---------|-------------|
| `createoraclekey <id>` | Generate oracle Schnorr keypair (one-time) |
| `getoraclepubkey <id>` | Show oracle public key from wallet |
| `startoracle <id>` | Start running as an oracle operator |
| `stoporacle <id>` | Stop your oracle |
| `getoracleprice` | Get the consensus price |
| `getalloracleprices` | Per-oracle price breakdown |
| `getoracles` | Network-wide oracle status |
| `listoracle` | Show local oracle status |
| `sendoracleprice` | Manually submit a price (testing) |

---

## Configuration

```ini
testnet=1

[test]
digidollar=1
txindex=1
addnode=oracle1.digibyte.io
```

> **Note:** `txindex=1` is enforced at startup for DD-enabled nodes. Make sure it's in the correct section (`[test]` for testnet, `[main]` for mainnet). Global placement (above all sections) also works.

---

## Network Information

| Setting | Value |
|---------|-------|
| Network | Testnet (`testnet20`) |
| Default P2P Port | **12034** |
| Default RPC Port | **14025** |
| Oracle Consensus | **6-of-11** |
| Oracle Bundle Format | **MuSig2 aggregate signing (`v0x03`)** |
| Exchange Sources | 6 (Binance, CoinGecko, KuCoin, Gate.io, HTX, Crypto.com) |

---

## Downloads

| Platform | File |
|----------|------|
| Windows 64-bit (Installer) | `digibyte-9.26.0-rc27-win64-setup.exe` |
| Windows 64-bit (Portable) | `digibyte-9.26.0-rc27-win64.zip` |
| macOS Apple Silicon | `digibyte-9.26.0-rc27-arm64-apple-darwin.dmg` |
| macOS Intel | `digibyte-9.26.0-rc27-x86_64-apple-darwin.dmg` |
| Linux x86_64 | `digibyte-9.26.0-rc27-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc27-aarch64-linux-gnu.tar.gz` |

---

## Known Issues

- This is a fresh DigiDollar testnet reset. Operators should expect to migrate wallets manually if they want to preserve old oracle keys.
- As the network warms up, make sure enough oracle operators are online for **6-of-11** participation.

---

## Troubleshooting

### "My oracle did not start automatically"

Load the oracle wallet and start it manually:

```bash
digibyte-cli -testnet loadwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

### "I still see the old chain"

You are probably still looking at `testnet19` data. RC27 uses **`testnet20`**.

### "My peers are on the old testnet port"

RC27 testnet uses **12034**. If you hardcoded peers or firewall rules for **12033**, update them.

---

## Feedback & Community

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues
