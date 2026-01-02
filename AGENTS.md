# DIGIBYTE KNOWLEDGE BASE

**Generated:** 2026-01-02
**Commit:** 39227ce6fe
**Branch:** feature/digidollar-v1

## OVERVIEW

DigiByte Core v8.26 - Bitcoin Core v26.2 fork with multi-algorithm PoW mining, Dandelion++ privacy, and DigiDollar stablecoin. C++ codebase with Python functional tests.

## STRUCTURE

```
digibyte/
├── src/                    # Core C++ source (see src/AGENTS.md)
│   ├── consensus/          # DigiByte consensus rules (see consensus/AGENTS.md)
│   ├── crypto/             # Multi-algo hashing (see crypto/AGENTS.md)
│   ├── digidollar/         # DigiDollar stablecoin module
│   ├── oracle/             # Price oracle for DigiDollar
│   ├── wallet/             # Wallet with DGB address formats
│   ├── rpc/                # RPC including getblockreward, getmininginfo
│   ├── qt/                 # Qt GUI
│   └── node/               # Node management
├── test/functional/        # Python tests (see test/functional/AGENTS.md)
├── depends/                # Cross-compilation system
│   ├── bitcoin-v26.2-for-digibyte/  # Bitcoin reference
│   └── digibyte-v8.22.2/   # DigiByte v8.22 SOURCE OF TRUTH
├── doc/                    # Build guides, developer notes
├── digidollar/             # DigiDollar specs and design docs
├── contrib/                # Scripts, packaging, gitian
└── ci/                     # CI test infrastructure
```

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| Multi-algo mining | `src/pow.cpp`, `src/crypto/hash*.h` | 5 algos + Odocrypt |
| Difficulty adjustment | `src/consensus/dca.cpp` | DigiShield/MultiShield |
| DigiDollar | `src/digidollar/`, `src/consensus/digidollar*.cpp` | Stablecoin |
| Dandelion++ | `src/dandelion.cpp`, `src/stempool.h` | Read doc/DANDELION_INFO.md first |
| Address encoding | `src/base58.cpp`, `src/bech32.cpp` | dgb/dgbt/dgbrt prefixes |
| Test constants | `test/functional/test_framework/blocktools.py` | COINBASE_MATURITY=8 |
| RPC commands | `src/rpc/mining.cpp`, `src/rpc/blockchain.cpp` | getblockreward custom |
| Network params | `src/kernel/chainparams.cpp` | Ports 12024/12025 |

## DIGIBYTE CONSTANTS (CRITICAL)

```python
# ALWAYS use these, NOT Bitcoin defaults
BLOCK_TIME = 15              # seconds (NOT 600)
COINBASE_MATURITY = 8        # blocks (NOT 100)
COINBASE_MATURITY_2 = 100    # For wallet tests
SUBSIDY = 72000              # DGB per block (NOT 50)
MAX_MONEY = 21_000_000_000   # 21 billion DGB

# Fees - DigiByte uses KvB, NOT vB!
MIN_RELAY_TX_FEE = 0.001     # DGB/kB (100x Bitcoin)
DEFAULT_FEE = 0.1            # DGB/kB

# Addresses
REGTEST_BECH32 = 'dgbrt'     # NOT 'bcrt'
TESTNET_BECH32 = 'dgbt'      # NOT 'tb'
P2P_PORT = 12024             # NOT 8333
```

## MINING ALGORITHMS

| Algo | ID | Version Bits | Activation |
|------|----|--------------|------------|
| Scrypt | 0 | 0x0000 | Genesis |
| SHA256D | 2 | 0x0200 | Block 100 |
| Groestl | 4 | 0x0400 | Block 100 |
| Skein | 6 | 0x0600 | Block 100 |
| Qubit | 8 | 0x0800 | Block 100 |
| Odocrypt | 14 | 0x0E00 | Block 600 |

Block version = `0x20000002 | algo_bits` (with BIP9)

## FORK HEIGHTS (REGTEST)

| Fork | Height | Effect |
|------|--------|--------|
| MultiAlgo | 100 | 5 algos activated |
| MultiShield | 200 | Per-algo difficulty |
| DigiShield | 334 | Real-time difficulty |
| DigiSpeed | 400 | Faster difficulty response |
| Odocrypt | 600 | 6th algorithm |

## ANTI-PATTERNS

| NEVER | Reason |
|-------|--------|
| Use Bitcoin constants (50 BTC, 600s, 100 blocks) | DigiByte has different values |
| Fees in sat/vB | DigiByte uses sat/kB (100x) |
| Hardcode `bcrt1` addresses | Use `dgbrt1` |
| Ignore Dandelion++ in tests | Add `-dandelion=0` to all nodes |
| Mock scrypt in production | Causes PoW validation issues |
| Assume single-algo mining | Multi-algo active after block 100 |

## BUILD & TEST

```bash
# Build
./autogen.sh && ./configure --with-gui=qt5 && make -j$(nproc)

# Run single test
./test/functional/[test].py --loglevel=debug

# Run all tests
./test/functional/test_runner.py

# Test with options
./test/functional/[test].py --descriptors --v2transport
```

## THREE-WAY COMPARISON

When fixing issues, always compare:
1. `./` - Current v8.26 (working copy)
2. `depends/digibyte-v8.22.2/` - Known working DigiByte
3. `depends/bitcoin-v26.2-for-digibyte/` - Bitcoin reference

## CHILD AGENTS.md FILES

- `src/AGENTS.md` - Core source organization
- `src/consensus/AGENTS.md` - Consensus rules, DCA, DigiDollar validation
- `src/crypto/AGENTS.md` - Multi-algorithm hashing
- `test/functional/AGENTS.md` - Test patterns and DigiByte-specific fixes
