#!/bin/bash
set -e

echo "==============================================="
echo "QT DIGIDOLLAR END-TO-END TEST - CORRECT ORDER"
echo "==============================================="
echo ""

echo "STEP 1: Generate 655 blocks to pass DD activation height"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"1","method":"getnewaddress","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/addr.json
ADDR=$(python3 -c "import json,sys; print(json.load(open('/tmp/addr.json'))['result'])")
echo "   Mining to address: $ADDR"

curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"2\",\"method\":\"generatetoaddress\",\"params\":[655,\"$ADDR\"]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/gen1.json
python3 -c "import json; r=json.load(open('/tmp/gen1.json')); print(f'✅ Generated {len(r[\"result\"])} blocks')"

sleep 2

echo ""
echo "STEP 2: Mint \$100 DD (10000 cents) for Bob"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"3","method":"mintdigidollar","params":[10000,3]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/mint.json

python3 << 'PYEOF'
import json
with open('/tmp/mint.json') as f:
    r = json.load(f)
    if 'error' in r and r['error']:
        print(f"❌ MINT FAILED: {r['error']['message']}")
        exit(1)
    print(f"✅ Minted {r['result']['dd_minted']} cents")
    print(f"   TXID: {r['result']['txid'][:16]}...")
    with open('/tmp/mint_txid.txt', 'w') as f:
        f.write(r['result']['txid'])
PYEOF

if [ $? -ne 0 ]; then
    echo "Mint failed, exiting"
    exit 1
fi

MINT_TXID=$(cat /tmp/mint_txid.txt)

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
    if bal != 10000:
        print(f"   WARNING: Expected 10000 cents!")
PYEOF

echo ""
echo "STEP 5: Create Alice wallet"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"6","method":"createwallet","params":["Alice"]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/ > /tmp/alice_create.json
python3 -c "import json; r=json.load(open('/tmp/alice_create.json')); print(f'✅ Created {r[\"result\"][\"name\"]}' if 'result' in r else '✅ Already exists')"

sleep 1

echo ""
echo "STEP 6: Get Alice's DD address"
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"7","method":"getdigidollaraddress","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Alice > /tmp/alice_addr.json
ALICE_ADDR=$(python3 -c "import json; print(json.load(open('/tmp/alice_addr.json'))['result'])")
echo "   Alice DD address: $ALICE_ADDR"

echo ""
echo "STEP 7: Send \$75 DD (7500 cents) from Bob to Alice"
curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"8\",\"method\":\"senddigidollar\",\"params\":[\"$ALICE_ADDR\",7500]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/send.json

python3 << 'PYEOF'
import json
with open('/tmp/send.json') as f:
    r = json.load(f)
    if 'error' in r and r['error']:
        print(f"❌ SEND FAILED: {r['error']['message']}")
        exit(1)
    print(f"✅ SEND SUCCESSFUL!")
    print(f"   TXID: {r['result'][:16]}...")
PYEOF

if [ $? -ne 0 ]; then
    echo ""
    echo "TEST FAILED AT SEND STEP"
    exit 1
fi

echo ""
echo "STEP 8: Mine 5 blocks to confirm transfer"
curl --silent --user test:test --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"9\",\"method\":\"generatetoaddress\",\"params\":[5,\"$ADDR\"]}" -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/gen3.json
python3 -c "import json; r=json.load(open('/tmp/gen3.json')); print(f'✅ Mined {len(r[\"result\"])} blocks to confirm transfer')"

sleep 2

echo ""
echo "==============================================="
echo "FINAL BALANCES"
echo "==============================================="

curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"10","method":"getdigidollarbalance","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Bob > /tmp/bob_final.json
curl --silent --user test:test --data-binary '{"jsonrpc":"1.0","id":"11","method":"getdigidollarbalance","params":[]}' -H 'content-type:text/plain;' http://127.0.0.1:18443/wallet/Alice > /tmp/alice_final.json

python3 << 'PYEOF'
import json

with open('/tmp/bob_final.json') as f:
    bob = json.load(f)['result']['total']

with open('/tmp/alice_final.json') as f:
    alice = json.load(f)['result']['total']

print(f"Bob:   {bob} cents (\${bob/100})")
print(f"Alice: {alice} cents (\${alice/100})")
print("")

if bob == 2500 and alice == 7500:
    print("✅✅✅ QT DIGIDOLLAR END-TO-END TEST PASSED! ✅✅✅")
    print("")
    print("Bob minted \$100, sent \$75 to Alice, has \$25 remaining")
    print("Alice received \$75")
    print("")
    print("The txbuilder fix is working correctly in QT!")
else:
    print(f"❌ TEST FAILED: Wrong balances!")
    print(f"   Expected: Bob=2500, Alice=7500")
    print(f"   Got: Bob={bob}, Alice={alice}")
    exit(1)
PYEOF
