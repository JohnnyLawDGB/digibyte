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
