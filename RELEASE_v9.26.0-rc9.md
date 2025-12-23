**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im - Active development discussion happens here!

---

## What's New in RC9

### Critical Bug Fixes
- **Fix multi-input DD redemption** - Received DD now properly registered in metadata for cross-wallet redemptions
- **Fix critical DD change loss bug** - Redemption transactions now correctly preserve DD change
- **Fix wallet restore tier mismatch** - Lock tier explicitly stored in MINT OP_RETURN

### Coin Control & UI Improvements
- **DD Coin Control for Redemptions** - Manual UTXO selection in Redeem DD tab
- **DD Coin Control for Sends** - Manual UTXO selection for DigiDollar transfers
- **QR Code Popup Dialog** - Theme-styled QR codes for DD Receive tab
- **3-second Confirmation Delay** - Safety delay for DD send transactions
- **Clean DGB/DD Address Separation** - DD addresses no longer appear in DGB Receive tab

### Architecture Improvements
- **Decentralized DD Amount Lookup** - Fallback via blockchain/txindex for DD amounts
- **DCA System GREEN Phase** - Core Dynamic Collateral Adjustment functions implemented
- **DIGIDOLLAR Address Purpose** - Clean separation of DGB and DD address types

### Testing & Documentation
- **Comprehensive Validation Layer Tests** - GROUP 10 test coverage
- **Script Validation Tests** - GROUP 9 opcode order validation
- **ERR Tier Documentation** - GROUP 3 formula tests and clarification

---

## Commits Since RC8

- Fix multi-input DD redemption by registering received DD in metadata
- Fix DD amount extraction to be decentralized via blockchain lookup
- Add coin control UI to Redeem DD tab for manual DD input selection
- Add QR code popup dialog for DD Receive tab with theme styling
- Fix critical DD change loss bug in redemption transactions
- Store lock tier explicitly in MINT OP_RETURN to fix wallet restore tier mismatch
- Fix lock tier mismatch causing wallet restore position tier errors
- Implement DigiDollar Coin Control for manual UTXO selection
- Add 3-second confirmation delay for DigiDollar send transactions
- Fix DigiDollar addresses appearing in DGB Receive tab after wallet restart
- Add DIGIDOLLAR address purpose for clean DGB/DD separation
- Remove DD Vault Refresh Button (UI cleanup)
- GROUP 10: Add comprehensive validation layer tests
- GROUP 9: Add script validation tests for opcode order
- GROUP 3: Clarify ERR tier documentation and add formula tests
- GROUP 1: Implement DCA system GREEN phase functions
- Fix ValidateRedemptionAmount to reject partial redemptions
- Fix oracle exchange mock data and tests
- Fix DigiDollar unit test failures

---

## Upgrade Notes

**RC9 uses the same testnet10 network as RC8. No data deletion required if upgrading from RC8.**

### If Upgrading from RC7 or Earlier:
1. Close your old wallet
2. Delete old testnet data:
   - **Windows:** Delete `%APPDATA%\DigiByte\testnet9\`
   - **macOS:** Delete `~/Library/Application Support/DigiByte/testnet9/`
   - **Linux:** Delete `~/.digibyte/testnet9/`
3. Download and install RC9
4. Launch with `-testnet` flag

---

## Key Features Since RC1 (Cumulative)

### From RC8 - MAST Simplification
- Reduced to 2 redemption paths (Normal and ERR only)
- Removed Partial Redemption (users must redeem full vault amounts)
- Added CLTV to ERR path (both paths require timelock expiry)
- Fixed SignRedemptionTransaction MAST mismatch

### From RC7 - ERR & Oracle Fixes
- Corrected ERR semantics (increases DD burn, not reduces collateral)
- Multi-oracle consensus infrastructure
- Wallet restore fixes for redeemed vaults
- UI improvements for DD address filtering

### From RC6 - Network Reset & HD Keys
- Deterministic DigiDollar keys using HD derivation
- Tier derivation fixes for wallet restore
- UI improvements for Receive DD tab

### From RC5 - Wallet & Validation Fixes
- Conflicted DD transactions handled properly
- Minimum mint amount rule with activation height
- Historical block validation optimization
- Cross-platform theming fixes

### From RC4 - Network Reset & Core Fixes
- Fix Qt freeze from verbose logging
- Fix large DD amounts (8-byte CScriptNum)
- DigiDollarStatsIndex for network metrics
- Exact-amount redemptions

### From RC3 - Wallet Safety Fixes
- DGB loss bug fixed with wallet-controlled change addresses
- Non-P2TR outputs allowed for change
- Windows 64-bit integer fixes

### From RC2 - Core Transaction Fixes
- DD key persistence across wallet restarts
- Taproot signing fixed for transfers and redemptions
- UTXO scanning fixed for received DD tokens

---

## What is DigiDollar?

DigiDollar is a USD-pegged stablecoin built natively into DigiByte. It uses an over-collateralized model where users lock DGB to mint DUSD at the current oracle price of DGB.

The world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

DGB becomes the strategic reserve asset (21B max, only 2.23 DGB per person on Earth right now). Everything happens inside DigiByte Core wallet. You never give up control of your private keys. No centralized company, fund or pool. Pure decentralization.

**Learn more:** https://digibyte.io/digidollar

## Current Status

- **Single Oracle Testing** - This beta uses one oracle node for price feeds. Production will use a decentralized oracle network.
- **Testnet Only** - All DGB and DUSD on testnet have no real value.

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
Download `digibyte-9.26.0-rc9-win64-setup.exe` and install normally.

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
- Title bar should say **"DigiByte Core - Wallet [testnet10]"**
- You should see a **DigiDollar** tab in the sidebar

### Data Directory Reference
- Config: `%APPDATA%\DigiByte\digibyte.conf`
- Testnet data: `%APPDATA%\DigiByte\testnet10\`

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
- Title bar should say **"DigiByte Core - Wallet [testnet10]"**
- You should see a **DigiDollar** tab in the sidebar

### Data Directory Reference
- Config: `~/Library/Application Support/DigiByte/digibyte.conf`
- Testnet data: `~/Library/Application Support/DigiByte/testnet10/`

---

## Ubuntu/Linux Setup

### Data Directory
```
~/.digibyte/
```
Config file: `~/.digibyte/digibyte.conf`

Testnet data stored in: `~/.digibyte/testnet10/`

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
tar xzf digibyte-9.26.0-rc9-x86_64-linux-gnu.tar.gz
./digibyte-9.26.0-rc9/bin/digibyte-qt
```

### Verify It's Working
- Title bar should say **"DigiByte Core - Wallet [testnet10]"**
- You should see a **DigiDollar** tab in the sidebar

---

## Getting Testnet DGB

### Option 1: GUI Console Mining

Mine testnet DGB directly using the GUI console:

1. Go to the **Receive** tab and create a new address (copy your `dgbt1...` address)
2. Go to **Window > Console**
3. Type: `generatetoaddress 1 dgbt1qYOURADDRESSHERE`
4. Press Enter to mine 1 block
5. Wait for 8 confirmations before spending mined coins (reduced from 100 in testnet10)

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
- `-a sha256d` - Use SHA256d algorithm (matches `algo=sha256d` in config)
- `-o http://127.0.0.1:14026` - RPC endpoint (testnet RPC port)
- `-O digibyte:digibyte123` - RPC credentials from your config (user:password)
- `-t 4` - Number of CPU threads to use (adjust based on your CPU)
- `--coinbase-addr=dgbt1q...` - Your testnet receive address

The miner will continuously mine blocks and send rewards to your address.

---

## Testing DigiDollar Features

Once your wallet is synced and you have testnet DGB:

1. **View Network Status** - The DigiDollar Overview tab shows oracle price and network collateralization
2. **Mint DUSD** - Lock DGB as collateral to create DigiDollars
3. **Send DUSD** - Transfer DigiDollars to other testnet addresses
4. **Redeem DUSD** - Burn DigiDollars to unlock your DGB collateral (full position only)
5. **View History** - New DD Transactions tab shows complete transaction history
6. **Coin Control** - Use manual DD input selection for advanced redemptions

---

## Network Information

| Setting | Value |
|---------|-------|
| Network | Testnet (testnet10) |
| Default P2P Port | 12032 |
| Default RPC Port | 14026 |
| Oracle Node | oracle1.digibyte.io:12032 |
| Address Prefix | dgbt1... (bech32) |

### Fork Schedule (Testnet10)
| Feature | Block Height |
|---------|--------------|
| MultiAlgo | 100 |
| MultiShield | 200 |
| DigiSpeed | 400 |
| Odocrypt | 500 |
| DigiDollar | 550 |

### Emission Schedule (Testnet10)
| Period | Block Range | Reward |
|--------|-------------|--------|
| Period I | 0-66 | 72,000 DGB |
| Period IV | 67-199 | 8,000 DGB (with decay) |
| Period V | 200-399 | 2,459 DGB (with decay) |
| Period VI | 400+ | 1,078.5 DGB (with decay) |

---

## Troubleshooting

### "DigiDollar tab not appearing"
- Verify `digidollar=1` is under `[test]` section in config
- Verify `testnet=1` is at the top of config (not under any section)
- Restart the wallet after config changes

### "Not connecting to network"
- Check your firewall allows port 12032
- Verify `addnode=oracle1.digibyte.io` is under `[test]` in config

### "Oracle price shows 0 or N/A"
- Wait for sync to complete
- The oracle broadcasts price updates every few minutes
- Check Window > Console: `getoracleprice`

### "Mining not working"
- Ensure `algo=sha256d` is in your config under `[test]`
- SHA256d is recommended for fastest CPU mining

### "Transaction fee too high"
- DigiDollar transactions require 0.1 DGB minimum fee - this is expected
- This ensures reliable network propagation

### "Old testnet9 data causing crashes"
- Delete your testnet9 folder completely (see Upgrade Notes above)
- RC8+ requires a fresh testnet10 blockchain

---

## Downloads

| Platform | File |
|----------|------|
| Windows 64-bit (Installer) | `digibyte-9.26.0-rc9-win64-setup.exe` |
| Windows 64-bit (Portable) | `digibyte-9.26.0-rc9-win64.zip` |
| macOS Apple Silicon (M1/M2/M3/M4) | `digibyte-9.26.0-rc9-arm64-apple-darwin.dmg` |
| macOS Intel | `digibyte-9.26.0-rc9-x86_64-apple-darwin.dmg` |
| Linux x86_64 | `digibyte-9.26.0-rc9-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc9-aarch64-linux-gnu.tar.gz` |

---

## Feedback & Community

Please report issues and feedback to help us prepare for mainnet launch.

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues
