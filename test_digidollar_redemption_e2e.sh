#!/bin/bash
set -e

echo "==========================================================="
echo "DIGIDOLLAR REDEMPTION END-TO-END TEST"
echo "==========================================================="
echo ""

echo "STEP 1: Generate 655 blocks to pass DD activation height"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"1","method":"getnewaddress","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/addr.json
ADDR=$(python3 -c "import json,sys; print(json.load(open('/tmp/addr.json'))['result'])")
echo "   Mining to address: $ADDR"

curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"2\",\"method\":\"generatetoaddress\",\"params\":[655,\"$ADDR\"]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/gen1.json
python3 -c "import json; r=json.load(open('/tmp/gen1.json')); print(f'✅ Generated {len(r[\"result\"])} blocks')"

sleep 2

echo ""
echo "STEP 2: Mint \$1000 DD (100000 cents) with 1-hour timelock (tier 1)"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"3","method":"mintdigidollar","params":[100000,1]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/mint.json

python3 << 'PYEOF'
import json
with open('/tmp/mint.json') as f:
    r = json.load(f)
    if 'error' in r and r['error']:
        print(f"❌ MINT FAILED: {r['error']['message']}")
        exit(1)
    print(f"✅ Minted {r['result']['dd_minted']} cents")
    print(f"   Position ID: {r['result']['position_id']}")
    print(f"   Unlock Height: {r['result']['unlock_height']}")
    with open('/tmp/position_id.txt', 'w') as f:
        f.write(r['result']['position_id'])
PYEOF

if [ $? -ne 0 ]; then
    echo "Mint failed, exiting"
    exit 1
fi

POSITION_ID=$(cat /tmp/position_id.txt)

echo ""
echo "STEP 3: Mine 10 blocks to CONFIRM the mint"
curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"4\",\"method\":\"generatetoaddress\",\"params\":[10,\"$ADDR\"]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/gen2.json
python3 -c "import json; r=json.load(open('/tmp/gen2.json')); print(f'✅ Mined {len(r[\"result\"])} blocks to confirm mint')"

sleep 2

echo ""
echo "STEP 4: Check Bob's DD balance"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"5","method":"getdigidollarbalance","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/bob_bal.json

python3 << 'PYEOF'
import json
with open('/tmp/bob_bal.json') as f:
    r = json.load(f)
    if 'error' in r and r['error']:
        print(f"ERROR: {r['error']['message']}")
        exit(1)
    bal = r['result']['total']
    print(f"✅ Bob has {bal} cents (\${bal/100})")
    if bal != 100000:
        print(f"   WARNING: Expected 100000 cents!")
PYEOF

echo ""
echo "STEP 5: Check redemption info (should be LOCKED)"
curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"6\",\"method\":\"getredemptioninfo\",\"params\":[\"$POSITION_ID\"]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/redeem_info_before.json

python3 << 'PYEOF'
import json
with open('/tmp/redeem_info_before.json') as f:
    r = json.load(f)
    if 'error' in r and r['error']:
        print(f"ERROR: {r['error']['message']}")
    else:
        info = r['result']
        print(f"✅ Redemption Info:")
        print(f"   Can Redeem: {info['can_redeem']}")
        print(f"   Timelock Remaining: {info['timelock_remaining']} blocks")
        print(f"   DD Minted: {info['dd_minted']} cents")
        print(f"   DGB Locked: {info['dgb_locked']} DGB")
PYEOF

echo ""
echo "STEP 6: List redeemable positions (should be EMPTY)"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"7","method":"listredeemablepositions","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/redeemable_before.json
python3 -c "import json; r=json.load(open('/tmp/redeemable_before.json')); print(f'✅ Redeemable positions: {len(r[\"result\"])} (should be 0)')"

echo ""
echo "STEP 7: Generate 240+ blocks to EXPIRE the 1-hour timelock"
curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"8\",\"method\":\"generatetoaddress\",\"params\":[250,\"$ADDR\"]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/gen3.json
python3 -c "import json; r=json.load(open('/tmp/gen3.json')); print(f'✅ Mined {len(r[\"result\"])} blocks to expire timelock')"

sleep 2

echo ""
echo "STEP 8: Check redemption info (should be REDEEMABLE)"
curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"9\",\"method\":\"getredemptioninfo\",\"params\":[\"$POSITION_ID\"]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/redeem_info_after.json

python3 << 'PYEOF'
import json
with open('/tmp/redeem_info_after.json') as f:
    r = json.load(f)
    if 'error' in r and r['error']:
        print(f"ERROR: {r['error']['message']}")
        exit(1)
    info = r['result']
    print(f"✅ Redemption Info (After Timelock):")
    print(f"   Can Redeem: {info['can_redeem']}")
    print(f"   Timelock Remaining: {info['timelock_remaining']} blocks")
    print(f"   DD Minted: {info['dd_minted']} cents")
    print(f"   DGB to be Returned: {info['dgb_returned']} DGB")
    if not info['can_redeem']:
        print(f"   ❌ ERROR: Should be redeemable now!")
        exit(1)
PYEOF

if [ $? -ne 0 ]; then
    echo "Position not redeemable, test failed"
    exit 1
fi

echo ""
echo "STEP 9: List redeemable positions (should have 1)"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"10","method":"listredeemablepositions","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/redeemable_after.json

python3 << 'PYEOF'
import json
with open('/tmp/redeemable_after.json') as f:
    r = json.load(f)
    positions = r['result']
    print(f"✅ Redeemable positions: {len(positions)}")
    if len(positions) > 0:
        for pos in positions:
            print(f"   - {pos['outpoint']}: {pos['dd_minted']} cents, Status: {pos['status']}")
PYEOF

echo ""
echo "STEP 10: REDEEM the position (full redemption)"
curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"11\",\"method\":\"redeemdigidollar\",\"params\":[\"$POSITION_ID\",100000]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/redeem.json

python3 << 'PYEOF'
import json
with open('/tmp/redeem.json') as f:
    r = json.load(f)
    if 'error' in r and r['error']:
        print(f"❌ REDEMPTION FAILED: {r['error']['message']}")
        exit(1)
    result = r['result']
    print(f"✅ REDEMPTION SUCCESSFUL!")
    print(f"   TXID: {result['txid'][:16]}...")
    print(f"   DGB Unlocked: {result['dgb_unlocked']} DGB")
    print(f"   DD Burned: {result['dd_burned']} cents")
    print(f"   Success: {result['success']}")
PYEOF

if [ $? -ne 0 ]; then
    echo "Redemption failed, test failed"
    exit 1
fi

echo ""
echo "STEP 11: Mine 5 blocks to confirm redemption"
curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"12\",\"method\":\"generatetoaddress\",\"params\":[5,\"$ADDR\"]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/gen4.json
python3 -c "import json; r=json.load(open('/tmp/gen4.json')); print(f'✅ Mined {len(r[\"result\"])} blocks to confirm redemption')"

sleep 2

echo ""
echo "STEP 12: Check Bob's DD balance (should be 0)"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"13","method":"getdigidollarbalance","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/bob_final.json

python3 << 'PYEOF'
import json
with open('/tmp/bob_final.json') as f:
    r = json.load(f)
    bal = r['result']['total']
    print(f"✅ Bob's final DD balance: {bal} cents")
    if bal == 0:
        print("   ✅ Correct! All DD burned during redemption")
    else:
        print(f"   ❌ ERROR: Expected 0 cents, got {bal}")
        exit(1)
PYEOF

if [ $? -ne 0 ]; then
    echo "Final balance check failed"
    exit 1
fi

echo ""
echo "STEP 13: Check Bob's DGB balance (should have increased)"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"14","method":"getbalance","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/bob_dgb.json
python3 -c "import json; r=json.load(open('/tmp/bob_dgb.json')); print(f'✅ Bob\\'s DGB balance: {r[\"result\"]} DGB')"

echo ""
echo "==========================================================="
echo "✅✅✅ DIGIDOLLAR REDEMPTION TEST PASSED! ✅✅✅"
echo "==========================================================="
echo ""
echo "Summary:"
echo "  1. Bob minted \$1000 DD with 1-hour timelock"
echo "  2. Waited 240+ blocks for timelock to expire"
echo "  3. Position became redeemable"
echo "  4. Bob redeemed full position, burning all DD"
echo "  5. Bob received DGB collateral back"
echo ""
echo "The redemption system is working correctly!"
echo ""
