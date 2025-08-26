# DigiByte v8.26 Application Bugs Found During Test Fixes

This document tracks actual bugs in the DigiByte application code (not test code) discovered while fixing Python functional tests. These are bugs that would affect production if not fixed.

## Bug Tracking Format

Each bug should be documented using this template:
```markdown
## BUG-[NUMBER]: [Short Description]
**File**: src/[filename].cpp:[line]
**Test**: [test_name.py] that exposed this bug
**Severity**: Critical | High | Medium | Low
**Status**: 🔴 Open | 🟡 In Progress | 🟢 Fixed

### Issue
[Clear description of what's broken]

### Root Cause  
[Why the Bitcoin v26.2 merge broke this]

### Symptoms
- [User-visible symptom 1]
- [User-visible symptom 2]

### Fix Applied
\```cpp
// OLD (broken):
[code snippet]

// NEW (fixed):
[code snippet]
\```

### Impact If Unfixed
[What happens in production without this fix]

### Verification
[How to test this fix works]

### Related Tests
- [test1.py]
- [test2.py]

### PR/Commit
[Link to PR or commit hash]
```

---

*This document is critical for production stability. Every bug here represents a real issue that affects users. Update immediately when bugs are found or fixed.*