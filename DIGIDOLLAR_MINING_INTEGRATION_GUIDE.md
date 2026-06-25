# DigiDollar Mining Integration Guide

This guide explains how CPU miners, solo miners, Stratum pools, and DigiHashV2
should mine DigiDollar-aware DigiByte blocks.

The short version:

- Old miners can keep mining normal DigiByte blocks safely.
- DigiDollar-aware miners and pools must explicitly request the
  `digidollar-oracle` getblocktemplate rule.
- When Core returns `default_oracle_commitment`, the miner or pool must copy it
  into the coinbase as a zero-value output.
- Pools do not need oracle private keys. They only need an upgraded DigiByte
  node that can provide a fresh oracle-aware block template.

Validated locally against:

- DigiByte Core GBT opt-in path: `src/rpc/mining.cpp`, `src/node/miner.cpp`
- DigiByte functional tests: `digidollar_gbt_optin.py`,
  `digidollar_oracle_gbt_stale_cache.py`
- cpuminer-multi DigiDollar mode: `cpu-miner.c`,
  `tests/test_gbt_regressions.py`
- node-stratum-pool DigiDollar mode: `lib/pool.js`,
  `lib/transactions.js`, `lib/test.js`
- DigiHashV2 coin config opt-in:
  `coins/digibyte.{sha256,scrypt,skein,qubit,odo}.json`

## 1. What Changed

DigiDollar mint and redeem transactions require a fresh oracle price. The
oracle price is committed into the block coinbase through an `OP_ORACLE`
commitment output.

That means a block producer must do two things:

1. Ask DigiByte Core for a DigiDollar-aware block template.
2. Preserve the oracle commitment in the coinbase when Core provides it.

Core now separates mining into two safe paths.

### Legacy GBT Path

Request:

```json
{"rules":["segwit"]}
```

Result:

- Mines normal DigiByte blocks.
- Does not receive `default_oracle_commitment`.
- Does not receive oracle-priced DigiDollar mint/redeem work.
- Remains valid and safe for non-upgraded miners.

### DigiDollar-Aware GBT Path

Request:

```json
{"rules":["segwit","digidollar-oracle"]}
```

Result:

- Mines normal DigiByte blocks when no oracle bundle is ready.
- When a fresh MuSig2 oracle bundle is ready, receives
  `default_oracle_commitment`.
- Can include DigiDollar mint/redeem transactions that depend on that oracle
  price.

Core advertises this in the returned `rules` array as:

```json
"!digidollar-oracle"
```

The `!` means the miner must understand and preserve that rule.

## 2. Direct Core GBT Calls

For a direct DigiByte Core check, pass `digidollar-oracle` in `rules`.

Example for scrypt:

```bash
digibyte-cli getblocktemplate \
  '{"rules":["segwit","digidollar-oracle"],"capabilities":["coinbasetxn","coinbasevalue","coinbase/append","workid"]}' \
  scrypt
```

Examples for the five DigiByte algos:

```bash
digibyte-cli getblocktemplate '{"rules":["segwit","digidollar-oracle"]}' scrypt
digibyte-cli getblocktemplate '{"rules":["segwit","digidollar-oracle"]}' sha256d
digibyte-cli getblocktemplate '{"rules":["segwit","digidollar-oracle"]}' skein
digibyte-cli getblocktemplate '{"rules":["segwit","digidollar-oracle"]}' qubit
digibyte-cli getblocktemplate '{"rules":["segwit","digidollar-oracle"]}' odo
```

When oracle data is ready, the response should include:

```json
"default_oracle_commitment": "<hex script>"
```

That hex is the exact scriptPubKey for a zero-value coinbase output. Do not
decode and rebuild it unless your code already has a safe script parser. The
simple and correct behavior is to copy it exactly.

If `default_oracle_commitment` is absent:

- Do not invent an oracle output.
- Do not force mint/redeem transactions into the block.
- Mine the returned template as a normal block, or wait for the next template.

Core is responsible for excluding oracle-priced DigiDollar transactions when a
fresh bundle is not ready.

## 3. Coinbase Requirements

A DigiDollar-aware miner or pool that builds its own coinbase must preserve
these template commitments:

- `default_witness_commitment`, when present
- `default_oracle_commitment`, when present

Each commitment must be included as a zero-value coinbase output.

Pseudo-code:

```text
outputs = []

outputs.append(pool_reward_output)

if template.default_witness_commitment exists:
    outputs.append(value=0, scriptPubKey=hex_to_bytes(template.default_witness_commitment))

if template.default_oracle_commitment exists:
    outputs.append(value=0, scriptPubKey=hex_to_bytes(template.default_oracle_commitment))
```

Output order is less important than preserving the exact scripts, but do not
drop either commitment.

The pool must also build the merkle root correctly:

- Use each template transaction's `txid` for the merkle leaf when `txid` is
  provided.
- Use raw transaction `data` in the final serialized block.
- Do not hash witness-inclusive raw transaction data as the merkle leaf when a
  `txid` is available.

This matters for modern templates because raw `data` may include witness bytes,
but the block merkle root commits to txids.

## 4. CPU Mining With cpuminer

The patched cpuminer uses an explicit opt-in flag:

```bash
--digidollar
```

With that flag, cpuminer requests:

```json
["segwit","digidollar-oracle"]
```

It also preserves `default_oracle_commitment` in its miner-built coinbase.

Example:

```bash
./cpuminer \
  -a scrypt \
  -o http://127.0.0.1:14046/ \
  -O rpcuser:rpcpassword \
  --coinbase-addr=<SCRYPT_PAYOUT_ADDRESS> \
  --no-stratum \
  --no-getwork \
  --no-longpoll \
  --digidollar \
  --threads=6
```

Run one process per supported algo:

```bash
./cpuminer -a scrypt  -o http://127.0.0.1:14046/ -O rpcuser:rpcpassword --coinbase-addr=<SCRYPT_ADDRESS>  --no-stratum --no-getwork --no-longpoll --digidollar --threads=6
./cpuminer -a sha256d -o http://127.0.0.1:14047/ -O rpcuser:rpcpassword --coinbase-addr=<SHA256_ADDRESS>  --no-stratum --no-getwork --no-longpoll --digidollar --threads=6
./cpuminer -a skein   -o http://127.0.0.1:14048/ -O rpcuser:rpcpassword --coinbase-addr=<SKEIN_ADDRESS>   --no-stratum --no-getwork --no-longpoll --digidollar --threads=6
./cpuminer -a qubit   -o http://127.0.0.1:14049/ -O rpcuser:rpcpassword --coinbase-addr=<QUBIT_ADDRESS>   --no-stratum --no-getwork --no-longpoll --digidollar --threads=6
```

Current cpuminer support:

- Supported here: `scrypt`, `sha256d`, `skein`, `qubit`
- Not supported here: DigiByte `odo` / Odocrypt

For Odo CPU/GPU/pool mining, use an Odo-capable miner or pool stack that follows
the same GBT and coinbase rules in this guide.

## 5. Stratum Pool Requirements

A Stratum pool has two sides:

- Upstream side: pool asks DigiByte Core for work through GBT.
- Downstream side: miners connect to the pool using Stratum.

Most ASIC/GPU miners do not need to know about DigiDollar. The pool server must
know about DigiDollar because the pool server builds the coinbase and the block
template.

Required upstream GBT behavior:

```json
{
  "capabilities": ["coinbasetxn", "workid", "coinbase/append"],
  "rules": ["segwit", "digidollar-oracle"]
}
```

For DigiByte Core, the algo is passed as the second RPC argument:

```text
getblocktemplate(template_request, "scrypt")
getblocktemplate(template_request, "sha256d")
getblocktemplate(template_request, "skein")
getblocktemplate(template_request, "qubit")
getblocktemplate(template_request, "odo")
```

Another valid deployment pattern is one daemon per algo with `algo=<algo>` in
each daemon's `digibyte.conf`. In that setup the pool can call GBT without the
second algo argument because each daemon already defaults to the correct algo.

Required coinbase behavior:

- Add pool payout output.
- Preserve witness commitment if present.
- Preserve oracle commitment if present.
- Add normal pool recipients/fees as before.
- Submit the final serialized block with `submitblock`.

Do not require every downstream miner to upgrade. The pool controls the coinbase
and GBT request; downstream ASIC/GPU miners only need valid Stratum jobs for
their algo.

## 6. node-stratum-pool Pattern

The patched node-stratum-pool implementation uses a strict boolean coin config:

```json
"digidollar": true
```

When that is present, `lib/pool.js` requests:

```json
["segwit","digidollar-oracle"]
```

When it is absent or not a boolean `true`, the pool keeps legacy behavior:

```json
["segwit"]
```

That strict opt-in prevents unrelated coins from accidentally requesting
DigiDollar rules.

The patched `lib/transactions.js` copies the oracle commitment into the
coinbase:

```javascript
if (rpcData.default_oracle_commitment !== undefined) {
    var oracleCommitment = Buffer.from(rpcData.default_oracle_commitment, 'hex');
    txOutputBuffers.unshift(Buffer.concat([
        util.packInt64LE(0),
        util.varIntBuffer(oracleCommitment.length),
        oracleCommitment
    ]));
}
```

The test coverage verifies:

- `digidollar: true` requests `["segwit","digidollar-oracle"]`
- `digidollar: false`, missing, or string `"true"` stays legacy
- `default_oracle_commitment` is included exactly once
- the oracle commitment output has zero DGB value
- no oracle commitment is invented when GBT did not provide one

## 7. DigiHashV2 Example

DigiHashV2 uses one coin JSON per DigiByte mainnet algo.

Each mainnet DigiByte coin config must contain:

```json
{
  "symbol": "DGB",
  "digidollar": true,
  "algorithm": "scrypt"
}
```

The local DigiHashV2 example enables this for all five mainnet algo configs:

- `coins/digibyte.sha256.json`
- `coins/digibyte.scrypt.json`
- `coins/digibyte.skein.json`
- `coins/digibyte.qubit.json`
- `coins/digibyte.odo.json`

Example `coins/digibyte.scrypt.json` pattern:

```json
{
  "name": "DigiByte-Scrypt",
  "symbol": "DGB",
  "digidollar": true,
  "algorithm": "scrypt",
  "peerMagic": "fac3b6da",
  "coinbase": "DigiHashV2"
}
```

The pool config then points to that coin file:

```json
{
  "enabled": true,
  "coin": "digibyte.scrypt.json",
  "address": "<POOL_PAYOUT_ADDRESS>",
  "daemons": [
    {
      "host": "127.0.0.1",
      "port": 14022,
      "user": "user",
      "password": "password"
    }
  ]
}
```

Important release requirement:

DigiHashV2 must run with a patched `node-stratum-pool` that contains the
DigiDollar GBT and coinbase support. In the local validation, that is
`node-stratum-pool` commit:

```text
433d38a Add DigiDollar oracle mining template support
```

Do not ship a DigiHashV2 release whose `package-lock.json` pins an older
`node-stratum-pool` commit. A correct install must resolve to a stratum-pool
version that:

- requests `digidollar-oracle` when `coin.digidollar === true`
- preserves `default_oracle_commitment`
- keeps legacy behavior for coins without `digidollar: true`

## 8. Pool Integration Checklist

Use this checklist for any pool implementation.

### GBT Request

- Include `segwit` in `rules`.
- Include `digidollar-oracle` in `rules` only for DigiDollar-aware DigiByte
  mining.
- Continue using legacy `["segwit"]` for coins or miners that are not
  DigiDollar-aware.
- Pass the correct DigiByte algo, either through one daemon per algo or through
  Core's second GBT argument.

### Template Handling

- Read `transactions` from the template.
- Use template `txid` values for merkle leaves when present.
- Serialize raw transaction `data` into the block.
- Keep longpoll GBT requests using the same `rules` as normal GBT requests.
- Do not mix cached legacy templates with DigiDollar-aware templates.

### Coinbase Handling

- Build a normal pool coinbase.
- Preserve `default_witness_commitment` if present.
- Preserve `default_oracle_commitment` if present.
- Put the oracle commitment in a zero-value output.
- Do not invent an oracle commitment.
- Do not drop the oracle commitment from a template that contains mint/redeem
  work.

### Submission Handling

- Submit full blocks with `submitblock`.
- If a proposal mode is used, propose the full block that includes the oracle
  commitment.
- Treat missing oracle commitment as a normal no-oracle template, not as a
  reason to fabricate data.

### Operational Checks

Before enabling production pool work, verify:

```bash
digibyte-cli getblocktemplate '{"rules":["segwit"]}' scrypt
```

Expected:

- no `default_oracle_commitment`
- no oracle-priced mint/redeem work

Then verify:

```bash
digibyte-cli getblocktemplate '{"rules":["segwit","digidollar-oracle"]}' scrypt
```

Expected when an oracle bundle is ready:

- `default_oracle_commitment` exists
- `rules` includes `!digidollar-oracle`
- mint/redeem transactions may appear when pending

If no oracle bundle is ready:

- `default_oracle_commitment` may be absent
- mint/redeem transactions that need a fresh price should not appear
- normal DigiByte mining can continue

## 9. TDD Requirements

Minimum tests for a correct integration:

1. Legacy GBT test
   - Request `["segwit"]`.
   - Assert no `default_oracle_commitment`.
   - Assert oracle-priced mint/redeem txs are not included.

2. DigiDollar-aware GBT test
   - Request `["segwit","digidollar-oracle"]`.
   - With a fresh oracle bundle, assert `default_oracle_commitment` exists.
   - With a pending mint/redeem, assert the tx appears.

3. Stale oracle test
   - Let the oracle bundle age out.
   - Assert stale templates do not keep old oracle commitments.
   - Assert price-dependent mint/redeem txs are removed until a fresh bundle is
     ready.

4. Coinbase preservation test
   - Build a coinbase from a template containing `default_oracle_commitment`.
   - Assert exactly one zero-value output contains the exact script.

5. No-invention test
   - Build a coinbase from a template without `default_oracle_commitment`.
   - Assert no oracle output is created.

6. Multi-algo block acceptance test
   - Mine and submit blocks for `scrypt`, `sha256d`, `skein`, `qubit`, and
     `odo` with the pool/miner implementation that supports each algo.
   - Verify blocks with pending mint/redeem txs are accepted when the oracle
     bundle is ready.

7. Non-DigiDollar coin safety test
   - Verify unrelated coin configs do not request `digidollar-oracle`.

## 10. Common Failure Modes

### Miner Does Not Ask For `digidollar-oracle`

Symptom:

- Normal blocks mine.
- Mints/redeems sit unconfirmed.
- GBT response has no `default_oracle_commitment`.

Fix:

- Add `digidollar-oracle` to GBT `rules`.
- For cpuminer, use `--digidollar`.
- For pools, enable strict DigiDollar coin config opt-in.

### Pool Drops `default_oracle_commitment`

Symptom:

- Pool asks for DigiDollar-aware templates.
- Template has `default_oracle_commitment`.
- Submitted block has no `OP_ORACLE` coinbase output.
- Mint/redeem work cannot confirm correctly.

Fix:

- Copy `default_oracle_commitment` into the coinbase as a zero-value output.

### Pool Uses Raw Witness Transaction Data For Merkle Leaves

Symptom:

- Block submission fails even though the template looked valid.

Fix:

- Use template `txid` for merkle leaves when provided.
- Serialize raw `data` into the final block only.

### Pool Runs An Old Dependency

Symptom:

- DigiHashV2 coin configs say `digidollar: true`.
- Pool still requests only `["segwit"]`.
- No oracle commitment appears in the pool's submitted blocks.

Fix:

- Update/pin the patched stratum-pool dependency.
- Reinstall dependencies.
- Re-run the pool's DigiDollar GBT and coinbase tests.

## 11. Plain-English Summary

DigiDollar mining does not require every miner on the network to be an oracle.
It also does not require every old miner to upgrade immediately.

The rule is simple:

- Old miners mine normal DigiByte blocks.
- DigiDollar-aware miners and pools ask Core for DigiDollar-aware templates.
- Core provides an oracle commitment only when it has fresh oracle data.
- The miner or pool copies that commitment into the coinbase.
- Then mints and redeems can confirm in that block.

That is the safe split: no fork risk for old miners, and correct DigiDollar
support for upgraded miners and pools.
