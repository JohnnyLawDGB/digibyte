STARTER PROMPT FOR A NEW CLAUDE CODE SESSION

You are working in the DigiByte Core repo at `~/Code/digibyte` on branch `feature/digidollar-v1`.

Code is truth. Docs are navigation aids. Scope is DigiDollar and DigiDollar-oracle only.

Before doing any work, read these files in this order:
1. `CLAUDE.md`
2. `ARCHITECTURE.md`
3. `REPO_MAP.md`
4. `REPO_MAP_GUIDE.md`
5. `DIGIDOLLAR_ARCHITECTURE.md`
6. `DIGIDOLLAR_ORACLE_ARCHITECTURE.md`
7. `REPO_MAP_DIGIDOLLAR.md`
8. `DIGIDOLLAR_EXPLAINER.md`
9. `DIGIDOLLAR_ORACLE_EXPLAINER.md`
10. `DIGIDOLLAR_ACTIVATION_EXPLAINER.md`
11. `DIGIDOLLAR_WALLET_INTEGRATION.md`
12. `DIGIDOLLAR_EXCHANGE_INTEGRATION.md`
13. `DIGIDOLLAR_OPRETURN_PQC_MINT_PLAN.md`
14. `ORACLE_DISCOVERY_ARCHITECTURE.md`
15. `DIGIDOLLAR_ORACLE_SETUP.md`
16. `docs/ORACLE_OPERATOR_GUIDE.md`
17. `digidollar/DIGIDOLLAR_FLOWCHART.md`
18. `digidollar/DIGIDOLLAR_ORACLE_PHASE_ONE_SPEC.md`
19. `digidollar/ORACLE_PHASE_2_SPEC_PRD.md`
20. `digidollar/4_tier_collateral.md`
21. `RELEASE_v9.26.0-rc31.md`

Then map the real DD/oracle surface before analyzing anything:
```bash
cd ~/Code/digibyte
find src/digidollar src/oracle src/wallet src/rpc src/qt src/test src/test/fuzz test/functional -type f \
  | grep -E 'digidollar|oracle|musig2|rh' | sort
```

Rules:
- DigiDollar and DigiDollar-oracle only. Do not wander into unrelated main DGB work.
- You may inspect shared files like `src/kernel/chainparams.cpp`, `src/validation.cpp`, `src/deploymentstatus.*`, wallet plumbing, RPC registration, or Qt model glue only when they directly affect DigiDollar or oracle behavior.
- Every claimed bug must be proven real in code with exact file and line references.
- Every fix must use TDD: failing test first, smallest fix second, full relevant reruns third.
- Never break existing functionality.
- Never push. Commit locally only if asked.

---

## PROMPT 1: DigiDollar Functional Bug Hunt, Wiring, Plumbing, and Coverage Hardening

You are the orchestrator for a **DigiDollar-only functional bug hunt** in `~/Code/digibyte` on branch `feature/digidollar-v1`.

Your goal is to harden **all DigiDollar and DigiDollar-oracle functionality** across:
- `src/digidollar/`
- `src/oracle/`
- DigiDollar/oracle slices of `src/rpc/`, `src/wallet/`, `src/qt/`, `src/kernel/`, `src/consensus/`, `src/validation.cpp`, `src/deploymentstatus.*`
- DigiDollar/oracle unit tests, functional tests, fuzz harnesses, wallet tests, Qt tests, and attack/regression tests already in tree

This is **not** a general DigiByte bug hunt. Ignore unrelated base-chain cleanup. Only touch shared core code when it is directly part of DigiDollar or oracle plumbing.

### Required reading for the main agent and every sub-agent
Read these first, in order, before code analysis:
1. `CLAUDE.md`
2. `ARCHITECTURE.md`
3. `REPO_MAP.md`
4. `REPO_MAP_GUIDE.md`
5. `DIGIDOLLAR_ARCHITECTURE.md`
6. `DIGIDOLLAR_ORACLE_ARCHITECTURE.md`
7. `REPO_MAP_DIGIDOLLAR.md`
8. `DIGIDOLLAR_EXPLAINER.md`
9. `DIGIDOLLAR_ORACLE_EXPLAINER.md`
10. `DIGIDOLLAR_ACTIVATION_EXPLAINER.md`
11. `DIGIDOLLAR_WALLET_INTEGRATION.md`
12. `DIGIDOLLAR_EXCHANGE_INTEGRATION.md`
13. `DIGIDOLLAR_OPRETURN_PQC_MINT_PLAN.md`
14. `ORACLE_DISCOVERY_ARCHITECTURE.md`
15. `DIGIDOLLAR_ORACLE_SETUP.md`
16. `docs/ORACLE_OPERATOR_GUIDE.md`
17. `digidollar/DIGIDOLLAR_FLOWCHART.md`
18. `digidollar/DIGIDOLLAR_ORACLE_PHASE_ONE_SPEC.md`
19. `digidollar/ORACLE_PHASE_2_SPEC_PRD.md`
20. `digidollar/4_tier_collateral.md`
21. `RELEASE_v9.26.0-rc31.md`

### Fleet and wave model
- Run **20 sequential waves**.
- Each wave launches **3 sub-agents in parallel**.
- Keep slices non-overlapping inside a wave.
- Be bold and creative about where bugs might hide, but disciplined about proof.
- Every bug must be validated as real in code before you call it a bug.
- Every accepted fix must land through TDD and must not break existing behavior.

### Binding workflow rules
1. Before each wave, enumerate the live DD/oracle surface from the repo, not from memory.
2. Before each wave, run the current DD/oracle test baseline.
3. Every sub-agent report must include:
   - exact `file:line`
   - why the behavior is wrong
   - a deterministic reproducer or failing test
   - why it is in DigiDollar/oracle scope
4. Reject any report that is only speculative.
5. For each validated bug, do this exact flow:
   - write or extend a test so it fails on the current bug
   - run it and confirm the failure is for the real bug
   - apply the smallest fix possible
   - rerun the new test and prove it passes
   - rerun the full relevant DD/oracle suites and prove no regression
6. If a bug touches shared plumbing, expand the rerun set to cover the affected shared surface.
7. Never refactor for style. Never broaden scope. Never break existing functionality.

### Minimum test surfaces you must keep exercising
Use the real file lists from the repo, but at minimum cover these families:
- Unit: `src/test/digidollar_*_tests.cpp`
- Unit: `src/test/oracle_*_tests.cpp`
- Unit: `src/test/musig2_*_tests.cpp`
- Unit/attack/regression: `src/test/rh*_tests.cpp` where the test is DigiDollar/oracle-related
- Wallet unit: `src/wallet/test/digidollar_*_tests.cpp`
- Qt unit: `src/qt/test/digidollarwidgettests.cpp`
- Fuzz: `src/test/fuzz/digidollar_*.cpp`
- Fuzz: `src/test/fuzz/oracle_*.cpp`
- Functional: `test/functional/digidollar_*.py`
- Functional: `test/functional/wallet_digidollar_*.py`
- Functional: `test/functional/feature_oracle_p2p.py`
- Functional: `test/functional/rpc_getoracles_pending.py`

### What to hunt
Hunt functional bugs in:
- mint, send, receive, redeem, wallet restore, rescans, encrypted wallet flows, watch-only flows
- collateral math, health tracking, DCA, ERR, volatility, state caching, reorg rollback
- txbuilder change/fee logic, OP_RETURN formats, address encoding, script metadata, opcode validation
- RPC command gating, JSON fields, units, error messages, wallet routing, deployment gating
- Qt model/view wiring, stale state, unit display mismatches, action enable/disable bugs, coin control, positions and transactions views
- oracle bundle creation, validation, roster alignment, config sanity, price freshness, exchange aggregation, P2P message flow, MuSig2 state transitions, bitmap/serialization/order mismatches
- missing or weak test coverage, especially where recent code landed without meaningful assertions

### 20-wave hunt plan
1. Baseline, inventory, and coverage-gap map
2. DigiDollar core structures, amounts, activation flags
3. Mint path and txbuilder collateral logic
4. Transfer path, change logic, and DD conservation
5. Redeem path, timelocks, ERR path, and collateral return
6. Script recognition, opcode checks, OP_RETURN formats, metadata registry
7. Health monitor, DCA, ERR, volatility, multi-block state
8. Activation, deployment gates, chainparams, shared validation hooks
9. Wallet persistence, restore, rescan, descriptors, backup, encryption
10. Wallet-to-RPC wiring and display/state bugs
11. RPC command input validation and output schema correctness
12. Qt overview, send, receive, mint, redeem, positions, transactions, coin control
13. Oracle bundle manager, threshold logic, config alignment, roster ordering
14. Oracle price feeds, staleness, aggregation math, fallback behavior
15. Oracle P2P messages, getoracles flow, pending bundle handling
16. MuSig2 session lifecycle, orchestrator, session manager, epoch transitions
17. MuSig2 aggregation, bitmaps, serialization, nonce and partial sig message flow
18. Functional test hardening, flake detection, restart/reorg/replay coverage
19. Fuzz audit and new fuzz/regression additions where coverage is weak
20. Final full rerun, cleanup of only validated backlog items, and summary

### Output contract for every wave
For each wave produce:
- validated bugs found
- tests added or strengthened
- fixes landed
- exact test commands run
- remaining hypotheses not yet proven
- explicit note that no unrelated main DGB code was changed

If a suspected issue is not proven in code, label it `hypothesis only` and do not fix it.

---

## PROMPT 2: Red Hornet, DigiDollar + Oracle Security Attack Hunt

You are the orchestrator for a **Red Hornet security campaign** against the DigiDollar and DigiDollar-oracle system in `~/Code/digibyte` on branch `feature/digidollar-v1`.

Your mission is to find and fix **real security issues** in DigiDollar only, not generic DigiByte. Think like an attacker, but only report attacks that are actually reachable in the current code. Be aggressive in imagination and conservative in claims.

Focus on these risk classes:
- inflation or unauthorized DigiDollar creation
- under-collateralized mint or incorrect collateral release
- invalid redemption or bypass of timelock / ERR rules
- consensus split risk in DD/oracle activation or validation
- oracle spoofing, stale-price acceptance, replay, bundle manipulation, roster mismatch
- MuSig2 session/serialization/bitmap/order attacks
- wallet corruption, restore loss, privacy leaks, or fund-accounting drift in DD flows
- RPC abuse, input-validation gaps, hidden unsafe code paths, UI deception that could cause loss
- mempool, reorg, cache, persistence, and DoS paths that materially affect DigiDollar safety

### Required reading for the main agent and every sub-agent
Read these first, in order, before code analysis:
1. `CLAUDE.md`
2. `ARCHITECTURE.md`
3. `REPO_MAP.md`
4. `REPO_MAP_GUIDE.md`
5. `DIGIDOLLAR_ARCHITECTURE.md`
6. `DIGIDOLLAR_ORACLE_ARCHITECTURE.md`
7. `REPO_MAP_DIGIDOLLAR.md`
8. `DIGIDOLLAR_EXPLAINER.md`
9. `DIGIDOLLAR_ORACLE_EXPLAINER.md`
10. `DIGIDOLLAR_ACTIVATION_EXPLAINER.md`
11. `DIGIDOLLAR_WALLET_INTEGRATION.md`
12. `DIGIDOLLAR_EXCHANGE_INTEGRATION.md`
13. `DIGIDOLLAR_OPRETURN_PQC_MINT_PLAN.md`
14. `ORACLE_DISCOVERY_ARCHITECTURE.md`
15. `DIGIDOLLAR_ORACLE_SETUP.md`
16. `docs/ORACLE_OPERATOR_GUIDE.md`
17. `digidollar/DIGIDOLLAR_FLOWCHART.md`
18. `digidollar/DIGIDOLLAR_ORACLE_PHASE_ONE_SPEC.md`
19. `digidollar/ORACLE_PHASE_2_SPEC_PRD.md`
20. `digidollar/4_tier_collateral.md`
21. `RELEASE_v9.26.0-rc31.md`

### Fleet and wave model
- Run **20 sequential waves**.
- Each wave launches **3 sub-agents in parallel**.
- Every wave should have 3 non-overlapping attack surfaces.
- Every exploit claim must be demonstrated with code evidence and a reproducer or failing regression test.
- Every real fix must follow TDD and must preserve intended DigiDollar behavior.

### Binding Red Hornet rules
1. Never confuse a scary idea with a real exploit. Prove reachability in the current code.
2. Every finding must include:
   - severity
   - exact `file:line`
   - exploit path or proof of reachability
   - minimal reproducer, failing regression test, or deterministic attack steps
   - expected secure behavior and why the current code violates it
3. Fixes must be surgical and test-first.
4. Add or strengthen attack tests whenever you close a bug.
5. If a case is only theoretical, log it as `theoretical / not yet reachable` and move on.
6. Stay inside DigiDollar/oracle scope unless a shared file directly gates the exploit.
7. Never break legitimate mint, transfer, redeem, wallet, RPC, Qt, oracle, or MuSig2 behavior while hardening.

### Minimum test surfaces you must use
- Existing DigiDollar attack and redteam unit tests
- Existing oracle and MuSig2 attack/security tests
- Existing DD/oracle functional tests
- Existing DD/oracle fuzz harnesses
- New regression tests for every confirmed vulnerability

### 20-wave Red Hornet campaign
1. Baseline security map, threat model, and known attack-test inventory
2. Inflation and supply-integrity attacks in mint and transfer flows
3. Collateral accounting, rounding, overflow, and redemption-return abuse
4. Timelock, ERR, DCA, and volatility bypass attempts
5. Script-template confusion, opcode abuse, and malformed OP_RETURN attacks
6. Activation and deployment-gate consensus-split risks
7. Reorg, replay, multiblock ordering, and rollback-state exploits
8. Mempool relay, admission, replacement, and conflict attacks on DD tx paths
9. Wallet restore, rescan, persistence, backup, and descriptor attacks
10. Encrypted wallet, watch-only, and key-handling abuse
11. RPC input-validation, wallet routing, schema confusion, and hidden dangerous paths
12. Qt misrepresentation, stale state, dangerous UX mismatches, and loss-of-funds UI bugs
13. Oracle roster mismatch, threshold bypass, stale data, and config corruption
14. Oracle exchange feed manipulation, outlier poisoning, and fallback abuse
15. Oracle P2P message forgery, replay, flooding, and pending-bundle attacks
16. MuSig2 session poisoning, epoch confusion, bitmap/order mismatch, and nonce misuse
17. MuSig2 serialization, partial signature, aggregate verification, and replay protection
18. Cache, persistence, logging hot-path, and resource-exhaustion / DoS attacks
19. Fuzz-driven adversarial harness expansion and invariant hardening
20. Final exploit-chain sweep, full rerun, severity summary, and remaining theoretical risks

### Red Hornet output contract for every wave
For each wave produce:
- confirmed vulnerabilities
- rejected false positives
- tests added or upgraded
- fixes landed
- exact commands run
- remaining theoretical risks
- explicit note that scope stayed within DigiDollar/oracle

If a vulnerability is not reproducible in code, do not count it as real.
