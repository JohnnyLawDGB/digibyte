# DigiByte v9.26.0-rc13 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## What's New in RC13

### 🐛 Bug Fixes & Improvements

RC13 is a stabilization release with Qt GUI improvements and RPC fixes.

#### Qt GUI Fixes

- **Oracle Price Auto-Refresh in Mint Widget** — The DigiDollar Mint widget now automatically refreshes the oracle price, ensuring users always see current pricing when minting DUSD
- **Improved RPC Exception Handling** — Better error handling in DigiDollar RPC calls prevents GUI crashes when the backend returns unexpected data
- **Lock Tier Display Names Fixed** — The DigiDollar GUI widgets now correctly display lock tier names (was showing incorrect tier labels)

#### RPC Fixes

- **`getalloracleprices` Client Conversion** — Fixed missing entry in the client conversion table that was preventing proper RPC response handling

---

## Commits Since RC12

```
fix(rpc): add getalloracleprices to client conversion table
fix(qt): add oracle price auto-refresh timer to DD Mint widget
fix(qt): improve exception handling in DigiDollar RPC calls
fix(qt): correct lock tier display names in DigiDollar GUI widgets
```

---

## Technical Changes

| File | Change |
|------|--------|
| `configure.ac` | Version bump RC12 → RC13 |
| `src/qt/digidollarmintwidget.cpp` | Add oracle price auto-refresh timer |
| `src/qt/digidollar*.cpp` | Improve RPC exception handling |
| `src/qt/digidollarpositionswidget.cpp` | Fix lock tier display names |
| `src/rpc/client.cpp` | Add `getalloracleprices` to conversion table |
| `src/qt/res/icons/digibyte_wallet.png` | Update wallet splash to RC13 |

---

## What is DigiDollar?

DigiDollar is a USD-pegged stablecoin built natively into DigiByte. It uses an over-collateralized model where users lock DGB to mint DUSD at the current oracle price of DGB.

The world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

DGB becomes the strategic reserve asset (21B max, only 2.23 DGB per person on Earth right now). Everything happens inside DigiByte Core wallet. You never give up control of your private keys. No centralized company, fund or pool. Pure decentralization.

**Learn more:** https://digibyte.io/digidollar

---

## Current Status

- **Phase 2 Multi-Oracle Testing** — This release continues 4-of-7 Schnorr threshold oracle consensus on testnet with live exchange prices.
- **Testnet Only** — All DGB and DUSD on testnet have no real value.

---

## Upgrade Notes

**RC13 uses the same testnet13 network as RC12 (port 12030). Your existing testnet data and wallets will work — no migration needed.**

If you're upgrading from RC12, simply replace the binaries and restart.

### If Upgrading from RC11 or Earlier:
1. Close your old wallet
2. Delete old testnet data:
   - **Windows:** Delete `%APPDATA%\DigiByte\testnet10\`, `testnet11\`, and `testnet12\`
   - **macOS:** Delete `~/Library/Application Support/DigiByte/testnet10/`, `testnet11/`, and `testnet12/`
   - **Linux:** Delete `~/.digibyte/testnet10/`, `~/.digibyte/testnet11/`, and `~/.digibyte/testnet12/`
3. Download and install RC13
4. Launch with `-testnet` flag

---

## Quick Start Guide

### Step 1: Download

Download the appropriate file for your platform from the Downloads section below and extract it.

### Step 2: Create Config File

Create the config file in your platform's data directory with the following contents:

```ini
# DigiByte Configuration
# Global settings (apply to all networks)
testnet=1
server=1
txindex=1

# Testnet-specific settings
[test]
digidollar=1
digidollarstatsindex=1
algo=sha256d
addnode=oracle1.digibyte.io
rpcuser=digibyte
rpcpassword=digibyte123
```

---

## Windows Setup

### Step 1: Download and Install
Download `digibyte-9.26.0-rc13-win64-setup.exe` and install normally.

### Step 2: First Launch (Testnet Mode)
You must launch in testnet mode. Open **PowerShell** and run:
```powershell
& "C:\Program Files\DigiByte\digibyte-qt.exe" -testnet
```

The wallet will start in testnet mode and create the data directory automatically.

### Step 3: Create Config File
1. Press `Win + R`, type `%APPDATA%\DigiByte` and press Enter
2. Create a new text file named `digibyte.conf` (remove the `.txt` extension)
3. Paste the config contents from Step 2 above and save

### Step 4: Restart
Close the wallet and launch again from PowerShell:
```powershell
& "C:\Program Files\DigiByte\digibyte-qt.exe" -testnet
```

### Verify It's Working
- Title bar should say **"DigiByte Core - Wallet [testnet13]"**
- You should see a **DigiDollar** tab in the sidebar

### Data Directory Reference
- Config: `%APPDATA%\DigiByte\digibyte.conf`
- Testnet data: `%APPDATA%\DigiByte\testnet13\`

---

## macOS Setup

### Step 1: Download and Extract
Download the `.dmg` file for your Mac and open it. Drag **DigiByte-Qt** to your Desktop.

### Step 2: First Launch (Testnet Mode)
You must launch in testnet mode. Open **Terminal** and run:
```bash
cd ~/Desktop
./DigiByte-Qt.app/Contents/MacOS/DigiByte-Qt -testnet
```

If you get a security warning, right-click the app and select "Open", or run:
```bash
xattr -cr ~/Desktop/DigiByte-Qt.app
```
Then try the launch command again.

The wallet will start in testnet mode and create the data directory automatically.

### Step 3: Create Config File
In Terminal:
```bash
mkdir -p ~/Library/Application\ Support/DigiByte
cat > ~/Library/Application\ Support/DigiByte/digibyte.conf << 'EOF'
# DigiByte Configuration
# Global settings (apply to all networks)
testnet=1
server=1
txindex=1

# Testnet-specific settings
[test]
digidollar=1
digidollarstatsindex=1
algo=sha256d
addnode=oracle1.digibyte.io
rpcuser=digibyte
rpcpassword=digibyte123
EOF
```

### Step 4: Restart
Close the wallet and launch again from Terminal:
```bash
cd ~/Desktop
./DigiByte-Qt.app/Contents/MacOS/DigiByte-Qt -testnet
```

### Verify It's Working
- Title bar should say **"DigiByte Core - Wallet [testnet13]"**
- You should see a **DigiDollar** tab in the sidebar

### Data Directory Reference
- Config: `~/Library/Application Support/DigiByte/digibyte.conf`
- Testnet data: `~/Library/Application Support/DigiByte/testnet13/`

---

## Ubuntu/Linux Setup

### Data Directory
```
~/.digibyte/
```
Config file: `~/.digibyte/digibyte.conf`

Testnet data stored in: `~/.digibyte/testnet13/`

### Steps:
1. Open Terminal and create config:
```bash
mkdir -p ~/.digibyte
cat > ~/.digibyte/digibyte.conf << 'EOF'
# DigiByte Configuration
# Global settings (apply to all networks)
testnet=1
server=1
txindex=1

# Testnet-specific settings
[test]
digidollar=1
digidollarstatsindex=1
algo=sha256d
addnode=oracle1.digibyte.io
rpcuser=digibyte
rpcpassword=digibyte123
EOF
```

2. Extract and run:
```bash
cd ~/Downloads
tar xzf digibyte-9.26.0-rc13-x86_64-linux-gnu.tar.gz
./digibyte-9.26.0-rc13/bin/digibyte-qt
```

### Verify It's Working
- Title bar should say **"DigiByte Core - Wallet [testnet13]"**
- You should see a **DigiDollar** tab in the sidebar

---

## Getting Testnet DGB

### Option 1: GUI Console Mining

Mine testnet DGB directly using the GUI console:

1. Go to the **Receive** tab and create a new address (copy your `dgbt1...` address)
2. Go to **Window > Console**
3. Type: `generatetoaddress 1 dgbt1qYOURADDRESSHERE`
4. Press Enter to mine 1 block
5. Wait for 8 confirmations before spending mined coins (reduced from 100 in testnet)

### Option 2: CPU Miner (Recommended for Continuous Mining)

Use cpuminer to mine testnet DGB in the background. Make sure your wallet is running and synced first.

**Download cpuminer:** https://github.com/pooler/cpuminer

Build from source or download a release for your platform.

**Run cpuminer:**

Linux/macOS:
```bash
./minerd -a sha256d -o http://127.0.0.1:14026 -O digibyte:digibyte123 -t 4 --coinbase-addr=dgbt1qYOURADDRESSHERE
```

Windows (PowerShell):
```powershell
.\minerd.exe -a sha256d -o http://127.0.0.1:14026 -O digibyte:digibyte123 -t 4 --coinbase-addr=dgbt1qYOURADDRESSHERE
```

**Parameters explained:**
- `-a sha256d` — Use SHA256d algorithm (matches `algo=sha256d` in config)
- `-o http://127.0.0.1:14026` — RPC endpoint (testnet RPC port)
- `-O digibyte:digibyte123` — RPC credentials from your config (user:password)
- `-t 4` — Number of CPU threads to use (adjust based on your CPU)
- `--coinbase-addr=dgbt1q...` — Your testnet receive address

The miner will continuously mine blocks and send rewards to your address.

---

## Testing DigiDollar Features

Once your wallet is synced and you have testnet DGB:

1. **View Network Status** — The DigiDollar Overview tab shows oracle price and network collateralization
2. **Mint DUSD** — Lock DGB as collateral to create DigiDollars
3. **Send DUSD** — Transfer DigiDollars to other testnet addresses
4. **Redeem DUSD** — Burn DigiDollars to unlock your DGB collateral (full position only)
5. **View History** — DD Transactions tab shows complete transaction history (now auto-refreshes!)
6. **Coin Control** — Use manual DD input selection for advanced redemptions
7. **Address Book** — Save frequently used DD addresses for quick sending
8. **Export History** — Export DD transactions to CSV for record keeping

---

## Oracle Operator Setup (Phase 2 Test)

Want to run an oracle node for the Phase 2 multi-oracle test? Here's the simple version:

### Prerequisites
- DigiByte Core RC13 built from source with curl support
- An assigned oracle ID (0–6 for testnet, contact the maintainer)

### Two-Command Setup

```bash
# Step 1: Create wallet and generate oracle key (one-time)
digibyte-cli -testnet createwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle createoraclekey <your_oracle_id>

# Step 2: Start your oracle (after every node restart)
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

**Step 1** generates a Schnorr keypair, stores the private key in your wallet, and returns the public key. Send the **X-only public key** (32-byte hex) to the maintainer for inclusion in `chainparams.cpp`.

**Step 2** loads the private key from your wallet and starts the oracle price feed thread. Your node will automatically fetch DGB/USD prices from multiple exchanges (minimum 2 sources required) and broadcast signed price messages to the network.

For the complete guide including configuration, security, troubleshooting, and RPC reference, see **`DIGIDOLLAR_ORACLE_SETUP.md`**.

---

## Oracle RPC Command Reference

### Monitoring Commands

| Command | Description |
|---------|-------------|
| `listoracle` | Shows your **local** oracle status. Is it running? What ID? What price am I submitting? What wallet has the key? If nothing's running, tells you to run `startoracle`. |
| `getoracles` | **Network-wide** view of ALL 7 oracles. Config (pubkey, endpoint), status, and actual last reported price from on-chain data — not just local runtime. This is what a stats site would call. |
| `getoracleprice` | Returns the single **consensus price** (median of all reporting oracles). This is the price DigiDollar actually uses for minting/redemption. |
| `getalloracleprices` | **Forensics deep dive.** Per-oracle breakdown with: exact price each oracle reported, % deviation from median, signature validity, which block it was in. Use this to catch anyone gaming the system. |

### Operator Commands

| Command | Description |
|---------|-------------|
| `createoraclekey <id>` | Generate a new oracle Schnorr keypair in your wallet. One-time setup. |
| `getoraclepubkey <id>` | Show the oracle public key stored in your wallet. |
| `startoracle <id>` | Start running as an oracle operator (requires wallet with oracle key). Must re-run after every node restart. |
| `stoporacle <id>` | Stop your oracle. |
| `sendoracleprice` | Manually submit a price to the network (for testing/debugging). |

---

## Network Information

| Setting | Value |
|---------|-------|
| Network | Testnet (testnet13) |
| Default P2P Port | 12030 |
| Default RPC Port | 14025 |
| Oracle Node | oracle1.digibyte.io:12030 |
| Address Prefix | dgbt1... (bech32) |
| Phase Two Activation | Block 100 |
| Oracle Consensus | 4-of-7 Schnorr threshold |

### Fork Schedule (Testnet13)
| Feature | Block Height |
|---------|--------------|
| MultiAlgo | 100 |
| MultiShield | 200 |
| DigiSpeed | 400 |
| Odocrypt | 500 |
| DigiDollar | 550 |

### Emission Schedule (Testnet13)
| Period | Block Range | Reward |
|--------|-------------|--------|
| Period I | 0–66 | 72,000 DGB |
| Period IV | 67–199 | 8,000 DGB (with decay) |
| Period V | 200–399 | 2,459 DGB (with decay) |
| Period VI | 400+ | 1,078.5 DGB (with decay) |

---

## Known Issues

- Testnet oracle keys (oracles 1–6) use known test values (SHA256 hash-derived). Real operator keys will be swapped before mainnet.
- Mainnet oracle consensus remains disabled (`nDigiDollarPhase2Height = INT_MAX`)
- `startoracle` must be re-run after every `digibyted` restart

---

## Troubleshooting

### "DigiDollar tab not appearing"
- Verify `digidollar=1` is under `[test]` section in config
- Verify `testnet=1` is at the top of config (not under any section)
- Restart the wallet after config changes

### "Not connecting to network"
- Check your firewall allows port 12030
- Verify `addnode=oracle1.digibyte.io` is under `[test]` in config

### "Oracle price shows 0 or N/A"
- Wait for sync to complete
- The oracle broadcasts price updates every few minutes
- Check Window > Console: `getoracleprice`

### "Mining not working"
- Ensure `algo=sha256d` is in your config under `[test]`
- SHA256d is recommended for fastest CPU mining

### "No wallet is loaded" when running oracle commands
- Add `-rpcwallet=oracle` to your `createoraclekey` and `startoracle` commands

### "Oracle not configured" from `startoracle`
- Run `createoraclekey` first to generate and store the key in your wallet

### "Transaction fee too high"
- DigiDollar transactions require 0.1 DGB minimum fee — this is expected
- This ensures reliable network propagation

### "Old testnet data causing crashes"
- Delete your testnet10, testnet11, and testnet12 folders completely (see Upgrade Notes above)
- RC13 uses testnet13 blockchain

---

## Downloads

| Platform | File |
|----------|------|
| Windows 64-bit (Installer) | `digibyte-9.26.0-rc13-win64-setup.exe` |
| Windows 64-bit (Portable) | `digibyte-9.26.0-rc13-win64.zip` |
| macOS Apple Silicon (M1/M2/M3/M4) | `digibyte-9.26.0-rc13-arm64-apple-darwin.tar.gz` |
| macOS Intel | `digibyte-9.26.0-rc13-x86_64-apple-darwin.tar.gz` |
| Linux x86_64 | `digibyte-9.26.0-rc13-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc13-aarch64-linux-gnu.tar.gz` |

---

## Feedback & Community

Please report issues and feedback to help us prepare for mainnet launch.

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues
