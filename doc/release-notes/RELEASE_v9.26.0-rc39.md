# DigiByte Core v9.26.0-rc39 Release Notes

**WARNING: RC39 is testnet-only until Jared reviews, tags, releases, and deploys it.**

RC39 is a DigiDollar wallet and RPC parity release on top of RC38. It keeps
`testnet24`, keeps the 9-of-17 V1 oracle quorum, and keeps the on-chain v0x03
oracle bundle format.

The headline:

> RC39 makes DigiDollar behave more like a first-class wallet asset: list
> unspent outputs, selected-input sends, Qt Send coin control, safer mint
> confirmations, consistent DD amount display, and editable persistent receive
> requests.

Development branch: `feature/digidollar-v1`

Developer chat: https://app.gitter.im/#/room/#digidollar:gitter.im

---

## Read This First

RC39 does not reset the public DigiDollar testnet.

It stays on `testnet24`. It does not change activation heights, oracle quorum,
epoch length, DigiDollar economics, wallet database format, oracle roster, or
the v0x03 bundle committed in blocks.

What changed from RC38:

- DigiDollar Qt mint limits now come from active chain parameters instead of
  stale hardcoded UI limits.
- DigiDollar Qt mint confirmation refreshes oracle price, required collateral,
  and available DGB immediately before broadcast.
- DigiDollar Qt amount display now uses consistent two-decimal DD formatting
  across overview, send, receive, and redeem screens.
- DigiDollar redeem metadata and locked-key bookkeeping are preserved more
  reliably through wallet state changes and reloads.
- `sendmanydigidollar` now preflights transaction capacity before build and
  broadcast so users get clearer errors for oversized sends.
- New wallet RPCs:
  - `listdigidollarunspent`
  - `listdigidollarutxos`
- DigiDollar send RPCs now accept optional selected DD inputs:
  - `senddigidollar(address, amount, comment, fee_rate, selected_inputs)`
  - `sendmanydigidollar("", amounts, comment, selected_inputs)`
- DigiDollar selected-input planning now validates ownership, spend status,
  confirmation depth, standard DD token output shape, amount conservation,
  minimum DD output/change policy, and transaction capacity.
- DigiDollar Qt Send now exposes coin-control style selected DD inputs and
  passes selected inputs into the real wallet send path.
- DigiDollar Qt Receive requests can now be edited and removed with persistent
  wallet storage updates.
- DigiDollar receive requests are kept out of the normal DGB recent-request
  table, and normal DGB requests are kept out of the DigiDollar request table.
- The binary version and Qt splash image were bumped to v9.26.0-rc39.

What did not change:

- No mainnet activation is included in RC39.
- No testnet reset is included in RC39.
- The V1 oracle quorum remains 9-of-17.
- The on-chain v0x03 bundle format is unchanged.
- The oracle roster, activation heights, epoch length, and DigiDollar economic
  rules are unchanged.
- No production mock or fallback price path was added.
- Validators still verify the final oracle bundle from the block.

---

## Testnet24 Network Details

These values are unchanged from RC38.

| Item | RC39 value |
| --- | --- |
| Testnet name | `testnet24` |
| Data directory | `testnet24` |
| Genesis hash | `0xe42636c490059fafe7e0278acc6fb451b901b6a316b31e10d7ccff565baf23df` |
| Merkle root | `0x502bf477644933ced36281bbfdcc6755895b3d9f75262eb148d2c1c2c21d7e73` |
| Genesis time | `2026-05-11 13:53:00 UTC` |
| Genesis nonce | `57535` |
| Network magic | `fe c4 b7 e5` |
| Default P2P port | `12031` |
| Default RPC port | `14026` |
| DigiDollar activation height | `600` |
| Oracle activation height | `600` |
| Oracle epoch length | `40` blocks |
| Oracle quorum | `9-of-17` |
| Oracle bundle format | `v0x03` MuSig2 aggregate bundle |

---

## RC39 In Plain English

RC38 focused on oracle liveness and operator visibility.

RC39 focuses on wallet behavior.

The goal is simple: when a user works with DigiDollar in the wallet or RPC,
the behavior should feel like normal DGB wallet behavior where that makes
sense.

For RPC users, RC39 adds DigiDollar list-unspent support and selected-input
sends. A caller can see spendable DD outputs, filter them by confirmations or
address, and then spend specific DD inputs.

For Qt users, RC39 improves the Send and Receive flows:

1. DD Send has coin-control style input selection.
2. DD Send shows selected input count and amount.
3. DD Send warns before sending when a manual selection cannot cover the
   requested amount.
4. DD Send passes the selected inputs into the real wallet backend.
5. DD Receive requests can be edited.
6. DD Receive requests can be removed.
7. DD Receive request edits and removals persist in wallet storage.
8. DD requests do not pollute the normal DGB request table.

This release also cleans up several rough edges in minting, redemptions, and
amount display so wallet screens agree with consensus parameters and with each
other.

---

## Architecture Record

### 1. Qt mint limits now follow chain parameters

The issue:

The Qt mint screen still had old hardcoded DigiDollar limits. Testers could see
the UI reject or describe amounts differently than the active testnet consensus
parameters.

The fix:

The mint amount validator and tooltip now read the minimum and maximum mint
amounts from the active DigiDollar chain parameters.

Why it matters:

The wallet should not teach users a different rule than the chain enforces.

Code/tests touched:

- `src/kernel/chainparams.cpp`
- `src/qt/digidollarmintwidget.cpp`
- `src/qt/test/digidollarwidgettests.cpp`
- `src/qt/test/digidollarwidgettests.h`

### 2. Qt mint confirmation revalidates live collateral

The issue:

The mint confirmation dialog could be based on stale oracle price and collateral
data if the oracle price moved while the user was reviewing the mint.

The fix:

Before broadcasting a mint, Qt refreshes the oracle price, recalculates required
collateral, rechecks available DGB, and warns the user if the collateral changed
enough to matter.

Why it matters:

The final user approval should be based on the current collateral requirement,
not a stale preview.

Code touched:

- `src/qt/digidollarmintwidget.cpp`

### 3. DD amounts display consistently in Qt

The issue:

DigiDollar screens displayed amounts with inconsistent precision and formatting.

The fix:

Overview, send, receive, and redeem screens now use the same two-decimal DD
display style.

Why it matters:

DD is denominated in cents. The wallet should make cents easy to compare across
screens.

Code touched:

- `src/qt/digidollaroverviewwidget.cpp`
- `src/qt/digidollarreceivewidget.cpp`
- `src/qt/digidollarredeemwidget.cpp`
- `src/qt/digidollarsendwidget.cpp`

### 4. Redeem metadata and locked keys are preserved

The issue:

Some redemption paths could lose or fail to preserve wallet metadata needed to
recognize locked collateral and DigiDollar ownership keys after wallet reloads
or state transitions.

The fix:

The wallet now preserves the redemption metadata and locked-key bookkeeping
needed by DigiDollar positions.

Why it matters:

A wallet should be able to reload and still understand which DD positions and
keys are required for future redemption.

Code/tests touched:

- `src/wallet/digidollarwallet.cpp`
- `src/test/digidollar_txbuilder_tests.cpp`
- `src/wallet/test/digidollar_wave16_persistence_tests.cpp`

### 5. `sendmanydigidollar` preflights transaction capacity

The issue:

Large DD batch sends could fail late during transaction build or relay because
of OP_RETURN metadata size, transaction weight, or fee input requirements.

The fix:

The wallet now preflights DD transfer capacity before build and broadcast. It
checks projected metadata size, transaction weight, and required DGB fees.

Why it matters:

Users get a direct error explaining that they need fewer recipients, split the
send, consolidate UTXOs, or add fee funds.

Code/tests touched:

- `src/rpc/digidollar.cpp`
- `src/wallet/digidollarwallet.cpp`
- `src/test/digidollar_txbuilder_tests.cpp`
- `test/functional/digidollar_rpc_amount_filters.py`

### 6. DigiDollar has list-unspent RPCs

The issue:

Normal DGB users have `listunspent`. DigiDollar users did not have an equivalent
wallet RPC for seeing spendable DD outputs.

The fix:

RC39 adds:

- `listdigidollarunspent`
- `listdigidollarutxos`

The RPCs support:

- `minconf`
- `maxconf`
- DigiDollar address filtering
- duplicate-address errors
- invalid-address errors
- `include_unsafe`
- `txid`
- `vout`
- `address`
- `scriptPubKey`
- DD `amount` in cents
- `confirmations`
- `spendable`
- `safe`

Spent DD outputs are hidden.

Why it matters:

Wallets, scripts, and operators can inspect DD UTXOs using DGB-style semantics.

Code/tests touched:

- `src/rpc/client.cpp`
- `src/rpc/digidollar.cpp`
- `src/rpc/digidollar.h`
- `src/wallet/rpc/wallet.cpp`
- `test/functional/digidollar_listunspent.py`
- `test/functional/digidollar_rpc_amount_filters.py`
- `test/functional/test_runner.py`

### 7. DigiDollar sends support selected inputs

The issue:

Normal DGB coin control lets users choose which coins to spend. DigiDollar sends
needed the same capability for DD UTXOs.

The fix:

RC39 adds selected DD input support to the backend and RPC send paths.

Accepted RPC input format:

```json
[
  {
    "txid": "hex transaction id",
    "vout": 1
  }
]
```

The planner rejects:

- empty selected-input lists
- duplicate outpoints
- unknown or not-owned DD inputs
- already-spent DD inputs
- unconfirmed DD inputs
- non-standard DD token outputs
- insufficient selected DD amount
- selected DD change below the minimum output amount
- transfer metadata or weight that would exceed policy capacity

Why it matters:

Manual DD input selection cannot bypass ownership, spend status, confirmation,
amount, change, or standardness checks.

Code/tests touched:

- `src/rpc/client.cpp`
- `src/rpc/digidollar.cpp`
- `src/wallet/digidollarwallet.cpp`
- `src/wallet/digidollarwallet.h`
- `src/wallet/test/digidollar_wave17_spendability_tests.cpp`
- `test/functional/digidollar_send.py`

### 8. Qt Send has DigiDollar coin-control parity

The issue:

The DigiDollar Send screen needed a real user flow for selected DD inputs, not
only backend support.

The fix:

RC39 adds DD Send coin-control behavior:

- an `Inputs...` selector
- automatic/manual selected-input state
- selected quantity display
- selected DD amount display
- insufficient-selection warning before send
- confirmation text that includes selected-input information
- selected-input handoff through `WalletModel::sendDigiDollar`

Why it matters:

Qt users can make deliberate DD spends using the same mental model as normal
DGB coin control.

Code/tests touched:

- `src/qt/digidollarsendwidget.cpp`
- `src/qt/digidollarsendwidget.h`
- `src/qt/walletmodel.cpp`
- `src/qt/walletmodel.h`
- `src/qt/test/digidollarwidgettests.cpp`
- `src/qt/test/digidollarwidgettests.h`

### 9. Qt Receive can edit and remove DigiDollar requests

The issue:

Normal DGB receive requests can be managed by the wallet. DigiDollar receive
requests needed equivalent edit, remove, filtering, and persistence behavior.

The fix:

RC39 adds:

- DD receive request edit button
- DD receive request context-menu edit action
- label/message/amount edit dialog
- persistent wallet storage update after edit
- address-book label update after edit
- persistent wallet storage removal after delete
- DD address decoding through `DecodeDigiDollarAddress`
- filtering so DD requests do not appear in the normal DGB receive table
- filtering so DGB requests do not appear in the DD receive table

Why it matters:

Receive requests should survive wallet reloads and should not leak across DGB
and DD request lists.

Code/tests touched:

- `src/qt/digidollarreceivewidget.cpp`
- `src/qt/digidollarreceivewidget.h`
- `src/qt/recentrequeststablemodel.cpp`
- `src/qt/test/digidollarwidgettests.cpp`
- `src/qt/test/digidollarwidgettests.h`

### 10. Version and splash image were bumped to RC39

The issue:

The binary and wallet branding needed to identify this as the next testnet
release candidate.

The fix:

The version was bumped to v9.26.0-rc39 and the Qt wallet splash image was
regenerated with the RC39 DigiDollar text.

Code/assets touched:

- `configure.ac`
- `src/qt/res/icons/digibyte_wallet.png`

---

## Operator Upgrade Notes

### Everyone

1. Back up wallet and oracle key material.
2. Stop old RC38 or earlier nodes.
3. Keep using `testnet24`; do not wipe into a new testnet for RC39.
4. Start RC39 and confirm the client version reports RC39.
5. Reconnect to upgraded peers.

### Wallet Testers

Focus on wallet behavior that should match normal DGB behavior:

1. Create normal DGB receive requests and confirm the normal DGB request table
   still behaves correctly.
2. Create DD receive requests and confirm they appear only in the DD Receive
   request table.
3. Edit DD receive request label, message, and amount.
4. Remove DD receive requests.
5. Restart the wallet and confirm DD request edits/removals persisted.
6. Use `listdigidollarunspent` to inspect DD UTXOs.
7. Send DD automatically.
8. Send DD with selected inputs.
9. Confirm the Qt Send selected-input count and amount match the chosen DD
   inputs.
10. Confirm insufficient selected DD input warnings appear before send.

Useful commands:

```bash
digibyte-cli -testnet getblockchaininfo
digibyte-cli -testnet getnetworkinfo
digibyte-cli -testnet getwalletinfo
digibyte-cli -testnet getdigidollarbalance
digibyte-cli -testnet listdigidollarunspent
digibyte-cli -testnet listdigidollarutxos
```

Selected-input send example:

```bash
digibyte-cli -testnet senddigidollar \
  "TD..." \
  1000 \
  "" \
  0 \
  '[{"txid":"...","vout":1}]'
```

### Oracle Operators

RC39 does not change the RC38 oracle protocol or on-chain bundle format.

Keep using the RC38 oracle operating checks:

```bash
digibyte-cli -testnet listoracle
digibyte-cli -testnet getoracles
digibyte-cli -testnet getoracleprice
```

Useful log filter:

```bash
tail -n 500 ~/.digibyte/testnet24/debug.log | rg -i 'oracle|heartbeat|musig|context|partial|bundle|digidollar'
```

---

## Validation Status

Final RC39 validation was performed from `feature/digidollar-v1`.

| Gate | Status |
| --- | --- |
| Build: `make -j$(nproc) src/digibyted src/test/test_digibyte src/qt/test/test_digibyte-qt` | PASS |
| Unit tests: `./src/test/test_digibyte --catch_system_errors=no` | PASS |
| Qt tests: `./src/qt/test/test_digibyte-qt -platform offscreen --catch_system_errors=no` | PASS |
| Focused functional tests: `test_runner.py --jobs=4 digidollar_listunspent.py digidollar_rpc_amount_filters.py digidollar_send.py digidollar_transfer.py` | PASS |
| Full functional tests: `python3 test/functional/test_runner.py --jobs=4` | PASS |
| Fuzz target build: `make -j$(nproc) src/test/fuzz/fuzz` | PASS |
| Fuzz all-target runner: `test/fuzz/test_runner.py -l INFO --par=4 /tmp/digibyte_fuzz_corpus` | PASS |
| Seeded DD/oracle/MuSig fuzz subset | PASS |
| Multi-oracle Qt mini-testnet script: `test_multi_oracle_testnet.sh` | PARTIAL, see note below |

Observed validation details:

- Full unit run passed 3372 test cases.
- Full Qt run passed, including 47 `DigiDollarWidgetTests` and 13
  `DigiDollarWave19WidgetTests`.
- Full functional runner passed 371 scripts selected by the runner.
- Fuzz runner detected and exercised 247 registered targets in standalone
  harness mode.
- A seeded DD/oracle/MuSig subset exercised 52 selected targets with seed input.

Fuzz mode used:

- The local build was the standalone fuzz harness mode, not a long libFuzzer
  mutation marathon.
- All 247 registered targets were built/discovered and run through the fuzz
  runner against an empty corpus directory.
- DD/oracle/MuSig-related targets were also run with a minimal seed corpus.

Multi-oracle Qt mini-testnet note:

- The script launched 8 Qt wallet nodes and 16 active oracle slots.
- Live 9-of-17 oracle consensus was reached.
- DD mint, redeem, send, receive balance tracking, transfer chain, wallet
  restart, wallet backup/restore, reindex, rescan, descriptor export/import,
  and restored-wallet DD send checks passed.
- Final script result was 219 passed and 1 failed.
- The single failure was Alice's second mint failing with "Insufficient funds
  for collateral and fees" while the live oracle price was about $0.0036/DGB.
  At that price, the script's static Alice funding assumption was too low.
  This is a script funding sensitivity under live price conditions, not a
  selected-input, receive-request, list-unspent, or Qt Send regression.

---

## Known Risks

- RC39 changes wallet/RPC behavior, not oracle consensus. Operators still need
  at least 9 valid fresh oracle slots online for price bundles.
- The new selected-input path is intentionally strict. Unconfirmed, spent,
  unknown, non-standard, or insufficient DD inputs are rejected.
- `sendmanydigidollar` can now fail earlier with clearer preflight errors. That
  is expected when the requested transaction would exceed standard policy or
  fee capacity.
- Public testnet DD activity still depends on live oracle prices and available
  collateral. Low DGB/USD prices require more DGB collateral for the same DD
  mint amount.
- The broad mini-testnet script has a static funding assumption for Alice that
  can be too low during very low live DGB oracle prices.

---

## Commit Scope Reviewed

RC39 includes these commits after `v9.26.0-rc38`:

- `3c58167a2a56f8e88619c05778c811e1529b532d` - `qt: derive DigiDollar mint limits from chain params`
- `53b0c66afe903f9e1a8879514e0b8bba5df301cb` - `qt: revalidate DigiDollar mint confirmations`
- `0de1625353024ab1e01ec1f5f907eddae9f28344` - `qt: standardize DigiDollar amount display precision`
- `f248fcb406a18e8d4f256d9dad0d7e3c00ed1021` - `wallet: preserve DigiDollar redeem metadata and locked keys`
- `4fcf45c652435695d4a861a605a114d8e28cf775` - `wallet: preflight DigiDollar sendmany transaction capacity`
- `5669a2af82d158677282170c948900c305de96ab` - `release: bump version to v9.26.0-rc39`
- `5ab5403cf4ad215731e4b13c83febe54fe355335` - `rpc: add DigiDollar list unspent wallet RPCs`
- `ca3ac0e48a3c79a07241a02ac174a04a505a6f24` - `wallet: add DigiDollar selected-input transfer planning`
- `33d91023a10d36eb9a2b55408c50b403ee04aa67` - `qt: add DigiDollar send coin-control parity`
- `b422716d61f40471039d0d251bdff60c384b1d4e` - `qt: edit DigiDollar receive requests`

---

## Bottom Line

RC39 keeps the RC38 oracle network and testnet intact.

The release makes DigiDollar wallet behavior closer to normal DGB wallet
behavior:

- users can list DD UTXOs,
- users can manually choose DD inputs,
- Qt Send can pass those selected inputs to the backend,
- Qt Receive requests can be edited and removed persistently,
- DD and DGB request history stay separated,
- mint and send screens give clearer, more current information.

The goal is not a new DigiDollar-only UX. The goal is to make DigiDollar act
like a first-class wallet asset inside DigiByte Core.
