# DigiSync — Oracle-Attested Fast Sync for DigiByte

*Updated: 2026-08-12*
*Targets DigiByte Core v9.26.x — proposal stage, no code changed yet*
*Working name; rename freely.*

## Overview

DigiSync lets a new DigiByte node become usable in about an hour instead of several days, by starting from a signed snapshot of the current coin set and verifying the full 24-million-block history in the background afterwards. The signature comes from DigiByte's existing oracle network — the same 35 operators who already secure DigiDollar — so the trust anchor lives **on the chain itself**, not in a hash hardcoded at release time.

### Key Points

- **Nothing needs to be backported from Bitcoin.** The entire snapshot mechanism already exists in DigiByte Core, verified line-by-line against Bitcoin Core v26.2 and confirmed working on live v9.26.4 nodes.
- **We compile in the verifier, not the answer.** Bitcoin's version hardcodes one specific hash for one specific height, so every new snapshot needs a new software release. DigiSync hardcodes the 35 oracle keys instead — **one release supports every future snapshot, forever.**
- **No new trust is introduced.** Any node running DigiDollar already trusts these oracles for price data that affects consensus. Vouching for a coin-set hash is a *smaller* ask — and unlike price data, it is temporary and self-checking.
- **It makes the network harder to attack, for everyone.** The same attestation doubles as a checkpoint that refreshes automatically, closing a real gap that exists today (see *A Free Security Upgrade*).
- **The download is about 1 GB.** DigiByte's coin set is roughly 11× smaller than Bitcoin's — 15.86 million coins, measured on mainnet. Plain HTTP is enough; no torrent infrastructure required.
- **Status:** capability audit complete and evidence-backed. One integration blocker identified, reproduced, and covered by a failing regression test. Attestation format spec is the next piece of work.

---

## The Problem

A new DigiByte node must download and verify roughly 24 million blocks — about 36 GB — before its wallet is trustworthy. That takes days. For someone who just wants to receive a payment, days is the difference between using DigiByte and not using DigiByte.

Bitcoin solved this shape of problem with a feature called AssumeUTXO: ship a snapshot of the current coin set, let the node start from it, verify the history afterwards. DigiByte inherited the machinery in its v26.2 rebase — **all of it, working** — but never switched it on, because Bitcoin's design requires the snapshot's fingerprint to be **hardcoded into the software at release time**.

That hardcoded fingerprint is the whole problem. It is stale the moment it ships. It only covers heights the release knew about. Every new snapshot means a new release, a new download, a new coordination exercise. DigiByte's mainnet table is simply empty as a result.

---

## The Idea: Ask the Oracles

DigiByte already runs something Bitcoin does not: **a live network of 35 independent oracle operators** who publish threshold-signed data into blocks every epoch, securing DigiDollar.

DigiSync uses that same network for a second purpose. Four times a year, at a predictable block height, each oracle independently computes the fingerprint of the coin set and signs it. Once **18 of the 35** agree, that signature is published on-chain like any other oracle data.

A new node then does something no Bitcoin node can:

1. Downloads block headers and finds the attestation **in the chain itself**
2. Verifies the 18 signatures against oracle keys **that shipped inside the software**
3. Downloads the ~1 GB snapshot from anywhere at all
4. Checks the snapshot against the fingerprint the chain just gave it
5. Starts working — usually inside an hour
6. Verifies all 24 million blocks in the background, and re-checks the oracles' homework

No hardcoded fingerprints. No new release for each snapshot. No trusted download server.

### The Bookkeeping Analogy

Imagine joining a bookkeeping cooperative that has been running for twelve years and has twenty-four million pages of receipts in the basement.

**The old way:** read every page before you are allowed to trade. It is completely trustworthy, and it takes days.

**DigiSync:** accept a one-page summary of everyone's current balances — signed by eighteen of the thirty-five independent auditors, each of whom worked it out separately from their own copy of the books. Start trading today. Meanwhile, in the background, you read all twenty-four million pages yourself and check whether that summary was honest.

If it was, you have lost nothing and saved days. If it was not, your node discovers this, throws the summary away, keeps the version it verified itself, and tells you loudly.

**You are not trusting the auditors permanently. You are borrowing their word until you can check it.**

---

## Why This Is Safe

**The download server is never trusted.** The fingerprint comes from the chain; the file comes from a mirror. A hostile mirror can only cause a failed check — it can never cause a bad coin set to be accepted. Anyone can mirror without asking permission, and mirrors need no vetting.

**The trust is temporary and self-checking.** Background validation independently recomputes the coin set from genesis and compares. Disagreement is treated as a serious failure: the node discards the snapshot, reverts to the chain it verified itself, and stops for the operator's attention.

**18 of 35 is a deliberate majority.** DigiDollar's price data uses 7 of 35, but the stakes differ — a distorted price is bounded and self-correcting, while a forged coin set is not. A majority buys a structural guarantee: two contradictory attestations for the same height **cannot both exist** without at least one oracle provably signing two different answers — which identifies exactly whose key to remove.

**Oracles must each do the work themselves.** Every participating oracle computes the fingerprint from its own copy of the chain and signs only what it derived. None of them accepts a number from a coordinator. This is the requirement the whole design rests on.

**Users choose.** Conventional full sync remains the default and is never removed. DigiSync is offered, explained, and declined as easily as accepted.

---

## A Free Security Upgrade

This is the part worth dwelling on, because it is not a trade-off.

Today, a DigiByte node syncing from scratch has checkpoints only up to block 23,500,000, and no minimum-work requirement above that. Roughly half a million recent blocks have no built-in anchor — which is where an attacker feeding a fake chain to a brand-new node would aim.

Because each attestation ties a block height to a specific block, signed by a majority of oracles, **it works as a checkpoint that refreshes itself four times a year without any software release.** For every height above 23.5 million, that is stronger than what ships today.

Enforcement is deliberately limited to nodes still performing initial sync. Once your node has verified the chain itself, attestations become advisory — a mismatch raises an alarm, but nothing can reorganise or halt a node that has done its own homework. Oracles can guide newcomers; they cannot reach back and constrain an established node.

**DigiSync is not asking for a security concession in exchange for speed. It closes a gap that is open right now.**

---

## What It Costs

| | Conventional sync | DigiSync |
|---|---|---|
| Usable wallet | days | **~1 hour** |
| First download | ~36 GB | **~3 GB** |
| Fully verified | days | days *(in background)* |
| Trust required | none | 18-of-35, temporary |

The ~3 GB breaks down as about 1.83 GB of block headers, 0.95 GB of snapshot, and a little over 0.2 GB of recent blocks. Notably the *headers* are the larger part — the snapshot itself is smaller than the header chain it is checked against.

Disk usage after background validation is identical to a conventional node. DigiSync changes **when** the work happens, not how much of it there is.

---

## Technical Implementation

The audit found the entire mechanism present and upstream-identical: snapshot creation and loading, dual-chainstate background validation, the MuHash coin-set hash, and the merkle-proof RPCs. The functional test suite passes on current `develop`.

New work is confined to:

- **The attestation format** — what the oracles sign, and how it is published on-chain
- **Oracle publication** — a quarterly job at a predictable height
- **A bootstrap sequence in the node** — find the attestation, verify it, fetch, check, load
- **A small anchor resolver** — four call sites currently read the hardcoded table; they would consult the chain instead

One genuine blocker was found and reproduced. DigiDollar rebuilds its price and volatility state at every startup by reading the previous thirty days of blocks, and refuses to start if any are missing — which is exactly the situation a freshly bootstrapped node is in. A failing regression test now covers it, and the fix is to fetch that thirty-day window (about 60 MB) during bootstrap. This is a DigiDollar startup-ordering problem, not a flaw in the snapshot mechanism.

---

## Status and Next Steps

**Done:** capability audit against `develop`, verified live on mainnet 9.26.4; adversarial security review; core design decisions settled; the integration blocker reproduced with a regression test.

**Next:** the attestation format specification, then implementation.

**Open for discussion:** the working name; the exact snapshot cadence (~500,000 blocks is the current assumption, roughly quarterly); and whether the node should fetch the snapshot itself or leave that to the user.

Supporting documents:

- [**Capability audit**](doc/design/2026-08-11-assumeutxo-capability-audit.md) — the evidence base. What exists in this tree, verified against Bitcoin Core v26.2 and against live mainnet nodes, with file and line citations throughout. Also contains the design decisions and the two claims I got wrong and retracted.
- [**Adversarial review**](doc/design/2026-08-12-attested-snapshot-threat-model.md) — eleven findings on how this could be attacked, with the mitigations each one implies.
- [**Regression test**](test/functional/feature_digidollar_snapshot_startup.py) — reproduces the startup blocker described above. It **fails on current `develop` by design**, asserting the behaviour we want rather than the behaviour we have.

Feedback on the trust model and the oracle workload is especially welcome — those are the two places where this design most depends on being wrong in a way we have not yet spotted.
