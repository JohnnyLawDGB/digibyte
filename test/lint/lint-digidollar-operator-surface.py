#!/usr/bin/env python3
#
# Copyright (c) 2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Check DigiDollar operator-facing scripts/docs against current testnet/RPC surface."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

FORBIDDEN = {
    "deploy_testnet_oracle.sh": [
        "12028",
        "14028",
        "testnet5",
        "getoracleinfo",
        "ORACLE_PRIVATE_KEY",
        "<assigned_private_key_hex>",
        "0000000000000000000000000000000000000000000000000000000000000001",
    ],
    "docs/ORACLE_OPERATOR_GUIDE.md": [
        "raw_hex_private_key",
    ],
    "src/rpc/digidollar.cpp": [
        "Provide private_key parameter",
        "Oracle private key must be provided",
        "your_private_key_hex",
    ],
    "build_digibyte_ubuntu.sh": [
        "Testnet P2P:  12028",
        "RPC: 14028",
        "~/.digibyte/testnet5",
    ],
    "ORACLE_BUNDLE_EXPLAINER.md": [
        "getoracleinfo",
        "listoraclekeys",
    ],
    "WALLET_MIGRATION_GUIDE.md": [
        "future testnet resets",
        "17-oracle list",
        "**rc30 (current)**",
    ],
    "docs/DIGIDOLLAR_TESTNET_ORACLE_SETUP.md": [
        "oracle=1",
        "oracleid=<your-slot-id>",
    ],
    "finish_oracle_setup.sh": [
        "getoracleinfo",
        "WALLET_NAME=\"${WALLET_NAME:-oracle_wallet}\"",
        "ORACLE_PRIVATE_KEY",
        "<assigned_private_key_hex>",
        "echo \"Oracle status:\"\n$CLI listoracle",
        "0000000000000000000000000000000000000000000000000000000000000001",
    ],
    "test_multi_oracle_testnet.sh": [
        "startoracle 0  \"$ORACLE_KEY_0\"  2>/dev/null || true",
        "mv \"$BOB_DATADIR/$TESTNET_SUBDIR/wallets/bob_original_backup\" \"$BOB_DATADIR/$TESTNET_SUBDIR/wallets/bob\"",
        "trap \"kill $BOB_PID",
        "pkill -f \"digibyte-qt.*testnet\"",
        "pkill -9 -f \"digibyte-qt.*testnet\"",
        "Below threshold rejection - 6-of-21 (Step 27B)",
        "Median filter with live exchange outlier evidence (Step 27C)",
        "Oracle recovery after disagreement (Step 27D)",
        "BOB_POSITIONS_BEFORE_EXPORT=$($BOB_CLI -rpcwallet=bob listdigidollarpositions 2>/dev/null | jq 'length')",
        "BOB_RESTORED_POSITIONS=$($BOB_CLI -rpcwallet=bob_restored listdigidollarpositions 2>/dev/null | jq 'length')",
        "select(.is_active == false)",
        "select(.is_active == true)",
    ],
}

REQUIRED = {
    "finish_oracle_setup.sh": [
        "-testnet",
        "WALLET_NAME=\"${WALLET_NAME:-Oracle_Seed}\"",
        "listoracle",
    ],
    "build_digibyte_ubuntu.sh": [
        "Testnet P2P:  12033",
        "RPC: 14026",
        "~/.digibyte/testnet26",
    ],
    "ORACLE_BUNDLE_EXPLAINER.md": [
        "getoraclesigners",
        "listoracle",
    ],
    "WALLET_MIGRATION_GUIDE.md": [
        "ARCHIVED RC30-ONLY",
        "Do not use this guide for RC44/testnet26",
        "DIGIDOLLAR_ORACLE_SETUP.md",
    ],
    "docs/DIGIDOLLAR_TESTNET_ORACLE_SETUP.md": [
        "digidollar=1",
        "createoraclekey <oracle_id>",
        "startoracle <oracle_id>",
    ],
    "deploy_testnet_oracle.sh": [
        "TESTNET_NAME=\"testnet26\"",
        "TESTNET_P2P_PORT=12033",
        "TESTNET_RPC_PORT=14026",
        "TESTNET_GENESIS_HASH=\"0135174514d831ecc687a15e1ae31164bebf92d58bf279ab929226b8470b1dd3\"",
        "createoraclekey \"$ORACLE_ID\"",
        "install -d -m 700 \"$DATA_DIR\"",
        "chmod 600 \"$CONFIG_FILE\"",
        "ACTUAL_GENESIS=\\$(\\$CLI getblockhash 0 2>/dev/null || true)",
        "listoracle",
    ],
    "test_multi_oracle_testnet.sh": [
        "WARN_TESTS=0",
        "ORACLE_CACHE_REBUILT=false",
        "stop_existing_harness_processes",
        "cleanup_qt_nodes()",
        "start_oracle_checked",
        "bob_hash=$($BOB_CLI getbestblockhash",
        "[ \"$bob_hash\" = \"$alice_hash\" ]",
        "restorewallet \"bob\" \"$BACKUP_FILE\"",
        "if [ \"$HEARTBEAT_COUNT\" -eq 21 ]",
        "Manual oracle injection RPC removed (Step 27B)",
        "Live exchange outlier filter observation is optional",
        "BOB_POSITIONS_BEFORE_EXPORT=$($BOB_CLI -rpcwallet=bob listdigidollarpositions false",
        "BOB_RESTORED_POSITIONS=$($BOB_CLI -rpcwallet=bob_restored listdigidollarpositions false",
        "Bob restore fixture has no redeemed positions to validate",
        "def dd_active:",
        "Redeemed-position recovery coverage was incomplete",
        "DD transaction history restored partially; see category counts above",
    ],
}


def main() -> None:
    failures = []
    for relpath, needles in FORBIDDEN.items():
        text = (ROOT / relpath).read_text(encoding="utf8")
        for needle in needles:
            if needle in text:
                failures.append(f"{relpath}: forbidden stale operator surface `{needle}`")

    for relpath, needles in REQUIRED.items():
        text = (ROOT / relpath).read_text(encoding="utf8")
        for needle in needles:
            if needle not in text:
                failures.append(f"{relpath}: missing current operator surface `{needle}`")

    if failures:
        raise SystemExit("\n".join(failures))


if __name__ == "__main__":
    main()
