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
        "0000000000000000000000000000000000000000000000000000000000000001",
    ],
    "finish_oracle_setup.sh": [
        "getoracleinfo",
        "0000000000000000000000000000000000000000000000000000000000000001",
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
}

REQUIRED = {
    "deploy_testnet_oracle.sh": [
        "TESTNET_NAME=\"testnet26\"",
        "TESTNET_P2P_PORT=12033",
        "TESTNET_RPC_PORT=14026",
        "ORACLE_PRIVATE_KEY",
        "listoracle",
    ],
    "finish_oracle_setup.sh": [
        "-testnet",
        "ORACLE_PRIVATE_KEY",
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
