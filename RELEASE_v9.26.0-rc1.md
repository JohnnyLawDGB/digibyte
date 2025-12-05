**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**


**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im - Active development discussion happens here!

## What is DigiDollar?

DigiDollar is a USD-pegged stablecoin built natively into DigiByte. It uses an over-collateralized model where users lock DGB to mint DUSD at the current oracle price of DGB.

The world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

DGB becomes the strategic reserve asset (21B max, only 2.23 DGB per person on Earth right now). Everything happens inside DigiByte Core wallet. You never give up control of your private keys. No centralized company, fund or pool. Pure decentralization.

**Learn more:** https://digibyte.io/digidollar

## Current Status

- **Single Oracle Testing** - This beta uses one oracle node for price feeds. Production will use a decentralized oracle network.
- **Testnet Only** - All DGB and DUSD on testnet have no real value.

## Quick Start

### 1. Use a Separate Data Directory

If you already have DigiByte Core installed, use a separate data directory for DigiDollar testing. This keeps your existing installation completely isolated.

| Platform | Data Directory |
|----------|----------------|
| **Linux** | `~/.digibyte-digidollar` |
| **macOS** | `~/Library/Application Support/DigiByte-DigiDollar` |
| **Windows** | `%USERPROFILE%\DigiByte-DigiDollar` |

### 2. Create Config File

Create `digibyte.conf` inside your data directory:

```ini
# DigiDollar Testnet Config
testnet=1
server=1
txindex=1

[test]
digidollar=1
debug=digidollar
addnode=44.251.30.49
```

### 3. Launch the Wallet

| Platform | Command |
|----------|---------|
| **Linux** | `./digibyte-qt -testnet -datadir=~/.digibyte-digidollar` |
| **macOS** | `./DigiByte-Qt.app/Contents/MacOS/DigiByte-Qt -testnet -datadir=~/Library/Application\ Support/DigiByte-DigiDollar` |
| **Windows** | `digibyte-qt.exe -testnet -datadir=%USERPROFILE%\DigiByte-DigiDollar` |

The DigiDollar tab will appear automatically.

### 4. Get Testnet DGB

Since no faucet is available yet, you'll mine testnet DGB directly. This only works on testnet - mainnet requires real mining hardware.

**Step 1: Get your receive address**

In DigiByte-Qt, go to the **Receive** tab and click **Create new receiving address**. Copy your `dgbt1...` address.

**Step 2: Mine testnet DGB**

You can mine using either the GUI console or command line:

**Option A: GUI Console (easiest)**
1. In DigiByte-Qt, go to **Window → Console**
2. Type: `generatetoaddress 1 dgbt1qYOURADDRESSHERE`
3. Press Enter and wait - this mines 1 blocks, need to wait 100 blocks to confirm

**Option B: Command Line**

| Platform | Command |
|----------|---------|
| **Linux** | `./digibyte-cli -testnet -datadir=~/.digibyte-digidollar generatetoaddress 1 dgbt1qYOURADDRESSHERE` |
| **macOS** | `./digibyte-cli -testnet -datadir=~/Library/Application\ Support/DigiByte-DigiDollar generatetoaddress 1 dgbt1qYOURADDRESSHERE` |
| **Windows** | `digibyte-cli.exe -testnet -datadir=%USERPROFILE%\DigiByte-DigiDollar generatetoaddress 1 dgbt1qYOURADDRESSHERE` |

**Note:** Mined coins require 100 block confirmations before spending. On a shared testnet, you can only mine 1 block at a time. Keep mining blocks or wait for others on the network to mine - your coins become spendable after 100 confirmations.

### 5. Test DigiDollar features

- View oracle price and network stats on the Overview tab
- Mint DUSD by locking DGB collateral
- Redeem DUSD back to DGB
- Send/receive DUSD

## Network Info

| Setting | Value |
|---------|-------|
| Oracle Node | 44.248.21.71:12028 |
| Testnet P2P Port | 12028 |
| Testnet RPC Port | 14028 |

## Documentation

For developers and those wanting to understand how DigiDollar works:

| Document | Description |
|----------|-------------|
| [DIGIDOLLAR_EXPLAINER.md](DIGIDOLLAR_EXPLAINER.md) | High-level overview of DigiDollar for users |
| [DIGIDOLLAR_ARCHITECTURE.md](DIGIDOLLAR_ARCHITECTURE.md) | Technical architecture and implementation details |
| [DIGIDOLLAR_ORACLE_EXPLAINER.md](DIGIDOLLAR_ORACLE_EXPLAINER.md) | How the oracle price feed system works |
| [DIGIDOLLAR_ORACLE_ARCHITECTURE.md](DIGIDOLLAR_ORACLE_ARCHITECTURE.md) | Oracle network technical specification |

## Downloads

| Platform | File |
|----------|------|
| Linux x86_64 | `digibyte-9.26.0-rc1-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc1-aarch64-linux-gnu.tar.gz` |
| Windows | `digibyte-9.26.0-rc1-win64.zip` |
| macOS Intel | `digibyte-9.26.0-rc1-x86_64-apple-darwin.tar.gz` |
| macOS Apple Silicon | `digibyte-9.26.0-rc1-arm64-apple-darwin.tar.gz` |

## Feedback & Community

Please report issues and feedback to help us prepare for mainnet launch.

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im - Active development discussion happens here
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues

