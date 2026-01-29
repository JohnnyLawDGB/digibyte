# DigiByte v9.26.0-rc12 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## What's New in RC12

### 🔮 Multi-Oracle System (3-of-5 Schnorr Threshold)

The headline feature of RC12 is the activation of **Phase Two oracle consensus** on testnet. This moves from the single-oracle (1-of-1) system used in RC11 to a decentralized **3-of-5 Schnorr threshold** system.

**Key changes:**
- **3-of-5 consensus:** Any 3 of 5 authorized oracle operators must provide valid signed price messages for a bundle to be accepted into a block
- **Phase Two activation at block 100:** Testnet activates multi-oracle consensus early for testing
- **5 oracle nodes enabled:** Oracle operators 0-4 are now active (previously only oracle 0)
- **Phase Two compact format (v0x02):** New on-chain encoding for multi-oracle bundles, maintaining the same 22-byte footprint
- **Phase-aware block validation:** `ValidateBlockOracleData()` now routes to Phase One or Phase Two validation based on block height

### Oracle Operator Guide

New comprehensive guide for third-party oracle operators:
- **`ORACLE_OPERATOR_GUIDE.md`** — Complete instructions for key generation, node setup, configuration, security best practices, and troubleshooting
- Covers the secure key exchange process: operators generate keypairs locally, share only X-only public keys
- Explains the non-interactive Schnorr threshold scheme

### Technical Changes

- `configure.ac`: Version bump RC11 → RC12
- `src/kernel/chainparams.cpp`: Testnet oracle params updated (3-of-5, Phase2Height=100, oracles 0-4 active)
- `src/oracle/bundle_manager.cpp`: 
  - Phase-aware `ValidateBlockOracleData()` (routes to Phase One or Phase Two validation)
  - Phase Two `CreateOracleScript()` (version 0x02 compact format)
  - Phase Two `ExtractOracleBundle()` (version 0x02 decoding)
  - Phase Two `AddOracleBundleToBlock()` (multi-message bundle creation from pending messages)

---

## Known Issues

- Testnet oracle keys (oracles 1-4) use known test values (SHA256 hash-derived). Real operator keys will be swapped before mainnet.
- Mainnet oracle consensus remains disabled (`nDigiDollarPhase2Height = INT_MAX`)
- RegTest still uses Phase One (1-of-1) for unit test compatibility

---

## Upgrade Notes

**RC12 uses testnet12 network (port 12034). No data wipe required if upgrading from RC10 or RC11.**

### If Upgrading from RC9 or Earlier:
1. Close your old wallet
2. Delete old testnet data:
   - **Windows:** Delete `%APPDATA%\DigiByte\testnet10\` and `testnet11\`
   - **macOS:** Delete `~/Library/Application Support/DigiByte/testnet10/` and `testnet11/`
   - **Linux:** Delete `~/.digibyte/testnet10/` and `~/.digibyte/testnet11/`
3. Download and install RC12
4. Launch with `-testnet` flag

---

## What is DigiDollar?

DigiDollar is a USD-pegged stablecoin built natively into DigiByte. It uses an over-collateralized model where users lock DGB to mint DUSD at the current oracle price of DGB.

The world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

**Learn more:** https://digibyte.io/digidollar

---

## Commits Since RC11

- Activate Phase Two multi-oracle consensus (3-of-5) on testnet
- Enable oracle nodes 1-4 for testnet Phase Two
- Set Phase Two activation height to block 100 on testnet
- Add Phase Two compact format (version 0x02) to CreateOracleScript
- Add Phase Two decoding to ExtractOracleBundle
- Add Phase Two bundle creation from pending messages in AddOracleBundleToBlock
- Update ValidateBlockOracleData with phase-aware consensus validation
- Version bump to v9.26.0-rc12
- Add ORACLE_OPERATOR_GUIDE.md for third-party oracle operators

---

## Network Information

| Setting | Value |
|---------|-------|
| Network | Testnet (testnet12) |
| Default P2P Port | 12034 |
| Default RPC Port | 14026 |
| Oracle Node | oracle1.digibyte.io:12034 |
| Address Prefix | dgbt1... (bech32) |
| Phase Two Activation | Block 100 |
| Oracle Consensus | 3-of-5 Schnorr threshold |

---

## Downloads

| Platform | File |
|----------|------|
| Windows 64-bit (Installer) | `digibyte-9.26.0-rc12-win64-setup.exe` |
| Windows 64-bit (Portable) | `digibyte-9.26.0-rc12-win64.zip` |
| macOS Apple Silicon (M1/M2/M3/M4) | `digibyte-9.26.0-rc12-arm64-apple-darwin.dmg` |
| macOS Intel | `digibyte-9.26.0-rc12-x86_64-apple-darwin.dmg` |
| Linux x86_64 | `digibyte-9.26.0-rc12-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc12-aarch64-linux-gnu.tar.gz` |

---

## Feedback & Community

Please report issues and feedback to help us prepare for mainnet launch.

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues
