DigiByte Core version 9.26.4
============================

DigiByte Core v9.26.4 is a patch release on top of v9.26.3. It lets mining pools
and users run **pruned** nodes with DigiDollar, so they no longer have to store the
full ~12-year block history. **It contains no consensus rule changes** — v9.26.2's
Groestl algolock and the DigiDollar BIP9 deployment are carried forward unchanged, and
a pruned v9.26.4 node validates every block identically to a full node. Upgrading is
optional; nodes that do not set `-prune` behave exactly like v9.26.3.

How to Upgrade
==============

Shut down DigiByte Core, replace the binaries, and restart. A reindex is **not**
required to upgrade a normal (full) node.

To run in pruned mode, add `prune=N` to `digibyte.conf` (or `-prune=N` on the command
line), where `N` is your target size in MiB (minimum 550). See "Pruning with
DigiDollar" below.

Notable changes
===============

Pruning with DigiDollar
-----------------------

Previously a DigiDollar node had to run `-txindex` and could not prune: the two are
mutually exclusive, and DigiDollar validation was wired to resolve a spent DigiDollar
output's amount and lock term through the transaction index. v9.26.4 removes that
restriction.

A DigiDollar output can only be *created* at or after DigiDollar activates, and
activation cannot happen below the deployment's minimum activation height. So every
block that DigiDollar validation ever needs to read lives in the window from that
activation height to the chain tip. A pruned v9.26.4 node keeps that whole window and
deletes the older history:

- A **prune lock** is registered at the DigiDollar activation floor, so automatic
  pruning and the `pruneblockchain` RPC never delete a block a DigiDollar spend might
  need.
- `-txindex` is **no longer required** under `-prune`. DigiDollar amount/lock lookups
  read the creating transaction directly from the retained block at the coin's height
  instead of from the index.
- If a pruned data directory is ever missing a DigiDollar-era block (for example, it
  was pruned under different rules), the node **refuses to start** and asks you to
  restart with `-reindex`, rather than validating DigiDollar with incomplete data.

Result: a pruned node validates, mines, and lets you mint/send/redeem DigiDollar
exactly like a full node, on a fraction of the disk.

Example `digibyte.conf` for a pruned pool or user node:

    prune=2000
    # no txindex needed

Notes and limitations for pruned nodes
--------------------------------------

- **Disk.** A pruned node keeps only the DigiDollar-era window plus the chainstate,
  rather than the full chain plus a transaction index. Note that the DigiDollar-era
  window grows with the chain: after activation, every block from just below the
  activation height to the tip is retained permanently, so disk usage grows over time
  and can exceed the `prune=N` target. What pruning saves is the ~12 years of
  pre-DigiDollar history (and the transaction index) — a one-time saving, not a fixed
  size cap.
- **Serving history.** A pruned node advertises `NODE_NETWORK_LIMITED` and serves only
  the most recent blocks to peers; it does not help new nodes with their initial sync.
  The network's full (archival) nodes continue to do that.
- **`getrawtransaction`** for transactions in deleted (pre-DigiDollar-era) blocks is
  unavailable without a transaction index, as on any pruned node.
- **`getdigidollarstats`** still reports current supply/collateral/health/position
  counts on a pruned node via a live UTXO scan; the optional DigiDollar stats index
  (used only for historical per-height queries) is left off under `-prune`. The scan
  walks the whole UTXO set on each call, so avoid polling it at high frequency on a
  pruned node.
- **Wallets.** A wallet whose birthday predates the pruned history requires `-reindex`
  to rescan (standard pruned-node behavior). Wallets created on or after DigiDollar
  activation are entirely within the retained window and rescan normally.
- Setting `-prune` together with an explicit `-txindex=1` is still rejected, as before.

Credits
=======

Thanks to everyone testing pruned-mode operation ahead of DigiDollar activation.
