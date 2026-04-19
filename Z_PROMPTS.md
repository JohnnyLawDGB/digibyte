STARTER PROMPT FOR A NEW CLAUDE CODE SESSION

You are working in the DigiByte Core repo at `~/Code/digibyte` on branch `feature/digidollar-v1`.

Before doing any work, read these files in this order to understand the repo and DigiDollar:
1. `ARCHITECTURE.md`
2. `REPO_MAP.md`
3. `DIGIDOLLAR_ARCHITECTURE.md`
4. `REPO_MAP_DIGIDOLLAR.md`

Rules:
- Treat code as truth, docs as guidance.
- For DigiDollar work, use the repo maps to find the real source files first.
- Verify assumptions against the current code before changing anything.
- For any significant fix or feature, use a TDD flow when practical: write or update the failing test first, make the code change, then prove it passes.
- Always run the full unit and functional test suites before saying work is done.
- Run all relevant repo tests, not just DigiDollar-specific tests.
- Never push to GitHub. Commit locally only if asked.
- Make detailed local commits separated by functionality. Do not squash unrelated fixes together.

---

You are an orchestrator for the DigiByte Core repo at `~/Code/digibyte` on branch `feature/digidollar-v1`. Spawn up to 5 sub-agents in parallel — one per document below. Each sub-agent must read the assigned document, use `REPO_MAP.md` and `REPO_MAP_DIGIDOLLAR.md` to locate the relevant source files, then validate every claim in the doc against the actual code: constants, function names, RPC commands, data structures, flows, fees, limits, and consensus rules. Fix any inaccuracy directly in the document — code is always truth, never the doc. If something is described as working but isn't in the code, mark it `⚠️ Not yet implemented`. Return a short PASS/FAIL/FIXED findings list and the corrected document. After all sub-agents finish, commit all corrections with `docs(digidollar): validate and correct DigiDollar docs against source code`. Never push — commit locally only.

Documents to validate:
- `DIGIDOLLAR_EXPLAINER.md`
- `DIGIDOLLAR_ARCHITECTURE.md`
- `DIGIDOLLAR_ORACLE_EXPLAINER.md`
- `DIGIDOLLAR_ORACLE_ARCHITECTURE.md`
- `DIGIDOLLAR_ACTIVATION_EXPLAINER.md`
- `DIGIDOLLAR_ORACLE_SETUP.md`
- `REPO_MAP.md`
- `REPO_MAP_DIGIDOLLAR.md`

---

RC30 PROMPT

You are working in `~/Code/digibyte` on `feature/digidollar-v1`. First read `ARCHITECTURE.md`, `REPO_MAP.md`, `DIGIDOLLAR_ARCHITECTURE.md`, and `REPO_MAP_DIGIDOLLAR.md`. You are a world class expert C++ software engineer.

Goal: prepare DigiByte Core `v9.26.0-rc30` by making all code, docs, tests, and scripts changes needed for 9-of-17 oracle consensus, then draft RC30 release notes.

Requirements:
- Start by making a concrete task list and keep it updated as work progresses.
- Do not stop at partial progress. Continue until the full RC30 scope is complete, all required tests pass, and the deliverables are finished.
- Use up to 3 sub-agents in parallel.
- Treat code as truth, docs as guidance.
- Use TDD for any significant fix when practical.
- Use Gitter context plus repo evidence to identify and assign the new oracle additions.
- The new oracle additions are `DaPunzy`, `Neel`, and `GTO90`.
- Set `GTO90` as the 17th oracle.
- Update the full oracle list and sort it everywhere it matters.
- Update all affected consensus code, configs, docs, tests, fixtures, and tooling.
- Draft RC30 release notes using RC28 and RC29 release notes as the base pattern, then update them from the final RC30 code and validation results.
- Include the relevant Gitter context in the work: the oracle set is being expanded to 17, and `DaPunzy`, `Neel`, and `GTO90` are being added as new oracles.
- Use this oracle list as the target truth for RC30 unless the repo code is explicitly updated to a newer approved list:
  - 0: Jared
  - 1: Green Candle
  - 2: Bastian
  - 3: DanGB
  - 4: Shenger
  - 5: Ycagel
  - 6: Aussie
  - 7: LookInto
  - 8: JohnnyLawDGB
  - 9: Ogilvie
  - 10: ChopperBrian
  - 11: hallvardo
  - 12: BlindDave
  - 13: DigiByteForce
  - 14: DaPunzy
  - 15: Neel
  - 16: GTO90
- After all code and config changes are made, get all unit tests, all functional tests, and all fuzz tests passing.
- Then modify `test_multi_oracle_testnet.sh` to preserve all existing test coverage while adapting it to validate and debug 9-of-17 oracle consensus with 100% confidence.
- `test_multi_oracle_testnet.sh` must use testnet, not regtest.
- It is acceptable to add new wallet nodes to the test setup.
- Plan for up to 2 oracle identities per wallet node if needed.
- Use the script as a debugger to fully verify multi-oracle consensus behavior end-to-end.
- You will likely need to temporarily enable the testnet pubkeys session code and enable easy PoW on testnet for this validation flow. That temporary testnet-only debug code is currently commented out in the testnet pubkey session area and must be reverted before final RC30 completion.
- Get the updated `test_multi_oracle_testnet.sh` passing on testnet.
- Make detailed local commits separated by functionality.
- Never push. Commit locally only.

Deliverables:
1. Working 9-of-17 oracle consensus implementation.
2. All unit, functional, and fuzz tests passing after the final changes.
3. Updated and passing `test_multi_oracle_testnet.sh` on testnet.
4. Draft `RC30` release notes based on RC28 and RC29 notes, updated for the final RC30 code and results.
5. Short summary of changes, tests run, temporary testnet-only debug steps used and reverted, and any follow-up risks.
