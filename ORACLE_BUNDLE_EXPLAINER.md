# DigiDollar Oracle Bundle Explainer

This document explains the DigiDollar oracle bundle system in plain language first, then in enough technical detail that another engineer can build, review, test, and operate it.

It describes the current DigiDollar V1 design used by this branch:

- Live exchange prices feed independent oracle operators.
- Oracles use MuSig2 to create one aggregate Schnorr signature.
- Blocks carry compact v0x03 oracle bundles in the coinbase transaction.
- Full nodes verify the bundle before accepting DigiDollar price-dependent blocks.
- Wallet and RPC code fail closed when the chain does not have a valid price.

The main idea is simple:

> The chain only trusts a price when enough independent oracle keys signed the same price, timestamp, signer set, and nonce set for the same epoch.

## Who This Is For

If you are not a programmer, read:

- "The Short Version"
- "What Problem The Oracle Solves"
- "How The 9 Oracles Are Selected"
- "What Happens When Oracles Go Offline"
- "Why The Bundle Is Safe To Trust"

If you are building or reviewing code, also read:

- "The Full Flow"
- "MuSig2 Signing Context"
- "v0x03 Bundle Format"
- "Validator Rules"
- "Recovery Rules"
- "Implementation Map"
- "Testing Blueprint"

## The Short Version

DigiDollar needs a live DGB/USD price. The blockchain itself cannot call exchanges, so independent oracle operators fetch prices and sign a shared answer.

Every oracle does three jobs:

1. Fetch a live DGB/USD price from exchanges.
2. Broadcast a signed price message to other nodes.
3. Join a MuSig2 signing round when selected for the current epoch.

An epoch is a fixed block window. In the current V1 testnet setup, oracle epochs rotate every 40 blocks.

During an epoch, oracle nodes collect nonce messages. A nonce is a one-time public commitment used for one signing round. The matching secret nonce must never be reused.

The signer set is not supposed to be "oracle IDs 0 through 8 forever." Current V1 code uses deterministic epoch scoring:

1. Look at the oracle IDs that submitted valid nonces for the epoch.
2. Score each one with `GetOracleEpochSelectionHash(epoch, oracle_id)`.
3. Sort by score.
4. Take the first threshold set, currently 9 signers on testnet/mainnet V1.

That gives every node the same answer without a coordinator. It also means higher-numbered oracle IDs count. If IDs 6, 7, and 8 are offline but IDs 10, 14, and 15 are online and submit nonces, the session can still reach 9 signers.

Once the selected signers agree on the price and timestamp, they produce one aggregate MuSig2 signature. The miner puts that signature and the price into the block as a v0x03 oracle bundle.

Every full node verifies the bundle from the block. If the bundle is missing, malformed, stale, or signed by too few valid oracle keys, price-dependent DigiDollar actions fail closed.

## What Problem The Oracle Solves

DigiDollar minting and redemption need to know how much DGB equals a given amount of DD.

Example:

- If DGB is worth $0.004, then 1 DGB is 0.4 cents.
- A mint for $100 of DD needs enough DGB collateral for that live price and the chosen lock tier.
- A redemption must release collateral according to the rules for the vault being redeemed.

The chain needs one price that all nodes can verify. It cannot accept:

- A wallet's local price cache.
- A miner's private price.
- A fallback test price.
- A single exchange quote.
- A single oracle operator's word.

The oracle bundle solves this by committing one quorum-signed price into the block.

## Core Terms

### Oracle

An oracle is an operator with an assigned oracle ID and public key in chainparams. The operator runs DigiByte Core with oracle signing enabled and uses the matching private key.

### Oracle Roster

The roster is the list of oracle public keys known by consensus parameters.

The current V1 shape is:

- Testnet/mainnet V1 target: 17 oracle public keys, 9 required signatures.
- Regtest: 7 oracle public keys, 4 required signatures.

Changing the roster or threshold is a consensus-sensitive change. Do not do it casually.

### Epoch

An epoch is a block window used for oracle signing rotation.

Current testnet/mainnet V1 code uses:

- `nDDOracleEpochBlocks = 40`
- About 10 minutes at roughly 15 seconds per block.
- About 6 oracle epochs per hour.

Each epoch gets its own nonce exchange and signing context.

### Price Message

A price message says:

- Oracle ID.
- DGB/USD price in micro-USD.
- Timestamp.
- Signature by that oracle.

All oracle operators can broadcast price messages. Price gossip is not the final chain truth. It is input to the MuSig2 signing round.

### Nonce

A nonce is a one-time MuSig2 signing commitment.

There are two parts:

- Public nonce: broadcast to peers.
- Secret nonce: kept private in memory and destroyed after signing.

Never reuse a secret nonce. Reusing signing nonces can leak private keys.

### Partial Signature

Each selected oracle signs the same message and broadcasts one partial signature.

The partial signature is not useful by itself. When enough matching partial signatures arrive, they combine into one 64-byte Schnorr aggregate signature.

### Bundle

The bundle is the compact data committed to the block. It includes:

- Version byte `0x03`.
- Signer bitmap.
- Epoch.
- Price.
- Timestamp.
- Aggregate Schnorr signature.

### Bitmap

The bitmap says which oracle IDs signed.

If bit 5 is set, oracle ID 5 signed. If bit 14 is set, oracle ID 14 signed.

Validators use the bitmap to reconstruct the aggregate public key and verify the aggregate signature.

### Fail Closed

Fail closed means "do not guess."

If the node cannot prove a valid oracle price from the chain, DigiDollar mint/redeem behavior that needs a price stops instead of inventing one.

## System Flow At A Glance

```text
               LIVE EXCHANGES
                     |
                     v
        +---------------------------+
        | Independent oracle nodes  |
        | fetch DGB/USD prices      |
        +---------------------------+
                     |
                     v
        +---------------------------+
        | Signed price gossip       |
        | ORACLEPRICE messages      |
        +---------------------------+
                     |
                     v
        +---------------------------+
        | Epoch MuSig2 session      |
        | nonce round + signing     |
        +---------------------------+
                     |
                     v
        +---------------------------+
        | v0x03 oracle bundle       |
        | in coinbase transaction   |
        +---------------------------+
                     |
                     v
        +---------------------------+
        | Full nodes validate       |
        | price, bitmap, signature  |
        +---------------------------+
                     |
                     v
        +---------------------------+
        | DigiDollar mint/redeem    |
        | uses validated price      |
        +---------------------------+
```

## The Full Flow

### Phase 1: Price Fetching

Each oracle operator continuously fetches live DGB/USD prices.

The node rejects bad prices before they can become consensus input:

- Price must be within the configured minimum and maximum.
- Timestamp must be fresh enough.
- Oracle ID must be part of the active consensus keyset.
- Signature must authenticate the oracle message.

Good messages are stored in the oracle bundle manager pending pool.

```text
Oracle 0 price ---> pending_messages[0]
Oracle 1 price ---> pending_messages[1]
Oracle 2 price ---> pending_messages[2]
...
Oracle 15 price --> pending_messages[15]
```

The pending pool is local memory. It is not the chain truth yet.

### Phase 2: Start An Epoch Session

When blocks connect, the signing orchestrator ticks the current epoch session.

For each local oracle key running on the node:

1. Compute the aggregate key context for the roster.
2. Generate a fresh MuSig2 nonce.
3. Keep the secret nonce private.
4. Broadcast the public nonce.

```text
Block connected
      |
      v
TickEpochSession(epoch, height)
      |
      v
Generate public nonce for local oracle IDs
      |
      v
Broadcast ORACLEMUSIGNONCE
```

### Phase 3: Collect Nonces

Every node collects valid public nonces.

The nonce map is keyed by oracle ID:

```text
m_pubnonces = {
  0: nonce_from_oracle_0,
  1: nonce_from_oracle_1,
  2: nonce_from_oracle_2,
  5: nonce_from_oracle_5,
  10: nonce_from_oracle_10,
  14: nonce_from_oracle_14,
  ...
}
```

Once enough valid nonces exist, the node can select the threshold signing set.

## How The 9 Oracles Are Selected

No central node picks the signers.

Every honest node can compute the same ranking from public data:

```text
score = GetOracleEpochSelectionHash(epoch, oracle_id)
```

Then nodes sort by score and take the first threshold set from the nonce submitters.

Example with 9 required signers:

```text
Epoch: 40
Nonce submitters: 0, 1, 2, 3, 4, 5, 10, 14, 15

Compute score for each submitter:

  score(40, 14) = lowest
  score(40, 5)
  score(40, 1)
  score(40, 3)
  score(40, 10)
  score(40, 0)
  score(40, 4)
  score(40, 2)
  score(40, 15) = highest in this set

Sorted committee:

  14, 5, 1, 3, 10, 0, 4, 2, 15
```

All 9 submitted nonces, so the session can move forward.

This is the important behavior:

```text
Wrong behavior:
  Always wait for 0,1,2,3,4,5,6,7,8.
  If 6,7,8 are offline, signing stalls forever.

Current V1 behavior:
  Rank nonce submitters by epoch score.
  Take the threshold set from actual submitters.
  Higher IDs can sign when low IDs are offline.
```

The ranking changes when the epoch changes, because the epoch number is part of the score.

```text
Epoch 40 committee order: 14, 5, 1, 3, 10, 0, 4, 2, 15
Epoch 41 committee order: different score order
Epoch 42 committee order: different score order
```

This is deterministic rotation, not a hidden coordinator.

Important precision:

- Current V1 code uses epoch-scored deterministic rotation.
- A future hardening can seed selection with the epoch-start block hash if the consensus rules are updated to validate that exact seed.
- Do not call the future block-hash design implemented unless the code actually changes and tests prove it.

## Phase 4: Compute The Price To Sign

The signing round must sign one exact message.

That message is built from:

- Consensus price.
- Consensus timestamp.

For RC36, the price calculation used for the signing context is scoped to the selected signer IDs.

That matters because the signed message must not drift while nodes are signing.

```text
Selected signers:
  0, 1, 2, 3, 4, 5, 10, 14, 15

Use price messages from exactly those IDs:
  pending_messages[0]
  pending_messages[1]
  pending_messages[2]
  pending_messages[3]
  pending_messages[4]
  pending_messages[5]
  pending_messages[10]
  pending_messages[14]
  pending_messages[15]

Ignore non-signer prices for this signing message.
```

The price algorithm uses the existing oracle consensus price calculation. Outliers are filtered before the median price is chosen.

If a selected signer does not have a fresh price message, the node waits instead of signing a different message.

## Phase 5: Freeze The Signing Context

This is the RC36 hardening point.

MuSig2 partial signatures are only valid for one exact signing transcript. The transcript includes:

- Chain genesis hash.
- Epoch.
- Context version.
- Price/timestamp message hash.
- Signer bitmap.
- Public nonce set hash.

RC36 computes a `session_context_id` from those values.

```text
session_context_id =
  H(
    "DigiDollar/MuSig2SessionContext/v1",
    chain_genesis_hash,
    epoch,
    context_version,
    message_hash,
    signer_bitmap,
    nonce_set_hash
  )
```

That context ID is carried by partial signature P2P messages.

Why it matters:

- A partial signature for price A must not be accepted into a session signing price B.
- A partial signature for signer set A must not be accepted into signer set B.
- A partial signature using nonce set A must not be accepted into nonce set B.
- A partial signature from another chain must not be accepted here.

Before this hardening, partial signature messages identified the epoch and oracle ID, but not the exact transcript. That was safe in the sense that invalid aggregate signatures failed verification, but it hurt liveness because honest nodes could produce partial signatures for slightly different local contexts and then reject each other's partials.

RC36 makes the context explicit.

## Phase 6: Broadcast Partial Signatures

Each selected local oracle signs the exact message and broadcasts:

```text
OracleMusigPartialSigMsg {
  epoch,
  context_version,
  session_context_id,
  oracle_id,
  partial_sig,
  auth_signature
}
```

The authentication signature also commits to the context.

If a peer changes the context version or context ID after signing, authentication fails.

If a node receives a partial signature for a different context than its local session, it ignores it.

Pending partial signatures are buffered by:

```text
epoch -> session_context_id -> oracle_id
```

That prevents a stale or different-context partial signature from replacing the right one.

The buffer is also bounded per context and per epoch. That matters because an attacker with an authenticated oracle key must not be able to create unlimited pending entries by grinding random context IDs.

## Phase 7: Aggregate The Signature

When enough valid partial signatures arrive for the same context:

1. The node aggregates the partial signatures.
2. The result is one 64-byte BIP-340 Schnorr signature.
3. The completed session records the signed price and signed timestamp.

```text
Partial sig 0  \
Partial sig 1   \
Partial sig 2    \
...              +--> AggregateSignature() --> 64-byte Schnorr signature
Partial sig 14  /
Partial sig 15 /
```

The miner can now include a v0x03 bundle in a block.

## Phase 8: Mine The Bundle

The miner adds the oracle bundle to the coinbase transaction as an `OP_RETURN OP_ORACLE` output.

```text
Block
  |
  +-- coinbase transaction
        |
        +-- OP_RETURN OP_ORACLE <v0x03 bundle>
```

This keeps the oracle proof inside the block. Validators do not need the off-chain P2P messages to validate the final block.

## v0x03 Bundle Format

The on-chain bundle format is unchanged by RC36.

```text
+------------+---------------+-------------------------------+
| Field      | Size          | Meaning                       |
+------------+---------------+-------------------------------+
| version    | 1 byte        | 0x03                          |
| bitmap_len | 1 byte        | Number of bitmap bytes        |
| bitmap     | bitmap_len    | One bit per oracle ID         |
| epoch      | 4 bytes       | Little-endian uint32          |
| price      | 8 bytes       | Little-endian uint64 microUSD |
| timestamp  | 8 bytes       | Little-endian uint64 unix     |
| signature  | 64 bytes      | BIP-340 Schnorr aggregate sig |
+------------+---------------+-------------------------------+
```

Example shape for a 17-oracle roster:

```text
version       = 03
bitmap_len    = 03
bitmap        = 3 bytes, enough for oracle IDs 0..16
epoch         = current oracle epoch
price         = DGB/USD in micro-USD
timestamp     = oracle consensus timestamp
signature     = 64-byte aggregate Schnorr signature
```

The bitmap is critical. It tells validators exactly which oracle public keys to aggregate.

## Validator Rules

Full nodes validate the block bundle. They do not trust the miner.

The validator checks:

```text
Bundle version is 0x03
Bitmap length matches the configured oracle roster size
Bitmap has at least the required number of signers
Every set bit is a valid oracle ID
Price is inside allowed min/max bounds
Timestamp is not stale or too far in the future
Epoch matches the block height's oracle epoch
Aggregate public key reconstructed from bitmap signers
Aggregate signature verifies against the signed price/timestamp message
```

If these checks fail, the bundle is invalid. Price-dependent DigiDollar behavior cannot use it.

## What Happens When Oracles Go Offline

The system is designed to fail closed and recover.

### If fewer than threshold oracles are available

If fewer than 9 valid testnet/mainnet V1 oracles are online and fresh:

- No valid MuSig2 aggregate bundle can be produced.
- Minting that needs a fresh price pauses.
- Redemption paths that need a fresh price pause unless their rules can use already locked data.
- The node does not invent a price.
- Existing chain history remains valid.

This is an outage, not a consensus split.

### If enough oracles come back

When enough oracles return:

1. They fetch live prices again.
2. They broadcast fresh price messages.
3. They broadcast fresh nonces for the current epoch.
4. Nodes select the threshold signer set from nonce submitters.
5. A new context-bound MuSig2 signing round completes.
6. New v0x03 bundles appear in blocks.

No manual fake price is required.

### If an oracle misses an epoch

Missing an epoch does not permanently break the node.

The next epoch is a fresh session with fresh nonces and a fresh context ID.

### If an oracle goes down for days

The oracle can recover by starting again with its assigned key on the correct chain. It does not need old secret nonces. Old nonces are not reusable and should be gone.

The network still needs threshold participation. If too many operators are offline at once, the system waits.

## Why The Bundle Is Safe To Trust

The bundle is safe only because several protections stack together:

```text
Live exchange fetching
      |
      v
Independent oracle signatures on price messages
      |
      v
Deterministic threshold signer selection
      |
      v
MuSig2 aggregate signature over one exact message
      |
      v
Signer bitmap committed in the bundle
      |
      v
Full-node validation of bitmap, price, timestamp, and signature
```

The miner cannot choose an arbitrary price because it cannot forge the aggregate signature.

A single oracle cannot choose the price because threshold signing is required.

A stale partial signature cannot be mixed into a new context because RC36 binds partial signatures to `session_context_id`.

A message from another network cannot be replayed because oracle message hashes bind to the chain genesis hash.

## Decentralization Model

No single node decides the bundle.

Several independent pieces must line up:

- Oracles independently fetch price data.
- Oracles independently broadcast price and nonce messages.
- Every node independently scores nonce submitters for the epoch.
- Every node independently computes the selected signer set.
- Every selected oracle independently signs the same context.
- Any miner can include a completed bundle.
- Every validator independently verifies the final bundle from the block.

The bundle only becomes useful when enough independent oracle keys signed the same thing.

## What RC36 Hardens

RC36 keeps the on-chain v0x03 format and the 9-of-17 V1 model. It hardens the off-chain signing path.

### Problem

Live logs showed nodes collecting prices and nonces, then getting stuck with too few valid partial signatures.

The deeper issue was that partial signature messages did not carry enough context. A partial signature was identified by epoch and oracle ID, but not by the exact price/timestamp, signer bitmap, and nonce set.

Honest nodes could end up signing slightly different local contexts in the same epoch. Verification rejected the mismatched partials, which protected safety, but signing stalled.

### Fix

RC36 adds an explicit MuSig2 session context ID to partial signature messages.

The context ID binds:

- Chain.
- Epoch.
- Price/timestamp message.
- Selected signer bitmap.
- Public nonce set.

Nodes now reject or buffer partial signatures by exact context.

### Result

Nodes only aggregate partial signatures that belong to the same signing transcript.

That improves liveness without weakening validation and without changing the on-chain bundle format.

## Startup And First Epoch

At startup, oracle nodes may not all be online at once.

The correct behavior is:

1. Start fetching live prices as soon as oracle mode is running.
2. Start nonce exchange for the current epoch.
3. Wait until threshold fresh prices and threshold nonces exist.
4. Sign only when the exact context is ready.
5. Mine a bundle once the aggregate signature completes.

If the first epoch after activation does not get threshold participation, the system waits for the next viable epoch. It should not fall back to a fake price.

This works for testnet and mainnet because the bundle is validated from the block, not from a local startup assumption.

## Existing Testnet And Mainnet Readiness

### Existing testnet

RC36 does not reset testnet24 and does not change the v0x03 bundle format.

Nodes running the RC36 P2P partial-signature format need to be upgraded together for live oracle signing, because partial signature messages now include context fields.

The chain-visible bundle remains v0x03. Validators still verify the same on-chain data shape.

### Mainnet deployment

Mainnet deployment still needs Jared's review, tag, release, and deploy decision.

Before mainnet activation, the must-pass behavior is:

- Live exchange price path works.
- At least threshold oracles are online.
- MuSig2 sessions complete with context-bound partial signatures.
- Miners include valid v0x03 bundles.
- Validators reject invalid bundles.
- Wallet/RPC/Qt fail closed when no price is available.
- Restart, rescan, reindex, backup, and restore do not corrupt DigiDollar state.

## Implementation Map

Key files:

```text
src/primitives/oracle.{h,cpp}
  Oracle price messages, oracle constants, epoch selection helpers.

src/oracle/musig2_session.{h,cpp}
  Nonce collection, participant selection, nonce aggregation,
  session context ID, partial signature aggregation.

src/oracle/signing_orchestrator.{h,cpp}
  Per-block epoch ticking, local oracle nonce/signature broadcast,
  pending partial signature buffering.

src/oracle/bundle_manager.{h,cpp}
  Price message pool, selected-oracle consensus values,
  bundle assembly and validation helpers.

src/oracle/musig2_messages.h
  P2P message structures for MuSig2 nonce and partial signatures.

src/protocol.cpp
  Oracle P2P message hashes and authentication hash binding.

src/net_processing.cpp
  P2P relay and rejection rules for oracle messages.

src/kernel/chainparams.cpp
  Oracle roster, threshold, epoch length, activation parameters.

src/digidollar/validation.cpp
  DigiDollar block and transaction validation using chain oracle data.

src/wallet, src/rpc, src/qt
  User-facing mint, redeem, transfer, balance, and status behavior.
```

## Testing Blueprint

The oracle bundle system is not proven by one unit test. It needs layered testing.

### Unit tests

Unit tests should prove:

- Epoch scoring is deterministic.
- Committee selection changes across epochs.
- Offline low IDs do not block a valid threshold set.
- Partial signature authentication binds the session context.
- Partial signatures from the wrong context are rejected.
- Selected-oracle price calculation ignores non-signer outliers for the signing context.
- v0x03 bundle parsing and validation reject malformed bundles.

### Functional tests

Functional tests should prove:

- DigiDollar activation boundaries are respected.
- Mint/redeem paths require valid oracle data.
- Wallet state survives restart, rescan, reindex, backup, and restore.
- Regtest mock oracle prices only work when explicitly enabled.
- No production path silently uses a fake price.

### Fuzz tests

Fuzz tests should exercise:

- Oracle bundle parsing.
- Bitmap parsing.
- MuSig2 aggregate validation boundaries.
- Partial signature message serialization.
- Price and timestamp edge cases.

### Live multi-oracle script

The highest-value gate is:

```bash
./test_multi_oracle_testnet.sh
```

That script proves the system as a whole:

- Live prices.
- Multiple oracle operators.
- MuSig2 bundles.
- Mining.
- Minting.
- Redemption.
- Transfers.
- Oracle recovery.
- Wallet restart.
- Wallet restore.
- Rescan.
- Reindex.

Partial progress is not a pass. It must finish end-to-end.

## Operator Checklist

For oracle operators:

```text
1. Confirm the node is on the intended network.
2. Confirm the wallet/oracle key for the assigned oracle ID is available.
3. Confirm live price fetching is working.
4. Confirm nonce messages are being sent and received.
5. Confirm partial signature messages include a non-null session context.
6. Confirm v0x03 bundles appear after activation.
7. Confirm getoracleprice returns a nonzero validated chain price.
8. Do not use mock prices on production-style testnet or mainnet.
```

Useful RPCs:

```bash
digibyte-cli -testnet getblockchaininfo
digibyte-cli -testnet getnetworkinfo
digibyte-cli -testnet getoracleinfo
digibyte-cli -testnet listoraclekeys
digibyte-cli -testnet getoracleprice
digibyte-cli -testnet getdigidollarstats
```

## Failure Cheat Sheet

```text
Symptom:
  Prices are visible, but no bundle is mined.

Check:
  Are at least threshold fresh oracle price messages present?
  Are at least threshold public nonces present for the epoch?
  Are partial signatures using the same session_context_id?

Symptom:
  Partial signatures arrive but do not aggregate.

Check:
  Does the message context match the local session context?
  Are all partials for the same price/timestamp, bitmap, and nonce set?
  Are old buffered partials being ignored by context?

Symptom:
  Bundle is mined but rejected.

Check:
  Does bitmap length match roster size?
  Does popcount meet threshold?
  Does the aggregate signature verify against the bitmap signer keys?
  Is timestamp within allowed window?
  Is price within min/max bounds?

Symptom:
  Minting fails with no oracle price.

Check:
  Is there a validated on-chain price?
  Did the block include a valid v0x03 bundle?
  Is this a real oracle outage? If yes, fail-closed is correct.
```

## Rules That Should Not Be Weakened

Do not weaken these just to make tests pass:

- Do not add production fallback prices.
- Do not let a single oracle set the chain price.
- Do not accept partial signatures without a valid session context.
- Do not accept stale or future oracle timestamps.
- Do not accept invalid signer bitmap bits.
- Do not accept bundles with too few signers.
- Do not make wallet caches consensus truth.
- Do not treat regtest mock prices as production behavior.

## Final Mental Model

Think of a DigiDollar oracle bundle as a notarized price receipt:

- The receipt is the v0x03 bundle.
- The notaries are the selected oracle keys.
- The signatures are combined into one compact Schnorr signature.
- The bitmap lists exactly which notaries signed.
- The block carries the receipt.
- Every full node checks the receipt before using the price.

If the receipt is valid, DigiDollar can use the price.

If the receipt is missing or invalid, DigiDollar stops rather than guessing.
