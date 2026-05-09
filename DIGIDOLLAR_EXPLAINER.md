# DigiDollar - Decentralized USD Stablecoin on DigiByte
*Updated: 2026-04-30*
*Document Version: 3.8 — V1 alignment*

## Overview

DigiDollar is the world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

### Key Points
- **DGB becomes the strategic reserve asset** (21B max supply, ~1.94 per person on Earth at 8.1B population)
- **Everything happens inside DigiByte Core wallet** — you never give up control of your private keys
- **Status (V1, `feature/digidollar-v1`)**: Testnet active (BIP9 bit 23 already past `min_activation_height`); mainnet activation gate is configured for height 22,014,720 (start time 2026-05-01) — see `DIGIDOLLAR_ARCHITECTURE.md` for details

---

## What is DigiDollar?

### DGB as a Limited, Finite Strategic Reserve Asset

With a maximum supply of 21 billion DGB, there are only **1.94 DGB per person** on Earth (based on 8.1 billion world population). Combined with DigiByte's **15-second block speed** (40x faster than Bitcoin), this extreme scarcity and fast settlement makes DGB ideal as collateral for DigiDollar - a truly finite backing for instant, stable currency transactions.

### Simple Explanation

DigiDollar is a stable digital currency that equals $1 USD, created by locking up DigiByte (DGB) as collateral. DGB becomes a strategic reserve asset - with only 21 billion max supply (just 1.94 DGB per person on Earth), it's a truly finite asset backing the stability of DigiDollars.

Unlike traditional stablecoins backed by bank accounts, DigiDollar is the world's first truly decentralized stablecoin on a UTXO blockchain. No company or bank controls it.

**Most importantly**: Everything happens directly in your DigiByte Core wallet - you never give up control of your private keys or trust a third party.

**Transaction Limits**: Minimum mint $100, maximum $100,000 per transaction (testnet: $10,000 max). Minimum output $1.

### Key Benefits

- ✅ World's first truly decentralized stablecoin on UTXO blockchain
- ✅ Always worth $1 USD - stable and predictable
- ✅ You keep full control of private keys in Core wallet
- ✅ DGB becomes strategic reserve asset
- ✅ 15-second blocks (40x faster than BTC), $0.01 fees

---

## How It Works

### Core Idea: The Silver Safe Analogy

**Imagine DGB is silver** stored in your basement safe. You have $1,000 worth of silver but need cash today. Instead of selling your silver (and losing future gains), you lock it in a special time-locked safe.

The safe gives you $500 cash to spend today. **The silver NEVER leaves your possession** - it stays in YOUR basement, in YOUR safe. You just can't access it until the timelock expires.

10 years later, your silver is worth $10,000 (10x gain)! To unlock: Simply return the $500 to the safe → get your $10,000 silver back. You kept ALL the appreciation.

#### That's EXACTLY how DigiDollar works:

- ✅ Lock DGB in YOUR wallet (never leaves your control)
- ✅ You ALWAYS keep control of your private keys
- ✅ Get DigiDollars to spend today
- ✅ When timelock expires, burn DD → get your DGB back
- ✅ Keep ALL the DGB price appreciation

### The Tax Advantage: Liquidity Without Selling

In most jurisdictions, **borrowing against assets is NOT a taxable event**. This is exactly what billionaires do - they never sell their stocks, they borrow against them.

**Traditional Crypto Sale:**
- ❌ Sell DGB → Pay 20-40% capital gains tax
- ❌ Lose future appreciation
- ❌ Taxable event recorded

**DigiDollar Method:**
- ✅ Lock DGB → Get DigiDollars
- ✅ No taxable event (in most jurisdictions)
- ✅ Keep ALL future DGB gains
- ✅ Theoretically never need to sell DGB

_* Tax laws vary by jurisdiction. Consult a tax professional for your specific situation._

### Economic Incentives: Why This Benefits Everyone

#### 🔒 DGB Becomes More Scarce

**1.94 DGB per person on Earth** (21B max supply ÷ 8.1B population)

With only 21 billion DGB ever to exist, locking DGB for DigiDollars makes an already scarce asset even more scarce. This creates natural price support.

- **Reduced selling pressure**: Locked DGB can't be panic sold during market volatility
- **Supply shock potential**: Significant locking could create supply squeeze
- **Benefits all DGB holders**: Even unlocked DGB benefits from reduced circulating supply

#### 💰 Personal Financial Benefits

DigiDollar provides unprecedented financial flexibility for DGB holders, enabling sophisticated wealth management strategies.

- **Tax-efficient liquidity**: Access funds without triggering capital gains
- **Keep upside potential**: Maintain full exposure to DGB price appreciation
- **Strategic flexibility**: Lock portions based on liquidity needs

**The Network Effect**: The more people use DigiDollar, the stronger the DGB ecosystem becomes. Locked DGB creates scarcity → drives price → attracts more users → creates more demand for both DGB and DigiDollar. It's a positive feedback loop that benefits all participants.

### The Technical Process

#### 1. Lock DGB Collateral
Users lock DigiByte as collateral in a P2TR (Pay-to-Taproot) time-locked vault. The amount depends on the lock period (200%-1000% of DigiDollar value, with shorter locks requiring more collateral).

#### 2. Mint DigiDollars
DigiDollars are automatically minted based on the locked DGB value and current USD exchange rate from decentralized oracles.

#### 3. Use & Redeem
Use DigiDollars for stable transactions. Redeem them anytime to unlock your DGB collateral after the lock period expires.

---

## Collateral Requirements

DigiDollar uses a sliding collateral scale to prevent attacks while rewarding long-term participants:

| Lock Period | Collateral Ratio | Undercollateralized After | DGB for $100 | Notes |
|------------|------------------|---------------------------|--------------|-------|
| 1 hour     | 1000%           | 90% drop                  | 1000 DGB     | Shortest/onboarding tier (canonical on all networks) |
| 30 days    | 500%            | 80% drop                  | 500 DGB      | |
| 3 months   | 400%            | 75% drop                  | 400 DGB      | |
| 6 months   | 350%            | 71.4% drop                | 350 DGB      | |
| 1 year     | 300%            | 66.7% drop                | 300 DGB      | |
| 2 years    | 275%            | 63.6% drop                | 275 DGB      | |
| 3 years    | 250%            | 60% drop                  | 250 DGB      | |
| 5 years    | 225%            | 55.6% drop                | 225 DGB      | |
| 7 years    | 212%            | 52.8% drop                | 212 DGB      | |
| 10 years   | 200%            | 50% drop                  | 200 DGB      | |

**Note**: The updated collateral schedule (1000% → 200%) provides enhanced stability. The 1-hour tier is canonical on all networks and locks real collateral until expiry. The "Undercollateralized After" column shows how much DGB price can drop before position becomes undercollateralized.

---

## Revolutionary Use Cases

### Corporate Bonds
**$140.7 Trillion market** - Instant settlement vs 2-3 day traditional clearing

### Real Estate
**$79.7 Trillion market** - Fractional ownership democratizes property investment

### Autonomous Vehicles
**$13.7 Trillion by 2030** - Self-driving cars manage their own finances

### Global Remittances
**$685 Billion market** - Reduce costs from 6.3% average to $0.01 flat fee

### Healthcare Payments
**$550 Billion market** - Real-time claim adjudication and transparent pricing

### And 45+ More Use Cases
From supply chain to gaming, DigiDollar enables countless innovations

---

## Technical Implementation

### Revolutionary Architecture

DigiDollar is the world's first truly decentralized stablecoin built natively on a UTXO (Unspent Transaction Output) blockchain. All operations occur directly in DigiByte Core wallet — users maintain complete control of their private keys throughout the entire process.

**Implementation Status (V1, `feature/digidollar-v1`)**: Core transaction system, MAST collateral, DCA/ERR/Volatility protections, network-wide UTXO scanning, MuSig2 oracle bundles, Qt GUI, and RPC surface are feature-complete. The May 1, 2026 BIP9 start time has passed; mainnet remains gated by the configured minimum height/threshold, testnet-only RC34 status, the green Wave 26 backward-compatibility/activation proof, and Jared's architecture-review decisions. See `DIGIDOLLAR_ARCHITECTURE.md` for the complete code-to-spec mapping.

### Core Technologies

#### Taproot Integration
Enhanced privacy using P2TR outputs and Schnorr signatures

#### Decentralized Oracles
Mainnet/testnet: 17 consensus-active oracle slots (9-of-17 MuSig2 BIP-327 threshold consensus producing a single BIP-340 Schnorr aggregate signature). Mainnet also carries reserve metadata in `vOracleNodes` slots 17-29, but those reserve entries are not in `consensus.vOraclePublicKeys` and do not participate in V1 quorum; testnet23 has only the 17 active slots configured. Regtest: 4-of-7 (chainparams overrides the header defaults). Oracle prices are reported in micro-USD format (1,000,000 = $1.00). The legacy constants in `primitives/oracle.h` (30/15/8) are header defaults; `consensus.nOracleTotalOracles`, `consensus.nOracleRequiredMessages`, and `consensus.nOracleConsensusRequired` from chainparams are authoritative.

#### MAST Implementation
Efficient script execution with Merkleized Alternative Script Trees. The collateral vault uses **2 redemption paths**:
1. **Normal Path**: CLTV timelock expiry + owner signature (system health ≥ 100%)
2. **ERR Path**: CLTV timelock expiry + OP_CHECKCOLLATERAL + owner signature (system health < 100%)

Both paths **require the timelock to expire first** - there is no early redemption, no forced liquidation, and no exceptions.

**Implementation Note**: Partial redemption is rejected at consensus. `ValidateCollateralReleaseAmount` (`src/digidollar/validation.cpp:1888+`) requires the redeemer to burn at least `requiredDDBurn` (= `originalDDMinted` for healthy systems, or the ERR-adjusted amount when health < 100%) AND release the full locked collateral; otherwise the transaction is rejected with `bad-collateral-release-partial-burn`. Non-DD transactions cannot spend a registered collateral vault at all (`bad-collateral-spend-missing-dd-burn`).

### Key Features

- ✅ No forced liquidations during market volatility
- ✅ All transactions appear identical on-chain (privacy)
- ✅ Batch signature verification for efficiency
- ✅ Native blockchain integration (no side chains)

---

## Technical Implementation Details

DigiDollar leverages advanced Bitcoin Script opcodes and DigiByte's unique capabilities to create a trustless, decentralized stablecoin system:

### Time Lock Mechanism

#### OP_CHECKLOCKTIMEVERIFY (CLTV)
Enforces canonical time-based collateral lock periods (1 hour to 10 years)

#### OP_CHECKSEQUENCEVERIFY (CSV)
⚠️ Not yet implemented: Listed as a capability but not currently used in DigiDollar scripts. Only CLTV (absolute timelocks) is used.

#### nLockTime
Prevents transactions from being mined until specified block height

### Core Script Functions

#### Multi-Sig Oracle Validation
9-of-17 BIP-327 MuSig2 Schnorr threshold (single 64-byte BIP-340 aggregate signature) for price consensus on mainnet/testnet; regtest uses 4-of-7. `OP_CHECKPRICE` consults the live consensus price via `g_get_oracle_consensus_price` and fails closed when no price is available — there is no production fallback to a mock value.

#### Taproot Script Paths
Multiple redemption conditions in a single P2TR output

#### MAST Trees
Merkleized scripts for privacy and efficiency

### How It Works - Simple Technical Flow

#### 1. Minting Process
User creates a P2TR output with DGB collateral, embedding time lock (CLTV) and oracle price data. Script validates collateral ratio and mints corresponding DigiDollars.

#### 2. Oracle Verification
Mainnet/testnet expose 17 active oracle slots in `consensus.vOraclePublicKeys`. Mainnet additionally carries reserve metadata in `vOracleNodes` slots 17-29, while testnet23 has no reserve metadata slots configured. Each DD-touching block carries a MuSig2 oracle bundle in the coinbase whose aggregate Schnorr signature represents 9-of-17 oracles signing the same price (BIP-327 MuSig2 over BIP-340 Schnorr). Pre-V1 (legacy) oracle bundle versions are rejected once DigiDollar is active.

#### 3. Redemption Process
After time lock expires (verified by CLTV), user can redeem DigiDollars to unlock DGB. Script burns DigiDollars and releases collateral to user's address.

**Key Innovation**: Unlike Ethereum-based stablecoins that require smart contracts and gas fees, DigiDollar uses native UTXO script capabilities for superior security, lower costs, and true decentralization. The entire system operates without intermediaries, smart contract risks, or custody requirements.

---

## Five-Layer Protection System

### The Time-Lock Challenge

**CRITICAL RULE**: DGB locked as collateral **CAN NEVER BE UNLOCKED** until the timelock expires. No exceptions. No early redemption. Ever.

Since collateral is cryptographically time-locked, there are **NO forced liquidations, NO margin calls, and NO early exit**. Positions must ride out the full term regardless of market conditions. This is intentional - it prevents manipulation and panic selling. This requires a unique protection approach.

### 1️⃣ Higher Collateral Requirements (First Defense)

The 1000%→200% sliding scale provides massive buffer against price drops. The one-hour tier requires 10x collateral, protecting against short-term volatility.

**Example**: With 1000% collateral, DGB can drop 90% before undercollateralization; with 500% collateral, DGB can drop 80%.

### 2️⃣ Dynamic Collateral Adjustment (Second Defense)

As system health changes, collateral requirements automatically adjust (`src/consensus/dca.cpp` HEALTH_TIERS, basis points):

- **≥150% healthy**: 1.00× (no adjustment, 10000 bps)
- **120–149% warning**: 1.25× (+25% collateral, 12500 bps)
- **110–119% critical**: 1.50× (+50% collateral, 15000 bps)
- **<110% emergency**: 2.00× (+100% collateral, 20000 bps)

DCA math runs entirely in `__int128` ceiling arithmetic and `ApplyDCA` fails closed (returns INT_MAX) when supplied with health that's stale relative to the canonical cached value, so an attacker cannot trick collateral calculations by feeding a frozen old health number.

### 3️⃣ Emergency Redemption Ratio (Third Defense)

**CRITICAL: ERR increases DD burn requirement, NOT reduces collateral return!**

If system drops below 100% collateralized, users must burn MORE DigiDollars to redeem their FULL collateral:

| System Health | ERR Ratio | DD Burn Required | Collateral Return |
|--------------|-----------|------------------|-------------------|
| 95-100% | 0.95 | 105.3% (1/0.95) | 100% (FULL) |
| 90-95% | 0.90 | 111.1% (1/0.90) | 100% (FULL) |
| 85-90% | 0.85 | 117.6% (1/0.85) | 100% (FULL) |
| <85% | 0.80 | 125% (1/0.80) | 100% (FULL) |

**Example**: At 80% system health with a $100 DD position:
- You must burn: $100 / 0.80 = **$125 DD**
- You receive back: **100% of your locked collateral** (not reduced!)

**Why this design?** ERR creates buying pressure on DD during crises - people need more DD to redeem, which helps stabilize the system. Reducing collateral would harm innocent DD holders.

ERR activates automatically when system health drops below 100%. New minting is BLOCKED during ERR until health recovers above 100%.

### 4️⃣ Volatility Protection (Fourth Defense)

Automatic freezes during extreme market volatility:

| Timeframe | Threshold | Action |
|-----------|-----------|--------|
| 1-hour | 10% | Warning logged |
| 1-hour | 20% | Freeze new minting |
| 24-hour | 30% | Freeze all DD operations |
| 7-day | 50% | Emergency mode |

Cooldown period: 8640 blocks (~36 hours at 15s blocks) after volatility subsides. There is NO oracle override of volatility freeze - the system must wait for the full cooldown period.

### 5️⃣ Supply & Demand Dynamics (Natural Defense)

Locked DGB reduces circulating supply, creating natural price support. With only 21B DGB max, locking creates scarcity.

**Effect**: More locking → Less supply → Higher DGB price → Better collateralization

### Real-Time System Monitoring

The system continuously tracks critical health metrics to ensure stability:

- Total DGB locked per tier
- Total DigiDollars minted
- Per-tier collateral ratios
- Aggregate system health

Accessible via RPC command: `getdigidollarstats`

**Key Insight**: These five layers work together without forced liquidations. Prevention (higher collateral), adaptation (dynamic adjustment), volatility freezes (circuit breakers), crisis management (emergency ratios), and market forces (scarcity) create a self-balancing, resilient system.

---

## Wallet Backup & Restore

### How DigiDollar Keys Work

DigiDollar uses HD (Hierarchical Deterministic) key derivation from your wallet's seed. This means:

- ✅ **Keys are derived from your wallet seed** - Not generated randomly
- ✅ **Positions can be restored from descriptors** - Using standard wallet backup
- ✅ **All operations use HD keys** - Minting, redeeming, receiving

### Backup Methods

#### Method 1: Standard Wallet Backup
```bash
# Creates a complete backup including all DigiDollar data
digibyte-cli backupwallet /path/to/backup.dat
```

#### Method 2: Descriptor Export (Recommended)
```bash
# Export descriptors with private keys
digibyte-cli listdescriptors true > descriptors.json
```

### Restore Process

```bash
# 1. Create a new wallet
digibyte-cli createwallet "restored" false false "" false true

# 2. Import descriptors
digibyte-cli -rpcwallet=restored importdescriptors '[...]'

# 3. Rescan blockchain to reconstruct DD positions
digibyte-cli -rpcwallet=restored rescanblockchain
```

**What gets restored during rescan:**
- ✅ All DGB balances and UTXOs
- ✅ DigiDollar positions (from OP_RETURN metadata)
- ✅ DD token balances (from blockchain scan)
- ✅ Position status (active/redeemed)

### Important Notes

- **Legacy wallets**: Unsupported for DigiDollar V1 mint/address creation; migrate to a descriptor/bech32m HD wallet before using DD
- **Descriptor wallets** (default since v8.23): Required for DD V1 and provide full restore capability via descriptors
- **Position data**: Reconstructed from blockchain during rescan, not stored in descriptors

---

## Learn More

- **White Paper**: https://github.com/orgs/DigiByte-Core/discussions/319
- **Tech Specs**: https://github.com/orgs/DigiByte-Core/discussions/324
- **50 Use Cases**: https://github.com/orgs/DigiByte-Core/discussions/325
- **Join Discussion**: https://github.com/orgs/DigiByte-Core/discussions

---

## Implementation Status & Code Alignment

**Last Verified**: 2026-04-30

| Feature | Document Spec | Code Reference |
|---------|---------------|----------------|
| 2 MAST Paths | Normal + ERR only | `src/digidollar/scripts.cpp:117-177` |
| Emergency oracle override | Removed | Comment at `src/digidollar/scripts.cpp:85-86` records removal |
| Partial redemption | Rejected at consensus | `src/digidollar/validation.cpp:2047-2055` (`bad-collateral-release-partial-burn`) |
| Non-DD spend of collateral vault | Rejected at consensus | `src/digidollar/validation.cpp:2212-2219` (`bad-collateral-spend-missing-dd-burn`) |
| ERR semantics | 100% collateral, MORE DD burned | `src/consensus/err.cpp:100-149` (`__int128` ceiling math) |
| Minting blocked during ERR | Yes; also blocked when oracle absent | `src/consensus/err.cpp:417-469` |
| Both MAST paths require CLTV | Both leaves prefix-match `<lockHeight> OP_CLTV OP_DROP` | `src/digidollar/scripts.cpp:73-99` |
| Lock tiers | 10 tiers (1h, 30d, 90d, 180d, 1y, 2y, 3y, 5y, 7y, 10y) | `src/consensus/digidollar.h:50-61` |
| Custom durations rejected | Mint validation enforces canonical tier and exact lock-block math | `src/digidollar/validation.cpp:980-1008, 1221-1227` |
| DCA tiers | 1.00 / 1.25 / 1.50 / 2.00 (≥150 / 120-149 / 110-119 / <110) | `src/consensus/dca.cpp:51-57` (HEALTH_TIERS) and `src/consensus/digidollar.h:87-92` (dcaLevels) |
| ERR ratios | 0.95 / 0.90 / 0.85 / 0.80 | `src/consensus/err.cpp:53-58` (ERR_TIERS) |
| Oracle config | 9-of-17 (mainnet/testnet) and 4-of-7 (regtest) | `src/kernel/chainparams.cpp` (`nOracleTotalOracles`, `nOracleRequiredMessages`, `nOracleConsensusRequired`) |
| Cooldown period | 8640 blocks (~36h) | `src/consensus/volatility.h:63` (`COOLDOWN_BLOCKS`) |
| DD amount unit | Cents (100 = $1.00) | `src/consensus/digidollar.h:64-66`, `src/digidollar/digidollar.h` |
| Oracle price unit | Micro-USD (1,000,000 = $1.00) | `src/oracle/bundle_manager.*`, `src/script/interpreter.cpp` |
| DD supply alert | Monitoring only — no hard cap | `src/digidollar/health.h:83` (`ALERT_DD_SUPPLY`) |

The full code-to-spec verification table lives in `DIGIDOLLAR_ARCHITECTURE.md` Section 18.

---

## V1 State at a Glance

The V1 branch closes the consensus and policy gaps that the previous draft of this document called out. The status is now:

| Subsystem | Source | Status |
|-----------|--------|--------|
| OP_CHECKPRICE production wiring | `src/script/interpreter.cpp:436-746` | Live `g_get_oracle_consensus_price`; fails closed on missing price |
| MuSig2-only oracle bundles | `src/validation.cpp:115-217` | Pre-V1 (legacy) bundles rejected; mempool requires recent valid MuSig2 quote |
| Mainnet/testnet validator parity | `src/validation.cpp` | Mainnet short-circuit removed (commit `f0d9a7b2c7`) |
| DCA/ERR integer math | `src/consensus/dca.cpp`, `src/consensus/err.cpp` | `__int128` ceiling arithmetic; `ApplyDCA` fails closed on stale health |
| Confirmed-only DD chaining | `src/digidollar/validation.cpp:1425, 1564` | `MEMPOOL_HEIGHT` DD inputs rejected (commit `0b4959f563`) |
| Mining graceful degradation | `src/node/miner.cpp:707-744` | Failing DD txs are stripped from `mapModifiedTx`; assembler continues |
| DD supply alert (not a cap) | `src/digidollar/health.h:83` | Monitoring threshold only |

**Where this leaves operators**:
- **Regtest / testnet**: Fully exercisable today (testnet23 is past its `min_activation_height`).
- **Mainnet**: Configuration is in place (BIP9 bit 23, start time 2026-05-01, `min_activation_height = 22014720`). Outstanding work is operational — mainnet oracle operator deployment and continued testnet validation.

---

_DigiDollar is a decentralized stablecoin on a UTXO blockchain where DGB serves as the strategic reserve asset and users retain custody of their keys throughout mint, transfer, and redeem operations._
