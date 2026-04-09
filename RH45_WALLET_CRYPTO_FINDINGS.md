# RH-45: HD Key Derivation & Wallet Crypto — Security Findings

## Summary: 2 CRITICAL, 2 HIGH, 3 MEDIUM issues found (code review, no test commits)

### 🔴 CRITICAL #1: Owner key reuse across transfer change outputs
- **File:** `src/wallet/digidollarwallet.cpp:1464`
- **Bug:** `StoreOwnerKey(result.tx.GetHash(), spenderKey)` reuses the spender key for change
- **Fix:** Generate fresh HD key for each transfer change output

### 🔴 CRITICAL #2: Random key fallback on locked wallet in RedeemDigiDollar
- **File:** `src/wallet/digidollarwallet.cpp:4448, 4785`
- **Bug:** `ownerKey.MakeNewKey(true)` generates RANDOM key when wallet locked
- **Fix:** Return explicit error instead of generating random key

### 🟡 HIGH #1: DD keys share standard BIP86 derivation — no dedicated path
- **Impact:** DD owner/address keys consume from same keypool as regular DGB addresses
- **Fix:** Dedicated derivation purpose for DD (e.g., `m/86'/20'/1'`)

### 🟡 HIGH #2: DD keys not independently recoverable from HD seed
- **Impact:** After wallet restore, DD-tweaked output keys require expensive brute-force scan
- **Fix:** Store DD key metadata in recoverable format

### 🟠 MEDIUM #1-3:
- Dead key material never cleaned up after redemption
- Extended key lifetime in `dd_owner_keys` map
- TransferDigiDollar key lookup is O(n²) — potential RPC DoS
