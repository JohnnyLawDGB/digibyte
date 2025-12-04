#!/bin/bash
# Finish Oracle Setup - Run this after the deploy script times out but node is running

CLI="sudo /root/digibyte/src/digibyte-cli -datadir=/root/.digibyte-testnet"

echo "Checking node status..."
$CLI getblockchaininfo | head -5

echo ""
echo "Creating wallet..."
$CLI createwallet oracle_wallet 2>/dev/null || $CLI loadwallet oracle_wallet 2>/dev/null || echo "Wallet already loaded"

echo ""
echo "Getting mining address..."
ADDRESS=$($CLI getnewaddress)
echo "Mining Address: $ADDRESS"
sudo bash -c "echo '$ADDRESS' > /root/.digibyte-testnet/mining_address.txt"

echo ""
echo "Starting Oracle 0..."
$CLI startoracle 0 "0000000000000000000000000000000000000000000000000000000000000001"

echo ""
echo "Oracle status:"
$CLI getoracleinfo

echo ""
echo "Done! Oracle is running."
echo "To mine blocks: $CLI generatetoaddress 1 $ADDRESS"
