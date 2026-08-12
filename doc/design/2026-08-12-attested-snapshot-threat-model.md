# Adversarial review — oracle-attested UTXO snapshot bootstrap

**Scope:** the design in `2026-08-11-assumeutxo-capability-audit.md` §9.
**Tree:** `origin/develop` @ `16159311b3` (9.26.5); behaviour identical in deployed 9.26.4.
**Date:** 2026-08-12. **Read-only** — no code written.
**Posture:** assume the attacker is competent, patient, and reads this document.

---

## 0. The one-line threat-model shift

> `loadtxoutset` was designed for a file **an operator chose and placed on disk**. This design has the **node fetch it automatically from an untrusted mirror**. Every assumption upstream made about who supplied that file no longer holds.

Most findings below descend from that single change. Upstream's code is not wrong; it is being asked to do a different job.

---

## 1. Findings by severity

### C-1 · CRITICAL · Oracles must independently compute the hash

**Attack.** If the attestation ceremony takes the UTXO hash *as input* from the generation node (or any coordinator), then compromising that one machine forges an attestation carrying 7-of-35 signatures. The threshold becomes decorative: 35 oracles would be rubber-stamping one host's claim.

**Why it is easy to get wrong.** The natural implementation — generator computes hash, publishes it, oracles sign what they're given — is also the natural way to build a signing ceremony, and it is fatal here.

**Mitigation (design, non-negotiable).** Each oracle MUST compute the UTXO hash from **its own chainstate at the specified height** and sign only a value it derived itself. An oracle that cannot compute it must abstain, never defer. Determinism makes this safe: `Coin` serialization is upstream-identical (`src/coins.h:31`), so honest oracles always agree.

**Spec language:** state this as a MUST, in bold, with the failure mode spelled out. It is the single requirement that the whole security model rests on.

---

### H-1 · HIGH · Threshold reuse across wildly different value at risk

**Attack.** 7-of-35 currently protects oracle *price* data. Compromising 7 keys today lets an attacker distort a price — economically bounded, and the volatility/freeze machinery constrains it further. The same 7-of-35 would attest **the entire UTXO set**: an attacker who reaches the threshold can credit themselves arbitrary coins in the snapshot every bootstrapping node loads.

**Asymmetry.** Same threshold, radically different blast radius. Price manipulation is bounded and self-correcting; a forged UTXO set is unbounded until background validation completes days later.

**Mitigation (design).** Use a **higher threshold for snapshot attestations** than for price. Snapshot attestations are quarterly and non-latency-sensitive, so a higher bar costs almost nothing operationally, while price attestation must run every epoch.

**RESOLVED 2026-08-12: 18-of-35** (audit §9.1c D1). A majority threshold makes two contradictory attestations for the same height **cryptographically impossible without provable double-signing** — two disjoint 18-signer sets would need 36 signatures from 35 keys, so any conflicting pair shares a signer who has demonstrably signed two hashes for one height. Below a majority, an attacker at threshold can mint a competing attestation and the node faces genuine, unattributable ambiguity.

---

### H-2 · HIGH · Attestation must bind network, height, and blockhash

**Attack.** If the signed payload is just a UTXO hash, it can be replayed: a testnet attestation presented on mainnet, an old height's attestation presented as current, or a valid attestation attached to an attacker's fork. The existing `SnapshotMetadata` is only `{m_base_blockhash, m_coins_count}` (`src/node/utxo_snapshot.h:41`) — **no height, no network magic** — so the file format offers no help here.

**Mitigation (design).** The signed payload MUST commit to, at minimum:

```
{ network_magic, height, blockhash, utxo_hash, hash_type, coins_count, file_size }
```

and the node MUST additionally verify that **`blockhash` is the block at `height` in its own header chain**. That last check ties the attestation to the node's independently PoW-verified chain and defeats replay onto a different chain or height.

---

### H-3 · HIGH · Unbounded attacker-controlled metadata drives the parse loop

**Attack.** `PopulateAndValidateSnapshot` reads `m_coins_count` **from the file** and loops on it (`src/validation.cpp:6432-6438`):

```cpp
const uint64_t coins_count = metadata.m_coins_count;
uint64_t coins_left = metadata.m_coins_count;
while (coins_left > 0) { coins_file >> outpoint; coins_file >> coin; ... }
```

There is **no magnitude cap on `m_coins_count` anywhere in the tree** (grep: only the declaration, the constructor, this loop, and an RPC echo). The hash is only checkable *after* the whole file is consumed, so a hostile mirror's bytes are fully parsed **before** anything is verified. Memory is bounded — the loop flushes to disk via `FlushSnapshotToDisk` as the cache grows (`:6491`) — which converts the attack from RAM exhaustion into **disk exhaustion and unbounded wall-clock**, on a path that also holds significant locks.

**Precision, after running the suite (audit §3d).** `feature_assumeutxo.py:87-94` does test a corrupted count — but only `±1`, which lands in the truncation path (`std::ios_base::failure` → *"bad snapshot format or truncated"* → `return false`). That confirms the loop **terminates safely when the file runs out**, which is the real backstop and lowers this below "unbounded loop". What remains untested and uncapped is a **large count backed by matching bulk data**: the node parses everything supplied before checking anything. So the exposure is bounded by *what an attacker is willing to serve*, and the attacker chooses that — which is exactly the assumption that changed when the fetch became automatic.

**Still worth attempting** (a hand-built file with an absurd count) before finalising the severity, but the mitigations below are cheap enough to adopt regardless.

Upstream is not exposed to this, because upstream's file came from the operator.

**Mitigation (design).**
1. Cross-check `metadata` against the **attestation** *before* entering the parse loop: reject immediately if `m_coins_count` or `m_base_blockhash` disagree.
2. Cap the download at the attested `file_size`; abort the transfer past it.
3. Check free disk against attested size before starting.
4. Treat any mismatch as *this mirror is bad* → rotate to the next mirror; never as *this snapshot is bad*.

This is why H-2's `coins_count` and `file_size` fields matter operationally, not just cryptographically.

---

### M-1 · MEDIUM · The design leans on header-chain integrity that is weaker than assumed

**Finding.** `consensus.nMinimumChainWork = uint256S("0x00")` on mainnet (`src/kernel/chainparams.cpp:197`) — **no minimum-chain-work floor at all.** The highest mainnet checkpoint is **23,500,000** (`:251` ff.), with `defaultAssumeValid` at the same height. Tip is ~24.02 M, so roughly **516 K blocks sit above any compiled anchor**, and that unprotected zone is exactly where snapshot attestations would live (DD floor is 23,627,520).

A new node eclipsed during headers sync has checkpoints below 23.5 M but no work floor above it, and DigiShield's per-block retarget lets a low-hashrate attacker ratchet difficulty down after a fork.

**This is a pre-existing DigiByte property, not something the design introduces** — but the design's security argument depends on the header chain, so it inherits the weakness.

**Mitigation — and this one is an upside.** The attestation binds `{height → blockhash}` with a 7-of-35 threshold signature. A node can therefore **reject any header chain lacking that blockhash at that height**. That is a *moving checkpoint*, refreshed quarterly, requiring no release — strictly stronger than the compiled checkpoints for every height above 23.5 M.

**Recommendation:** specify the attestation as a header-chain anchor as well as a snapshot anchor. It turns M-1 from a dependency into a net security improvement, and it is a strong point for the maintainer pitch.

---

### M-2 · MEDIUM · The trust window is where the money is

**Attack.** Between snapshot load and background-validation completion (days), the node operates on **unverified state**. An attacker reaching the H-1 threshold publishes a snapshot crediting themselves, spends those coins to a victim who bootstrapped from it, and receives goods. Validation fails days later; the victim's node halts and reverts — after the goods have shipped.

**Mitigation (design + docs).**
- Expose validation status prominently: a field in `getblockchaininfo` and a persistent GUI indicator — *"snapshot-bootstrapped, background validation N% complete."*
- Consider withholding "verified" balance semantics until completion, or labelling balances as provisional.
- **Documentation MUST state:** services must not credit deposits, and users should not accept irreversible high-value payments, until background validation completes.

Exchanges and merchants are the realistic victims here, and they are exactly the operators most likely to want fast bootstrap.

---

### M-3 · MEDIUM · Validation failure halts the node — a network-wide lever

**Finding.** On hash mismatch, `handle_invalid_snapshot` (`src/validation.cpp:6642-6671`) deletes the snapshot, reverts to the validated chain, and calls `fatalError` — *"deleting snapshot, reverting to validated chain, and stopping node"*.

**Good:** the operator is **not** left with a poisoned chainstate; the honest background chain survives.
**Bad:** an attacker who reached the H-1 threshold could halt **every node that bootstrapped from that snapshot**, simultaneously, at a time of their choosing — a coordinated, network-wide outage of exactly the newest and least-experienced operators.

**Mitigation.** Accept the halt (it is the correct conservative behaviour), but ensure the error text tells the operator plainly that their chain is intact and how to restart on the validated chain without a full resync. Document the recovery procedure **before** it is ever needed.

---

### M-4 · MEDIUM · Restart during bootstrap produces an unstartable node

**Attack / accident.** Per audit §5.2, a snapshot-booted node whose trailing 172,800 blocks are absent hits the fail-closed branch and refuses to start, advising `-reindex` — catastrophic on a 24 M-block chain. Any crash, power loss, or impatient Ctrl-C between snapshot load and trailing-window fetch triggers it. An attacker who can crash a node mid-bootstrap turns this into a persistent denial of service.

**Mitigation.** Adopt resolution (A′) *and* keep (C) as a safety net: the DigiDollar rescan should recognise a snapshot chainstate with an incomplete window and **defer** rather than refuse. Belt and braces, because the failure mode is unrecoverable-looking to an ordinary user.

---

### L-1 · LOW · Privacy leak to mirrors

Fetching the blob reveals to the mirror: a new DigiByte node exists, at this IP, at this time, bootstrapping at this height — correlatable with subsequent on-chain activity.

**Mitigation.** Route the fetch through the node's existing proxy/Tor support; prefer the torrent path where available; **document the leak** rather than letting a privacy researcher discover it.

---

### L-2 · LOW · DNS manifest poisoning and hostile mirrors

Poisoned DNS or a hostile mirror can only cause a failed hash check → denial of service, never a forged UTXO set (audit §9.1). Compounding risk is with H-3: a poisoned manifest is the delivery vehicle for a resource-exhaustion payload, which is why H-3's pre-parse checks matter.

**Mitigation.** DNSSEC where available; randomised mirror order; small compiled-in fallback list; automatic rotation on any failure.

---

### L-3 · LOW · Miner censorship of the attestation

A miner can omit an attestation from the block it mines. Consequence: no attestation at that exact height.

**Mitigation.** The node MUST search **backward** from the deterministic target rather than requiring an exact height — and MUST bound how far back it will accept, and require sufficient PoW above the accepted attestation, so an eclipsing peer cannot walk it arbitrarily far into the past.

---

## 2. Documentation changes to make in advance

These cost nothing now and are expensive to retrofit after someone else words them for you.

| # | Change | Why |
|---|---|---|
| D-1 | Never describe this as "trustless bootstrap." It is **trust-until-verified** | Precision here pre-empts the strongest critique |
| D-2 | State the trust window explicitly, with the "do not credit deposits until validation completes" warning aimed at services | M-2 |
| D-3 | State that **mirrors are untrusted by design** | Prevents users treating mirror provenance as a security question, and pre-empts "why is this on some random host?" |
| D-4 | Document the privacy leak of fetching | L-1 |
| D-5 | Document roster-rotation behaviour: old binaries fall back to full sync, they do not break | audit §9.1b |
| D-6 | Publish the recovery procedure for a failed validation **before** it can happen | M-3 |
| D-7 | State that oracles compute independently — as a property users can verify, not just an internal rule | C-1; it is also the most reassuring fact in the design |

---

## 3. Summary

| ID | Severity | Issue | Fix type |
|---|---|---|---|
| C-1 | Critical | Oracles must derive the hash independently | Design (MUST) |
| H-1 | High | Threshold reuse across unequal blast radius | Design |
| H-2 | High | Attestation must bind network/height/blockhash | Format |
| H-3 | High | Unbounded attacker-controlled `m_coins_count` | Design + format |
| M-1 | Medium | Weak header-chain anchoring above 23.5 M | Design — **and an upside** |
| M-2 | Medium | Exploitable trust window | Design + docs |
| M-3 | Medium | Failure halts nodes network-wide | Docs + error text |
| M-4 | Medium | Restart mid-bootstrap bricks startup | Design |
| L-1 | Low | Privacy leak to mirrors | Docs + proxy |
| L-2 | Low | DNS/mirror poisoning | Design (rotation) |
| L-3 | Low | Miner censorship of attestation | Design (backward search) |

**Three that must be settled before the attestation format freezes:** H-1 (threshold), H-2 (bound fields), H-3 (`coins_count`/`file_size` in the payload). All three change what the attestation carries.

**One that is a gift:** M-1. Specifying the attestation as a moving header-chain anchor makes the network measurably more eclipse-resistant than it is today — an argument *for* the proposal rather than a concession.

**Caveat on this review.** It is a design analysis grounded in code reading, not a penetration test. No attack here has been executed. H-3 in particular deserves an actual attempt — hand-craft a snapshot file with an absurd `m_coins_count` and feed it to `loadtxoutset` on regtest — before anyone relies on the severity rating.
