# CLAUDE.md - AI Assistant Guide for DigiByte Development

## Overview
This file provides context and guidance for AI assistants working on the DigiByte codebase, particularly for the Bitcoin Core v26.2 merge creating DigiByte v8.26.

## Repository Structure
**Working directory:** Always run from the root `digibyte/` directory (v8.26 merged code)

**Required reference repositories (subdirectories within digibyte/):**
- `bitcoin-v26.2-for-digibyte/` (Bitcoin v26.2 reference)
- `digibyte-v8.22.2/` (DigiByte v8.22.2 - SOURCE OF TRUTH for DigiByte values)

**Directory layout:**
```
digibyte/                        # Current v8.26 working directory (YOU ARE HERE)
├── src/                         # Source code
├── test/                        # Test files
├── bitcoin-v26.2-for-digibyte/  # Bitcoin v26.2 reference code
├── digibyte-v8.22.2/           # DigiByte v8.22.2 reference code
└── ... (other project files)
```

## Python Functional Test Fix Strategy

### Overview
315 Python functional tests need fixing after Bitcoin v26.2 merge. Current status:
- **Passed**: 109 tests
- **Failed**: 136 tests
- **Skipped**: 68 tests (mostly BDB not compiled)

### Test Status Tracking
- **FUNCTIONAL_CHECKLIST_TESTS.md** - Complete list of all tests with pass/fail status
- **ATTACK_LIST.md** - Failed tests organized into 10 groups for systematic fixing

### Prerequisites
```bash
pip install --break-system-packages digibyte-scrypt
```

### Fix Methodology

1. **Always Compare Three Codebases:**
   - v8.26 (current - what we're fixing)
   - v8.22.2 (SOURCE OF TRUTH for DigiByte values)
   - Bitcoin v26.2 (to understand what changed)

2. **Common Failure Patterns:**
   - RPC Method Not Found (-32601): Missing DigiByte-specific methods
   - Assertion Failures: Wrong constants (fees, rewards, timing)
   - Address Format Issues: Need DigiByte prefixes
   - Dandelion++ Issues: See doc/DANDELION_INFO.md FIRST

3. **Critical DigiByte Constants:**
   ```python
   P2P_PORT = 12024  # Mainnet
   P2P_PORT_TESTNET = 12025
   BLOCK_TIME = 15  # seconds
   COINBASE_MATURITY = 8  # blocks for spending
   MIN_RELAY_FEE = Decimal('0.00001000')  # DGB/kB
   MAX_MONEY = 21000000000  # 21 billion DGB
   SUBSIDY = 72000  # DGB current reward
   ```

4. **Test Categories (for parallel work):**
   - P2P Network (55 tests) - Dandelion++, protocol
   - Wallet (114 tests) - Address formats, fees
   - RPC Interface (51 tests) - Custom commands
   - Mining (3 tests) - Multi-algorithm critical
   - Mempool (18 tests) - Fee rates, Dandelion++
   - Feature (62 tests) - Core functionality
   - Interface (12 tests) - CLI, REST, ZMQ
   - Tool & Misc (5 tests) - Utilities

### Test Framework Fixes Already Applied
1. Updated private keys to DigiByte testnet format
2. Fixed address generation for dgbrt1 addresses
3. Added digibyte_scrypt module requirement
4. Fixed coinbase subsidy calculations (blocktools.py)
5. Updated test parameters for KvB units
6. Added stempool existence checks (transaction.cpp)

### Application Bug Reporting Format
```markdown
## APPLICATION BUG FIXED
**File**: src/[filename].cpp:XXX
**Test**: [test_name.py]
**Issue**: [description]
**Root Cause**: [Bitcoin v26.2 merge impact]
**Fix Applied**: [code]
**Impact**: [consequence if unfixed]
**Testing**: [how verified]
```


## DigiByte Unique Features

### Multi-Algorithm Mining
DigiByte uses 5 mining algorithms:
- SHA256D (0), Scrypt (1), Groestl (2), Skein (3), Qubit (4)
- Odocrypt (7) - Activates at height 9,112,320
- Each targets 75-second block time (15s × 5 algos)

### Critical Constants
```cpp
POW_TARGET_SPACING = 15; // 15 seconds
MAX_MONEY = 21000000000 * COIN; // 21 billion DGB
MAINNET_DEFAULT_PORT = 12024;
TESTNET_DEFAULT_PORT = 12025;
```

### Custom RPC Commands
- `getblockreward` - Current block reward
- Enhanced `getmininginfo` - Per-algorithm stats
- Enhanced `getdifficulty` - All algorithm difficulties

### Dandelion++ Privacy
**CRITICAL**: Always read doc/DANDELION_INFO.md before any Dandelion++ changes
- Two-pool system: stempool (private) and mempool (public)
- Transaction embargo and routing system
- Files: src/dandelion.cpp, src/stempool.h

### Difficulty Adjustment
- DigiShield V1 (block 67,200)
- MultiAlgo V2 (block 145,000)
- MultiShield V3 (block 400,000)
- DigiSpeed V4 (block 1,430,000)
