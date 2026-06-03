# DigiByte Core v9.26.0-rc44 Release Notes

RC44 is the DigiDollar release candidate for a full public testnet reset and
the 35-slot oracle roster. Mainnet and testnet26 now expose 35 active MuSig2
oracle slots with a 7-signature quorum. Slots without final operator keys use
placeholder pubkeys so the full roster shape can be tested before launch.

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
| 28 | DigiHash Mining Pool placeholder | `02902ba3cda1883801594b6e1b452790cc53948fda6c45e47c74e0fb4e8a8088bb` |
| 29 | Michael E / medgboracle3452 | `03d566a244719aa577d828da31ad9863f94686f710ac1f0638914eff5692ec7d58` |
| 30 | Scott K / DigibyteDaily | `03603a0175197a1fe28859c71c69fd0081710c5569fceceaa96ce7de386d3ebf61` |
| 31 | DigiRoos / Oracle31-Peer2Peer placeholder pending corrected key | `02e30e9349b7afcac60fb1db2997512079b3c8b6942c450f4f942c7d5e69e9a42b` |
| 32 | Oracle32 placeholder | `03efb70f482f919cc3abd8929b0f584736f88068e611724228f148a2fde7df5bd7` |
| 33 | Oracle33 placeholder | `02e5cba4a02116ae376a38fb71e759095e6f169a5328868530d18035522af076bc` |
| 34 | Oracle34 placeholder | `03b66508e1ec994f2451314d7a8c517a0da8ba9f9b9e93491a9470a5065a0bcc3a` |

All oracle operators, miners, seeders, and testnet nodes must run the same RC44
binary before signing or mining on the new public testnet.

Note: DigiRoos / Oracle31-Peer2Peer is assigned in the roster, but the latest
submitted key string was odd-length hex and is not shipped as a consensus key.
Replace slot 31's placeholder only after receiving a corrected full compressed
public key.
