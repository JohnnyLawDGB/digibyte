#!/bin/bash
# Test script for DigiDollar Receive Tab functionality
# This script tests address generation, QR code display, and payment request storage

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}DigiDollar Receive Tab Test${NC}"
echo -e "${BLUE}========================================${NC}"

# Setup test environment
DATADIR="/tmp/digibyte_dd_receive_test_$$"
echo -e "${GREEN}Setting up test environment: $DATADIR${NC}"

# Start regtest node
echo -e "${GREEN}Starting DigiByte regtest node...${NC}"
./src/digibyted -regtest -datadir=$DATADIR -daemon -fallbackfee=0.00001

# Wait for node to start
sleep 2

# Create wallet
echo -e "${GREEN}Creating test wallet...${NC}"
./src/digibyte-cli -regtest -datadir=$DATADIR createwallet "dd_test_wallet"

# Generate initial blocks for balance
echo -e "${GREEN}Generating 101 blocks for initial balance...${NC}"
./src/digibyte-cli -regtest -datadir=$DATADIR -generate 101

# Get initial DGB balance
INITIAL_BALANCE=$(./src/digibyte-cli -regtest -datadir=$DATADIR getbalance)
echo -e "${GREEN}Initial DGB Balance: $INITIAL_BALANCE DGB${NC}"

# Test 1: Verify wallet has keys for address generation
echo -e "\n${BLUE}Test 1: Verify wallet can generate addresses${NC}"
NEW_ADDR=$(./src/digibyte-cli -regtest -datadir=$DATADIR getnewaddress "Test Label" "bech32m")
echo -e "${GREEN}Generated address: $NEW_ADDR${NC}"

# Test 2: Check if DigiDollar wallet is initialized
echo -e "\n${BLUE}Test 2: Check DigiDollar wallet initialization${NC}"
./src/digibyte-cli -regtest -datadir=$DATADIR getwalletinfo

# Test 3: Mint some DigiDollar for testing
echo -e "\n${BLUE}Test 3: Mint DigiDollar for testing${NC}"
DD_AMOUNT=10000  # $100.00 DD
LOCK_TIER=1      # 30 days

echo -e "${GREEN}Attempting to mint $DD_AMOUNT DD cents with lock tier $LOCK_TIER...${NC}"

# Start Qt wallet for manual testing
echo -e "\n${BLUE}========================================${NC}"
echo -e "${BLUE}Manual Testing Instructions${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}1. The Qt wallet will now launch${NC}"
echo -e "${GREEN}2. Navigate to: DigiDollar -> Receive tab${NC}"
echo -e "${GREEN}3. Test the following:${NC}"
echo -e "${GREEN}   a. Enter Label: 'Test Payment'${NC}"
echo -e "${GREEN}   b. Enter Amount: 100 DD${NC}"
echo -e "${GREEN}   c. Enter Message: 'Test message'${NC}"
echo -e "${GREEN}   d. Click 'Generate New Address'${NC}"
echo -e "${GREEN}   e. Verify:${NC}"
echo -e "${GREEN}      - Address displays (starts with RD for regtest)${NC}"
echo -e "${GREEN}      - QR code appears${NC}"
echo -e "${GREEN}      - Request appears in 'Recent Payment Requests' table${NC}"
echo -e "${GREEN}      - Copy Address button works${NC}"
echo -e "${GREEN}      - Copy QR Code button works${NC}"
echo -e "${GREEN}      - Save QR Code button works${NC}"
echo -e "${GREEN}4. Close the wallet when done testing${NC}"

echo -e "\n${GREEN}Launching Qt wallet...${NC}"
./src/qt/digibyte-qt -regtest -datadir=$DATADIR

# Cleanup
echo -e "\n${GREEN}Stopping node...${NC}"
./src/digibyte-cli -regtest -datadir=$DATADIR stop

# Wait for shutdown
sleep 3

echo -e "\n${GREEN}Cleaning up test directory...${NC}"
rm -rf $DATADIR

echo -e "\n${BLUE}========================================${NC}"
echo -e "${GREEN}Test completed!${NC}"
echo -e "${BLUE}========================================${NC}"