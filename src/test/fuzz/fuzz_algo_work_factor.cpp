#include <consensus/amount.h>
#include <primitives/block.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>

#include <cassert>
#include <cstdint>

extern int GetAlgoWorkFactor(int nHeight, int algo);

FUZZ_TARGET(fuzz_algo_work_factor_phase2a)
{
    FuzzedDataProvider fdp(buffer.data(), buffer.size());
    const int height = fdp.ConsumeIntegral<int>();
    const int algo = fdp.ConsumeIntegralInRange<int>(-4, 16);

    const int wf = GetAlgoWorkFactor(height, algo);
    assert(wf >= 0);
    assert(wf <= 1000000); // geometric mean multiplier should remain bounded

    // MAX_MONEY overflow boundary sanity in same harness.
    assert(MoneyRange(MAX_MONEY));
    assert(!MoneyRange(MAX_MONEY + 1));
}
