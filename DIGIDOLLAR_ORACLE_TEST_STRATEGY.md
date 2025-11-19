# DigiDollar Oracle Phase One: Comprehensive Test Strategy

**Version**: 1.0
**Date**: 2025-11-18
**Author**: Test Engineer (Sub-Agent)
**Target**: DigiByte Core v8.26 Oracle Phase One
**Status**: PLANNING PHASE - DO NOT IMPLEMENT YET

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [TDD Enforcement Strategy](#2-tdd-enforcement-strategy)
3. [Unit Test Catalog (145+ Tests)](#3-unit-test-catalog-145-tests)
4. [Functional Test Catalog (20+ Tests)](#4-functional-test-catalog-20-tests)
5. [Test File Organization](#5-test-file-organization)
6. [TDD Workflow Timeline (6 Weeks)](#6-tdd-workflow-timeline-6-weeks)
7. [Coverage Measurement Plan](#7-coverage-measurement-plan)
8. [Test Infrastructure Requirements](#8-test-infrastructure-requirements)
9. [Test Data & Fixtures](#9-test-data--fixtures)
10. [Integration Test Dependencies](#10-integration-test-dependencies)

---

## 1. Executive Summary

### 1.1 Test Strategy Overview

This document defines the **complete testing strategy** for Phase One oracle implementation, achieving **90%+ code coverage** through strict **Test-Driven Development (TDD)** methodology.

**Key Metrics**:
- **145+ Unit Tests** (Boost Test framework)
- **20+ Functional Tests** (Python framework)
- **93% Average Code Coverage**
- **RED-GREEN-REFACTOR** cycle enforced for all components
- **6-week timeline** aligned with implementation schedule

**Critical Success Factors**:
1. **Tests written FIRST** (RED phase) before any implementation code
2. **Minimal implementation** (GREEN phase) to pass tests only
3. **Continuous refactoring** while keeping tests passing
4. **Coverage enforcement** via lcov/gcov with CI/CD integration

### 1.2 Coverage Targets by Component

| Component | Unit Tests | Functional Tests | Coverage Target | Priority |
|-----------|-----------|------------------|-----------------|----------|
| Exchange API Clients | 56 tests | 2 tests | 90% | CRITICAL |
| Schnorr Signatures | 18 tests | 1 test | 100% | CRITICAL |
| Oracle Message Handling | 15 tests | 4 tests | 95% | CRITICAL |
| Bundle Validation | 22 tests | 3 tests | 100% | CRITICAL |
| Oracle Price Cache | 12 tests | 2 tests | 95% | HIGH |
| P2P Protocol | 14 tests | 4 tests | 95% | HIGH |
| Consensus Integration | 8 tests | 3 tests | 95% | CRITICAL |
| DigiDollar Integration | 6 tests | 3 tests | 95% | CRITICAL |
| **TOTAL** | **151 tests** | **22 tests** | **93.8% avg** | - |

### 1.3 TDD Philosophy for Oracle System

**Why TDD is Critical for Oracle**:
- Oracle system is **consensus-critical** (bugs = chain splits)
- Price manipulation risks require **100% validation coverage**
- External API integrations need **comprehensive error handling**
- Cryptographic signatures must be **mathematically verified**

**TDD Workflow Example**:
```
Week 2, Day 1: Exchange API Integration

RED (2 hours):
├─ Write test: test_binance_api_parsing_success()
├─ Write test: test_binance_api_network_error()
├─ Write test: test_binance_api_json_malformed()
├─ Write test: test_binance_api_rate_limit()
├─ Write test: test_binance_api_invalid_symbol()
└─ All tests FAIL (ParseBinanceResponse() not implemented)

GREEN (3 hours):
├─ Implement ParseBinanceResponse() with minimal logic
├─ All tests PASS
└─ Code ugly but functional

REFACTOR (1 hour):
├─ Extract JSON parsing logic
├─ Add error message constants
├─ Improve variable names
└─ All tests still PASS
```

---

## 2. TDD Enforcement Strategy

### 2.1 Mandatory TDD Process

**RULE: Every feature MUST follow this cycle**:

```
┌─────────────────────────────────────────────────────────────┐
│ PHASE 1: RED (Write Failing Tests)                         │
├─────────────────────────────────────────────────────────────┤
│ ✓ Write unit tests that FAIL                               │
│ ✓ Define expected interface (function signature)           │
│ ✓ Define expected behavior (assertions)                    │
│ ✓ Document edge cases (error paths)                        │
│ ✓ Git commit: "RED: Add tests for [feature]"               │
│ ✗ DO NOT write implementation code yet                     │
└─────────────────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────────────────┐
│ PHASE 2: GREEN (Implement Minimal Code)                    │
├─────────────────────────────────────────────────────────────┤
│ ✓ Write MINIMUM code to pass tests                         │
│ ✓ Don't worry about elegance yet                           │
│ ✓ Focus on correctness only                                │
│ ✓ All tests must PASS                                      │
│ ✓ Git commit: "GREEN: Implement [feature]"                 │
│ ✗ DO NOT add features not covered by tests                 │
└─────────────────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────────────────┐
│ PHASE 3: REFACTOR (Improve Code Quality)                   │
├─────────────────────────────────────────────────────────────┤
│ ✓ Improve code structure                                   │
│ ✓ Extract functions, improve naming                        │
│ ✓ Add documentation comments                               │
│ ✓ Tests still PASS                                         │
│ ✓ Git commit: "REFACTOR: Improve [feature]"                │
│ ✗ DO NOT change test behavior                              │
└─────────────────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────────────────┐
│ PHASE 4: EXPAND (Add Edge Cases)                           │
├─────────────────────────────────────────────────────────────┤
│ ✓ Add tests for edge cases discovered during implementation│
│ ✓ Add tests for error paths                                │
│ ✓ Return to RED phase for new tests                        │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Test-First Enforcement Mechanisms

**Pre-Commit Hook** (Automated):
```bash
#!/bin/bash
# .git/hooks/pre-commit

# Prevent commits without tests
DIFF=$(git diff --cached --name-only)

# Check if any .cpp/.h files changed in /src/oracle/
if echo "$DIFF" | grep -q "src/oracle/.*\\.\\(cpp\\|h\\)$"; then
    # Require corresponding test file changes
    if ! echo "$DIFF" | grep -q "src/test/oracle.*tests\\.cpp"; then
        echo "ERROR: Implementation code changed without corresponding test changes"
        echo "TDD VIOLATION: Write tests FIRST (RED phase)"
        exit 1
    fi
fi
```

**Code Review Checklist**:
- [ ] Test file committed BEFORE implementation file (git log timestamps)
- [ ] All new functions have corresponding unit tests
- [ ] All error paths have test coverage
- [ ] Tests written in RED-GREEN-REFACTOR pattern (visible in commit history)

**CI/CD Pipeline Gates**:
```yaml
# .github/workflows/oracle-tests.yml
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - name: Run Unit Tests
        run: make check

      - name: Generate Coverage Report
        run: lcov --capture --directory . --output-file coverage.info

      - name: Enforce Coverage Threshold
        run: |
          COVERAGE=$(lcov --summary coverage.info | grep lines | awk '{print $2}' | sed 's/%//')
          if (( $(echo "$COVERAGE < 90.0" | bc -l) )); then
            echo "FAIL: Coverage $COVERAGE% below 90% threshold"
            exit 1
          fi
```

### 2.3 TDD Violation Consequences

**IF RED-GREEN-REFACTOR is skipped**:
- ❌ Code review REJECTED by orchestrator
- ❌ Must revert implementation and start over
- ❌ Sub-agent task marked as INCOMPLETE
- ❌ No credit for implementation without tests

**Example Violation**:
```
BAD COMMIT HISTORY:
├─ Commit 1: "Implement Binance API fetcher" (❌ NO TEST!)
├─ Commit 2: "Add tests for Binance API" (❌ TESTS AFTER CODE!)
└─ RESULT: REJECTED - Not TDD

GOOD COMMIT HISTORY:
├─ Commit 1: "RED: Add failing tests for Binance API"
├─ Commit 2: "GREEN: Implement Binance API to pass tests"
├─ Commit 3: "REFACTOR: Extract JSON parsing logic"
└─ RESULT: APPROVED - Proper TDD
```

---

## 3. Unit Test Catalog (145+ Tests)

### 3.1 Exchange API Tests (56 tests)

**File**: `/home/jared/Code/digibyte/src/test/oracle_exchange_tests.cpp`

#### 3.1.1 Binance API Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-BIN-001 | `test_binance_api_success` | Valid API response parses correctly | CRITICAL |
| EXC-BIN-002 | `test_binance_api_network_timeout` | Network timeout returns nullopt | CRITICAL |
| EXC-BIN-003 | `test_binance_api_json_malformed` | Malformed JSON handled gracefully | CRITICAL |
| EXC-BIN-004 | `test_binance_api_rate_limit` | HTTP 429 rate limit detected | HIGH |
| EXC-BIN-005 | `test_binance_api_invalid_symbol` | Invalid symbol returns error | MEDIUM |
| EXC-BIN-006 | `test_binance_api_price_conversion` | Price converted to micro-USD correctly | CRITICAL |
| EXC-BIN-007 | `test_binance_api_https_enforcement` | Non-HTTPS URL rejected | HIGH |

**Test Example**:
```cpp
BOOST_AUTO_TEST_CASE(test_binance_api_success)
{
    ExchangePriceFetcher fetcher;

    // Mock HTTP response from Binance
    std::string mock_response = R"({"symbol":"DGBUSDT","price":"0.012340"})";

    // Parse should return 12,340 micro-USD (0.012340 * 1,000,000)
    auto price = fetcher.ParseBinanceResponse(mock_response);

    BOOST_REQUIRE(price.has_value());
    BOOST_CHECK_EQUAL(*price, 12340);
}
```

#### 3.1.2 CoinMarketCap API Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-CMC-001 | `test_coinmarketcap_api_success` | Valid API response with API key | CRITICAL |
| EXC-CMC-002 | `test_coinmarketcap_api_no_key` | Missing API key returns error | CRITICAL |
| EXC-CMC-003 | `test_coinmarketcap_api_invalid_key` | Invalid API key returns 401 | HIGH |
| EXC-CMC-004 | `test_coinmarketcap_api_json_nested` | Nested JSON structure parsed correctly | CRITICAL |
| EXC-CMC-005 | `test_coinmarketcap_api_rate_limit` | API rate limit handled | HIGH |
| EXC-CMC-006 | `test_coinmarketcap_api_multiple_quotes` | Correct quote selected from array | MEDIUM |
| EXC-CMC-007 | `test_coinmarketcap_api_header_auth` | API key sent in X-CMC_PRO_API_KEY header | HIGH |

#### 3.1.3 CoinGecko API Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-CG-001 | `test_coingecko_api_success` | Valid API response (no key required) | CRITICAL |
| EXC-CG-002 | `test_coingecko_api_network_error` | Network error handled gracefully | CRITICAL |
| EXC-CG-003 | `test_coingecko_api_json_simple` | Simple JSON structure parsed | CRITICAL |
| EXC-CG-004 | `test_coingecko_api_price_usd` | USD price extracted from vs_currencies | CRITICAL |
| EXC-CG-005 | `test_coingecko_api_rate_limit` | Rate limit (50 req/min) handled | HIGH |
| EXC-CG-006 | `test_coingecko_api_invalid_id` | Invalid coin ID returns error | MEDIUM |
| EXC-CG-007 | `test_coingecko_api_cache_header` | Cache-Control header respected | LOW |

#### 3.1.4 Coinbase API Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-CB-001 | `test_coinbase_api_success` | Valid spot price response | CRITICAL |
| EXC-CB-002 | `test_coinbase_api_network_error` | Network error handled | CRITICAL |
| EXC-CB-003 | `test_coinbase_api_json_data_wrapper` | Data wrapper structure parsed | CRITICAL |
| EXC-CB-004 | `test_coinbase_api_amount_currency` | Amount and currency fields validated | CRITICAL |
| EXC-CB-005 | `test_coinbase_api_rate_limit` | Rate limit handled | HIGH |
| EXC-CB-006 | `test_coinbase_api_invalid_pair` | Invalid currency pair returns error | MEDIUM |
| EXC-CB-007 | `test_coinbase_api_precision` | Price precision maintained | HIGH |

#### 3.1.5 Kraken API Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-KR-001 | `test_kraken_api_success` | Valid ticker response | CRITICAL |
| EXC-KR-002 | `test_kraken_api_network_error` | Network error handled | CRITICAL |
| EXC-KR-003 | `test_kraken_api_json_result_wrapper` | Result wrapper parsed | CRITICAL |
| EXC-KR-004 | `test_kraken_api_pair_name_mapping` | Pair name DGBUSD mapped correctly | HIGH |
| EXC-KR-005 | `test_kraken_api_error_field` | Error field in response handled | HIGH |
| EXC-KR-006 | `test_kraken_api_last_price_array` | Last price from array extracted | CRITICAL |
| EXC-KR-007 | `test_kraken_api_volume_validation` | Volume field validated (sanity check) | MEDIUM |

#### 3.1.6 Messari API Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-MS-001 | `test_messari_api_success` | Valid market data response | CRITICAL |
| EXC-MS-002 | `test_messari_api_network_error` | Network error handled | CRITICAL |
| EXC-MS-003 | `test_messari_api_json_deep_nested` | Deep nested JSON structure parsed | CRITICAL |
| EXC-MS-004 | `test_messari_api_market_data_path` | Market data path navigated correctly | CRITICAL |
| EXC-MS-005 | `test_messari_api_price_usd_field` | price_usd field extracted | CRITICAL |
| EXC-MS-006 | `test_messari_api_rate_limit` | Rate limit handled | HIGH |
| EXC-MS-007 | `test_messari_api_null_fields` | Null fields handled gracefully | MEDIUM |

#### 3.1.7 KuCoin API Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-KC-001 | `test_kucoin_api_success` | Valid orderbook level1 response | CRITICAL |
| EXC-KC-002 | `test_kucoin_api_network_error` | Network error handled | CRITICAL |
| EXC-KC-003 | `test_kucoin_api_json_data_field` | Data field parsed | CRITICAL |
| EXC-KC-004 | `test_kucoin_api_best_ask_bid` | Best ask/bid used for price | HIGH |
| EXC-KC-005 | `test_kucoin_api_code_field` | Code field (success/error) validated | HIGH |
| EXC-KC-006 | `test_kucoin_api_symbol_format` | Symbol format (DGB-USDT) validated | MEDIUM |
| EXC-KC-007 | `test_kucoin_api_timestamp_field` | Timestamp field parsed | LOW |

#### 3.1.8 Crypto.com API Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-CR-001 | `test_cryptocom_api_success` | Valid ticker response | CRITICAL |
| EXC-CR-002 | `test_cryptocom_api_network_error` | Network error handled | CRITICAL |
| EXC-CR-003 | `test_cryptocom_api_json_result_array` | Result array parsed | CRITICAL |
| EXC-CR-004 | `test_cryptocom_api_instrument_name` | Instrument name validated (DGB_USD) | HIGH |
| EXC-CR-005 | `test_cryptocom_api_last_price_field` | Last price field extracted | CRITICAL |
| EXC-CR-006 | `test_cryptocom_api_code_field` | Response code validated | HIGH |
| EXC-CR-007 | `test_cryptocom_api_bid_ask_spread` | Bid/ask spread sanity checked | MEDIUM |

#### 3.1.9 Median & Aggregation Tests (7 tests)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| EXC-AGG-001 | `test_median_calculation_odd_count` | Median of odd number of prices | CRITICAL |
| EXC-AGG-002 | `test_median_calculation_even_count` | Median of even number of prices | CRITICAL |
| EXC-AGG-003 | `test_outlier_filtering_mad_algorithm` | MAD algorithm removes outliers | CRITICAL |
| EXC-AGG-004 | `test_outlier_filtering_no_outliers` | No outliers removed when all valid | HIGH |
| EXC-AGG-005 | `test_insufficient_exchanges_error` | Error when < 4 exchanges respond | CRITICAL |
| EXC-AGG-006 | `test_parallel_fetching_performance` | Parallel fetch faster than sequential | MEDIUM |
| EXC-AGG-007 | `test_all_exchanges_fail_error` | Error when all exchanges fail | CRITICAL |

### 3.2 Schnorr Signature Tests (18 tests)

**File**: `/home/jared/Code/digibyte/src/test/oracle_signature_tests.cpp`

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| SIG-001 | `test_schnorr_signature_creation` | Valid signature created | CRITICAL |
| SIG-002 | `test_schnorr_signature_verification` | Valid signature verified | CRITICAL |
| SIG-003 | `test_schnorr_signature_invalid` | Invalid signature rejected | CRITICAL |
| SIG-004 | `test_schnorr_signature_wrong_key` | Wrong public key rejected | CRITICAL |
| SIG-005 | `test_schnorr_signature_tampered_message` | Tampered message fails verification | CRITICAL |
| SIG-006 | `test_schnorr_signature_64_bytes` | Signature exactly 64 bytes | HIGH |
| SIG-007 | `test_schnorr_signature_deterministic` | Same input = same signature | MEDIUM |
| SIG-008 | `test_schnorr_signature_empty_message` | Empty message hash handled | MEDIUM |
| SIG-009 | `test_message_hash_construction` | Message hash includes all fields | CRITICAL |
| SIG-010 | `test_message_hash_nonce_uniqueness` | Different nonce = different hash | CRITICAL |
| SIG-011 | `test_message_hash_timestamp_uniqueness` | Different timestamp = different hash | CRITICAL |
| SIG-012 | `test_message_hash_price_uniqueness` | Different price = different hash | CRITICAL |
| SIG-013 | `test_message_hash_serialization` | Hash serialization deterministic | HIGH |
| SIG-014 | `test_replay_attack_prevention_nonce` | Nonce prevents replay attacks | CRITICAL |
| SIG-015 | `test_replay_attack_prevention_timestamp` | Timestamp prevents replay | CRITICAL |
| SIG-016 | `test_pubkey_extraction_from_privkey` | Public key derived correctly | HIGH |
| SIG-017 | `test_xonly_pubkey_format` | X-only pubkey (32 bytes) format | HIGH |
| SIG-018 | `test_signature_batch_validation` | Multiple signatures validated (future) | LOW |

**Test Example**:
```cpp
BOOST_AUTO_TEST_CASE(test_schnorr_signature_creation)
{
    // Generate oracle key pair
    CKey oracle_privkey;
    oracle_privkey.MakeNewKey(true);
    XOnlyPubKey oracle_pubkey(oracle_privkey.GetPubKey());

    // Create oracle message
    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 12340;
    msg.timestamp = GetTime();
    msg.block_height = 100;
    msg.nonce = GetRand<uint64_t>();
    msg.oracle_pubkey = oracle_pubkey;

    // Create message hash
    uint256 msg_hash = msg.GetSignatureHash();

    // Sign with Schnorr
    msg.schnorr_sig.resize(64);
    bool sign_result = oracle_privkey.SignSchnorr(msg_hash, msg.schnorr_sig);

    BOOST_CHECK(sign_result);
    BOOST_CHECK_EQUAL(msg.schnorr_sig.size(), 64);

    // Verify signature immediately
    bool verify_result = oracle_pubkey.VerifySchnorr(msg_hash, msg.schnorr_sig);
    BOOST_CHECK(verify_result);
}
```

### 3.3 Oracle Message Handling Tests (15 tests)

**File**: `/home/jared/Code/digibyte/src/test/oracle_message_tests.cpp`

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| MSG-001 | `test_message_serialization` | Message serializes correctly | CRITICAL |
| MSG-002 | `test_message_deserialization` | Message deserializes correctly | CRITICAL |
| MSG-003 | `test_message_round_trip` | Serialize + deserialize = original | CRITICAL |
| MSG-004 | `test_message_validation_valid` | Valid message passes validation | CRITICAL |
| MSG-005 | `test_message_validation_negative_price` | Negative price rejected | CRITICAL |
| MSG-006 | `test_message_validation_zero_price` | Zero price rejected | CRITICAL |
| MSG-007 | `test_message_validation_future_timestamp` | Future timestamp rejected | CRITICAL |
| MSG-008 | `test_message_validation_old_timestamp` | Old timestamp (> 5 min) rejected | CRITICAL |
| MSG-009 | `test_message_validation_invalid_oracle_id` | Invalid oracle ID rejected | CRITICAL |
| MSG-010 | `test_message_validation_missing_signature` | Missing signature rejected | CRITICAL |
| MSG-011 | `test_message_validation_price_range` | Price range validation (sanity) | HIGH |
| MSG-012 | `test_message_comparison` | Messages comparable by timestamp | MEDIUM |
| MSG-013 | `test_message_hash_consistency` | GetHash() deterministic | HIGH |
| MSG-014 | `test_message_copy_construction` | Copy constructor works | MEDIUM |
| MSG-015 | `test_message_assignment_operator` | Assignment operator works | MEDIUM |

### 3.4 Oracle Bundle Tests (22 tests)

**File**: `/home/jared/Code/digibyte/src/test/oracle_bundle_tests.cpp`

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| BDL-001 | `test_bundle_creation_single_oracle` | 1-of-1 bundle created (testnet) | CRITICAL |
| BDL-002 | `test_bundle_creation_multiple_oracles` | 8-of-15 bundle created (future) | MEDIUM |
| BDL-003 | `test_bundle_extraction_from_coinbase` | Bundle extracted from OP_RETURN | CRITICAL |
| BDL-004 | `test_bundle_extraction_missing_opreturn` | No OP_RETURN handled | CRITICAL |
| BDL-005 | `test_bundle_extraction_malformed_opreturn` | Malformed OP_RETURN rejected | CRITICAL |
| BDL-006 | `test_bundle_validation_valid` | Valid bundle passes validation | CRITICAL |
| BDL-007 | `test_bundle_validation_insufficient_sigs` | Insufficient signatures rejected | CRITICAL |
| BDL-008 | `test_bundle_validation_invalid_signature` | Invalid signature rejected | CRITICAL |
| BDL-009 | `test_bundle_validation_threshold_1of1` | 1-of-1 threshold enforced (testnet) | CRITICAL |
| BDL-010 | `test_bundle_validation_threshold_8of15` | 8-of-15 threshold enforced (future) | MEDIUM |
| BDL-011 | `test_bundle_median_price_calculation` | Median price calculated correctly | CRITICAL |
| BDL-012 | `test_bundle_timestamp_validation` | Bundle timestamp validated | HIGH |
| BDL-013 | `test_bundle_merkle_root` | Merkle root calculated correctly | HIGH |
| BDL-014 | `test_bundle_serialization` | Bundle serializes correctly | CRITICAL |
| BDL-015 | `test_bundle_deserialization` | Bundle deserializes correctly | CRITICAL |
| BDL-016 | `test_bundle_round_trip` | Serialize + deserialize = original | CRITICAL |
| BDL-017 | `test_bundle_size_limit` | Bundle size within limits (< 1 KB) | MEDIUM |
| BDL-018 | `test_bundle_duplicate_oracle_rejection` | Duplicate oracle messages rejected | HIGH |
| BDL-019 | `test_bundle_price_deviation_check` | Price deviation from previous checked | MEDIUM |
| BDL-020 | `test_bundle_empty_messages` | Empty messages array rejected | CRITICAL |
| BDL-021 | `test_bundle_network_type_validation` | Network type (testnet) validated | HIGH |
| BDL-022 | `test_bundle_op_return_magic_bytes` | OP_RETURN magic bytes ('ORCL') validated | HIGH |

### 3.5 Oracle Price Cache Tests (12 tests)

**File**: `/home/jared/Code/digibyte/src/test/oracle_cache_tests.cpp`

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| CACHE-001 | `test_cache_add_price` | Price added to cache | CRITICAL |
| CACHE-002 | `test_cache_get_price_by_height` | Price retrieved by height | CRITICAL |
| CACHE-003 | `test_cache_get_latest_price` | Latest price retrieved | CRITICAL |
| CACHE-004 | `test_cache_get_nonexistent_price` | Nonexistent price returns nullopt | HIGH |
| CACHE-005 | `test_cache_pruning_old_entries` | Old entries pruned (> 1000 blocks) | HIGH |
| CACHE-006 | `test_cache_pruning_keeps_recent` | Recent entries kept | HIGH |
| CACHE-007 | `test_cache_thread_safety_reads` | Concurrent reads thread-safe | CRITICAL |
| CACHE-008 | `test_cache_thread_safety_writes` | Concurrent writes thread-safe | CRITICAL |
| CACHE-009 | `test_cache_overwrite_same_height` | Same height overwrites price | MEDIUM |
| CACHE-010 | `test_cache_memory_usage` | Memory usage within limits | MEDIUM |
| CACHE-011 | `test_cache_clear` | Cache clear works | LOW |
| CACHE-012 | `test_cache_size_query` | Cache size queryable | LOW |

### 3.6 P2P Protocol Tests (14 tests)

**File**: `/home/jared/Code/digibyte/src/test/oracle_p2p_tests.cpp`

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| P2P-001 | `test_oracleprice_message_type` | ORACLEPRICE message type defined | CRITICAL |
| P2P-002 | `test_oraclebundle_message_type` | ORACLEBUNDLE message type defined | CRITICAL |
| P2P-003 | `test_getoracles_message_type` | GETORACLES message type defined | HIGH |
| P2P-004 | `test_message_broadcast` | Message broadcast to all peers | CRITICAL |
| P2P-005 | `test_message_relay` | Message relayed by peers | CRITICAL |
| P2P-006 | `test_message_rate_limiting` | Rate limiting enforced (1 msg/60s) | CRITICAL |
| P2P-007 | `test_message_duplicate_filtering` | Duplicate messages filtered | HIGH |
| P2P-008 | `test_message_invalid_signature_ban` | Invalid signatures ban peer | HIGH |
| P2P-009 | `test_message_size_limit` | Message size limit enforced (< 1 KB) | MEDIUM |
| P2P-010 | `test_message_propagation_speed` | Propagation < 2 seconds | MEDIUM |
| P2P-011 | `test_message_orphan_handling` | Orphan messages handled | MEDIUM |
| P2P-012 | `test_message_dos_protection` | DoS protection (flood prevention) | HIGH |
| P2P-013 | `test_message_peer_disconnection` | Disconnection handled gracefully | MEDIUM |
| P2P-014 | `test_message_network_partition` | Network partition recovery | LOW |

### 3.7 Consensus Integration Tests (8 tests)

**File**: `/home/jared/Code/digibyte/src/test/oracle_consensus_tests.cpp`

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| CONS-001 | `test_checkblock_with_valid_bundle` | CheckBlock accepts valid bundle | CRITICAL |
| CONS-002 | `test_checkblock_with_invalid_bundle` | CheckBlock rejects invalid bundle | CRITICAL |
| CONS-003 | `test_checkblock_without_bundle` | CheckBlock allows missing bundle | CRITICAL |
| CONS-004 | `test_connectblock_cache_update` | ConnectBlock updates cache | CRITICAL |
| CONS-005 | `test_chainparams_oracle_pubkeys` | Chainparams oracle pubkeys loaded | CRITICAL |
| CONS-006 | `test_chainparams_testnet_only` | Oracle only enabled on testnet | CRITICAL |
| CONS-007 | `test_consensus_threshold_enforcement` | Consensus threshold enforced | CRITICAL |
| CONS-008 | `test_block_rejection_invalid_oracle` | Block rejected for invalid oracle data | CRITICAL |

### 3.8 DigiDollar Integration Tests (6 tests)

**File**: `/home/jared/Code/digibyte/src/test/oracle_digidollar_tests.cpp`

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| DD-001 | `test_mint_uses_oracle_price` | Mint transaction uses oracle price | CRITICAL |
| DD-002 | `test_redeem_uses_oracle_price` | Redeem transaction uses oracle price | CRITICAL |
| DD-003 | `test_dca_uses_oracle_price` | DCA health uses oracle price | CRITICAL |
| DD-004 | `test_err_uses_oracle_price` | ERR adjustment uses oracle price | HIGH |
| DD-005 | `test_volatility_uses_oracle_price` | Volatility monitor uses oracle price | HIGH |
| DD-006 | `test_oracle_price_fallback_mock` | Fallback to mock on mainnet | CRITICAL |

**Total Unit Tests: 151 tests**

---

## 4. Functional Test Catalog (20+ Tests)

### 4.1 Oracle Price Broadcasting Tests (3 tests)

**File**: `/home/jared/Code/digibyte/test/functional/feature_oracle_price.py`

| Test ID | Test Name | Description | Scenario |
|---------|-----------|-------------|----------|
| FUNC-PRICE-001 | `test_oracle_price_broadcast_single_node` | Single oracle broadcasts price | 1 oracle node broadcasts, 2 regular nodes receive |
| FUNC-PRICE-002 | `test_oracle_price_propagation` | Price propagates to all nodes | Oracle → Node1 → Node2 → Node3 |
| FUNC-PRICE-003 | `test_oracle_price_new_node_sync` | New node syncs oracle price | New node joins, receives historical prices |

**Test Scenario Example**:
```python
def test_oracle_price_broadcast_single_node(self):
    """Test single oracle broadcasts price to network."""
    # Setup: 3 nodes (1 oracle, 2 regular)
    oracle_node = self.nodes[0]
    regular_node1 = self.nodes[1]
    regular_node2 = self.nodes[2]

    # Connect nodes
    self.connect_nodes(0, 1)
    self.connect_nodes(1, 2)
    self.sync_all()

    # Mine blocks to maturity
    oracle_node.generate(100)
    self.sync_all()

    # Verify oracle price broadcast
    oracle_price = oracle_node.getoracleprice()
    assert oracle_price is not None
    assert oracle_price['price_micro_usd'] > 0
    assert oracle_price['oracle_id'] == 0

    # Verify all nodes see same price
    price1 = regular_node1.getoracleprice()
    price2 = regular_node2.getoracleprice()

    assert_equal(oracle_price['price_micro_usd'], price1['price_micro_usd'])
    assert_equal(oracle_price['price_micro_usd'], price2['price_micro_usd'])

    self.log.info("✓ Oracle price broadcast successful")
```

### 4.2 Oracle Bundle Validation Tests (3 tests)

**File**: `/home/jared/Code/digibyte/test/functional/feature_oracle_bundle.py`

| Test ID | Test Name | Description | Scenario |
|---------|-----------|-------------|----------|
| FUNC-BDL-001 | `test_block_with_valid_bundle_accepted` | Block with valid bundle accepted | Miner includes valid bundle, network accepts |
| FUNC-BDL-002 | `test_block_with_invalid_bundle_rejected` | Block with invalid bundle rejected | Miner includes invalid signature, network rejects |
| FUNC-BDL-003 | `test_block_without_bundle_accepted` | Block without bundle accepted (optional) | Non-oracle miner creates block, network accepts |

### 4.3 DigiDollar Integration Tests (3 tests)

**File**: `/home/jared/Code/digibyte/test/functional/feature_digidollar_oracle.py`

| Test ID | Test Name | Description | Scenario |
|---------|-----------|-------------|----------|
| FUNC-DD-001 | `test_mint_transaction_oracle_price` | Mint uses oracle price (not mock) | User mints DD, tx validates with oracle price |
| FUNC-DD-002 | `test_redeem_transaction_oracle_price` | Redeem uses oracle price | User redeems DD, tx validates with oracle price |
| FUNC-DD-003 | `test_dca_system_oracle_price` | DCA health calculated with oracle | System health uses oracle price for calculations |

### 4.4 Testnet Reset Tests (2 tests)

**File**: `/home/jared/Code/digibyte/test/functional/feature_oracle_testnet_reset.py`

| Test ID | Test Name | Description | Scenario |
|---------|-----------|-------------|----------|
| FUNC-RST-001 | `test_testnet_reset_procedure` | Complete testnet reset works | Full reset: stop nodes, delete data, restart |
| FUNC-RST-002 | `test_oracle_operational_after_reset` | Oracle operational after reset | Reset complete, oracle broadcasts prices |

### 4.5 P2P Network Tests (4 tests)

**File**: `/home/jared/Code/digibyte/test/functional/feature_oracle_p2p.py`

| Test ID | Test Name | Description | Scenario |
|---------|-----------|-------------|----------|
| FUNC-P2P-001 | `test_oracle_message_relay` | Oracle messages relayed by peers | Oracle → Peer1 → Peer2 (relay) |
| FUNC-P2P-002 | `test_oracle_message_rate_limit` | Rate limiting enforced | Oracle floods messages, peers rate limit |
| FUNC-P2P-003 | `test_oracle_message_invalid_ban` | Invalid messages ban peer | Node sends invalid signatures, banned |
| FUNC-P2P-004 | `test_oracle_network_partition_recovery` | Network partition recovery | Network splits, rejoins, prices sync |

### 4.6 Exchange API Integration Tests (2 tests)

**File**: `/home/jared/Code/digibyte/test/functional/feature_oracle_exchange.py`

| Test ID | Test Name | Description | Scenario |
|---------|-----------|-------------|----------|
| FUNC-EX-001 | `test_exchange_api_live_fetch` | Live exchange API fetch (testnet) | Oracle fetches real prices from APIs |
| FUNC-EX-002 | `test_exchange_api_failure_handling` | Exchange failure handled gracefully | 1-2 exchanges fail, oracle still works |

### 4.7 Performance & Stress Tests (2 tests)

**File**: `/home/jared/Code/digibyte/test/functional/feature_oracle_performance.py`

| Test ID | Test Name | Description | Scenario |
|---------|-----------|-------------|----------|
| FUNC-PERF-001 | `test_oracle_price_fetch_latency` | Price fetch latency < 5 seconds | Measure median fetch time from 8 exchanges |
| FUNC-PERF-002 | `test_oracle_bundle_validation_speed` | Bundle validation < 10ms | Measure validation time in ConnectBlock |

### 4.8 Security Tests (3 tests)

**File**: `/home/jared/Code/digibyte/test/functional/feature_oracle_security.py`

| Test ID | Test Name | Description | Scenario |
|---------|-----------|-------------|----------|
| FUNC-SEC-001 | `test_signature_forgery_prevention` | Signature forgery prevented | Invalid signature rejected by all nodes |
| FUNC-SEC-002 | `test_replay_attack_prevention` | Replay attacks prevented | Old messages rejected by timestamp |
| FUNC-SEC-003 | `test_dos_attack_protection` | DoS attack protection | Flood of messages rate limited and banned |

**Total Functional Tests: 22 tests**

---

## 5. Test File Organization

### 5.1 Unit Test File Structure

```
/home/jared/Code/digibyte/src/test/
├── oracle_exchange_tests.cpp       (56 tests - Exchange API integration)
├── oracle_signature_tests.cpp      (18 tests - Schnorr signatures)
├── oracle_message_tests.cpp        (15 tests - Message handling)
├── oracle_bundle_tests.cpp         (22 tests - Bundle creation/validation)
├── oracle_cache_tests.cpp          (12 tests - Price caching)
├── oracle_p2p_tests.cpp            (14 tests - P2P protocol)
├── oracle_consensus_tests.cpp      (8 tests - Consensus integration)
└── oracle_digidollar_tests.cpp     (6 tests - DigiDollar integration)

TOTAL: 151 unit tests across 8 files
```

### 5.2 Functional Test File Structure

```
/home/jared/Code/digibyte/test/functional/
├── feature_oracle_price.py         (3 tests - Price broadcasting)
├── feature_oracle_bundle.py        (3 tests - Bundle validation)
├── feature_digidollar_oracle.py    (3 tests - DigiDollar integration)
├── feature_oracle_testnet_reset.py (2 tests - Testnet reset)
├── feature_oracle_p2p.py           (4 tests - P2P network)
├── feature_oracle_exchange.py      (2 tests - Exchange APIs)
├── feature_oracle_performance.py   (2 tests - Performance)
└── feature_oracle_security.py      (3 tests - Security)

TOTAL: 22 functional tests across 8 files
```

### 5.3 Test Makefile Integration

**File**: `/home/jared/Code/digibyte/src/Makefile.test.include`

```makefile
# Oracle Unit Tests
DIGIBYTE_TESTS = \
  test/oracle_exchange_tests.cpp \
  test/oracle_signature_tests.cpp \
  test/oracle_message_tests.cpp \
  test/oracle_bundle_tests.cpp \
  test/oracle_cache_tests.cpp \
  test/oracle_p2p_tests.cpp \
  test/oracle_consensus_tests.cpp \
  test/oracle_digidollar_tests.cpp

# Add to existing test suite
test_digibyte_SOURCES += $(DIGIBYTE_TESTS)
```

### 5.4 Functional Test Runner Configuration

**File**: `/home/jared/Code/digibyte/test/functional/test_runner.py`

```python
# Oracle Functional Tests
BASE_SCRIPTS = [
    # Existing tests...

    # Oracle tests (Phase One)
    'feature_oracle_price.py',
    'feature_oracle_bundle.py',
    'feature_digidollar_oracle.py',
    'feature_oracle_testnet_reset.py',
    'feature_oracle_p2p.py',
    'feature_oracle_exchange.py',
    'feature_oracle_performance.py',
    'feature_oracle_security.py',
]
```

---

## 6. TDD Workflow Timeline (6 Weeks)

### 6.1 Week 1: Core Infrastructure (Days 1-7)

**Week 1, Day 1-2: Chainparams & Dependencies**

**RED Phase** (4 hours):
- Write failing tests for chainparams oracle configuration
- Write failing tests for libcurl HTTP client integration
- Tests MUST fail (code not implemented yet)
- **Git Commit**: `RED: Add chainparams and libcurl tests`

**GREEN Phase** (4 hours):
- Implement chainparams oracle pubkey loading
- Implement basic libcurl HTTP GET functionality
- All tests PASS
- **Git Commit**: `GREEN: Implement chainparams and libcurl`

**REFACTOR Phase** (2 hours):
- Extract HTTP error handling
- Improve chainparams validation
- Tests still PASS
- **Git Commit**: `REFACTOR: Improve chainparams and HTTP client`

**Week 1, Day 3-5: Exchange API Integration**

**RED Phase** (8 hours):
- Write failing tests for ALL 8 exchanges (56 tests total)
  - Binance (7 tests)
  - CoinMarketCap (7 tests)
  - CoinGecko (7 tests)
  - Coinbase (7 tests)
  - Kraken (7 tests)
  - Messari (7 tests)
  - KuCoin (7 tests)
  - Crypto.com (7 tests)
- Write failing tests for median calculation (7 tests)
- **Git Commit**: `RED: Add exchange API and aggregation tests`

**GREEN Phase** (12 hours):
- Implement ParseBinanceResponse()
- Implement ParseCoinMarketCapResponse()
- Implement ParseCoinGeckoResponse()
- Implement ParseCoinbaseResponse()
- Implement ParseKrakenResponse()
- Implement ParseMessariResponse()
- Implement ParseKuCoinResponse()
- Implement ParseCryptoComResponse()
- Implement CalculateMedian()
- Implement FilterOutliers()
- All tests PASS
- **Git Commit**: `GREEN: Implement all 8 exchange API clients`

**REFACTOR Phase** (4 hours):
- Extract common JSON parsing logic
- Improve error messages
- Add documentation comments
- **Git Commit**: `REFACTOR: Improve exchange API code quality`

**Week 1, Day 6-7: Oracle Price Cache**

**RED Phase** (2 hours):
- Write failing cache tests (12 tests)
- **Git Commit**: `RED: Add oracle price cache tests`

**GREEN Phase** (4 hours):
- Implement COraclePriceCache class
- Implement AddPrice(), GetPrice(), GetLatestPrice(), Prune()
- All tests PASS
- **Git Commit**: `GREEN: Implement oracle price cache`

**REFACTOR Phase** (2 hours):
- Add thread safety (mutex)
- Optimize memory usage
- **Git Commit**: `REFACTOR: Add cache thread safety and optimization`

### 6.2 Week 2: Oracle Messages & Signatures (Days 8-14)

**Week 2, Day 1-2: Schnorr Signatures**

**RED Phase** (4 hours):
- Write failing Schnorr signature tests (18 tests)
- **Git Commit**: `RED: Add Schnorr signature tests`

**GREEN Phase** (6 hours):
- Implement GetSignatureHash() in COraclePriceMessage
- Implement SignSchnorr() integration
- Implement VerifySchnorr() integration
- Implement ValidateOracleSignature()
- All tests PASS
- **Git Commit**: `GREEN: Implement Schnorr signatures`

**REFACTOR Phase** (2 hours):
- Extract hash calculation logic
- Add signature validation error messages
- **Git Commit**: `REFACTOR: Improve signature code`

**Week 2, Day 3: Oracle Message Handling**

**RED Phase** (2 hours):
- Write failing message tests (15 tests)
- **Git Commit**: `RED: Add oracle message tests`

**GREEN Phase** (4 hours):
- Implement message serialization
- Implement message validation
- All tests PASS
- **Git Commit**: `GREEN: Implement oracle message handling`

**REFACTOR Phase** (2 hours):
- Improve validation error messages
- **Git Commit**: `REFACTOR: Improve message validation`

**Week 2, Day 4-5: P2P Protocol**

**RED Phase** (4 hours):
- Write failing P2P tests (14 tests)
- **Git Commit**: `RED: Add P2P protocol tests`

**GREEN Phase** (8 hours):
- Implement ORACLEPRICE message handler
- Implement ORACLEBUNDLE message handler
- Implement GETORACLES message handler
- Implement message relay logic
- Implement rate limiting
- All tests PASS
- **Git Commit**: `GREEN: Implement P2P oracle protocol`

**REFACTOR Phase** (2 hours):
- Extract rate limiting logic
- Improve DoS protection
- **Git Commit**: `REFACTOR: Improve P2P protocol`

**Week 2, Day 6-7: Buffer for Week 2 Completion**
- Complete any remaining Week 2 tasks
- Fix bugs discovered during testing
- Ensure 100% test pass rate

### 6.3 Week 3: Consensus & Validation (Days 15-21)

**Week 3, Day 1-2: Oracle Bundle Creation/Validation**

**RED Phase** (4 hours):
- Write failing bundle tests (22 tests)
- **Git Commit**: `RED: Add oracle bundle tests`

**GREEN Phase** (8 hours):
- Implement CreateBundle() in OracleBundleManager
- Implement ExtractOracleBundle()
- Implement ValidateOracleConsensus()
- Implement CheckOracleBundle()
- All tests PASS
- **Git Commit**: `GREEN: Implement oracle bundle creation and validation`

**REFACTOR Phase** (2 hours):
- Improve bundle validation error messages
- Optimize Merkle root calculation
- **Git Commit**: `REFACTOR: Improve bundle code`

**Week 3, Day 3: Consensus Integration**

**RED Phase** (2 hours):
- Write failing consensus tests (8 tests)
- **Git Commit**: `RED: Add consensus integration tests`

**GREEN Phase** (4 hours):
- Integrate CheckOracleBundle() into CheckBlock()
- Integrate into ContextualCheckBlock()
- Integrate into ConnectBlock() for cache updates
- All tests PASS
- **Git Commit**: `GREEN: Integrate oracle validation into consensus`

**REFACTOR Phase** (2 hours):
- Improve consensus error handling
- **Git Commit**: `REFACTOR: Improve consensus integration`

**Week 3, Day 4-5: DigiDollar Integration**

**RED Phase** (2 hours):
- Write failing DigiDollar integration tests (6 tests)
- **Git Commit**: `RED: Add DigiDollar oracle integration tests`

**GREEN Phase** (6 hours):
- Implement GetOraclePriceForHeight()
- Integrate into DigiDollar mint validation
- Integrate into DigiDollar redeem validation
- Integrate into DCA health calculation
- All tests PASS
- **Git Commit**: `GREEN: Integrate oracle into DigiDollar`

**REFACTOR Phase** (2 hours):
- Improve price lookup performance
- **Git Commit**: `REFACTOR: Optimize DigiDollar oracle integration`

**Week 3, Day 6-7: Buffer for Week 3 Completion**
- Complete any remaining Week 3 tasks
- Run full unit test suite (151 tests)
- Ensure 100% pass rate

### 6.4 Week 4: Block Mining & Integration (Days 22-28)

**Week 4, Day 1-2: Miner Integration**

**RED Phase** (2 hours):
- Write failing miner integration tests
- **Git Commit**: `RED: Add miner oracle bundle tests`

**GREEN Phase** (6 hours):
- Implement AddOracleBundleToBlock() in miner.cpp
- Embed bundle in coinbase OP_RETURN
- All tests PASS
- **Git Commit**: `GREEN: Implement miner oracle bundle integration`

**REFACTOR Phase** (2 hours):
- Optimize bundle embedding
- **Git Commit**: `REFACTOR: Improve miner bundle code`

**Week 4, Day 3: Oracle Node Daemon**

**RED Phase** (2 hours):
- Write failing oracle node daemon tests
- **Git Commit**: `RED: Add oracle daemon tests`

**GREEN Phase** (4 hours):
- Implement OracleNode::Run() main loop
- Implement price fetching every 60 seconds
- Implement message broadcasting
- All tests PASS
- **Git Commit**: `GREEN: Implement oracle node daemon`

**REFACTOR Phase** (2 hours):
- Improve daemon error handling
- **Git Commit**: `REFACTOR: Improve daemon reliability`

**Week 4, Day 4-7: Full Integration Testing**

- Run complete unit test suite (151 tests)
- Fix any integration bugs discovered
- Ensure all components work together
- Run valgrind memory leak checks
- **Git Commit**: `TEST: Full integration validation`

### 6.5 Week 5: Functional Testing (Days 29-35)

**Week 5, Day 1-2: Price Broadcasting Tests**

**RED Phase** (2 hours):
- Write failing functional test: `feature_oracle_price.py`
- **Git Commit**: `RED: Add price broadcasting functional test`

**GREEN Phase** (4 hours):
- Ensure oracle daemon broadcasts prices
- Ensure nodes receive and validate prices
- Test PASSES
- **Git Commit**: `GREEN: Price broadcasting functional test passes`

**Week 5, Day 3: Bundle Validation Tests**

**RED Phase** (2 hours):
- Write failing functional test: `feature_oracle_bundle.py`
- **Git Commit**: `RED: Add bundle validation functional test`

**GREEN Phase** (4 hours):
- Ensure blocks with valid bundles accepted
- Ensure blocks with invalid bundles rejected
- Test PASSES
- **Git Commit**: `GREEN: Bundle validation functional test passes`

**Week 5, Day 4-5: DigiDollar Integration Tests**

**RED Phase** (2 hours):
- Write failing functional test: `feature_digidollar_oracle.py`
- **Git Commit**: `RED: Add DigiDollar oracle functional test`

**GREEN Phase** (6 hours):
- Ensure mint transactions use oracle price
- Ensure redeem transactions use oracle price
- Ensure DCA system uses oracle price
- Test PASSES
- **Git Commit**: `GREEN: DigiDollar oracle functional test passes`

**Week 5, Day 6-7: Complete All Functional Tests**

- Write and pass remaining functional tests:
  - `feature_oracle_testnet_reset.py`
  - `feature_oracle_p2p.py`
  - `feature_oracle_exchange.py`
  - `feature_oracle_performance.py`
  - `feature_oracle_security.py`
- **Git Commit**: `GREEN: All 22 functional tests passing`

### 6.6 Week 6: Testnet Reset & Deployment (Days 36-42)

**Week 6, Day 1: Testnet Reset Procedures**

- Execute testnet reset following documented procedures
- Verify all nodes reset successfully
- **Git Commit**: `DEPLOY: Execute testnet reset`

**Week 6, Day 2: Deploy Testnet Oracle**

- Deploy oracle node on testnet
- Verify oracle broadcasts prices
- Monitor for errors
- **Git Commit**: `DEPLOY: Testnet oracle operational`

**Week 6, Day 3-4: Bug Fixes & Monitoring**

- Monitor oracle operation
- Fix any bugs discovered in live operation
- Run all tests again to verify fixes
- **Git Commits**: `FIX: [bug description]`

**Week 6, Day 5: Deploy Additional Testnet Nodes**

- Deploy 2-3 additional testnet nodes
- Verify P2P message propagation
- Verify consensus validation
- **Git Commit**: `DEPLOY: Additional testnet nodes`

**Week 6, Day 6-7: User Acceptance Testing**

- Test DigiDollar mint/redeem with oracle prices
- Verify end-to-end functionality
- Generate final test coverage report
- **Git Commit**: `TEST: User acceptance complete`

---

## 7. Coverage Measurement Plan

### 7.1 Coverage Tools & Configuration

**Tool**: lcov + gcov (integrated with make check)

**Configuration File**: `.lcovrc`
```ini
# Coverage configuration
lcov_branch_coverage = 1
genhtml_branch_coverage = 1

# Exclude patterns
lcov_excl_line = LCOV_EXCL_LINE|assert
lcov_excl_br_line = LCOV_EXCL_BR_LINE

# Output settings
genhtml_hi_limit = 90
genhtml_med_limit = 75
```

### 7.2 Coverage Report Generation

**Command Sequence**:
```bash
# Clean previous build
make clean

# Build with coverage flags
./configure CXXFLAGS="--coverage -O0" LDFLAGS="--coverage"
make -j$(nproc)

# Run unit tests
make check

# Run functional tests
python3 test/functional/test_runner.py \
  feature_oracle_price.py \
  feature_oracle_bundle.py \
  feature_digidollar_oracle.py \
  feature_oracle_testnet_reset.py \
  feature_oracle_p2p.py \
  feature_oracle_exchange.py \
  feature_oracle_performance.py \
  feature_oracle_security.py

# Generate coverage report
lcov --capture --directory . --output-file coverage.info

# Filter to oracle code only
lcov --extract coverage.info '*/src/oracle/*' '*/src/primitives/oracle.*' \
     --output-file coverage_oracle.info

# Generate HTML report
genhtml coverage_oracle.info --output-directory coverage_html

# View report
firefox coverage_html/index.html
```

### 7.3 Coverage Enforcement Thresholds

**Minimum Coverage by Component**:

| Component | File Pattern | Minimum Line Coverage | Minimum Branch Coverage |
|-----------|--------------|----------------------|------------------------|
| Exchange APIs | `src/oracle/exchange.*` | 90% | 85% |
| Schnorr Signatures | `src/primitives/oracle.*` (signatures) | 100% | 100% |
| Oracle Messages | `src/oracle/message.*` | 95% | 90% |
| Bundle Validation | `src/oracle/bundle.*` | 100% | 100% |
| Price Cache | `src/oracle/cache.*` | 95% | 90% |
| P2P Protocol | `src/net_processing.cpp` (oracle handlers) | 95% | 90% |
| Consensus Integration | `src/validation.cpp` (oracle code) | 95% | 90% |
| DigiDollar Integration | `src/consensus/digidollar_transaction_validation.cpp` | 95% | 90% |

**Overall Target**: 93% line coverage, 88% branch coverage

### 7.4 CI/CD Coverage Gates

**GitHub Actions Workflow**: `.github/workflows/oracle-coverage.yml`
```yaml
name: Oracle Coverage Check

on:
  pull_request:
    paths:
      - 'src/oracle/**'
      - 'src/primitives/oracle.*'
      - 'src/test/oracle_*_tests.cpp'
      - 'test/functional/feature_oracle_*.py'

jobs:
  coverage:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout code
        uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y lcov libboost-all-dev libcurl4-openssl-dev

      - name: Configure with coverage
        run: ./configure CXXFLAGS="--coverage -O0" LDFLAGS="--coverage"

      - name: Build
        run: make -j$(nproc)

      - name: Run tests
        run: make check

      - name: Generate coverage
        run: |
          lcov --capture --directory . --output-file coverage.info
          lcov --extract coverage.info '*/src/oracle/*' '*/src/primitives/oracle.*' \
               --output-file coverage_oracle.info

      - name: Check coverage threshold
        run: |
          COVERAGE=$(lcov --summary coverage_oracle.info 2>&1 | grep lines | awk '{print $2}' | sed 's/%//')
          echo "Coverage: $COVERAGE%"

          if (( $(echo "$COVERAGE < 93.0" | bc -l) )); then
            echo "❌ FAIL: Coverage $COVERAGE% below 93% threshold"
            exit 1
          else
            echo "✅ PASS: Coverage $COVERAGE% meets threshold"
          fi

      - name: Upload coverage report
        uses: codecov/codecov-action@v3
        with:
          files: ./coverage_oracle.info
          flags: oracle
          name: oracle-coverage
```

### 7.5 Coverage Reporting Dashboard

**Coverage Badge**: Add to README.md
```markdown
[![Oracle Coverage](https://codecov.io/gh/digibyte/digibyte/branch/feature/oracle-phase-one/graph/badge.svg?flag=oracle)](https://codecov.io/gh/digibyte/digibyte)
```

**Weekly Coverage Report**: Generated automatically on Fridays
```bash
#!/bin/bash
# scripts/weekly_coverage_report.sh

# Generate coverage
make check
lcov --capture --directory . --output-file coverage.info

# Generate report
lcov --extract coverage.info '*/src/oracle/*' --output-file coverage_oracle.info
genhtml coverage_oracle.info --output-directory weekly_report_$(date +%Y%m%d)

# Email report to team (optional)
echo "Oracle coverage report: file://$(pwd)/weekly_report_$(date +%Y%m%d)/index.html"
```

---

## 8. Test Infrastructure Requirements

### 8.1 Build Dependencies

**Required Libraries**:
```bash
# Ubuntu/Debian
sudo apt-get install -y \
  libboost-test-dev \
  libboost-thread-dev \
  libcurl4-openssl-dev \
  libjsoncpp-dev \
  lcov \
  gcov

# macOS
brew install boost curl jsoncpp lcov
```

**Build Flags**:
```makefile
# Makefile.am (add for test builds)
if ENABLE_TESTS
  AM_CXXFLAGS += --coverage -O0
  AM_LDFLAGS += --coverage
endif
```

### 8.2 Test Fixtures & Setup

**Boost Test Fixture**:
```cpp
// File: src/test/util/oracle_test_setup.h

struct OracleTestSetup : public BasicTestingSetup {
    OracleTestSetup() : BasicTestingSetup(CBaseChainParams::TESTNET) {
        // Setup testnet chainparams
        SelectParams(CBaseChainParams::TESTNET);

        // Generate test oracle key
        test_oracle_privkey.MakeNewKey(true);
        test_oracle_pubkey = XOnlyPubKey(test_oracle_privkey.GetPubKey());

        // Setup mock exchange responses
        mock_binance_response = R"({"symbol":"DGBUSDT","price":"0.012340"})";
        mock_coinmarketcap_response = R"({
          "data": {
            "DGB": {
              "quote": {
                "USD": {
                  "price": 0.01234
                }
              }
            }
          }
        })";

        // Initialize oracle bundle manager
        g_oracle_bundle_manager = std::make_unique<OracleBundleManager>();
    }

    ~OracleTestSetup() {
        g_oracle_bundle_manager.reset();
    }

    CKey test_oracle_privkey;
    XOnlyPubKey test_oracle_pubkey;
    std::string mock_binance_response;
    std::string mock_coinmarketcap_response;
};
```

**Usage in Tests**:
```cpp
BOOST_FIXTURE_TEST_SUITE(oracle_exchange_tests, OracleTestSetup)

BOOST_AUTO_TEST_CASE(test_binance_api_success)
{
    // test_oracle_privkey and mock_binance_response available
    ExchangePriceFetcher fetcher;
    auto price = fetcher.ParseBinanceResponse(mock_binance_response);
    BOOST_CHECK_EQUAL(*price, 12340);
}

BOOST_AUTO_TEST_SUITE_END()
```

### 8.3 Mock HTTP Client

**Purpose**: Avoid actual HTTP calls in unit tests

**Implementation**:
```cpp
// File: src/test/util/mock_http_client.h

class MockHttpClient {
public:
    // Set mock response for URL
    void SetMockResponse(const std::string& url, const std::string& response) {
        mock_responses[url] = response;
    }

    // Set mock error for URL
    void SetMockError(const std::string& url, int error_code) {
        mock_errors[url] = error_code;
    }

    // Perform mock HTTP GET
    std::optional<std::string> Get(const std::string& url) {
        if (mock_errors.count(url)) {
            return std::nullopt;
        }
        if (mock_responses.count(url)) {
            return mock_responses[url];
        }
        return std::nullopt;
    }

private:
    std::map<std::string, std::string> mock_responses;
    std::map<std::string, int> mock_errors;
};

// Global instance for tests
extern MockHttpClient g_mock_http_client;
```

**Usage**:
```cpp
BOOST_AUTO_TEST_CASE(test_binance_api_network_timeout)
{
    g_mock_http_client.SetMockError(BINANCE_URL, CURLE_OPERATION_TIMEDOUT);

    ExchangePriceFetcher fetcher;
    auto price = fetcher.FetchBinance();

    BOOST_CHECK(!price.has_value());
}
```

### 8.4 Python Test Framework Extensions

**Oracle Test Framework**:
```python
# File: test/functional/test_framework/oracle_util.py

from decimal import Decimal

def wait_for_oracle_price(node, timeout=30):
    """Wait for oracle price to be available."""
    import time
    start = time.time()
    while time.time() - start < timeout:
        try:
            price = node.getoracleprice()
            if price and price['price_micro_usd'] > 0:
                return price
        except:
            pass
        time.sleep(1)
    raise TimeoutError("Oracle price not available")

def assert_oracle_price_reasonable(price_micro_usd):
    """Assert oracle price is within reasonable range."""
    # 1 DGB should be between $0.001 and $1.00
    assert 1000 <= price_micro_usd <= 1000000, \
        f"Oracle price {price_micro_usd} micro-USD unreasonable"

def create_mock_oracle_bundle(oracle_privkey, price_micro_usd):
    """Create mock oracle bundle for testing."""
    # Implementation for creating test bundles
    pass
```

---

## 9. Test Data & Fixtures

### 9.1 Exchange API Mock Responses

**Binance**:
```json
{
  "symbol": "DGBUSDT",
  "price": "0.012340"
}
```

**CoinMarketCap**:
```json
{
  "data": {
    "DGB": {
      "quote": {
        "USD": {
          "price": 0.01234,
          "volume_24h": 1234567.89,
          "market_cap": 123456789.01
        }
      }
    }
  }
}
```

**CoinGecko**:
```json
{
  "digibyte": {
    "usd": 0.01234
  }
}
```

**Coinbase**:
```json
{
  "data": {
    "amount": "0.01234",
    "currency": "USD"
  }
}
```

**Kraken**:
```json
{
  "result": {
    "DGBUSD": {
      "c": ["0.01234", "100"]
    }
  }
}
```

**Messari**:
```json
{
  "data": {
    "market_data": {
      "price_usd": 0.01234
    }
  }
}
```

**KuCoin**:
```json
{
  "code": "200000",
  "data": {
    "price": "0.01234"
  }
}
```

**Crypto.com**:
```json
{
  "code": 0,
  "result": {
    "data": [{
      "i": "DGB_USD",
      "a": "0.01234"
    }]
  }
}
```

### 9.2 Test Oracle Keys

**Testnet Oracle Key** (for testing only):
```
Private Key (WIF): cVpF924EspNh8KjYsfhgY96mmxvT6DgdWiTYMtMjuM74hJaU5psW
Public Key (hex): 02a1633cafcc01ebfb6d78e39f687a1f0995c62fc95f51ead10a02ee0be551b5dc
Public Key (X-only): a1633cafcc01ebfb6d78e39f687a1f0995c62fc95f51ead10a02ee0be551b5dc
```

**WARNING**: Never use this key on mainnet or with real funds!

### 9.3 Test Price Data

**Typical Test Prices** (micro-USD):
```cpp
const CAmount TEST_PRICE_NORMAL = 12340;      // $0.01234 (typical)
const CAmount TEST_PRICE_LOW = 5000;          // $0.005 (low)
const CAmount TEST_PRICE_HIGH = 50000;        // $0.05 (high)
const CAmount TEST_PRICE_INVALID_ZERO = 0;    // Invalid
const CAmount TEST_PRICE_INVALID_NEG = -1000; // Invalid
const CAmount TEST_PRICE_OUTLIER = 1000000;   // $1.00 (outlier)
```

---

## 10. Integration Test Dependencies

### 10.1 Component Dependency Graph

```
Exchange APIs
     ↓
Oracle Message Creation (requires: Exchange APIs, Schnorr Signatures)
     ↓
P2P Broadcasting (requires: Oracle Messages)
     ↓
Oracle Bundle Manager (requires: Oracle Messages, P2P)
     ↓
Bundle Validation (requires: Oracle Bundles, Schnorr Signatures)
     ↓
Consensus Integration (requires: Bundle Validation, Chainparams)
     ↓
Miner Integration (requires: Bundle Manager, Consensus)
     ↓
DigiDollar Integration (requires: Oracle Price Cache, Consensus)
```

### 10.2 Test Execution Order

**Phase 1: Foundation** (can run in parallel)
- Exchange API tests
- Schnorr signature tests
- Oracle message tests
- Price cache tests

**Phase 2: Integration** (sequential dependencies)
1. P2P protocol tests (requires: oracle messages)
2. Bundle tests (requires: messages, signatures)
3. Consensus tests (requires: bundles)

**Phase 3: System Integration** (requires all previous)
- Miner integration tests
- DigiDollar integration tests
- Full system tests

### 10.3 Functional Test Prerequisites

**All Functional Tests Require**:
- Successful unit test completion (151/151 passing)
- Coverage threshold met (93%+)
- No memory leaks (valgrind clean)
- No compiler warnings

**Specific Prerequisites**:

| Functional Test | Requires Components |
|-----------------|---------------------|
| `feature_oracle_price.py` | P2P protocol, Oracle daemon |
| `feature_oracle_bundle.py` | Bundle validation, Consensus integration |
| `feature_digidollar_oracle.py` | DigiDollar integration, Price cache |
| `feature_oracle_testnet_reset.py` | All components |
| `feature_oracle_p2p.py` | P2P protocol, Message relay |
| `feature_oracle_exchange.py` | Exchange APIs (live or mock) |
| `feature_oracle_performance.py` | All components (for benchmarking) |
| `feature_oracle_security.py` | Schnorr signatures, Bundle validation |

---

## 11. Risk Mitigation & Contingency Planning

### 11.1 High-Risk Test Areas

**Risk 1: Exchange API Unreliability**
- **Mitigation**: Use mock HTTP client for unit tests
- **Contingency**: If live API tests fail, skip with warning (not failure)
- **Acceptance**: Unit tests with mocks MUST pass; live API tests optional

**Risk 2: Schnorr Signature Bugs**
- **Mitigation**: 100% coverage requirement for signature code
- **Contingency**: Independent cryptographic review before mainnet
- **Acceptance**: All 18 signature tests MUST pass

**Risk 3: P2P Message Flooding**
- **Mitigation**: Rate limiting tests MUST pass
- **Contingency**: Additional stress tests in Week 5
- **Acceptance**: DoS protection tests MUST pass

**Risk 4: Consensus Integration Bugs**
- **Mitigation**: 100% coverage for bundle validation
- **Contingency**: Manual testnet validation before deployment
- **Acceptance**: All consensus tests MUST pass

### 11.2 Timeline Contingency

**If behind schedule**:
- **Buffer**: 2 days built into Week 2, 2 days into Week 3
- **Priority**: CRITICAL tests implemented first
- **Sacrifice**: LOW priority tests deferred to post-Phase One

**If ahead of schedule**:
- **Enhancement**: Add additional edge case tests
- **Documentation**: Improve test documentation
- **Performance**: Add performance benchmarks

---

## 12. Success Metrics & Acceptance Criteria

### 12.1 Quantitative Metrics

| Metric | Target | Actual | Status |
|--------|--------|--------|--------|
| Total Unit Tests | 145+ | ___ | ⬜ |
| Total Functional Tests | 20+ | ___ | ⬜ |
| Unit Test Pass Rate | 100% | ___% | ⬜ |
| Functional Test Pass Rate | 100% | ___% | ⬜ |
| Average Code Coverage | 93%+ | ___% | ⬜ |
| Exchange API Coverage | 90%+ | ___% | ⬜ |
| Schnorr Signature Coverage | 100% | ___% | ⬜ |
| Bundle Validation Coverage | 100% | ___% | ⬜ |
| Memory Leaks (valgrind) | 0 | ___ | ⬜ |
| Compiler Warnings | 0 | ___ | ⬜ |

### 12.2 Qualitative Acceptance Criteria

**Code Quality**:
- [ ] All code follows DigiByte coding standards
- [ ] All functions have Doxygen comments
- [ ] All edge cases have test coverage
- [ ] All error paths have test coverage
- [ ] No magic numbers (all constants named)

**Test Quality**:
- [ ] All tests follow RED-GREEN-REFACTOR
- [ ] All tests have descriptive names
- [ ] All tests have comments explaining purpose
- [ ] All tests are deterministic (no flaky tests)
- [ ] All tests run in reasonable time (< 5 min total for unit, < 30 min for functional)

**Integration Quality**:
- [ ] DigiDollar mint uses oracle price (verified in functional test)
- [ ] DigiDollar redeem uses oracle price (verified in functional test)
- [ ] DCA system uses oracle price (verified in unit test)
- [ ] Testnet reset procedures work (verified in functional test)
- [ ] Oracle broadcasts prices every 60 seconds (verified in functional test)

**Documentation Quality**:
- [ ] Test strategy document complete (this document)
- [ ] All tests self-documenting (clear assertions)
- [ ] Coverage reports generated and accessible
- [ ] Test execution instructions documented

### 12.3 Final Sign-Off Checklist

**Before declaring Phase One testing complete**:

**Unit Testing**:
- [ ] 151/151 unit tests passing
- [ ] 0 memory leaks (valgrind)
- [ ] 0 compiler warnings
- [ ] Coverage ≥ 93% (all components)
- [ ] All RED-GREEN-REFACTOR commits visible in git history

**Functional Testing**:
- [ ] 22/22 functional tests passing
- [ ] Oracle price broadcast verified on testnet
- [ ] DigiDollar integration verified on testnet
- [ ] Testnet reset procedures verified
- [ ] Performance benchmarks met (<5s fetch, <10ms validation)

**Code Review**:
- [ ] Orchestrator code review complete
- [ ] No TDD violations found
- [ ] All integration points validated
- [ ] Security review complete (signatures, validation)

**Documentation**:
- [ ] Test strategy document (this file) finalized
- [ ] Coverage reports generated and stored
- [ ] Test execution guide created
- [ ] Known issues documented (if any)

**Deployment Readiness**:
- [ ] Testnet oracle operational
- [ ] 3+ testnet nodes validating oracle bundles
- [ ] DigiDollar transactions using oracle price
- [ ] 7+ days stable operation

---

## 13. Appendix

### 13.1 Glossary

**TDD**: Test-Driven Development - Write tests before code
**RED Phase**: Write failing tests that define expected behavior
**GREEN Phase**: Write minimal code to make tests pass
**REFACTOR Phase**: Improve code quality while keeping tests passing
**Coverage**: Percentage of code executed by tests
**Micro-USD**: Price format (1,000,000 = $1.00, NOT cents!)
**Schnorr Signature**: 64-byte cryptographic signature
**Oracle Bundle**: Collection of oracle price messages in block
**OP_RETURN**: Bitcoin script opcode for embedding data
**MAD**: Median Absolute Deviation (outlier filtering algorithm)

### 13.2 References

- **Phase One Spec**: `/home/jared/Code/digibyte/DIGIDOLLAR_ORACLE_PHASE_ONE_SPEC.md`
- **Sub-Agent Context**: `/home/jared/Code/digibyte/DIGIDOLLAR_ORACLE_SUBAGENT_CONTEXT.md`
- **DigiDollar Architecture**: `/home/jared/Code/digibyte/DIGIDOLLAR_ARCHITECTURE.md`
- **Boost Test Documentation**: https://www.boost.org/doc/libs/1_83_0/libs/test/doc/html/index.html
- **lcov Documentation**: http://ltp.sourceforge.net/coverage/lcov.php

### 13.3 Contact & Support

**Test Engineer**: Sub-Agent specialized in TDD methodology
**Orchestrator**: Main AI agent coordinating Phase One implementation
**Code Review**: All test code reviewed by orchestrator before acceptance

---

**END OF TEST STRATEGY DOCUMENT**

**Document Statistics**:
- Total Pages: ~35 pages
- Total Tests Defined: 173 tests (151 unit + 22 functional)
- Coverage Targets: 93% average
- Timeline: 6 weeks (42 days)
- Components: 8 major test suites
- Test Files: 16 files (8 unit + 8 functional)

**Next Steps**:
1. Orchestrator reviews and approves test strategy
2. Test Engineer begins RED phase (Week 1, Day 1)
3. Exchange Integration Engineer implements after tests written
4. Continuous validation throughout 6-week timeline

**Status**: ✅ READY FOR REVIEW - DO NOT IMPLEMENT UNTIL APPROVED
