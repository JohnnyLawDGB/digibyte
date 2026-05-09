## Mission

Bring DigiByte Core `feature/digidollar-v1` to **RC34 release readiness**.

RC34 must be **mainnet-grade**: clean commit history, no unexplained dirty code, all release tests passing, and the live multi-oracle testnet script running end-to-end. The endpoint is clear: RC34 is ready for Jared to review, tag, release, and deploy on testnet as the final proving step toward mainnet.

The tests are not paperwork. They are how you find real functionality bugs. Use them to expose regressions, validate whether the failure is a real implementation bug or a test/harness issue, then fix the correct thing without damaging the architecture.

## Most Important Gate

`./test_multi_oracle_testnet.sh` is the most important RC34 gate.

It exercises the whole DigiDollar ecosystem together: live oracle price data, MuSig2 multi-oracle consensus, mining, minting, redemption, transfer chains, wallet persistence, restart/rescan/reindex behavior, and end-to-end testnet behavior. It is good at exposing regressions that isolated unit tests miss.

Run it early after build to discover ecosystem failures. Get it passing end-to-end. Then run the full unit/functional/fuzz suites to prove the fixes did not break anything else. Finally rerun the multi-oracle script again as the final ecosystem proof.

Partial script progress is not a pass.

## Hard Rules

- Do **not** push to GitHub. Local commits only; Jared reviews and pushes.
- Do **not** make major architecture changes without discussing them with Jared first.
- Do **not** weaken consensus, oracle, wallet, or test coverage just to get green tests.
- Do **not** fake oracle prices or reintroduce mock/fallback production price paths.
- Do **not** assume a failure is “just a test issue.” Prove it.
- Do **not** leave dirty source changes unexplained.
- Keep working until every Definition of Done item passes or a real blocker is documented.

## Required Reading Before Code Changes

The main agent and every sub-agent must read these before touching code:

1. `ARCHITECTURE.md`
2. `REPO_MAP.md`
3. `DIGIDOLLAR_ARCHITECTURE.md`
4. `DIGIDOLLAR_EXPLAINER.md`
5. `DIGIDOLLAR_ORACLE_ARCHITECTURE.md`
6. `DIGIDOLLAR_ORACLE_EXPLAINER.md`
7. `REPO_MAP_DIGIDOLLAR.md`
8. `doc/DIGIDOLLAR_ORACLE_TESTING_GUIDE.md` if relevant

Each sub-agent must confirm it read these and state the subsystem it owns.

## Architecture Approval Required

Stop and ask Jared before changing any of this:

- DigiDollar consensus or economic rules
- activation heights, BIP9 behavior, chainparams, network params
- oracle quorum, roster, bitmap, epoch, timestamp, serialization, or v0x03 format
- MuSig2 nonce, partial signature, aggregate signature, or P2P authentication rules
- whether DD mint/redeem blocks require oracle bundles
- `OP_CHECKPRICE` fail-closed/live-price behavior
- wallet DB schema or irreversible migration behavior
- anything that reduces what `test_multi_oracle_testnet.sh` proves

Small bug fixes, waits, logs, harness observability, and commit cleanup are fine only if they preserve architecture.

## Correct Fix Process

For every failure:

1. Name the exact failing command and failure point.
2. Reproduce narrowly when possible.
3. Validate the root cause:
   - real implementation bug
   - test expectation bug
   - harness timing/environment bug
   - architecture question requiring Jared
4. If architecture is involved, stop and ask Jared before changing code.
5. Fix the smallest correct thing in the spirit of DigiDollar V1.
6. Add or update targeted regression coverage when practical.
7. Rerun the targeted test.
8. Rerun the affected suite.
9. Rerun all final release gates.

A test/harness fix is allowed only when proven. Otherwise fix the implementation.

## Phase 1 — Preserve And Inspect

```bash
cd /home/jared/Code/digibyte
mkdir -p /tmp/rc34_baseline

git branch "backup/rc34-pre-cleanup-$(date +%Y%m%d-%H%M%S)"
git status --short | tee /tmp/rc34_baseline/git_status_short.txt
git diff --stat | tee /tmp/rc34_baseline/uncommitted_diffstat.txt
git diff > /tmp/rc34_baseline/uncommitted.patch
git log --reverse --date=short --pretty=format:'%h %ad %s' v9.26.0-rc33..HEAD \
  | tee /tmp/rc34_baseline/commits_since_rc33.txt
```

Every dirty file and every commit since RC33 must be reviewed. Do not discard anything unless it is proven wrong or obsolete.

Build first:

```bash
make -j"$(nproc)" 2>&1 | tee /tmp/rc34_baseline/build.log
```

## Phase 2 — Run The Ecosystem Gate First

Run the multi-oracle testnet script early because it exposes real system-level regressions:

```bash
./test_multi_oracle_testnet.sh \
  2>&1 | tee /tmp/rc34_baseline/test_multi_oracle_testnet_outer.log
```

Use the script failure to drive TDD. Do not paper over failures. If the script exposes a real code bug, fix the code. If it exposes a real harness bug, fix the harness and explain why.

## Phase 3 — Run Full Test Gates

After the multi-oracle script is passing, run the full suites to prove nothing else broke:

```bash
./src/test/test_digibyte --show_progress \
  2>&1 | tee /tmp/rc34_baseline/unit_test_digibyte.log

test/functional/test_runner.py --jobs=4 \
  2>&1 | tee /tmp/rc34_baseline/functional_test_runner.log
```

If extended functional tests are outside the default run:

```bash
test/functional/test_runner.py --jobs=4 --extended \
  2>&1 | tee /tmp/rc34_baseline/functional_test_runner_extended.log
```

Fuzz all registered targets. First detect targets and fuzz mode:

```bash
./src/test/fuzz/fuzz -help=1 2>&1 | tee /tmp/rc34_baseline/fuzz_help.txt || true
PRINT_ALL_FUZZ_TARGETS_AND_ABORT=1 ./src/test/fuzz/fuzz \
  2>/tmp/rc34_baseline/fuzz_target_list.stderr \
  | tee /tmp/rc34_baseline/fuzz_target_list.txt
```

Preferred fuzz gate with qa-assets corpus:

```bash
export FUZZ_CORPUS=/path/to/qa-assets/fuzz_seed_corpus
test/fuzz/test_runner.py -l INFO --par=4 "$FUZZ_CORPUS" \
  2>&1 | tee /tmp/rc34_baseline/fuzz_test_runner.log
```

If no corpus exists, rebuild with libFuzzer per `doc/fuzzing.md`, then exercise every target:

```bash
./autogen.sh
CC=clang CXX=clang++ ./configure --enable-fuzz --with-sanitizers=fuzzer,address,undefined
make -j"$(nproc)"
mkdir -p /tmp/rc34_fuzz_corpus
test/fuzz/test_runner.py -l INFO --par=4 --empty_min_time=30 /tmp/rc34_fuzz_corpus \
  2>&1 | tee /tmp/rc34_baseline/fuzz_test_runner.log
```

## Phase 4 — Use Up To Five Sub-Agents

Spawn only the sub-agents needed. Use up to five:

1. **History/Release Custodian** — clean commits since RC33, preserve backup/range-diff, ensure commit bodies explain what/why/tests/risk.
2. **DigiDollar Consensus/Miner** — validation, activation, mint/transfer/redeem/burn, collateral, lock-tier, miner behavior.
3. **Oracle/MuSig2/P2P** — live exchange price path, v0x03 bundles, quorum, bitmap, sessions, nonce/partial sig flow, price cache.
4. **Wallet/RPC/Qt/Testnet Harness** — balances, positions, list RPCs, restart/rescan/reindex/backup/restore, harness observability.
5. **Tests/Fuzz/Functional Infrastructure** — unit/functional/fuzz registration, regression tests, fixture isolation, full-suite reliability.

Sub-agent rules:

- Read required docs first.
- Own a concrete failing test or cleanup bucket.
- Use the Correct Fix Process above.
- Report exact rerun commands/results.
- Main agent integrates. Sub-agents do not independently rewrite final history.

## Phase 5 — Clean Commit History Since RC33

The final history must be logical and reviewable. Similar changes belong together. Squash/reword/reorder as needed.

Good final grouping examples:

- RC34 version/release metadata
- DigiDollar consensus and activation hardening
- Oracle/MuSig2 v0x03 and live-price hardening
- Wallet/RPC/Qt safety and persistence fixes
- Functional/testnet harness reliability
- Unit/regression tests
- Fuzz coverage
- Docs/release notes

Safe cleanup flow:

```bash
cd /home/jared/Code/digibyte
git branch "backup/rc34-before-history-surgery-$(date +%Y%m%d-%H%M%S)"
git diff > /tmp/rc34_baseline/pre_history_surgery_uncommitted.patch
```

Use interactive rebase or reset/rebuild, whichever preserves the truth best:

```bash
git rebase -i v9.26.0-rc33
# or, if the series is too tangled:
git reset --soft v9.26.0-rc33
```

After cleanup:

```bash
mkdir -p /tmp/rc34_final
git log --reverse --date=short --pretty=format:'%h %ad %s%n%b%n---' v9.26.0-rc33..HEAD \
  | tee /tmp/rc34_final/cleaned_commits_since_rc33.txt

git range-diff v9.26.0-rc33..<backup-branch> v9.26.0-rc33..HEAD \
  | tee /tmp/rc34_final/range_diff_vs_backup.txt
```

## Phase 6 — Final Release Gates

From the cleaned final branch, run everything again. This is the final proof:

```bash
cd /home/jared/Code/digibyte
mkdir -p /tmp/rc34_final

git status --short | tee /tmp/rc34_final/git_status_short.txt
make -j"$(nproc)" 2>&1 | tee /tmp/rc34_final/build.log
./src/test/test_digibyte --show_progress 2>&1 | tee /tmp/rc34_final/unit_test_digibyte.log
test/functional/test_runner.py --jobs=4 2>&1 | tee /tmp/rc34_final/functional_test_runner.log
```

Run extended functional tests if applicable:

```bash
test/functional/test_runner.py --jobs=4 --extended \
  2>&1 | tee /tmp/rc34_final/functional_test_runner_extended.log
```

Run exactly one valid fuzz gate and state which one was used:

```bash
# Populated corpus gate
test/fuzz/test_runner.py -l INFO --par=4 "$FUZZ_CORPUS" \
  2>&1 | tee /tmp/rc34_final/fuzz_test_runner.log

# OR libFuzzer empty-corpus gate
test/fuzz/test_runner.py -l INFO --par=4 --empty_min_time=30 /tmp/rc34_fuzz_corpus \
  2>&1 | tee /tmp/rc34_final/fuzz_test_runner.log
```

Run the multi-oracle script again last:

```bash
./test_multi_oracle_testnet.sh \
  2>&1 | tee /tmp/rc34_final/test_multi_oracle_testnet_outer.log
```

## Final Report Format

```text
RC34 final status
Repo/branch/head:
Cleaned commits since RC33:
Git status:
Build: PASS/FAIL, log path
Multi-oracle testnet script: PASS/FAIL, baseline log path, final log path
Unit tests: PASS/FAIL, log path
Functional tests: PASS/FAIL, log path
Extended functional tests: PASS/FAIL/SKIPPED WITH REASON, log path
Fuzz all-target gate: PASS/FAIL, corpus/libFuzzer mode, log path
Commit history cleanup: PASS/FAIL, range-diff path
Bugs found and fixed:
Test/harness fixes and why they were valid:
Architecture changes: none / listed with Jared approval
Known risks/blockers:
Pushed to GitHub: NO
```

## Definition Of Done

RC34 is ready only when:

- Worktree has no unexplained source changes.
- Commits since `v9.26.0-rc33` are logically organized and well explained.
- Build passes.
- `./test_multi_oracle_testnet.sh` passes end-to-end with live multi-oracle behavior.
- `./src/test/test_digibyte --show_progress` passes.
- `test/functional/test_runner.py --jobs=4` passes.
- Extended functional tests pass or are explicitly not applicable.
- Every registered fuzz target is exercised and passes.
- The final `./test_multi_oracle_testnet.sh` rerun passes after all other fixes.
- RC34 release notes/docs are updated if changed by the final commit set.
- No major architecture change was made without Jared approval.
- Nothing was pushed externally.

If any item is false, RC34 is not ready. Keep working or report the exact blocker.
