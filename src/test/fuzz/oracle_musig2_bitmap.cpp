// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/oracle.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>

#include <cassert>
#include <cstdint>
#include <vector>

FUZZ_TARGET(oracle_musig2_bitmap)
{
    FuzzedDataProvider fdp(buffer.data(), buffer.size());

    // 1. Random bitmaps: set participation bits and verify SerializeV03Data doesn't crash
    LIMITED_WHILE(fdp.remaining_bytes() > 0, 200) {
        size_t oracle_count = fdp.ConsumeIntegralInRange<size_t>(0, ORACLE_TOTAL_COUNT + 10);
        size_t bitmap_bytes = (oracle_count + 7) / 8;

        auto bitmap = fdp.ConsumeBytes<unsigned char>(bitmap_bytes);

        COracleBundle bundle;
        bundle.version = 3;
        bundle.epoch = fdp.ConsumeIntegral<int32_t>();
        bundle.median_price_micro_usd = fdp.ConsumeIntegral<uint64_t>();
        bundle.timestamp = fdp.ConsumeIntegral<int64_t>();
        bundle.participation_bitmap = bitmap;
        bundle.aggregate_sig.assign(64, 0xAA); // Valid-length sig

        // Serialize and round-trip
        auto payload = bundle.SerializeV03Data();
        if (!payload.empty()) {
            COracleBundle decoded;
            bool ok = COracleBundle::DeserializeV03Data(payload, decoded);
            if (ok) {
                assert(decoded.participation_bitmap == bundle.participation_bitmap);
            }
        }
    }

    // 2. Edge cases: empty bitmap, single-byte bitmap, max-size bitmap
    for (size_t len : {size_t(0), size_t(1), size_t(4), size_t(8), size_t(32)}) {
        auto bits = fdp.ConsumeBytes<unsigned char>(len);

        COracleBundle bundle;
        bundle.version = 3;
        bundle.epoch = 1;
        bundle.median_price_micro_usd = 5000;
        bundle.timestamp = 1700000000;
        bundle.participation_bitmap = bits;
        bundle.aggregate_sig.assign(64, 0xBB);

        auto payload = bundle.SerializeV03Data();
        if (!payload.empty()) {
            COracleBundle decoded;
            (void)COracleBundle::DeserializeV03Data(payload, decoded);
        }
    }
}
