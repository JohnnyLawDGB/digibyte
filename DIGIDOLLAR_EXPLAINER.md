# DigiDollar - Decentralized USD Stablecoin on DigiByte
*Updated: 2026-04-04*
*Document Version: 3.5 - Re-verified against source code*

## Overview

DigiDollar is the world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

### Key Points
- **DGB becomes the strategic reserve asset** (21B max supply, 1.94 per person on Earth)
- **Everything happens inside DigiByte Core wallet** - you never give up control of your private keys
- **Status**: Testnet-ready implementation (Phase One) - see DIGIDOLLAR_MVP_STATUS.md for details

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
| 1 hour     | 1000%           | 90% drop                  | 1000 DGB     | Testing only (regtest/testnet) |
| 30 days    | 500%            | 80% drop                  | 500 DGB      | |
| 3 months   | 400%            | 75% drop                  | 400 DGB      | |
| 6 months   | 350%            | 71.4% drop                | 350 DGB      | |
| 1 year     | 300%            | 66.7% drop                | 300 DGB      | |
| 2 years    | 275%            | 63.6% drop                | 275 DGB      | |
| 3 years    | 250%            | 60% drop                  | 250 DGB      | |
| 5 years    | 225%            | 55.6% drop                | 225 DGB      | |
| 7 years    | 212%            | 52.8% drop                | 212 DGB      | |
| 10 years   | 200%            | 50% drop                  | 200 DGB      | |

**Note**: The updated collateral schedule (1000% → 200%) provides enhanced stability. The 1-hour tier is only available on testnet/regtest for development testing. The "Undercollateralized After" column shows how much DGB price can drop before position becomes undercollateralized.

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

DigiDollar is the world's first truly decentralized stablecoin built natively on a UTXO (Unspent Transaction Output) blockchain. All operations occur directly in DigiByte Core wallet - users maintain complete control of their private keys throughout the entire process.

**Implementation Status**: Core transaction system 90% complete, GUI 92% complete, RPC interface 95% complete. See DIGIDOLLAR_ARCHITECTURE.md for detailed status.

### Core Technologies

#### Taproot Integration
Enhanced privacy using P2TR outputs and Schnorr signatures

#### Decentralized Oracles
Mainnet/testnet: 11 oracle nodes with 6-of-11 Schnorr threshold consensus. Regtest: 1-of-1 single oracle. Oracle prices use micro-USD format (1,000,000 = $1.00). Note: `primitives/oracle.h` defines legacy constants (30/15/8) but chainparams overrides these per-network.

#### MAST Implementation
Efficient script execution with Merkleized Alternative Script Trees. The collateral vault uses **2 redemption paths**:
1. **Normal Path**: CLTV timelock expiry + owner signature (system health ≥ 100%)
2. **ERR Path**: CLTV timelock expiry + OP_CHECKCOLLATERAL + owner signature (system health < 100%)

Both paths **require the timelock to expire first** - there is no early redemption, no forced liquidation, and no exceptions.

**Implementation Note**: Partial redemption has wallet-level code (`CloseCollateralPosition()`), but consensus rules enforce FULL redemption only. Each collateral UTXO must be fully redeemed in a single transaction - partial redemption is validated as INVALID at the consensus layer (validation.cpp:ValidateCollateralReleaseAmount, security check T2-03).

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
Enforces time-based collateral lock periods (30 days to 10 years)

#### OP_CHECKSEQUENCEVERIFY (CSV)
⚠️ Not yet implemented: Listed as a capability but not currently used in DigiDollar scripts. Only CLTV (absolute timelocks) is used.

#### nLockTime
Prevents transactions from being mined until specified block height

### Core Script Functions

#### Multi-Sig Oracle Validation
6-of-11 Schnorr threshold signatures for price consensus (mainnet/testnet; regtest: 1-of-1)

#### Taproot Script Paths
Multiple redemption conditions in a single P2TR output

#### MAST Trees
Merkleized scripts for privacy and efficiency

### How It Works - Simple Technical Flow

#### 1. Minting Process
User creates a P2TR output with DGB collateral, embedding time lock (CLTV) and oracle price data. Script validates collateral ratio and mints corresponding DigiDollars.

#### 2. Oracle Verification
11 independent oracles sign price data. Script requires 6-of-11 signatures using Schnorr threshold aggregation, ensuring decentralized price consensus (mainnet/testnet configuration).

#### 3. Redemption Process
After time lock expires (verified by CLTV), user can redeem DigiDollars to unlock DGB. Script burns DigiDollars and releases collateral to user's address.

**Key Innovation**: Unlike Ethereum-based stablecoins that require smart contracts and gas fees, DigiDollar uses native UTXO script capabilities for superior security, lower costs, and true decentralization. The entire system operates without intermediaries, smart contract risks, or custody requirements.

---

## Five-Layer Protection System

### The Time-Lock Challenge

**CRITICAL RULE**: DGB locked as collateral **CAN NEVER BE UNLOCKED** until the timelock expires. No exceptions. No early redemption. Ever.

Since collateral is cryptographically time-locked, there are **NO forced liquidations, NO margin calls, and NO early exit**. Positions must ride out the full term regardless of market conditions. This is intentional - it prevents manipulation and panic selling. This requires a unique protection approach.

### 1️⃣ Higher Collateral Requirements (First Defense)

The 500%→200% sliding scale provides massive buffer against price drops. Short-term positions require up to 5x collateral, protecting against volatility.

**Example**: With 500% collateral, DGB can drop 80% before undercollateralization.

### 2️⃣ Dynamic Collateral Adjustment (Second Defense)

As system health changes, collateral requirements automatically adjust:

- **≥150% healthy**: Normal operations (1.0x multiplier)
- **120-149%**: Warning tier (+20% collateral required, 1.2x multiplier)
- **100-119%**: Critical tier (+50% collateral required, 1.5x multiplier)
- **<100%**: Emergency tier (+100% collateral required, 2.0x multiplier)

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

Cooldown period: 8640 blocks (~36 hours at 15s blocks) after volatility subsides. Oracle override available with 6-of-11 consensus.

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

- **Legacy wallets**: May have random keys that cannot be regenerated - always keep backups
- **Descriptor wallets** (default since v8.23): Full restore capability via descriptors
- **Position data**: Reconstructed from blockchain during rescan, not stored in descriptors

---

## Learn More

- **White Paper**: https://github.com/orgs/DigiByte-Core/discussions/319
- **Tech Specs**: https://github.com/orgs/DigiByte-Core/discussions/324
- **50 Use Cases**: https://github.com/orgs/DigiByte-Core/discussions/325
- **Join Discussion**: https://github.com/orgs/DigiByte-Core/discussions

---

## Implementation Status & Code Alignment

**Last Verified**: 2026-04-04

| Feature | Document Spec | Code Status | Notes |
|---------|---------------|-------------|-------|
| 2 MAST Paths | Normal + ERR only | ✅ Correct | Only 2 paths in MAST tree (scripts.cpp:113-173) |
| Emergency Path | Not used | ✅ Removed | `CreateEmergencyPath()` was dead code and removed |
| Partial Redemption | Consensus: FULL only | ⚠️ Clarified | Wallet code exists, but consensus enforces full redemption |
| ERR Returns | 100% collateral, burns more DD | ✅ Correct | `GetRequiredDDBurn()` increases burn, `GetAdjustedRedemption()` returns 100% |
| Minting Blocked During ERR | Yes | ✅ Correct | `ShouldBlockMinting()` returns true when health < 100% |
| Timelock Required | Both paths need CLTV | ✅ Correct | Both Normal and ERR paths start with CLTV check |
| Collateral Tiers | 10 tiers (1hr→10yr) | ✅ Correct | 2-year tier (275%) verified in consensus/digidollar.h |
| DCA Multipliers | 1.0x/1.2x/1.5x/2.0x | ✅ Correct | dca.cpp:GetDCAMultiplier() matches; note: ConsensusParams::dcaLevels uses 125 (1.25x) for warning but DCA class uses 1.2x |
| ERR Ratios | 0.95/0.90/0.85/0.80 | ✅ Correct | err.cpp:CalculateERRAdjustment() matches documentation |
| Oracle Config | 6-of-11 (mainnet/testnet) | ⚠️ Updated | primitives/oracle.h has legacy 30/15/8 constants; chainparams overrides to 11/11/6 |
| Cooldown Period | 8640 blocks (~36 hours) | ✅ Correct | volatility.h:COOLDOWN_BLOCKS = 8640 (was 144, fixed in RH-30a) |

**Code Verification Complete** (2026-04-04):
- MAST tree contains exactly 2 paths (Normal + ERR) - verified in `CreateCollateralP2TR()`
- Both redemption paths enforce CLTV timelock expiry before collateral can be unlocked
- Only 4 transaction types: NONE=0, MINT=1, TRANSFER=2, REDEEM=3
- Partial redemption: wallet code exists but consensus enforces FULL redemption only
- DD amounts stored in cents (100 = $1.00), oracle prices in micro-USD (1,000,000 = $1.00)
- Oracle config: mainnet/testnet use 6-of-11 (chainparams overrides default 8-of-15)
- Cooldown period: 8640 blocks (~36 hours), fixed from original 144 blocks in RH-30a

---

## 🚨 Critical Issues (Current State)

**Implementation Completion: ~85%** - Core functionality works but critical production blockers remain.

| Issue | Location | Impact |
|-------|----------|--------|
| System health hardcoded 150% | txbuilder.cpp:29,279 | ERR/DCA can never activate in production (validation.cpp has partial fix, txbuilder still uses constant) |
| MockOracleManager in non-regtest | ~~err.cpp:397~~ | **FIXED**: All MockOracleManager calls are now guarded by REGTEST checks |
| Mainnet validation disabled | bundle_manager.cpp:2227 | Returns `true` without validating on mainnet |
| GetBestHeight() stub | bundle_manager.cpp:47-51 | Returns hardcoded 0 instead of actual height |
| Tests use DD_TX_ERR=5 | test files | Transaction type 5 doesn't exist (only 0-3) |

**What This Means**:
- ✅ Testnet/Regtest: Fully functional for testing
- ⚠️ Mainnet: Requires fixes before deployment - ERR and DCA tiers will never activate due to hardcoded 150% system health

---

_DigiDollar represents a paradigm shift in decentralized finance - the world's first truly decentralized stablecoin on a UTXO blockchain where DGB becomes the strategic reserve asset and users never surrender control of their private keys._
