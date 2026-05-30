# DigiByte Core v9.26.0-rc44 Release Notes

RC44 is a DigiDollar release-candidate fix pass that resets public DigiDollar
testnet and publishes the coordinated 21-active-oracle roster with a 7-signature
MuSig2 quorum.

## Testnet26 Reset

RC44 starts a fresh public DigiDollar testnet: `testnet26`.

RC41/RC42/RC43 used `testnet25`. RC44 does not. Old `testnet25` blocks,
chainstate, peer addresses, cached oracle messages, signed nonces, and
DigiDollar positions are not RC44 proof. Back up wallet and oracle key material
before wiping old data, then start clean on `testnet26`.

| Parameter | Value |
|-----------|-------|
| Testnet name | `testnet26` |
| Data directory | `testnet26` |
| Genesis timestamp | `1780156800` (2026-05-30 16:00:00 UTC) |
| Genesis hash | `0x0135174514d831ecc687a15e1ae31164bebf92d58bf279ab929226b8470b1dd3` |
| Merkle root | `0xcfdf1bf7e7c947c54aab4ec81b963a5b869fa3e6e84db54970d5c224e0f1a170` |
| Genesis nonce | `711761` |
| Network magic | `fe c6 b9 e7` |
| Default P2P port | `12033` |
| RPC port | `14026` |
| DigiDollar activation | BIP9 bit 23, minimum height 600 |

## Operator Migration

1. Stop RC43 or older testnet nodes.
2. Back up wallets and oracle key material.
3. Do not copy old `blocks/`, `chainstate/`, `peers.dat`, oracle messages, or
   cached MuSig2 state from `testnet25`.
4. Start RC44 with `-testnet`, using the new `testnet26` data directory.
5. Open and advertise P2P port `12033`.
6. Update static `addnode` and oracle endpoint config from port `12032` to
   `12033`.
7. Confirm `getblockchaininfo` reports the RC44 genesis hash above.

## Oracle Roster

Mainnet and testnet now use 21 active oracle slots and require 7 valid MuSig2
signatures per oracle bundle. Slots 0-20 are active; slots 21-34 remain reserved
inactive metadata.

New active slots in this release:

| Slot | Operator | Compressed public key |
|------|----------|-----------------------|
| 17 | digibyte-maxi | `03649d750bcad5b42b3dd0f11c8d98d62ed5afd515cd986663f81c35f086e58d47` |
| 18 | Anthony | `0345f8cb22dfde6aff8f18552c338256e0df551ca2df007f6449d6da1dbb7f4d89` |
| 19 | mbah_jambon | `031758a6d7f1f87c95d1a4a38415608d41463a504ea28da7c6129e2a9d654add42` |
| 20 | Camden | `03018c81746d6ddc326c993d9f2e7f2015554e97a261f3b5fe637ac5098f421a4c` |

All oracle operators, miners, seeders, and testnet nodes must run the same RC44
binary before signing or mining on the new public testnet.
