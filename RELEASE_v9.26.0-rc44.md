# DigiByte Core v9.26.0-rc44 Release Notes

RC44 is the DigiDollar release candidate for a full public testnet reset and
the 35-slot oracle roster. Mainnet and testnet26 now expose 35 active MuSig2
oracle slots with a 7-signature quorum. Slot 28 now uses the DigiHash Mining
Pool key, and slot 31 remains reserved for Peer2Peer / DigiRoos until a valid
compressed secp256k1 oracle key is supplied.

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
3. Do not copy old `blocks/`, `chainstate/`, `peers.dat`, oracle messages, or cached MuSig2 state from `testnet25`.
4. Start RC44 with `-testnet`, using the new `testnet26` data directory.
5. Open and advertise P2P port `12033`.
6. Update static `addnode` and oracle endpoint config from port `12032` to `12033`.
7. Confirm `getblockchaininfo` reports the RC44 genesis hash above.
8. For mainnet, generate a new wallet/oracle key. Keep existing testnet keys for testnet26 only.

## Oracle Roster

Mainnet and testnet26 now use 35 active oracle slots and require 7 valid MuSig2
signatures per oracle bundle. Active oracle IDs are `0` through `34`; ID `35`
is outside the configured roster and must be rejected.

New slots added since the previous 24-slot code baseline:

| Slot | Operator | Compressed public key |
|------|----------|-----------------------|
| 24 | ckunchained | `03926ed40635d294a554ec046a96d3fa58587521385c7df58ff21ede12a31add0e` |
| 25 | JMag | `034103ed4168d11dcaafa96494d5b3dd37247fa6deefa08d47f7004568792b1672` |
| 26 | HashedMax / HMPool | `038adf7df5fcd114178643f16aa0e3be8fa1e221ca421479e48c0bd04f2561d3a8` |
| 27 | DennisPitallano | `02557029e2419af54984f3f2fb600004c0a6f8573dac5730cfaab2048c80ba6894` |
| 28 | DigiHash Mining Pool | `03532efd6277226f38903401ec8317ba7cc8f13eb48dc8cb1e102fdc23488d7cef` |
| 29 | Michael E / medgboracle3452 | `03d566a244719aa577d828da31ad9863f94686f710ac1f0638914eff5692ec7d58` |
| 30 | Scott K / DigibyteDaily | `03603a0175197a1fe28859c71c69fd0081710c5569fceceaa96ce7de386d3ebf61` |
| 31 | Peer2Peer / DigiRoos placeholder | `02e30e9349b7afcac60fb1db2997512079b3c8b6942c450f4f942c7d5e69e9a42b` |
| 32 | 3DogsKanab | `035878eb72be3710d8c3f9585e27374011a476378f5ea7f3ab2f2830ab052ec5f8` |
| 33 | LiberatedLark | `035b3729a255bfbcff1bfd5b72fdb1a971b098545db7e874751179bc22c7f7f0b0` |
| 34 | Manu_DGB_oracle | `03d8fe0cd773604fa78fb1cef7d1e88a05c2473961178e40a4ac3c9aeeb4cbca87` |

All oracle operators, miners, seeders, and testnet nodes must run the same RC44
binary before signing or mining on the new public testnet.

Note: slot 31 is intentionally reserved with a placeholder public key. Replace
slot 31 only after receiving a valid compressed secp256k1 key from Peer2Peer /
DigiRoos; the previously submitted value is not a valid compressed secp256k1
public key and is not used in consensus.
