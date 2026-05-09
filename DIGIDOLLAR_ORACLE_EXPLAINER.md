# DigiDollar Oracle System - V1 Explainer

*Updated: 2026-05-05*
*Document Version: 6.0 - Codex Wave 8 refresh against `feature/digidollar-v1`*

## V1 Summary

DigiDollar V1 uses a MuSig2 oracle network to bring a deterministic DGB/USD
price into block validation. Oracle operators fetch prices from the configured
exchange set, sign and relay off-chain attestations, run a MuSig2 signing
round, and produce one on-chain v0x03 oracle bundle for miners to place in the
coinbase.

The only production on-chain oracle bundle format is:

```text
OP_RETURN OP_ORACLE <version=0x03> <v03_data>
```

Raw v0x01 and v0x02 oracle payloads are legacy data. They are not accepted,
mined, relayed as fallback, or used to update the price cache in V1.

## Roster And Quorum

| Network | Oracle metadata slots | Consensus-active slots | Quorum |
|---------|-----------------------|------------------------|--------|
| Mainnet | 30 | 17, slots 0-16 | 9-of-17 |
| Testnet23 | 17 | 17, slots 0-16 | 9-of-17 |
| Regtest | 7 | 7, slots 0-6 | 4-of-7 |

Mainnet slots 17-29 are reserve metadata entries. They are not part of
`consensus.vOraclePublicKeys`, do not count toward pending-message quorum, do
not appear in the MuSig2 bitmap, and cannot contribute to the V1 aggregate
signature unless a future deterministic activation rule expands the consensus
roster. Testnet23 has no reserve metadata slots configured.

## Operator Flow

1. The oracle node fetches DGB/USD from the active exchange fetchers in
   `src/oracle/exchange.cpp`.
2. `OracleNode::FetchMedianPrice()` requires at least three responsive exchange
   sources before publishing a price.
3. The node signs an `oracleprice` message and relays it over P2P.
4. Nodes exchange `oracleconsns` and `oracleattest` messages to agree on the
   price/timestamp tuple.
5. Operators run the MuSig2 nonce and partial-signature rounds using
   `oramusnonce` and `oramusigpsig`.
6. A completed session yields a 64-byte aggregate BIP-340 signature and a
   participation bitmap.
7. The miner embeds the v0x03 bundle in the coinbase only when the block
   contains DigiDollar activity and a valid bundle is available.

Off-chain messages are signing inputs only. They are not accepted as on-chain
oracle bundles.

## On-Chain v0x03 Data

`COracleBundle::SerializeV03Data()` defines the payload:

```text
bitmap_len                 1 byte
participation_bitmap       bitmap_len bytes
epoch                      4 bytes little-endian
median_price_micro_usd     8 bytes little-endian
timestamp                  8 bytes little-endian
aggregate_sig              64 bytes
```

The version byte is pushed separately as `0x03` before this payload. Regtest
uses a one-byte bitmap; mainnet/testnet use a three-byte bitmap for the
17-slot active roster.

## Miner Behavior

`OracleBundleManager::CreateOracleScript()` emits only v0x03 scripts. It
returns an empty script unless the bundle has:

- `version == 3`
- a 64-byte aggregate signature
- a non-empty participation bitmap
- a decodable participant set
- at least the network quorum of participants

`OracleBundleManager::AddOracleBundleToBlock()` first checks completed MuSig2
sessions, then the cached current-epoch bundle. If no valid v0x03 bundle is
ready:

- DD-touching block templates cannot keep the DD transactions.
- ordinary non-DD block templates may omit the oracle bundle and remain valid.

## Block Validation

`OracleDataValidator::ValidateBlockOracleData()` runs after DigiDollar
activation on mainnet, testnet, and regtest. There is no mainnet validation
bypass.

Validation order:

1. If DigiDollar is not active for the historical block state, return true.
2. Count `OP_RETURN OP_ORACLE` coinbase outputs.
3. Reject more than one oracle output with `bad-oracle-multiple-outputs`.
4. If no oracle output exists, accept ordinary non-DD and DD transfer-only
   blocks; reject mint/redeem blocks with `bad-oracle-missing`.
5. Extract the bundle. Legacy v0x01/v0x02, unknown versions, truncated data,
   and malformed pushes fail extraction and reject with `bad-oracle-malformed`.
6. Require `bundle.IsMuSig2()`.
7. Run `ValidateMuSig2Bundle()`:
   - MuSig2 roster active at the block height
   - price in `MIN/MAX_ORACLE_PRICE_MICRO_USD`
   - aggregate signature is exactly 64 bytes
   - bitmap is non-empty and decodes
   - payload epoch matches block epoch
   - signer count meets `nOracleConsensusRequired`
   - every signer is inside `nOraclePubkeyCount`
   - aggregate BIP-340 signature verifies against the chain-bound message hash
8. Enforce timestamp freshness: not older than one hour and not more than
   60 seconds in the future relative to block time.

## Mempool And Price Source

DD transactions require a recent valid MuSig2 quote before mempool acceptance.
Ordinary DGB transactions do not consult DigiDollar or oracle validation.

Block validation uses the price extracted from the block's validated v0x03
bundle. It does not fall back to mock prices, exchange fetchers, or process-local
oracle state during consensus validation.

## Price Cache

After a block connects, `ConnectBlock()` updates the oracle price cache from
the validated v0x03 bundle. The cache stores micro-USD prices by block height,
keeps the most recent 1,000 entries, and removes/replays entries during
disconnect, reorg, reindex, and IBD flows.

## Activation

DigiDollar and the oracle validator activate together:

| Network | DigiDollar activation | Oracle activation | MuSig2 height |
|---------|-----------------------|-------------------|---------------|
| Mainnet | BIP9 bit 23, min height 22,014,720 | same trigger | 0 |
| Testnet23 | height 600 / BIP9 active | same trigger | 0 |
| Regtest | BIP9 `ALWAYS_ACTIVE`; DD/oracle P2P height gates 650 by default, or the direct `-digidollaractivationheight=N` override | same height trigger | 0 |

Before activation, DD-looking data does not trigger V1 consensus rules. After
activation, DD-touching blocks must satisfy the V1 oracle rules above.

## Regtest Mocking

Regtest exposes mock price RPCs for deterministic tests. The mock path is not a
production fallback for consensus validation. Functional tests use it to produce
signed regtest v0x03 quote blocks and then validate the same block rules that
mainnet/testnet use after activation.

## Rejection Matrix

| Block shape | Expected result |
|-------------|-----------------|
| Non-DD block, no oracle output | accepted |
| Non-DD block, one valid v0x03 oracle output | accepted |
| Non-DD block, malformed or legacy oracle output | rejected |
| Any block with two oracle outputs | `bad-oracle-multiple-outputs` |
| DD transfer-only block, no oracle output | accepted |
| DD mint/redeem block, no oracle output | `bad-oracle-missing` |
| DD mint/redeem block, raw v0x01/v0x02 oracle output | `bad-oracle-malformed` |
| DD mint/redeem block, malformed v0x03 output | `bad-oracle-malformed` |
| DD mint/redeem block, v0x03 below quorum or wrong signature | `bad-oracle-musig2` |
| DD-touching block, valid v0x03 output | accepted |

## Code References

- `src/oracle/bundle_manager.cpp` - bundle extraction, v0x03 serialization,
  MuSig2 validation, price cache, block oracle validator.
- `src/primitives/oracle.h` - `COracleBundle` and v0x03 payload helpers.
- `src/validation.cpp` - mempool oracle quote gate, ConnectBlock call sites,
  price-cache update and rollback.
- `src/node/miner.cpp` - miner template DD filtering and oracle-bundle
  insertion.
- `src/net_processing.cpp` - oracle P2P message handlers and legacy
  `oraclebundle` drop behavior.
- `src/kernel/chainparams.cpp` - active oracle keys, reserve metadata, quorum,
  and activation parameters.

## Tests

The V1 format and block rules are pinned by:

- `src/test/digidollar_oracle_musig2_tests.cpp`
- `src/test/digidollar_oracle_bundle_matrix_tests.cpp`
- `src/test/fuzz/oracle_bundle_version_reject.cpp`
- `src/test/fuzz/oracle_validate_block_data.cpp`
- `test/functional/digidollar_oracle_block_rules_relay.py`
- `test/functional/digidollar_oracle_bundle_reject_matrix.py`

These tests prove that legacy bundles are rejected, DD-touching blocks require a
valid v0x03 bundle, non-DD blocks can omit oracle data, and malformed oracle
payloads cannot update consensus price state.
