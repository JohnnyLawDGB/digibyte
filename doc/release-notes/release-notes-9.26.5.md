DigiByte Core version 9.26.5
============================

DigiByte Core v9.26.5 is a patch release on top of v9.26.4. It ships two changes:

1. **A startup fix:** the DigiDollar oracle startup scan no longer hangs the node
   for ~15 minutes on mainnet — it now completes in a few seconds.
2. **Deployment burial (BIP90):** the Taproot, DigiDollar, and AlgoLock soft
   forks — all three now ACTIVE on mainnet — are converted from live BIP9
   version-bits deployments into **buried deployments** with hardcoded
   activation heights, the same housekeeping Bitcoin Core performed for CSV and
   SegWit after their activations.

Neither change alters which blocks or transactions are valid on the current
mainnet or testnet chains. The burial does change several RPC output shapes and
regtest options — see "Breaking interface changes" below before upgrading
anything that parses deployment status.


How to Upgrade
==============

Shut down DigiByte Core, replace the binaries, and restart. A reindex is **not**
required, and no configuration changes are needed for normal (full or pruned)
nodes. Wallets, mining, pruning, and oracle operation continue to work as in
v9.26.4.


Notable changes
===============

DigiDollar oracle startup hang fixed (~15 minutes → ~3 seconds)
---------------------------------------------------------------

On startup the node rebuilds its in-memory oracle price and volatility cache by
scanning the last 172,800 blocks (~30 days). In v9.26.4, every scanned block
re-evaluated the DigiDollar activation gate through an overload of
`IsDigiDollarEnabled` that allocated a *throwaway* versionbits cache and re-ran
the BIP9 threshold state machine from scratch — an O(window) walk per block,
~172,800 times, with nothing memoized. On mainnet this took ~15 minutes, held
`cs_main` before RPC warmup finished (so RPC returned "Verifying blocks..."
/ error -28 and P2P stalled the whole time), and spammed 150k+ per-block log
lines.

The fix (commit `3ec927b8b4`) routes the startup per-block gate through the
node's shared, memoized versionbits cache — the exact same lookup `ConnectBlock`
uses — making it O(1) amortized, and replaces the per-block skip logging with a
single aggregate count. This is a pure performance change with no consensus
impact: unit and functional tests prove the startup gate computes the identical
activation boolean at every height, and a restart equals a `-reindex` equals the
pre-restart oracle state. Verified live on mainnet: the oracle startup scan
dropped from ~15 minutes to ~3 seconds.

With the deployment burial below, the DigiDollar activation check becomes a
plain height comparison, which removes this entire class of cost permanently.


Taproot, DigiDollar, and AlgoLock are now buried deployments (BIP90)
--------------------------------------------------------------------

All three BIP9 deployments have completed their lifecycle and are ACTIVE on
mainnet:

| Deployment | BIP9 bit | Mainnet activation height | Testnet26 | Signet / Regtest |
|------------|----------|---------------------------|-----------|------------------|
| Taproot (BIPs 340-342) | 2 | 21,168,000 | 0 (always active) | 0 |
| DigiDollar | 23 | 23,869,440 | 600 | 0 |
| AlgoLock (retired-algo rejection) | 0 | 23,869,440 | 0 (always active) | 0 |

v9.26.5 "buries" them (BIP90): the activation heights above are now hardcoded
per network in chainparams (`TaprootHeight` / `DigiDollarHeight` /
`AlgoLockHeight`, returned by `Consensus::Params::DeploymentHeight()`), and the
node no longer runs the BIP9 state machine for them. The heights are the
empirically verified BIP9 `since` heights — taken from live mainnet
`getdeploymentinfo` and verified block-by-block on testnet26 (DEFINED 0-199,
STARTED 200-399, LOCKED_IN 400-599, ACTIVE at 600).

**Why bury them:**

- The BIP9 state machine no longer serves any purpose for these deployments —
  they can never go back to signaling, time out, or fail. Burying removes
  per-block BIP9 state recomputation from `ConnectBlock` and startup, and turns
  every DigiDollar activation check into a single height comparison.
- Blocks stop signaling bits 2/23/0, so `getblocktemplate` version fields are
  clean for pool software (this also structurally resolves the bit-23
  version-rolling interference some SHA256D pool stacks reported).
- It matches upstream practice: Bitcoin Core buried CSV and SegWit the same way
  after their activations.

**What deliberately did NOT change:**

- The static DigiDollar gates keep their historical floors:
  `nDDActivationHeight = nOracleActivationHeight = nDigiDollarMuSig2Height =
  23,627,520` on mainnet (600 on testnet26; 650/650/0 on default regtest). The
  23,627,520 floor is intentionally *below* the 23,869,440 burial height; it
  continues to drive the pruning floor, the prune lock, and the pre-floor
  collateral gate exactly as in v9.26.4, so existing pruned nodes are
  unaffected.
- The AlgoLock static backstop (`nGroestlDeactivationHeight = 23,808,000`)
  remains and is still OR'd with the deployment check — retired-algorithm
  blocks are rejected over exactly the same height ranges as before.
- Taproot script validation flags were already applied unconditionally (with
  the standard exceptions list), so the Taproot burial has zero
  script-consensus effect.

**Consensus compatibility:** a v9.26.5 node accepts and rejects exactly the
same blocks as a v9.26.4 node for every block on the current mainnet and
testnet chains: below the burial heights those chains contain no
deployment-gated content that the BIP9 rules judged differently, and at/above
the burial heights the deployments are active under both versions. The standard
BIP90 caveat applies, as it did for Bitcoin's burials: in the hypothetical of
an (astronomically expensive) deep reorganization back below the burial
heights, a re-mined chain with different signaling could in principle be judged
differently by pre-burial and post-burial nodes. This affects no real chain and
does not change the security model in practice.

**Versionbits warning ranges:** `MinBIP9WarningHeight` is raised to 23,909,760
on mainnet (DigiDollar/AlgoLock activation height + one 40,320-block
confirmation window) and 800 on testnet26, so the historical bit-2/23/0
signaling periods do not trigger spurious "unknown new rules activated"
warnings now that upgraded nodes no longer set those bits.


Breaking interface changes
==========================

RPC
---

- **`getdigidollardeploymentinfo`** has a new shape. It now returns `enabled`,
  `type` (always `"buried"`), `status` (`"active"` or `"defined"`),
  `activation_height` (the burial height; omitted only if the deployment is
  disabled on the network), plus the unchanged oracle/MuSig2 fields
  (`oracle_activation_height`, `musig2_format_activation_height`,
  `oracle_pubkey_count`, `oracle_consensus_required`, `oracle_total_slots`,
  `oracle_seed_peers`, `musig2_session{...}`). The BIP9 fields are **removed**:
  `bit`, `start_time`, `timeout`, `min_activation_height`,
  `blocks_until_timeout`, `signaling_blocks`, `threshold`, `period_blocks`,
  `progress_percent`. `status` can no longer be `started`, `locked_in`, or
  `failed`. Note `activation_height` now reports the exact burial height
  (previously the back-scan reported the first active block of the tip's chain,
  which could differ by one).
- **`getdeploymentinfo`** (and REST `/deploymentinfo`): the `taproot`,
  `digidollar`, and `algolock` entries are now rendered as buried
  deployments — `{"type": "buried", "active": <bool>, "height": <N>}` — with
  **no `bip9` sub-object**. Anything reading
  `deployments.digidollar.bip9.status` must switch to `active`/`height`.
  (`getblockchaininfo` carries no deployment information in this codebase.)
- **`getblocktemplate`**: `rules` now always contains `"taproot"`,
  `"digidollar"`, and `"algolock"` once active (hardcoded, like `"csv"`; no
  `!` prefix, so clients that do not understand them may safely proceed).
  `vbavailable` no longer mentions them, and the template `version` never sets
  bits 2, 23, or 0. Block versions on the network stop signaling those bits.

Command-line options (regtest)
------------------------------

- **`-vbparams=taproot:...`, `-vbparams=digidollar:...`, and
  `-vbparams=algolock:...` are now a startup error** ("Invalid deployment").
  Only `testdummy` remains a versionbits deployment. Regtest scripts using
  these must migrate to `-testactivationheight` (below).
- **`-digidollaractivationheight=N` now activates DigiDollar at exactly height
  N.** It sets the buried `DigiDollarHeight` together with the static
  DD/oracle/MuSig2 gates. Pre-burial, this knob ran real BIP9 signaling and
  DigiDollar only became active at the first 144-block window boundary >=
  max(432, N) — e.g. N=650 used to activate at 720. Tests and scripts that
  relied on the old window-boundary timing must be updated.
- **New: `-testactivationheight=taproot@H` / `digidollar@H` / `algolock@H`**
  set only the buried deployment height. For `digidollar@H` the static
  DD/oracle gates keep their regtest defaults (650), while
  `nDigiDollarMuSig2Height` is derived as
  `min(nDDActivationHeight, DigiDollarHeight)` and so follows H below 650.
  `-digidollaractivationheight` takes precedence when both are given.


Credits
=======

Thanks to the node operators and pool engineers who reported the slow mainnet
startup and the versionbit signaling quirks, and to everyone who verified the
activation heights on the live mainnet and testnet26 chains ahead of the
burial.
