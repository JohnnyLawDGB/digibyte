# DigiByte v8.26 Multi-Algorithm Mining System Report

## 1. Simple Explanation (Executive Summary)

DigiByte's multi-algorithm mining system allows the blockchain to be secured by five (and later six) different mining algorithms simultaneously. Each algorithm targets its own difficulty level and competes fairly to find blocks every 75 seconds (15 seconds per algorithm on average). This design prevents mining centralization, increases security through algorithm diversity, and allows various types of mining hardware to participate in securing the network.

The system evolved through four major versions:
- **V1 (DigiShield)**: Single algorithm (Scrypt) with real-time difficulty adjustment
- **V2 (MultiAlgo)**: Five algorithms activated, each with independent difficulty
- **V3 (MultiShield)**: Improved per-algorithm difficulty adjustment with global balancing
- **V4 (DigiSpeed)**: Faster 15-second blocks with more responsive difficulty adjustment
- **Odocrypt Era**: Sixth algorithm added that changes its internal structure every 10 days

## 2. Flowchart

### Multi-Algorithm Mining Process Flow (DigiByte v8.26)

```
┌─────────────────┐
│  NEW BLOCK      │
│  REQUEST        │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│                   CHECK BLOCK HEIGHT                        │
├─────────────────────────────────────────────────────────────┤
│  Height < 100:     V1 DigiShield (Scrypt only)            │
│  100 ≤ H < 200:   V2 MultiAlgo (5 algorithms)             │
│  200 ≤ H < 400:   V3 MultiShield (5 algos, better adjust) │
│  400 ≤ H < 600:   V4 DigiSpeed (5 algos, 15-sec blocks)   │
│  Height ≥ 600:     Odocrypt Era (5 algos, Groestl→Odo)    │
└─────────────────────────────────────────────────────────────┘
                               │
         ┌─────────────────────┴─────────────────────┐
         ▼                                           ▼
┌──────────────────┐                      ┌───────────────────┐
│ HEIGHT < 100     │                      │ HEIGHT ≥ 100      │
│ ─────────────    │                      │ ──────────────    │
│ Only SCRYPT      │                      │ MULTI-ALGO MODE   │
│ Active           │                      │                   │
└────────┬─────────┘                      └─────────┬─────────┘
         │                                          │
         ▼                                          ▼
┌──────────────────┐              ┌────────────────────────────┐
│ V1 DIFFICULTY    │              │   SELECT ALGORITHM         │
│ ──────────────   │              ├────────────────────────────┤
│ • Retarget every │              │ Blocks 100-599:            │
│   67 blocks      │              │ • SHA256D (ASIC)           │
│ • Limits: +25%   │              │ • Scrypt (ASIC)            │
│   -50%           │              │ • Groestl (GPU)            │
└────────┬─────────┘              │ • Skein (CPU)              │
         │                        │ • Qubit (Mixed)            │
         │                        ├────────────────────────────┤
         │                        │ Blocks 600+:               │
         │                        │ • SHA256D (ASIC)           │
         │                        │ • Scrypt (ASIC)            │
         │                        │ • Skein (CPU)              │
         │                        │ • Qubit (Mixed)            │
         │                        │ • Odocrypt (FPGA)          │
         │                        │   [Groestl removed]        │
         │                        └─────────┬──────────────────┘
         │                                   │
         │                                   ▼
         │              ┌────────────────────────────────────┐
         │              │    GET DIFFICULTY VERSION         │
         │              ├────────────────────────────────────┤
         │              │ Height < 200:  V2 GetNextWorkReq  │
         │              │ • 10-block average per algo       │
         │              │ • Independent algo difficulties   │
         │              ├────────────────────────────────────┤
         │              │ 200 ≤ H < 400: V3 GetNextWorkReq │
         │              │ • Global median time adjustment   │
         │              │ • Per-algo local ±2% per block    │
         │              ├────────────────────────────────────┤
         │              │ Height ≥ 400:  V4 GetNextWorkReq  │
         │              │ • Faster 15-second blocks         │
         │              │ • Per-algo local ±4% per block    │
         │              └─────────┬──────────────────────────┘
         │                        │
         ▼                        ▼
┌──────────────────┐    ┌────────────────────────────────────┐
│ SET VERSION = 2  │    │    SET BLOCK VERSION WITH ALGO     │
│                  │    ├────────────────────────────────────┤
│ (Pre-MultiAlgo   │    │ Version = 0x20000000 | base | algo │
│  uses 0x20000002)│    │                                    │
│                  │    │                                    │
│                  │    │ Examples (with BIP9):              │
│                  │    │ • Scrypt:  0x20000002              │
│                  │    │ • SHA256D: 0x20000202              │
│                  │    │ • Groestl: 0x20000402              │
│                  │    │ • Skein:   0x20000602              │
│                  │    │ • Qubit:   0x20000802              │
│                  │    │ • Odo:     0x20000E02              │
└────────┬─────────┘    └─────────┬──────────────────────────┘
         │                        │
         │                        ▼
         │              ┌────────────────────────────────────┐
         │              │   APPLY ALGORITHM HASH FUNCTION    │
         │              ├────────────────────────────────────┤
         │              │ SHA256D:  Double SHA-256           │
         │              │ Scrypt:   Scrypt(N=1024,r=1,p=1)  │
         │              │ Groestl:  Groestl-512              │
         │              │ Skein:    Skein-512                │
         │              │ Qubit:    5-round chain            │
         │              │ Odocrypt: Odo(key=time/10days)    │
         │              └─────────┬──────────────────────────┘
         │                        │
         └────────────┬───────────┘
                      │
                      ▼
         ┌────────────────────────┐
         │   MINE BLOCK           │
         │   ──────────           │
         │   Find nonce where:    │
         │   hash < difficulty    │
         └──────────┬─────────────┘
                    │
                    ▼
         ┌────────────────────────┐
         │   VALIDATE POW         │
         │   ─────────────        │
         │   • Check algo active  │
         │   • Verify hash < diff │
         │   • Update chain       │
         └──────────┬─────────────┘
                    │
                    ▼
         ┌────────────────────────┐
         │   UPDATE DIFFICULTY    │
         │   ──────────────────   │
         │   • Per-algo tracking  │
         │   • Real-time adjust   │
         └──────────┬─────────────┘
                    │
                    ▼
         ┌────────────────────────┐
         │   BLOCK ACCEPTED       │
         └────────────────────────┘
```

## 3. Files & Functions Index

### Core Mining Files

#### `/src/primitives/block.h` & `/src/primitives/block.cpp`
- **Algorithm Definitions**:
  - `enum` defining `ALGO_SHA256D`, `ALGO_SCRYPT`, `ALGO_GROESTL`, `ALGO_SKEIN`, `ALGO_QUBIT`, `ALGO_ODO`
  - Block version constants: `BLOCK_VERSION_SHA256D`, `BLOCK_VERSION_SCRYPT`, etc.
- **Key Functions**:
  - `CBlockHeader::GetAlgo()` - Extracts algorithm from block version (line 25-47)
  - `CBlockHeader::GetPoWAlgoHash()` - Computes algorithm-specific hash (line 56-106)
  - `CBlockHeader::SetAlgo()` - Sets algorithm in block version
  - `GetAlgoName()` - Converts algorithm ID to string (line 126-148)
  - `GetAlgoByName()` - Converts string to algorithm ID (line 150-171)
  - `OdoKey()` - Calculates Odocrypt shapechange key (line 49-54)

#### `/src/pow.cpp` & `/src/pow.h`
- **Difficulty Adjustment Functions**:
  - `GetNextWorkRequired()` - Main entry point, routes to version (line 256-285)
  - `GetNextWorkRequiredV1()` - DigiShield (single algo) (line 27-93)
  - `GetNextWorkRequiredV2()` - MultiAlgo (per-algo averaging) (line 95-137)
  - `GetNextWorkRequiredV3()` - MultiShield (global + local) (line 139-190)
  - `GetNextWorkRequiredV4()` - DigiSpeed (faster adjustment) (line 192-254)
- **Algorithm Search**:
  - `GetLastBlockIndexForAlgo()` - Finds previous block of same algo (line 395-411)
  - `GetLastBlockIndexForAlgoFast()` - Optimized version using cache (line 413-430)
- **Validation**:
  - `CheckProofOfWork()` - Validates block hash meets difficulty (line 376-393)
  - `PermittedDifficultyTransition()` - Disabled for DigiByte (line 314-374)

#### `/src/validation.cpp`
- **Algorithm Activation**:
  - `IsAlgoActive()` - Determines which algorithms are active at height (line 1831-1858)
    - Before block 100: Scrypt only
    - Blocks 100-599: SHA256D, Scrypt, Groestl, Skein, Qubit
    - Block 600+: SHA256D, Scrypt, Skein, Qubit, Odocrypt (Groestl swapped out)

#### `/src/consensus/params.h`
- **Consensus Parameters Structure**:
  - Fork height definitions (lines 108-113)
  - Difficulty adjustment parameters (lines 136-167)
  - Multi-algo specific parameters (lines 147-148)
  - `DeploymentHeight()` - Returns activation height for features (line 188-211)

#### `/src/kernel/chainparams.cpp`
- **Network-Specific Parameters**:
  - **Mainnet** (lines 108-149):
    - `multiAlgoDiffChangeTarget = 145000`
    - `alwaysUpdateDiffChangeTarget = 400000`
    - `workComputationChangeTarget = 1430000`
    - `algoSwapChangeTarget = 9100000`
    - `OdoHeight = 9112320`
  - **Testnet** (lines 339-344):
    - `multiAlgoDiffChangeTarget = 100`
    - `alwaysUpdateDiffChangeTarget = 400`
    - `workComputationChangeTarget = 1430`
    - `algoSwapChangeTarget = 20000`
    - `OdoHeight = 600`
  - **Regtest** (lines 642-646):
    - `multiAlgoDiffChangeTarget = 100`
    - `alwaysUpdateDiffChangeTarget = 200`
    - `workComputationChangeTarget = 400`
    - `algoSwapChangeTarget = 600`

#### `/src/rpc/mining.cpp`
- **RPC Commands**:
  - `getmininginfo()` - Returns per-algorithm difficulties (line 441-513)
  - `getblocktemplate()` - Includes `odokey` for Odocrypt (line 617-1024)
  - Mining algorithm selection via `miningAlgo` global (line 53)

#### `/src/rpc/blockchain.cpp`
- **RPC Commands**:
  - `getdifficulty()` - Returns current difficulty (line 428-446)
  - `GetDifficulty()` - Helper supporting per-algorithm queries

#### `/src/chain.cpp` & `/src/chain.h`
- **Chain Index**:
  - `CBlockIndex::GetAlgo()` - Gets algorithm from stored block
  - `lastAlgoBlocks[]` - Array caching last block per algorithm

## 4. Technical Implementation Details (v8.26)

### 4.1 Block Version Structure

**CRITICAL**: Block version encoding is different across DigiByte's evolution and MUST be correct for each era.

#### Version Bit Layout
```
Bits 31-28: VERSIONBITS signaling (0x2 when active via VERSIONBITS_TOP_BITS)
Bits 11-8:  Algorithm identifier (masked by BLOCK_VERSION_ALGO = 0x0F00)
Bits 7-0:   Base version (BLOCK_VERSION_DEFAULT = 2)
```

#### Constants (from versionbits.h and primitives/block.h)
```cpp
VERSIONBITS_TOP_BITS = 0x20000000  // Set in bits 31-28
VERSIONBITS_TOP_MASK = 0xF0000000  // Mask for top 4 bits
BLOCK_VERSION_DEFAULT = 2           // Base version

// Algorithm bits (in bits 11-8):
BLOCK_VERSION_SCRYPT  = (0 << 8)  = 0x0000
BLOCK_VERSION_SHA256D = (2 << 8)  = 0x0200
BLOCK_VERSION_GROESTL = (4 << 8)  = 0x0400
BLOCK_VERSION_SKEIN   = (6 << 8)  = 0x0600
BLOCK_VERSION_QUBIT   = (8 << 8)  = 0x0800
BLOCK_VERSION_ODO     = (14 << 8) = 0x0E00
```

#### Complete Block Version Table for Regtest (v8.26)

| Era | Height Range | Active Algorithms | Version Calculation | Hex Values |
|-----|--------------|------------------|---------------------|------------|
| **Pre-MultiAlgo** | 0-99 | Scrypt only | VERSIONBITS + base + algo | `0x20000002` (Scrypt with VERSIONBITS) |
| **MultiAlgo** | 100-199 | 5 algorithms | VERSIONBITS + algo | `0x20000002` (Scrypt)<br/>`0x20000202` (SHA256D)<br/>`0x20000402` (Groestl)<br/>`0x20000602` (Skein)<br/>`0x20000802` (Qubit) |
| **MultiShield** | 200-399 | 5 algorithms | VERSIONBITS + algo | Same as MultiAlgo era |
| **DigiSpeed** | 400-599 | 5 algorithms | VERSIONBITS + algo | Same as MultiAlgo era |
| **Odocrypt** | 600+ | 5 algorithms<br/>(Groestl removed,<br/>Odocrypt added) | VERSIONBITS + algo | `0x20000002` (Scrypt)<br/>`0x20000202` (SHA256D)<br/>`0x20000602` (Skein)<br/>`0x20000802` (Qubit)<br/>`0x20000E02` (Odocrypt)<br/>**NO** `0x20000402` (Groestl removed) |

#### Version Creation Code (from versionbits.cpp:231-248)
```cpp
int32_t ComputeBlockVersion(pindexPrev, params, algo) {
    int32_t nVersion = VERSIONBITS_TOP_BITS | BLOCK_VERSION_DEFAULT;
    // ... add any BIP9 deployment bits ...
    nVersion |= GetVersionForAlgo(algo);  // Add algorithm bits
    return nVersion;
}
```

### 4.2 Difficulty Adjustment Eras

#### Era V1: DigiShield (Blocks 0-99 in regtest)
- Single algorithm (Scrypt only)
- Retargets based on `nInterval` parameter (depends on network)
- Adjustment limits: 25% up, 50% down (after `nDiffChangeTarget`)
- Real-time response to hash rate changes
- Code: `GetNextWorkRequiredV1()` in pow.cpp:27-93

#### Era V2: MultiAlgo (Blocks 100-199 in regtest)
- 5 algorithms activated simultaneously (SHA256D, Scrypt, Groestl, Skein, Qubit)
- Each algorithm maintains independent difficulty
- 10-block averaging window per algorithm (`nAveragingInterval = 10`)
- Target: 150 seconds between blocks of same algorithm (30s × 5 algos)
- Adjustment limits: 100% up, 40% down
- Code: `GetNextWorkRequiredV2()` in pow.cpp:95-137

#### Era V3: MultiShield (Blocks 200-399 in regtest)
- Global difficulty adjustment using median time past
- Per-algorithm local adjustment: `nLocalDifficultyAdjustment = 2%`
- 50-block lookback window (10 blocks × 5 algos)
- Dampened adjustment: `targetTimespan + (actual - target) / 6`
- Adjustment limits: 14% up, 9% down (calculated from params)
- Code: `GetNextWorkRequiredV3()` in pow.cpp:139-190

#### Era V4: DigiSpeed (Blocks 400+ in regtest)
- Faster 15-second target blocks
- 75-second target per algorithm (15s × 5 algos)
- More responsive adjustment: `targetTimespan + (actual - target) / 4`
- Adjustment limits: 8% up, 3% down (from `nMaxAdjustUpV4`, `nMaxAdjustDownV4`)
- Per-algorithm adjustment: `nLocalTargetAdjustment = 4%` per block gap
- Code: `GetNextWorkRequiredV4()` in pow.cpp:192-254

### 4.3 Algorithm Hash Functions

Each algorithm applies a different proof-of-work hash function:

1. **SHA256D** (`ALGO_SHA256D = 0`):
   - Standard Bitcoin double SHA-256
   - ASIC-friendly

2. **Scrypt** (`ALGO_SCRYPT = 1`):
   - Parameters: N=1024, r=1, p=1
   - Memory-hard function
   - ASIC-resistant initially

3. **Groestl** (`ALGO_GROESTL = 2`):
   - Groestl-512 hash function
   - GPU-friendly
   - **IMPORTANT**: Active blocks 100-599 only
   - **Swapped out** (not alongside) for Odocrypt at block 600

4. **Skein** (`ALGO_SKEIN = 3`):
   - Skein-512 hash function
   - Efficient on CPUs

5. **Qubit** (`ALGO_QUBIT = 4`):
   - Chain of 5 hash functions
   - ASIC-resistant design

6. **Odocrypt** (`ALGO_ODO = 7`):
   - Memory-hard with shapechange every `nOdoShapechangeInterval`
   - Key calculation: `nTime - (nTime % nOdoShapechangeInterval)` 
   - **Replaces Groestl** at block 600 (regtest), not added alongside
   - Only 5 algorithms remain active (Groestl removed)
   - Code: `OdoKey()` in primitives/block.cpp:49-54

### 4.4 Real-Time Difficulty Adjustment

The difficulty adjustment happens on **every single block** (not every 2016 blocks like Bitcoin):

```cpp
// V4 adjustment (current era on mainnet) - from pow.cpp:210-232
nActualTimespan = pindexLast->GetMedianTimePast() - pindexFirst->GetMedianTimePast();
nActualTimespan = nAveragingTargetTimespanV4 + (nActualTimespan - nAveragingTargetTimespanV4)/4;

// Apply limits
if (nActualTimespan < nMinActualTimespanV4) 
    nActualTimespan = nMinActualTimespanV4;
if (nActualTimespan > nMaxActualTimespanV4) 
    nActualTimespan = nMaxActualTimespanV4;

// Global retarget
bnNew = oldDifficulty;
bnNew *= nActualTimespan;
bnNew /= nAveragingTargetTimespanV4;

// Per-algorithm adjustment (pow.cpp:226-245)
nAdjustments = pindexPrevAlgo->nHeight + NUM_ALGOS - 1 - pindexLast->nHeight;
if (nAdjustments > 0) {
    for (int i = 0; i < nAdjustments; i++) {
        bnNew *= 100;
        bnNew /= (100 + nLocalTargetAdjustment);  // Make easier by 4% per block
    }
} else if (nAdjustments < 0) {
    for (int i = 0; i < -nAdjustments; i++) {
        bnNew *= (100 + nLocalTargetAdjustment);  // Make harder by 4% per block
        bnNew /= 100;
    }
}
```

### 4.5 Mining Process Flow

1. **Algorithm Selection**:
   - Miner chooses algorithm (via RPC parameter or config)
   - Checks if algorithm is active at current height

2. **Block Template Creation**:
   - Sets block version with algorithm bits
   - Gets current difficulty for chosen algorithm
   - For Odocrypt: includes `odokey` in template

3. **Proof-of-Work**:
   - Applies algorithm-specific hash function
   - Searches for nonce where `hash < target`

4. **Block Validation**:
   - Verifies algorithm is active
   - Checks version bits are correct
   - Validates PoW using algorithm-specific hash

5. **Difficulty Update**:
   - Updates global difficulty metrics
   - Adjusts per-algorithm difficulty
   - Maintains `lastAlgoBlocks` chain pointers

### 4.6 Fork Heights and Parameters

| Parameter | Mainnet | Testnet | Regtest |
|-----------|---------|---------|---------|
| MultiAlgo Activation | 145,000 | 100 | 100 |
| MultiShield (V3) | 400,000 | 400 | 200 |
| DigiSpeed (V4) | 1,430,000 | 1,430 | 400 |
| Odocrypt Activation | 9,112,320 | 600 | 600 |
| Block Time | 15 seconds | 15 seconds | 15 seconds |
| Algorithms (current) | 5 active | 5 active | 5/6 active |

### 4.7 Critical Version Validation Notes

#### ReserveAlgoBits Activation
- **Mainnet**: Block 8,547,840 - VERSIONBITS becomes mandatory
- **Testnet/Signet/Regtest**: Block 0 - Always active
- After activation, ALL blocks MUST have `VERSIONBITS_TOP_BITS` set (except pre-MultiAlgo)

#### Common Version Errors
1. **Using version 4** (Bitcoin style) instead of DigiByte versions
2. **Missing VERSIONBITS_TOP_BITS** after MultiAlgo activation
3. **Using wrong algorithm bits** for the current era
4. **Using Groestl (0x20000402)** after block 600 (it's removed!)
5. **Not using Odocrypt (0x20000E02)** when mining with Odo after block 600

#### Version Validation (validation.cpp:4128-4132)
```cpp
// DigiByte validates version requirements
if ((block.nVersion < 2 && DeploymentActiveAfter(pindexPrev, DEPLOYMENT_HEIGHTINCB)) ||
    (block.nVersion < 3 && DeploymentActiveAfter(pindexPrev, DEPLOYMENT_DERSIG)) ||
    (block.nVersion < 4 && DeploymentActiveAfter(pindexPrev, DEPLOYMENT_CLTV))) {
    return state.Invalid("bad-version");
}
```

### 4.8 RPC Interface

The system exposes mining information through enhanced RPCs:

- **`getmininginfo`**: Returns difficulties for all active algorithms
- **`getblocktemplate`**: Accepts `algo` parameter, includes `odokey` for Odocrypt
- **`getdifficulty`**: Can query specific algorithm difficulty
- **`generatetoaddress`**: Accepts `algo` parameter for testing

## 5. Validation Notes

### Source Code Verification

All findings were verified against the v8.26 source code:

1. **Algorithm Activation Heights**: Confirmed in `chainparams.cpp` lines 108-646
2. **Difficulty Algorithms**: Verified in `pow.cpp` functions V1-V4
3. **Block Version Encoding**: Checked in `primitives/block.h` lines 27-44
4. **Algorithm Hash Functions**: Validated in `primitives/block.cpp` lines 56-106
5. **IsAlgoActive Logic**: Confirmed in `validation.cpp` lines 1831-1858
6. **Consensus Parameters**: Verified in `consensus/params.h` complete structure
7. **RPC Implementations**: Checked in `rpc/mining.cpp` and `rpc/blockchain.cpp`

### Key Observations

1. **Real-time adjustment**: Unlike Bitcoin's 2016-block periods, DigiByte adjusts on every block
2. **Algorithm fairness**: Each algorithm targets proportional block time (15s total / 5 algos = 3s average per algo)
3. **Odocrypt innovation**: Only algorithm with time-based morphing (shapechange interval defined in params)
4. **Groestl replacement**: Verified in `IsAlgoActive()` at validation.cpp:1841-1857 - Groestl removed when Odocrypt activates
5. **Version bits reserved**: After `ReserveAlgoBitsHeight`, top bits used for BIP9 signaling

### Testing Verification

The implementation was cross-referenced with test suite patterns in `COMMON_FIXES.md`:
- Block version requirements change at fork heights
- Multi-algo activation causes difficulty spikes if using single algorithm
- Tests must use correct version bits after block 100
- Odocrypt requires special `odokey` parameter in mining templates

---

*Report generated from DigiByte Core v8.26 source code analysis*
*Date: 2025-08-30*