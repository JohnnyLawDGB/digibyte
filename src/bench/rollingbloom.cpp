<<<<<<< HEAD
// Copyright (c) 2016-2020 The Bitcoin Core developers
// Copyright (c) 2016-2020 The DigiByte Core developers
=======
// Copyright (c) 2016-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <bench/bench.h>
#include <common/bloom.h>
#include <crypto/common.h>

<<<<<<< HEAD
=======
#include <vector>

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
static void RollingBloom(benchmark::Bench& bench)
{
    CRollingBloomFilter filter(120000, 0.000001);
    std::vector<unsigned char> data(32);
    uint32_t count = 0;
    bench.run([&] {
        count++;
        WriteLE32(data.data(), count);
        filter.insert(data);

<<<<<<< HEAD
        data[0] = count >> 24;
        data[1] = count >> 16;
        data[2] = count >> 8;
        data[3] = count;
=======
        WriteBE32(data.data(), count);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        filter.contains(data);
    });
}

static void RollingBloomReset(benchmark::Bench& bench)
{
    CRollingBloomFilter filter(120000, 0.000001);
    bench.run([&] {
        filter.reset();
    });
}

<<<<<<< HEAD
BENCHMARK(RollingBloom);
BENCHMARK(RollingBloomReset);
=======
BENCHMARK(RollingBloom, benchmark::PriorityLevel::HIGH);
BENCHMARK(RollingBloomReset, benchmark::PriorityLevel::HIGH);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
