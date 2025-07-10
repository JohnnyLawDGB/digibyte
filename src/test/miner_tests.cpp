<<<<<<< HEAD
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2014-2021 The DigiByte Core developers
=======
// Copyright (c) 2011-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addresstype.h>
#include <coins.h>
#include <common/system.h>
#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
<<<<<<< HEAD
#include <miner.h>
#include <policy/policy.h>
#include <script/standard.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/time.h>
#include <validation.h>
=======
#include <node/miner.h>
#include <policy/policy.h>
#include <test/util/random.h>
#include <test/util/txmempool.h>
#include <timedata.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <validation.h>
#include <versionbits.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#include <test/util/setup_common.h>

#include <memory>

#include <boost/test/unit_test.hpp>

<<<<<<< HEAD
namespace miner_tests {
struct MinerTestingSetup : public TestingSetup {
    void TestPackageSelection(const CChainParams& chainparams, const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst) EXCLUSIVE_LOCKS_REQUIRED(::cs_main, m_node.mempool->cs);
    bool TestSequenceLocks(const CTransaction& tx, int flags) EXCLUSIVE_LOCKS_REQUIRED(::cs_main, m_node.mempool->cs)
    {
        CCoinsViewMemPool view_mempool(&m_node.chainman->ActiveChainstate().CoinsTip(), *m_node.mempool);
        return CheckSequenceLocks(m_node.chainman->ActiveChain().Tip(), view_mempool, tx, flags);
    }
    BlockAssembler AssemblerForTest(const CChainParams& params);
=======
using node::BlockAssembler;
using node::CBlockTemplate;

namespace miner_tests {
struct MinerTestingSetup : public TestingSetup {
    void TestPackageSelection(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
    void TestBasicMining(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst, int baseheight) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
    void TestPrioritisedMining(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
    bool TestSequenceLocks(const CTransaction& tx, CTxMemPool& tx_mempool) EXCLUSIVE_LOCKS_REQUIRED(::cs_main)
    {
        CCoinsViewMemPool view_mempool{&m_node.chainman->ActiveChainstate().CoinsTip(), tx_mempool};
        CBlockIndex* tip{m_node.chainman->ActiveChain().Tip()};
        const std::optional<LockPoints> lock_points{CalculateLockPointsAtTip(tip, view_mempool, tx)};
        return lock_points.has_value() && CheckSequenceLocksAtTip(tip, *lock_points);
    }
    CTxMemPool& MakeMempool()
    {
        // Delete the previous mempool to ensure with valgrind that the old
        // pointer is not accessed, when the new one should be accessed
        // instead.
        m_node.mempool.reset();
        m_node.mempool = std::make_unique<CTxMemPool>(MemPoolOptionsForTest(m_node));
        return *m_node.mempool;
    }
    BlockAssembler AssemblerForTest(CTxMemPool& tx_mempool);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
};
} // namespace miner_tests

BOOST_FIXTURE_TEST_SUITE(miner_tests, MinerTestingSetup)

static CFeeRate blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);

<<<<<<< HEAD
BlockAssembler MinerTestingSetup::AssemblerForTest(const CChainParams& params)
=======
BlockAssembler MinerTestingSetup::AssemblerForTest(CTxMemPool& tx_mempool)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    BlockAssembler::Options options;

    options.nBlockMaxWeight = MAX_BLOCK_WEIGHT;
    options.blockMinFeeRate = blockMinFeeRate;
<<<<<<< HEAD
    return BlockAssembler(m_node.chainman->ActiveChainstate(), *m_node.mempool, params, options);
=======
    return BlockAssembler{m_node.chainman->ActiveChainstate(), &tx_mempool, options};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

constexpr static struct {
    unsigned char extranonce;
    unsigned int nonce;
<<<<<<< HEAD
} blockinfo[] = {
    {6, 17069655}, {5, 17005807}, {3, 17104788}, {2, 17018511},
    {4, 17002289}, {2, 17031872}, {3, 17217824}, {3, 17392029},
    {3, 17132904}, {4, 17295494}, {2, 17106060}, {2, 17122061},
    {3, 17168597}, {2, 17080811}, {5, 17336605}, {2, 17016588},
    {3, 17131330}, {5, 17022216}, {4, 17063738}, {5, 17111047},
    {6, 17103113}, {5, 17044822}, {1, 17291116}, {2, 17146373},
    {1, 17018968}, {6, 17196421}, {4, 17227889}, {2, 17053482},
    {3, 17112617}, {4, 17132557}, {6, 17148920}, {3, 17034281},
    {5, 17916371}, {6, 17014566}, {3, 17079850}, {2, 17123491},
    {5, 17426925}, {4, 17208585}, {1, 17003919}, {5, 17583243},
    {6, 17063826}, {4, 17016670}, {3, 17383922}, {4, 17014999},
    {1, 17059546}, {3, 17140899}, {5, 17086887}, {2, 17067888},
    {4, 17153046}, {4, 17156194}, {5, 17067760}, {4, 17045640},
    {5, 17036048}, {4, 17026527}, {3, 17210999}, {6, 17048021},
    {1, 17032815}, {2, 17019910}, {6, 17056926}, {4, 17535571},
    {4, 17331417}, {3, 17044631}, {1, 17006590}, {1, 17216776},
    {4, 17286402}, {1, 17360551}, {3, 17001314}, {5, 17304096},
    {3, 17318425}, {6, 17323330}, {5, 17103733}, {3, 17298021},
    {2, 17010320}, {1, 17229603}, {6, 17129780}, {1, 17134717},
    {2, 17389955}, {5, 17026290}, {6, 17168863}, {5, 17027916},
    {4, 17127427}, {5, 17095206}, {1, 17666291}, {2, 17044212},
    {5, 17260045}, {2, 17139925}, {2, 17073229}, {6, 17079099},
    {6, 17000303}, {1, 17118443}, {6, 17015501}, {5, 17028977},
    {2, 17233841}, {4, 17022131}, {5, 17115489}, {1, 17174324},
    {5, 17015562}, {2, 17036612}, {6, 17046047}, {3, 17069896},
    {1, 17066558}, {5, 17418052}, {1, 17095816}, {5, 17694800},
    {6, 17197365}, {4, 17013904}, {4, 17008418}, {2, 17080399},
    {5, 17098279}, {1, 17358100}, {5, 17034203}, {5, 17732840},
    {2, 17322693}, {6, 17918728}, {6, 17018174}, {6, 17043893},
    {5, 17120690}, {2, 17166407}, {3, 17259491}, {5, 17311794},
    {1, 17150249}, {2, 17063136}, {2, 17293003}, {6, 17359996},
    {3, 17307334}, {2, 17478737}, {3, 17263283}, {5, 17011240},
    {5, 17128416}, {3, 17370203}, {4, 17036854}, {2, 17026802}, 
};

static CBlockIndex CreateBlockIndex(int nHeight, CBlockIndex* active_chain_tip) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CBlockIndex index;
    index.nHeight = nHeight;
    index.pprev = active_chain_tip;
=======
} BLOCKINFO[]{{8, 582909131},  {0, 971462344},  {2, 1169481553}, {6, 66147495},  {7, 427785981},  {8, 80538907},
              {8, 207348013},  {2, 1951240923}, {4, 215054351},  {1, 491520534}, {8, 1282281282}, {4, 639565734},
              {3, 248274685},  {8, 1160085976}, {6, 396349768},  {5, 393780549}, {5, 1096899528}, {4, 965381630},
              {0, 728758712},  {5, 318638310},  {3, 164591898},  {2, 274234550}, {2, 254411237},  {7, 561761812},
              {2, 268342573},  {0, 402816691},  {1, 221006382},  {6, 538872455}, {7, 393315655},  {4, 814555937},
              {7, 504879194},  {6, 467769648},  {3, 925972193},  {2, 200581872}, {3, 168915404},  {8, 430446262},
              {5, 773507406},  {3, 1195366164}, {0, 433361157},  {3, 297051771}, {0, 558856551},  {2, 501614039},
              {3, 528488272},  {2, 473587734},  {8, 230125274},  {2, 494084400}, {4, 357314010},  {8, 60361686},
              {7, 640624687},  {3, 480441695},  {8, 1424447925}, {4, 752745419}, {1, 288532283},  {6, 669170574},
              {5, 1900907591}, {3, 555326037},  {3, 1121014051}, {0, 545835650}, {8, 189196651},  {5, 252371575},
              {0, 199163095},  {6, 558895874},  {6, 1656839784}, {6, 815175452}, {6, 718677851},  {5, 544000334},
              {0, 340113484},  {6, 850744437},  {4, 496721063},  {8, 524715182}, {6, 574361898},  {6, 1642305743},
              {6, 355110149},  {5, 1647379658}, {8, 1103005356}, {7, 556460625}, {3, 1139533992}, {5, 304736030},
              {2, 361539446},  {2, 143720360},  {6, 201939025},  {7, 423141476}, {4, 574633709},  {3, 1412254823},
              {4, 873254135},  {0, 341817335},  {6, 53501687},   {3, 179755410}, {5, 172209688},  {8, 516810279},
              {4, 1228391489}, {8, 325372589},  {6, 550367589},  {0, 876291812}, {7, 412454120},  {7, 717202854},
              {2, 222677843},  {6, 251778867},  {7, 842004420},  {7, 194762829}, {4, 96668841},   {1, 925485796},
              {0, 792342903},  {6, 678455063},  {6, 773251385},  {5, 186617471}, {6, 883189502},  {7, 396077336},
              {8, 254702874},  {0, 455592851}};

static std::unique_ptr<CBlockIndex> CreateBlockIndex(int nHeight, CBlockIndex* active_chain_tip) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    auto index{std::make_unique<CBlockIndex>()};
    index->nHeight = nHeight;
    index->pprev = active_chain_tip;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    return index;
}

// Test suite for ancestor feerate transaction selection.
// Implemented as an additional function, rather than a separate test case,
// to allow reusing the blockchain created in CreateNewBlock_validity.
<<<<<<< HEAD
void MinerTestingSetup::TestPackageSelection(const CChainParams& chainparams, const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst)
=======
void MinerTestingSetup::TestPackageSelection(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    CTxMemPool& tx_mempool{MakeMempool()};
    LOCK(tx_mempool.cs);
    // Test the ancestor feerate transaction selection.
    TestMemPoolEntryHelper entry;

    // Test that a medium fee transaction will be selected after a higher fee
    // rate package with a low fee rate parent.
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(1);
    tx.vout[0].nValue = 5000000000LL - 1000;
    // This tx has a low fee: 1000 satoshis
    uint256 hashParentTx = tx.GetHash(); // save this txid for later use
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Fee(1000).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
=======
    tx_mempool.addUnchecked(entry.Fee(1000).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // This tx has a medium fee: 10000 satoshis
    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vout[0].nValue = 5000000000LL - 10000;
    uint256 hashMediumFeeTx = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Fee(10000).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
=======
    tx_mempool.addUnchecked(entry.Fee(10000).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // This tx has a high fee, but depends on the first transaction
    tx.vin[0].prevout.hash = hashParentTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000; // 50k satoshi fee
    uint256 hashHighFeeTx = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Fee(50000).Time(GetTime()).SpendsCoinbase(false).FromTx(tx));

    std::unique_ptr<CBlockTemplate> pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT);
=======
    tx_mempool.addUnchecked(entry.Fee(50000).Time(Now<NodeSeconds>()).SpendsCoinbase(false).FromTx(tx));

    std::unique_ptr<CBlockTemplate> pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 4U);
    BOOST_CHECK(pblocktemplate->block.vtx[1]->GetHash() == hashParentTx);
    BOOST_CHECK(pblocktemplate->block.vtx[2]->GetHash() == hashHighFeeTx);
    BOOST_CHECK(pblocktemplate->block.vtx[3]->GetHash() == hashMediumFeeTx);

    // Test that a package below the block min tx fee doesn't get included
    tx.vin[0].prevout.hash = hashHighFeeTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000; // 0 fee
    uint256 hashFreeTx = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Fee(0).FromTx(tx));
=======
    tx_mempool.addUnchecked(entry.Fee(0).FromTx(tx));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    size_t freeTxSize = ::GetSerializeSize(tx, PROTOCOL_VERSION);

    // Calculate a fee on child transaction that will put the package just
    // below the block min tx fee (assuming 1 child tx of the same size).
    CAmount feeToUse = blockMinFeeRate.GetFee(2*freeTxSize) - 1;

    tx.vin[0].prevout.hash = hashFreeTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000 - feeToUse;
    uint256 hashLowFeeTx = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Fee(feeToUse).FromTx(tx));
    pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT);
=======
    tx_mempool.addUnchecked(entry.Fee(feeToUse).FromTx(tx));
    pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    // Verify that the free tx and the low fee tx didn't get selected
    for (size_t i=0; i<pblocktemplate->block.vtx.size(); ++i) {
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashFreeTx);
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashLowFeeTx);
    }

    // Test that packages above the min relay fee do get included, even if one
    // of the transactions is below the min relay fee
    // Remove the low fee transaction and replace with a higher fee transaction
<<<<<<< HEAD
    m_node.mempool->removeRecursive(CTransaction(tx), MemPoolRemovalReason::REPLACED);
    tx.vout[0].nValue -= 2; // Now we should be just over the min relay fee
    hashLowFeeTx = tx.GetHash();
    m_node.mempool->addUnchecked(entry.Fee(feeToUse+2).FromTx(tx));
    pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT);
=======
    tx_mempool.removeRecursive(CTransaction(tx), MemPoolRemovalReason::REPLACED);
    tx.vout[0].nValue -= 2; // Now we should be just over the min relay fee
    hashLowFeeTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(feeToUse + 2).FromTx(tx));
    pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 6U);
    BOOST_CHECK(pblocktemplate->block.vtx[4]->GetHash() == hashFreeTx);
    BOOST_CHECK(pblocktemplate->block.vtx[5]->GetHash() == hashLowFeeTx);

    // Test that transaction selection properly updates ancestor fee
    // calculations as ancestor transactions get included in a block.
    // Add a 0-fee transaction that has 2 outputs.
    tx.vin[0].prevout.hash = txFirst[2]->GetHash();
    tx.vout.resize(2);
    tx.vout[0].nValue = 5000000000LL - 100000000;
    tx.vout[1].nValue = 100000000; // 1DGB output
    uint256 hashFreeTx2 = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Fee(0).SpendsCoinbase(true).FromTx(tx));
=======
    tx_mempool.addUnchecked(entry.Fee(0).SpendsCoinbase(true).FromTx(tx));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // This tx can't be mined by itself
    tx.vin[0].prevout.hash = hashFreeTx2;
    tx.vout.resize(1);
    feeToUse = blockMinFeeRate.GetFee(freeTxSize);
    tx.vout[0].nValue = 5000000000LL - 100000000 - feeToUse;
    uint256 hashLowFeeTx2 = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Fee(feeToUse).SpendsCoinbase(false).FromTx(tx));
    pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT);
=======
    tx_mempool.addUnchecked(entry.Fee(feeToUse).SpendsCoinbase(false).FromTx(tx));
    pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // Verify that this tx isn't selected.
    for (size_t i=0; i<pblocktemplate->block.vtx.size(); ++i) {
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashFreeTx2);
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashLowFeeTx2);
    }

    // This tx will be mineable, and should cause hashLowFeeTx2 to be selected
    // as well.
    tx.vin[0].prevout.n = 1;
<<<<<<< HEAD
    tx.vout[0].nValue = 100000000 - 100000; // 10k satoshi fee
    m_node.mempool->addUnchecked(entry.Fee(100000).FromTx(tx));
    pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT);
=======
    tx.vout[0].nValue = 100000000 - 10000; // 10k satoshi fee
    tx_mempool.addUnchecked(entry.Fee(10000).FromTx(tx));
    pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 9U);
    BOOST_CHECK(pblocktemplate->block.vtx[8]->GetHash() == hashLowFeeTx2);
}

void MinerTestingSetup::TestBasicMining(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst, int baseheight)
{
<<<<<<< HEAD
    // Note that by default, these tests run with size accounting enabled.
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const CChainParams& chainparams = *chainParams;

    CScript scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    std::unique_ptr<CBlockTemplate> pblocktemplate;
    CMutableTransaction tx;
    CScript script;
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    uint256 hash;
    CMutableTransaction tx;
    TestMemPoolEntryHelper entry;
    entry.nFee = 11;
    entry.nHeight = 11;

<<<<<<< HEAD
    fCheckpointsEnabled = false;

    // Simple block creation, nothing special yet:
    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));

    // We can't make transactions until we have inputs
    // Therefore, load 132 blocks :)
    static_assert(std::size(blockinfo) == 132, "Should have 132 blocks to import");
    int baseheight = 0;
    std::vector<CTransactionRef> txFirst;
    for (const auto& bi : blockinfo) {
        CBlock *pblock = &pblocktemplate->block; // pointer for convenience
        {
            LOCK(cs_main);
            pblock->nVersion = 1;
            pblock->nTime = m_node.chainman->ActiveChain().Tip()->GetMedianTimePast()+1;
            CMutableTransaction txCoinbase(*pblock->vtx[0]);
            txCoinbase.nVersion = 1;
            txCoinbase.vin[0].scriptSig = CScript();
            txCoinbase.vin[0].scriptSig.push_back(bi.extranonce);
            txCoinbase.vin[0].scriptSig.push_back(m_node.chainman->ActiveChain().Height());
            txCoinbase.vout.resize(1); // Ignore the (optional) segwit commitment added by CreateNewBlock (as the hardcoded nonces don't account for this)
            txCoinbase.vout[0].scriptPubKey = CScript();
            pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
            if (txFirst.size() == 0)
                baseheight = m_node.chainman->ActiveChain().Height();
            if (txFirst.size() < 4)
                txFirst.push_back(pblock->vtx[0]);
            pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
            pblock->nNonce = bi.nonce;
        }
        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
        BOOST_CHECK(Assert(m_node.chainman)->ProcessNewBlock(chainparams, shared_pblock, true, nullptr));
        pblock->hashPrevBlock = pblock->GetHash();
    }

    LOCK(cs_main);
    LOCK(m_node.mempool->cs);

    // Just to make sure we can still make simple blocks
    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));

    const CAmount BLOCKSUBSIDY = 50*COIN;
=======
    const CAmount BLOCKSUBSIDY = 50 * COIN;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    const CAmount LOWFEE = CENT;
    const CAmount HIGHFEE = COIN;
    const CAmount HIGHERFEE = 4 * COIN;

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // Just to make sure we can still make simple blocks
        auto pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
        BOOST_CHECK(pblocktemplate);

        // block sigops > limit: 1000 CHECKMULTISIG + 1
        tx.vin.resize(1);
        // NOTE: OP_NOP is used to force 20 SigOps for the CHECKMULTISIG
        tx.vin[0].scriptSig = CScript() << OP_0 << OP_0 << OP_0 << OP_NOP << OP_CHECKMULTISIG << OP_1;
        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vin[0].prevout.n = 0;
        tx.vout.resize(1);
        tx.vout[0].nValue = BLOCKSUBSIDY;
        for (unsigned int i = 0; i < 1001; ++i) {
            tx.vout[0].nValue -= LOWFEE;
            hash = tx.GetHash();
            bool spendsCoinbase = i == 0; // only first tx spends coinbase
            // If we don't set the # of sig ops in the CTxMemPoolEntry, template creation fails
            tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(spendsCoinbase).FromTx(tx));
            tx.vin[0].prevout.hash = hash;
        }

        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-blk-sigops"));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vout[0].nValue = BLOCKSUBSIDY;
        for (unsigned int i = 0; i < 1001; ++i) {
            tx.vout[0].nValue -= LOWFEE;
            hash = tx.GetHash();
            bool spendsCoinbase = i == 0; // only first tx spends coinbase
            // If we do set the # of sig ops in the CTxMemPoolEntry, template creation passes
            tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(spendsCoinbase).SigOpsCost(80).FromTx(tx));
            tx.vin[0].prevout.hash = hash;
        }
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // block size > limit
        tx.vin[0].scriptSig = CScript();
        // 18 * (520char + DROP) + OP_1 = 9433 bytes
        std::vector<unsigned char> vchData(520);
        for (unsigned int i = 0; i < 18; ++i) {
            tx.vin[0].scriptSig << vchData << OP_DROP;
        }
        tx.vin[0].scriptSig << OP_1;
        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vout[0].nValue = BLOCKSUBSIDY;
        for (unsigned int i = 0; i < 128; ++i) {
            tx.vout[0].nValue -= LOWFEE;
            hash = tx.GetHash();
            bool spendsCoinbase = i == 0; // only first tx spends coinbase
            tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(spendsCoinbase).FromTx(tx));
            tx.vin[0].prevout.hash = hash;
        }
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // orphan in tx_mempool, template creation fails
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).FromTx(tx));
        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-txns-inputs-missingorspent"));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // child with higher feerate than parent
        tx.vin[0].scriptSig = CScript() << OP_1;
        tx.vin[0].prevout.hash = txFirst[1]->GetHash();
        tx.vout[0].nValue = BLOCKSUBSIDY - HIGHFEE;
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(HIGHFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
        tx.vin.resize(2);
        tx.vin[1].scriptSig = CScript() << OP_1;
        tx.vin[1].prevout.hash = txFirst[0]->GetHash();
        tx.vin[1].prevout.n = 0;
        tx.vout[0].nValue = tx.vout[0].nValue + BLOCKSUBSIDY - HIGHERFEE; // First txn output + fresh coinbase - new txn fee
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(HIGHERFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // coinbase in tx_mempool, template creation fails
        tx.vin.resize(1);
        tx.vin[0].prevout.SetNull();
        tx.vin[0].scriptSig = CScript() << OP_0 << OP_1;
        tx.vout[0].nValue = 0;
        hash = tx.GetHash();
        // give it a fee so it'll get mined
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(false).FromTx(tx));
        // Should throw bad-cb-multiple
        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-cb-multiple"));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // double spend txn pair in tx_mempool, template creation fails
        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vin[0].scriptSig = CScript() << OP_1;
        tx.vout[0].nValue = BLOCKSUBSIDY - HIGHFEE;
        tx.vout[0].scriptPubKey = CScript() << OP_1;
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(HIGHFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        tx.vout[0].scriptPubKey = CScript() << OP_2;
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(HIGHFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-txns-inputs-missingorspent"));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // subsidy changing
        int nHeight = m_node.chainman->ActiveChain().Height();
        // Create an actual 209999-long block chain (without valid blocks).
        while (m_node.chainman->ActiveChain().Tip()->nHeight < 209999) {
            CBlockIndex* prev = m_node.chainman->ActiveChain().Tip();
            CBlockIndex* next = new CBlockIndex();
            next->phashBlock = new uint256(InsecureRand256());
            m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(next->GetBlockHash());
            next->pprev = prev;
            next->nHeight = prev->nHeight + 1;
            next->BuildSkip();
            m_node.chainman->ActiveChain().SetTip(*next);
        }
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
        // Extend to a 210000-long block chain.
        while (m_node.chainman->ActiveChain().Tip()->nHeight < 210000) {
            CBlockIndex* prev = m_node.chainman->ActiveChain().Tip();
            CBlockIndex* next = new CBlockIndex();
            next->phashBlock = new uint256(InsecureRand256());
            m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(next->GetBlockHash());
            next->pprev = prev;
            next->nHeight = prev->nHeight + 1;
            next->BuildSkip();
            m_node.chainman->ActiveChain().SetTip(*next);
        }
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));

        // invalid p2sh txn in tx_mempool, template creation fails
        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vin[0].prevout.n = 0;
        tx.vin[0].scriptSig = CScript() << OP_1;
        tx.vout[0].nValue = BLOCKSUBSIDY - LOWFEE;
        CScript script = CScript() << OP_0;
        tx.vout[0].scriptPubKey = GetScriptForDestination(ScriptHash(script));
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
        tx.vin[0].scriptSig = CScript() << std::vector<unsigned char>(script.begin(), script.end());
        tx.vout[0].nValue -= LOWFEE;
        hash = tx.GetHash();
<<<<<<< HEAD
        bool spendsCoinbase = i == 0; // only first tx spends coinbase
        // If we don't set the # of sig ops in the CTxMemPoolEntry, template creation fails
        m_node.mempool->addUnchecked(entry.Fee(LOWFEE).Time(GetTime()).SpendsCoinbase(spendsCoinbase).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
    }

    BOOST_CHECK_EXCEPTION(AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT), std::runtime_error, HasReason("bad-blk-sigops"));
    m_node.mempool->clear();

    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vout[0].nValue = BLOCKSUBSIDY;
    for (unsigned int i = 0; i < 1001; ++i)
    {
        tx.vout[0].nValue -= LOWFEE;
        hash = tx.GetHash();
        bool spendsCoinbase = i == 0; // only first tx spends coinbase
        // If we do set the # of sig ops in the CTxMemPoolEntry, template creation passes
        m_node.mempool->addUnchecked(entry.Fee(LOWFEE).Time(GetTime()).SpendsCoinbase(spendsCoinbase).SigOpsCost(80).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
    }
    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));
    m_node.mempool->clear();

    // block size > limit
    tx.vin[0].scriptSig = CScript();
    // 18 * (520char + DROP) + OP_1 = 9433 bytes
    std::vector<unsigned char> vchData(520);
    for (unsigned int i = 0; i < 18; ++i)
        tx.vin[0].scriptSig << vchData << OP_DROP;
    tx.vin[0].scriptSig << OP_1;
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vout[0].nValue = BLOCKSUBSIDY;
    for (unsigned int i = 0; i < 128; ++i)
    {
        tx.vout[0].nValue -= LOWFEE;
        hash = tx.GetHash();
        bool spendsCoinbase = i == 0; // only first tx spends coinbase
        m_node.mempool->addUnchecked(entry.Fee(LOWFEE).Time(GetTime()).SpendsCoinbase(spendsCoinbase).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
    }
    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));
    m_node.mempool->clear();

    // orphan in *m_node.mempool, template creation fails
    hash = tx.GetHash();
    m_node.mempool->addUnchecked(entry.Fee(LOWFEE).Time(GetTime()).FromTx(tx));
    BOOST_CHECK_EXCEPTION(AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT), std::runtime_error, HasReason("bad-txns-inputs-missingorspent"));
    m_node.mempool->clear();

    // child with higher feerate than parent
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vout[0].nValue = BLOCKSUBSIDY-HIGHFEE;
    hash = tx.GetHash();
    m_node.mempool->addUnchecked(entry.Fee(HIGHFEE).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
    tx.vin[0].prevout.hash = hash;
    tx.vin.resize(2);
    tx.vin[1].scriptSig = CScript() << OP_1;
    tx.vin[1].prevout.hash = txFirst[0]->GetHash();
    tx.vin[1].prevout.n = 0;
    tx.vout[0].nValue = tx.vout[0].nValue+BLOCKSUBSIDY-HIGHERFEE; //First txn output + fresh coinbase - new txn fee
    hash = tx.GetHash();
    m_node.mempool->addUnchecked(entry.Fee(HIGHERFEE).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));
    m_node.mempool->clear();

    // coinbase in *m_node.mempool, template creation fails
    tx.vin.resize(1);
    tx.vin[0].prevout.SetNull();
    tx.vin[0].scriptSig = CScript() << OP_0 << OP_1;
    tx.vout[0].nValue = 0;
    hash = tx.GetHash();
    // give it a fee so it'll get mined
    m_node.mempool->addUnchecked(entry.Fee(LOWFEE).Time(GetTime()).SpendsCoinbase(false).FromTx(tx));
    // Should throw bad-cb-multiple
    BOOST_CHECK_EXCEPTION(AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT), std::runtime_error, HasReason("bad-cb-multiple"));
    m_node.mempool->clear();

    // double spend txn pair in *m_node.mempool, template creation fails
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vout[0].nValue = BLOCKSUBSIDY-HIGHFEE;
    tx.vout[0].scriptPubKey = CScript() << OP_1;
    hash = tx.GetHash();
    m_node.mempool->addUnchecked(entry.Fee(HIGHFEE).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
    tx.vout[0].scriptPubKey = CScript() << OP_2;
    hash = tx.GetHash();
    m_node.mempool->addUnchecked(entry.Fee(HIGHFEE).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
    BOOST_CHECK_EXCEPTION(AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT), std::runtime_error, HasReason("bad-txns-inputs-missingorspent"));
    m_node.mempool->clear();

    // subsidy changing
    int nHeight = m_node.chainman->ActiveChain().Height();
    // Create an actual 209999-long block chain (without valid blocks).
    while (m_node.chainman->ActiveChain().Tip()->nHeight < 209999) {
        CBlockIndex* prev = m_node.chainman->ActiveChain().Tip();
        CBlockIndex* next = new CBlockIndex();
        next->phashBlock = new uint256(InsecureRand256());
        m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(next->GetBlockHash());
        next->nVersion |= BLOCK_VERSION_SCRYPT;
        next->pprev = prev;
        next->nHeight = prev->nHeight + 1;
        next->BuildSkip();
        m_node.chainman->ActiveChain().SetTip(next);
    }
    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));
    // Extend to a 210000-long block chain.
    while (m_node.chainman->ActiveChain().Tip()->nHeight < 210000) {
        CBlockIndex* prev = m_node.chainman->ActiveChain().Tip();
        CBlockIndex* next = new CBlockIndex();
        next->phashBlock = new uint256(InsecureRand256());
        m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(next->GetBlockHash());
        next->pprev = prev;
        next->nHeight = prev->nHeight + 1;
        next->BuildSkip();
        m_node.chainman->ActiveChain().SetTip(next);
    }
    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));

    // invalid p2sh txn in *m_node.mempool, template creation fails
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vout[0].nValue = BLOCKSUBSIDY-LOWFEE;
    script = CScript() << OP_0;
    tx.vout[0].scriptPubKey = GetScriptForDestination(ScriptHash(script));
    hash = tx.GetHash();
    m_node.mempool->addUnchecked(entry.Fee(LOWFEE).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
    tx.vin[0].prevout.hash = hash;
    tx.vin[0].scriptSig = CScript() << std::vector<unsigned char>(script.begin(), script.end());
    tx.vout[0].nValue -= LOWFEE;
    hash = tx.GetHash();
    m_node.mempool->addUnchecked(entry.Fee(LOWFEE).Time(GetTime()).SpendsCoinbase(false).FromTx(tx));
    // Should throw block-validation-failed
    BOOST_CHECK_EXCEPTION(AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT), std::runtime_error, HasReason("block-validation-failed"));
    m_node.mempool->clear();

    // Delete the dummy blocks again.
    while (m_node.chainman->ActiveChain().Tip()->nHeight > nHeight) {
        CBlockIndex* del = m_node.chainman->ActiveChain().Tip();
        m_node.chainman->ActiveChain().SetTip(del->pprev);
        m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(del->pprev->GetBlockHash());
        delete del->phashBlock;
        delete del;
    }

    // non-final txs in mempool
    SetMockTime(m_node.chainman->ActiveChain().Tip()->GetMedianTimePast()+1);
    int flags = LOCKTIME_VERIFY_SEQUENCE|LOCKTIME_MEDIAN_TIME_PAST;
=======
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(false).FromTx(tx));
        // Should throw block-validation-failed
        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("block-validation-failed"));

        // Delete the dummy blocks again.
        while (m_node.chainman->ActiveChain().Tip()->nHeight > nHeight) {
            CBlockIndex* del = m_node.chainman->ActiveChain().Tip();
            m_node.chainman->ActiveChain().SetTip(*Assert(del->pprev));
            m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(del->pprev->GetBlockHash());
            delete del->phashBlock;
            delete del;
        }
    }

    CTxMemPool& tx_mempool{MakeMempool()};
    LOCK(tx_mempool.cs);

    // non-final txs in mempool
    SetMockTime(m_node.chainman->ActiveChain().Tip()->GetMedianTimePast() + 1);
    const int flags{LOCKTIME_VERIFY_SEQUENCE};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    // height map
    std::vector<int> prevheights;

    // relative height locked
    tx.nVersion = 2;
    tx.vin.resize(1);
    prevheights.resize(1);
    tx.vin[0].prevout.hash = txFirst[0]->GetHash(); // only 1 transaction
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vin[0].nSequence = m_node.chainman->ActiveChain().Tip()->nHeight + 1; // txFirst[0] is the 2nd block
    prevheights[0] = baseheight + 1;
    tx.vout.resize(1);
    tx.vout[0].nValue = BLOCKSUBSIDY-HIGHFEE;
    tx.vout[0].scriptPubKey = CScript() << OP_1;
    tx.nLockTime = 0;
    hash = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Fee(HIGHFEE).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
    BOOST_CHECK(CheckFinalTx(m_node.chainman->ActiveChain().Tip(), CTransaction(tx), flags)); // Locktime passes
    BOOST_CHECK(!TestSequenceLocks(CTransaction(tx), flags)); // Sequence locks fail

    {
        CBlockIndex* active_chain_tip = m_node.chainman->ActiveChain().Tip();
        BOOST_CHECK(SequenceLocks(CTransaction(tx), flags, prevheights, CreateBlockIndex(active_chain_tip->nHeight + 2, active_chain_tip))); // Sequence locks pass on 2nd block
=======
    tx_mempool.addUnchecked(entry.Fee(HIGHFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
    BOOST_CHECK(CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime passes
    BOOST_CHECK(!TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks fail

    {
        CBlockIndex* active_chain_tip = m_node.chainman->ActiveChain().Tip();
        BOOST_CHECK(SequenceLocks(CTransaction(tx), flags, prevheights, *CreateBlockIndex(active_chain_tip->nHeight + 2, active_chain_tip))); // Sequence locks pass on 2nd block
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    // relative time locked
    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vin[0].nSequence = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | (((m_node.chainman->ActiveChain().Tip()->GetMedianTimePast()+1-m_node.chainman->ActiveChain()[1]->GetMedianTimePast()) >> CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) + 1); // txFirst[1] is the 3rd block
    prevheights[0] = baseheight + 2;
    hash = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Time(GetTime()).FromTx(tx));
    BOOST_CHECK(CheckFinalTx(m_node.chainman->ActiveChain().Tip(), CTransaction(tx), flags)); // Locktime passes
    BOOST_CHECK(!TestSequenceLocks(CTransaction(tx), flags)); // Sequence locks fail

    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; i++)
        m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i)->nTime += 512; //Trick the MedianTimePast

    {
        CBlockIndex* active_chain_tip = m_node.chainman->ActiveChain().Tip();
        BOOST_CHECK(SequenceLocks(CTransaction(tx), flags, prevheights, CreateBlockIndex(active_chain_tip->nHeight + 1, active_chain_tip))); // Sequence locks pass 512 seconds later
    }

    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; i++)
        m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i)->nTime -= 512; //undo tricked MTP
=======
    tx_mempool.addUnchecked(entry.Time(Now<NodeSeconds>()).FromTx(tx));
    BOOST_CHECK(CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime passes
    BOOST_CHECK(!TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks fail

    const int SEQUENCE_LOCK_TIME = 512; // Sequence locks pass 512 seconds later
    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; ++i)
        m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i)->nTime += SEQUENCE_LOCK_TIME; // Trick the MedianTimePast
    {
        CBlockIndex* active_chain_tip = m_node.chainman->ActiveChain().Tip();
        BOOST_CHECK(SequenceLocks(CTransaction(tx), flags, prevheights, *CreateBlockIndex(active_chain_tip->nHeight + 1, active_chain_tip)));
    }

    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; ++i) {
        CBlockIndex* ancestor{Assert(m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i))};
        ancestor->nTime -= SEQUENCE_LOCK_TIME; // undo tricked MTP
    }
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // absolute height locked
    tx.vin[0].prevout.hash = txFirst[2]->GetHash();
    tx.vin[0].nSequence = CTxIn::MAX_SEQUENCE_NONFINAL;
    prevheights[0] = baseheight + 3;
    tx.nLockTime = m_node.chainman->ActiveChain().Tip()->nHeight + 1;
    hash = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Time(GetTime()).FromTx(tx));
    BOOST_CHECK(!CheckFinalTx(m_node.chainman->ActiveChain().Tip(), CTransaction(tx), flags)); // Locktime fails
    BOOST_CHECK(TestSequenceLocks(CTransaction(tx), flags)); // Sequence locks pass
=======
    tx_mempool.addUnchecked(entry.Time(Now<NodeSeconds>()).FromTx(tx));
    BOOST_CHECK(!CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime fails
    BOOST_CHECK(TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks pass
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    BOOST_CHECK(IsFinalTx(CTransaction(tx), m_node.chainman->ActiveChain().Tip()->nHeight + 2, m_node.chainman->ActiveChain().Tip()->GetMedianTimePast())); // Locktime passes on 2nd block

    // absolute time locked
    tx.vin[0].prevout.hash = txFirst[3]->GetHash();
    tx.nLockTime = m_node.chainman->ActiveChain().Tip()->GetMedianTimePast();
    prevheights.resize(1);
    prevheights[0] = baseheight + 4;
    hash = tx.GetHash();
<<<<<<< HEAD
    m_node.mempool->addUnchecked(entry.Time(GetTime()).FromTx(tx));
    BOOST_CHECK(!CheckFinalTx(m_node.chainman->ActiveChain().Tip(), CTransaction(tx), flags)); // Locktime fails
    BOOST_CHECK(TestSequenceLocks(CTransaction(tx), flags)); // Sequence locks pass
=======
    tx_mempool.addUnchecked(entry.Time(Now<NodeSeconds>()).FromTx(tx));
    BOOST_CHECK(!CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime fails
    BOOST_CHECK(TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks pass
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    BOOST_CHECK(IsFinalTx(CTransaction(tx), m_node.chainman->ActiveChain().Tip()->nHeight + 2, m_node.chainman->ActiveChain().Tip()->GetMedianTimePast() + 1)); // Locktime passes 1 second later

    // mempool-dependent transactions (not added)
    tx.vin[0].prevout.hash = hash;
    prevheights[0] = m_node.chainman->ActiveChain().Tip()->nHeight + 1;
    tx.nLockTime = 0;
    tx.vin[0].nSequence = 0;
<<<<<<< HEAD
    BOOST_CHECK(CheckFinalTx(m_node.chainman->ActiveChain().Tip(), CTransaction(tx), flags)); // Locktime passes
    BOOST_CHECK(TestSequenceLocks(CTransaction(tx), flags)); // Sequence locks pass
    tx.vin[0].nSequence = 1;
    BOOST_CHECK(!TestSequenceLocks(CTransaction(tx), flags)); // Sequence locks fail
    tx.vin[0].nSequence = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG;
    BOOST_CHECK(TestSequenceLocks(CTransaction(tx), flags)); // Sequence locks pass
    tx.vin[0].nSequence = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | 1;
    BOOST_CHECK(!TestSequenceLocks(CTransaction(tx), flags)); // Sequence locks fail

    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));
=======
    BOOST_CHECK(CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime passes
    BOOST_CHECK(TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks pass
    tx.vin[0].nSequence = 1;
    BOOST_CHECK(!TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks fail
    tx.vin[0].nSequence = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG;
    BOOST_CHECK(TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks pass
    tx.vin[0].nSequence = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | 1;
    BOOST_CHECK(!TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks fail

    auto pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
    BOOST_CHECK(pblocktemplate);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // None of the of the absolute height/time locked tx should have made
    // it into the template because we still check IsFinalTx in CreateNewBlock,
    // but relative locked txs will if inconsistently added to mempool.
    // For now these will still generate a valid template until BIP68 soft fork
    BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), 3U);
<<<<<<< HEAD
    // However if we advance height by 1 and time by 512, all of them should be mined
    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; i++)
        m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i)->nTime += 512; //Trick the MedianTimePast
    m_node.chainman->ActiveChain().Tip()->nHeight++;
    SetMockTime(m_node.chainman->ActiveChain().Tip()->GetMedianTimePast() + 1);

    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey, ALGO_SCRYPT));
=======
    // However if we advance height by 1 and time by SEQUENCE_LOCK_TIME, all of them should be mined
    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; ++i) {
        CBlockIndex* ancestor{Assert(m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i))};
        ancestor->nTime += SEQUENCE_LOCK_TIME; // Trick the MedianTimePast
    }
    m_node.chainman->ActiveChain().Tip()->nHeight++;
    SetMockTime(m_node.chainman->ActiveChain().Tip()->GetMedianTimePast() + 1);

    BOOST_CHECK(pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), 5U);
}

<<<<<<< HEAD
    m_node.chainman->ActiveChain().Tip()->nHeight--;
    SetMockTime(0);
    m_node.mempool->clear();
=======
void MinerTestingSetup::TestPrioritisedMining(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst)
{
    CTxMemPool& tx_mempool{MakeMempool()};
    LOCK(tx_mempool.cs);

    TestMemPoolEntryHelper entry;

    // Test that a tx below min fee but prioritised is included
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vout.resize(1);
    tx.vout[0].nValue = 5000000000LL; // 0 fee
    uint256 hashFreePrioritisedTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashFreePrioritisedTx, 5 * COIN);

    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vout[0].nValue = 5000000000LL - 1000;
    // This tx has a low fee: 1000 satoshis
    uint256 hashParentTx = tx.GetHash(); // save this txid for later use
    tx_mempool.addUnchecked(entry.Fee(1000).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));

    // This tx has a medium fee: 10000 satoshis
    tx.vin[0].prevout.hash = txFirst[2]->GetHash();
    tx.vout[0].nValue = 5000000000LL - 10000;
    uint256 hashMediumFeeTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(10000).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashMediumFeeTx, -5 * COIN);

    // This tx also has a low fee, but is prioritised
    tx.vin[0].prevout.hash = hashParentTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 1000; // 1000 satoshi fee
    uint256 hashPrioritsedChild = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(1000).Time(Now<NodeSeconds>()).SpendsCoinbase(false).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashPrioritsedChild, 2 * COIN);

    // Test that transaction selection properly updates ancestor fee calculations as prioritised
    // parents get included in a block. Create a transaction with two prioritised ancestors, each
    // included by itself: FreeParent <- FreeChild <- FreeGrandchild.
    // When FreeParent is added, a modified entry will be created for FreeChild + FreeGrandchild
    // FreeParent's prioritisation should not be included in that entry.
    // When FreeChild is included, FreeChild's prioritisation should also not be included.
    tx.vin[0].prevout.hash = txFirst[3]->GetHash();
    tx.vout[0].nValue = 5000000000LL; // 0 fee
    uint256 hashFreeParent = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).SpendsCoinbase(true).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashFreeParent, 10 * COIN);

    tx.vin[0].prevout.hash = hashFreeParent;
    tx.vout[0].nValue = 5000000000LL; // 0 fee
    uint256 hashFreeChild = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).SpendsCoinbase(false).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashFreeChild, 1 * COIN);

    tx.vin[0].prevout.hash = hashFreeChild;
    tx.vout[0].nValue = 5000000000LL; // 0 fee
    uint256 hashFreeGrandchild = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).SpendsCoinbase(false).FromTx(tx));

    auto pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 6U);
    BOOST_CHECK(pblocktemplate->block.vtx[1]->GetHash() == hashFreeParent);
    BOOST_CHECK(pblocktemplate->block.vtx[2]->GetHash() == hashFreePrioritisedTx);
    BOOST_CHECK(pblocktemplate->block.vtx[3]->GetHash() == hashParentTx);
    BOOST_CHECK(pblocktemplate->block.vtx[4]->GetHash() == hashPrioritsedChild);
    BOOST_CHECK(pblocktemplate->block.vtx[5]->GetHash() == hashFreeChild);
    for (size_t i=0; i<pblocktemplate->block.vtx.size(); ++i) {
        // The FreeParent and FreeChild's prioritisations should not impact the child.
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashFreeGrandchild);
        // De-prioritised transaction should not be included.
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashMediumFeeTx);
    }
}

// NOTE: These tests rely on CreateNewBlock doing its own self-validation!
BOOST_AUTO_TEST_CASE(CreateNewBlock_validity)
{
    // Note that by default, these tests run with size accounting enabled.
    CScript scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    std::unique_ptr<CBlockTemplate> pblocktemplate;

    CTxMemPool& tx_mempool{*m_node.mempool};
    // Simple block creation, nothing special yet:
    BOOST_CHECK(pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));

    // We can't make transactions until we have inputs
    // Therefore, load 110 blocks :)
    static_assert(std::size(BLOCKINFO) == 110, "Should have 110 blocks to import");
    int baseheight = 0;
    std::vector<CTransactionRef> txFirst;
    for (const auto& bi : BLOCKINFO) {
        CBlock *pblock = &pblocktemplate->block; // pointer for convenience
        {
            LOCK(cs_main);
            pblock->nVersion = VERSIONBITS_TOP_BITS;
            pblock->nTime = m_node.chainman->ActiveChain().Tip()->GetMedianTimePast()+1;
            CMutableTransaction txCoinbase(*pblock->vtx[0]);
            txCoinbase.nVersion = 1;
            txCoinbase.vin[0].scriptSig = CScript{} << (m_node.chainman->ActiveChain().Height() + 1) << bi.extranonce;
            txCoinbase.vout.resize(1); // Ignore the (optional) segwit commitment added by CreateNewBlock (as the hardcoded nonces don't account for this)
            txCoinbase.vout[0].scriptPubKey = CScript();
            pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
            if (txFirst.size() == 0)
                baseheight = m_node.chainman->ActiveChain().Height();
            if (txFirst.size() < 4)
                txFirst.push_back(pblock->vtx[0]);
            pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
            pblock->nNonce = bi.nonce;
        }
        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
        BOOST_CHECK(Assert(m_node.chainman)->ProcessNewBlock(shared_pblock, true, true, nullptr));
        pblock->hashPrevBlock = pblock->GetHash();
    }

    LOCK(cs_main);

    TestBasicMining(scriptPubKey, txFirst, baseheight);

    m_node.chainman->ActiveChain().Tip()->nHeight--;
    SetMockTime(0);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    TestPackageSelection(scriptPubKey, txFirst);

    m_node.chainman->ActiveChain().Tip()->nHeight--;
    SetMockTime(0);

    TestPrioritisedMining(scriptPubKey, txFirst);
}

BOOST_AUTO_TEST_SUITE_END()
