// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2014-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>
#include <arith_uint256.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <type_traits>

// Helper function to parse public key from hex string
static CPubKey ParsePubKey(const std::string& hex) {
    std::vector<unsigned char> data = ParseHex(hex);
    CPubKey pubkey(data);
    if (!pubkey.IsValid()) {
        throw std::runtime_error("Invalid oracle public key: " + hex);
    }
    return pubkey;
}

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "USA Today: 10/Jan/2014, Target: Data stolen from up to 110M customers";
    const CScript genesisOutputScript = CScript() << 0x0 << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 8409600; // DigiByte halving interval
        // BIP16 and Taproot exceptions (script validation rules)
        consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256S("0x00000000000002dc756eebf4f49723ed8d30cc28a5f108eb94b1ba88ac4f9c22"), SCRIPT_VERIFY_NONE);
        consensus.script_flag_exceptions.emplace( // Taproot exception
            uint256S("0x0000000000000000000f14c35b2d841e986ab5441de8c585d5ffe55ea1e395ad"), SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS);

        // BIP34, BIP65 and BIP66, CSV and Segwit were activated simultaneously
        // DEPLOYMENT_NVERSIONBIPS, DEPLOYMENT_CSV, DEPLOYMENT_SEGWIT
        consensus.BIP34Hash = uint256S("0xadd8ca420f557f62377ec2be6e6f47b96cf2e68160d58aeb7b73433de834cca0");
        consensus.BIP34Height = consensus.BIP65Height = consensus.BIP66Height = 4394880; // add8ca420f557f62377ec2be6e6f47b96cf2e68160d58aeb7b73433de834cca0
        consensus.CSVHeight = consensus.SegwitHeight = 4394880;
        consensus.MinBIP9WarningHeight = 483840; // segwit activation height + miner confirmation window
        consensus.powLimit = ArithToUint256(~arith_uint256(0) >> 20);
        consensus.initialTarget[ALGO_ODO] = ArithToUint256(~arith_uint256(0) >> 40); // 256 difficulty
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60 / 4; // 15 seconds
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fEasyPow = false;
        consensus.fPowNoRetargeting = false;
        consensus.fRbfEnabled = false;

        // DigiByte Specific Consensus Code
        consensus.nOdoShapechangeInterval = 10*24*60*60; // 10 days
        consensus.nRuleChangeActivationThreshold = 28224; // 28224 - 70% of 40320 blocks
        consensus.nMinerConfirmationWindow = 40320; // nPowTargetTimespan / nPowTargetSpacing 40320 blocks main net - 1 week

        // Need to make sure we ignore activation warnings below Odo activation height, also ignores Segwit activation
        consensus.MinBIP9WarningHeight = 9152640; // Odo height + miner confirmation window

        // DigiByte Hard Fork Block Heights
        consensus.multiAlgoDiffChangeTarget = 145000; // Block 145,000 MultiAlgo Hard Fork
        consensus.alwaysUpdateDiffChangeTarget = 400000; // Block 400,000 MultiShield Hard Fork
        consensus.workComputationChangeTarget = 1430000; // Block 1,430,000 DigiSpeed Hard Fork
        consensus.algoSwapChangeTarget = 9100000; // Block 9,100,000 Odo PoW Hard Fork
        consensus.OdoHeight = 9112320; // 906b712a7b1f54f10b0faf86111e832ddb7b8ce86ac71a4edd2c61e5ccfe9428
        consensus.ReserveAlgoBitsHeight = 8547840; // d2c03966aeef35f739b222c8332b68df2676204d49c390b3a2544b967c46163f

        // DigiByte-specific difficulty adjustment parameters
        consensus.nTargetTimespan = 0.10 * 24 * 60 * 60; // 2.4 hours
        consensus.nTargetSpacing = 60; // 60 seconds
        consensus.nInterval = consensus.nTargetTimespan / consensus.nTargetSpacing;
        consensus.nDiffChangeTarget = 67200; // DigiShield Hard Fork Block BIP34Height 67,200

        // Old 1% monthly DGB Reward before 15 second block change
        consensus.patchBlockRewardDuration = 10080; //10080; - No longer used
        //4 blocks per min, x60 minutes x 24hours x 14 days = 80,160 blocks for 0.5% reduction in DGB reward supply - No longer used
        consensus.patchBlockRewardDuration2 = 80160; //80160;
        consensus.nTargetTimespanRe = 1*60; // 60 Seconds
        consensus.nTargetSpacingRe = 1*60; // 60 seconds
        consensus.nIntervalRe = consensus.nTargetTimespanRe / consensus.nTargetSpacingRe; // 1 block

        consensus.nAveragingInterval = 10; // 10 blocks
        consensus.multiAlgoTargetSpacing = 30*5; // NUM_ALGOS * 30 seconds
        consensus.multiAlgoTargetSpacingV4 = 15*5; // NUM_ALGOS * 15 seconds
        consensus.nAveragingTargetTimespan = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacing; // 10* NUM_ALGOS * 30
        consensus.nAveragingTargetTimespanV4 = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacingV4; // 10 * NUM_ALGOS * 15

        consensus.nMaxAdjustDown = 40; // 40% adjustment down
        consensus.nMaxAdjustUp = 20; // 20% adjustment up
        consensus.nMaxAdjustDownV3 = 16; // 16% adjustment down
        consensus.nMaxAdjustUpV3 = 8; // 8% adjustment up
        consensus.nMaxAdjustDownV4 = 16;
        consensus.nMaxAdjustUpV4 = 8;

        consensus.nMinActualTimespan = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUp) / 100;
        consensus.nMaxActualTimespan = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDown) / 100;
        consensus.nMinActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUpV3) / 100;
        consensus.nMaxActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDownV3) / 100;
        consensus.nMinActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 - consensus.nMaxAdjustUpV4) / 100;
        consensus.nMaxActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 + consensus.nMaxAdjustDownV4) / 100;

        consensus.nLocalTargetAdjustment = 4; //target adjustment per algo
        consensus.nLocalDifficultyAdjustment = 4; //local difficulty adjustment
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1736510438; // 10th January 2025
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1799582438; // 10th January 2027
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        // Deployment of DigiDollar stablecoin features
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].bit = 23;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = 1777593600; // May 1, 2026
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = 1840752000; // May 1, 2028
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 22014720; // Aligned to confirmation window (546 * 40320)

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid block 21,700,000.
        consensus.defaultAssumeValid = uint256S("0x457f6864b52e5076a433afe3c28e3ae0bbeeaba9036a782ddb691242326fcb80"); // Block 21,700,000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xc3;
        pchMessageStart[2] = 0xb6;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 12024;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 32;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlock(1389388394, 2447652, 0x1e0ffff0, 1, 8000);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x7497ea1b465eb39f1c8f507bc877078fe016d6fcb6dfad3a64c98dcc6e1e8496"));
        assert(genesis.hashMerkleRoot == uint256S("0x72ddd9496b004221ed0557358846d9248ecd4c440ebd28ed901efc18757d0fad"));

        // The current status of the DigiByte DNS Seed Servers can be checked here: http://digibyteseed.com/
        // If you notice a problem with an exiting Seed Server, please contact the DigiByte Critical Infrastructure team (DGBCIT)
        // via the #DGBCIT channel on the DigiByte Discord server: https://discord.com/channels/878200503815782400/1133815334013509764
        // Alternatively, create an issue ticket here: https://github.com/DigiByte-Core/digibyte/issues

        // When adding a new MAINNET Seed Server URL below, please include the name of the person in charge of it
        // and their Github handle so they can be contacted in an emergency.

        // DigiByte MAINNET DNS Seed Server:
        vSeeds.emplace_back("seed.digibyte.io"); // Jared Tate @JaredTate
        vSeeds.emplace_back("seed.diginode.tools"); // Olly Stedall @saltedlolly 
        vSeeds.emplace_back("seed.digibyteblockchain.org"); // John Song @j50ng
        vSeeds.emplace_back("eu.digibyteseed.com"); // Jan De Jong @jongjan88
        vSeeds.emplace_back("seed.digibyte.link"); // Bastian Driessen @bastiandriessen
        vSeeds.emplace_back("seed.quakeguy.com"); // Paul Morgan Quakeitup @SnKQuaKe
        vSeeds.emplace_back("seed.aroundtheblock.app"); // Mark McNiel @JohnnyLawDGB
        vSeeds.emplace_back("seed.digibyte.services"); // Craig Donnachie @cdonnachie

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,30);
        base58Prefixes[SCRIPT_ADDRESS_OLD] = std::vector<unsigned char>(1,5);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,63);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[SECRET_KEY_OLD] = std::vector<unsigned char>(1,158);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "dgb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {     0, uint256S("0x7497ea1b465eb39f1c8f507bc877078fe016d6fcb6dfad3a64c98dcc6e1e8496")},
                {  5000, uint256S("0x95753d284404118788a799ac754a3fdb5d817f5bd73a78697dfe40985c085596")},
                { 10000, uint256S("0x12f90b8744f3b965e107ad9fd8b33ba6d95a91882fbc4b5f8588d70d494bed88")},
                { 12000, uint256S("0xa1266acba91dc3d5737d9e8c6e21b7a91901f7f4c48082ce3d84dd394a13e415")},
                { 14300, uint256S("0x24f665d71b0c6c88f6f72a863e9f1ba8e835cc52d13ad895dc5426021c7d2c48")},
                { 30000, uint256S("0x17c69ef6b403571b1bd333c91fbe116e451ba8281be12aa6bafb0486764bb315")},
                { 60000, uint256S("0x57b2c612b60462a3d6c388c8b30a68cb6f7e2034eea962b12b7ef506454fa2c1")},
                {110000, uint256S("0xab2da24656493015f2fd288994661e1cc657d90aa34c755514af044aaaf1569d")},
                {141100, uint256S("0x145c2cb5239a4e019c730ce8468d927a3955529c2bae077850783da97ddbca05")},
                {141656, uint256S("0x683d27720429f28bcfa22d8385b7a06f307c8fd918d49215148fbd41a0dda595")},
                {245000, uint256S("0x852c475c605e1f20bbe60219c811abaeef08bf0d4ff87eef59200fd7a7567fa7")},
                {302000, uint256S("0xfb6d14ac5e0208f00d941db1fcbfe050f093cfd0c05ed151c809e4428bc14286")},
                {331000, uint256S("0xbd1a1d002750e1648746eb29c78d30fa1043c8b6f89d82924c4488be06fa3d19")},
                {360000, uint256S("0x8fee7e3f6c38dccd3047a3e4667c63406f835c2890024030a2ab2dc6dba7c912")},
                {400100, uint256S("0x82325a97cd97ac14b0a57408f881b1a9fc40174f8430a4580429499ac5d153c8")},
                {521000, uint256S("0xd23fd1e1f994c0586d761b71bb3530e9ab45bd0fabda3a5a2e394f3dc4d9bb04")},
                {1380000, uint256S("0x00000000000001969b1e5836dd8bf6a001d96f4a16d336e09405b62b29feead6")},
                {2000000, uint256S("0x10f522ec60d8af2e2cbd9e2268260c33fb8bbf9cd9f176b4fddcae7493c6791d")},
                {3500000, uint256S("0xbece76f2a3f53637e2ea84837a45a6ffdc0c86372ab4701c3146094f65832c80")},
                {5000000, uint256S("0x1dd2fdf6416343688eed463a7bc70b298a4f872e941e36f85cda0915d6488e25")},
                {6500000, uint256S("0xb168b7f70cbfd2e5fea07da55d9fa90dc7c65599ceb2700efe04ee6c45692e52")},
                {8000000, uint256S("0x1af919cb004bb05c369a862cb5ded70aaa123d0eac2432ceec859f6f42880660")},
                {9500000, uint256S("0x5b0351361414e520e9132ba6c5c4926d6f9ee55c41b77fffce3a16ea15d4a1be")},
                {11000000, uint256S("0x0f4ad10ae49b504246c0175f6cbab9b0f91b6568a88931e6341a83a731701054")},
                {12500000, uint256S("0x697a015b62140c9549fbc8d8b3c1d027626b2f94d337db32115e429fbf233ed7")},
                {14000000, uint256S("0xa33861c857eed46191cf6cdaf81693e0dfcd00b3a11133821b0c73fe1d7769d9")},
                {15500000, uint256S("0x000000000000000439d5c66b2fb3ec50f50a68b65f5790d338150b63488de645")},
                {17000000, uint256S("0xf167688cc0102743b135499ed9f9eff9c5bad096203150e438be0a6e783d5587")},
                {18500000, uint256S("0x745dc7b89208de482071a3a8d13eb5596d55bedc4f5ba2fa74cbea9ecf91169e")},
                {20000000, uint256S("0xf530a66ba6fe93e647f7d88a9b3f22bfe8c2c2ab1ec1b0286286f86b82d6a10f")},
                {21000000, uint256S("0x0000000000000001cb40d3be76bf601d98555a069669d963060d633ea3a140e8")},
                {21700000, uint256S("0x457f6864b52e5076a433afe3c28e3ae0bbeeaba9036a782ddb691242326fcb80")},
            }
        };

        m_assumeutxo_data = {
            {
                .height = 21'700'000,
                .hash_serialized = AssumeutxoHash{uint256S("0x0000000000000000000000000000000000000000000000000000000000000000")}, // TODO: Calculate actual UTXO set hash
                .nChainTx = 0, // TODO: Calculate actual total transaction count
                .blockhash = uint256S("0x457f6864b52e5076a433afe3c28e3ae0bbeeaba9036a782ddb691242326fcb80")
            },
        };

        chainTxData = ChainTxData{
            // DigiByte: Data from DigiByte blockchain
            // DigiByte has ~15 second blocks vs Bitcoin's ~10 minutes (40x faster)
            // As of block 16,500,000 (July 2024)
            .nTime    = 1720000000,  // Approximate July 2024 timestamp
            .nTxCount = 25000000,    // Approximate total DigiByte transactions
            .dTxRate  = 0.15,        // ~0.15 tx/sec for DigiByte (much lower than Bitcoin due to less usage)
        };

        // DigiDollar consensus parameters (mainnet)
        digidollarParams = DigiDollar::ConsensusParams();
        digidollarParams.oracleThreshold = 9;   // 9-of-17 consensus (RC30)
        digidollarParams.activeOracles = 17;    // 17 active oracles (RC30)
        digidollarParams.oracleCount = 17;      // 17 oracles defined (RC30)

        // Initialize DigiDollar Oracle Nodes (30 total, historical Phase One/Two providers)
        InitializeOracleNodes();

        // Mainnet-specific oracle and activation settings
        consensus.nDDOracleEpochBlocks = 100;      // Rotate oracles every 100 blocks (~25 minutes)
        consensus.nDDOracleUpdateInterval = 4;      // Update price every 4 blocks (~1 minute)
        consensus.nDDActivationHeight = 22014720;   // DigiDollar activation height — aligned with BIP9 min_activation_height

        // Oracle system — mainnet activates all phases at block 3,000,000
        // Mainnet skips Phase 1 (single-oracle); goes directly to Phase 3 (MuSig2)
        consensus.nOracleActivationHeight = 3000000;
        consensus.nOracleEpochLength = 1440;          // 24 hours (1440 blocks * 15 seconds)
        consensus.nOracleRequiredMessages = 9;        // Phase Two (pre-MuSig2): 9-of-17 consensus
        consensus.nOracleTotalOracles = 17;
        consensus.nDigiDollarPhase2Height = 3000000;  // Phase Two and Three activate together
        consensus.nDigiDollarPhase3Height = 0;  // Phase Three (MuSig2): active immediately on mainnet

        // Phase 3 MuSig2 oracle configuration — 9-of-17 quorum (RC30)
        // Same oracle operator set as testnet. To add/replace operators:
        //   1. Generate keypair via `digibyte-cli generateoraclekey <id>`
        //   2. Add x-only pubkey to vOraclePublicKeys in slot order
        //   3. Add OracleNodeInfo to vOracleNodes (same slot order)
        //   4. Update nOraclePubkeyCount/nOracleConsensusRequired
        //   5. Deploy via coordinated software upgrade
        consensus.nOraclePubkeyCount = 17;
        consensus.nOracleConsensusRequired = 9;

        // Oracle public keys (x-only, 32 bytes) — ordered by oracle slot (0-16)
        // 17 active operators (RC30 final roster includes DigiSwarm at slot 15).
        consensus.vOraclePublicKeys.clear();
        consensus.vOraclePublicKeys.push_back("e1dce189a530c1fb39dcd9282cf5f9de0e4eb257344be9fd94ce27c06005e8c7");  // 0: Jared
        consensus.vOraclePublicKeys.push_back("3dfb7a36ab40fa6fbc69b4b499eaa17bfa1958aa89ec248efc24b4c18694f990");  // 1: Green Candle
        consensus.vOraclePublicKeys.push_back("172755a320cec96c981d46c86d79a03578d73406a25e89d8edc616a8f361cb5c");  // 2: Bastian
        consensus.vOraclePublicKeys.push_back("546c07ee9d21640c4b4e96e6954bd49c3ab5bcf36c6a512603ebf75f8609da0c");  // 3: DanGB
        consensus.vOraclePublicKeys.push_back("9cef021f841794c1afc4e84d678f3c70dbe3a972330b2b6329852898443deb4f");  // 4: Shenger
        consensus.vOraclePublicKeys.push_back("85016758856ed27388501a54031fa3a678df705bf811fb8bc9abd2d7cfb6d9f7");  // 5: Ycagel
        consensus.vOraclePublicKeys.push_back("7a858e055099e4a9cf8273e9171da148d4fd00afd4376b60dc1cd09974731b51");  // 6: Aussie
        consensus.vOraclePublicKeys.push_back("2d8c9f054d7087e263016c0800ad1c2f8106859e772766b9f8179042d1792a09");  // 7: LookInto
        consensus.vOraclePublicKeys.push_back("89d5c588c8e0d311028f2f7e0db6df1a9fb0319c5e3b2cfc32efaee86538d250");  // 8: JohnnyLawDGB
        consensus.vOraclePublicKeys.push_back("d2f9b0e00ed2fb0a93d04f12eb250b4adf2e4ac8335692c7942e4cba6e462484");  // 9: Ogilvie
        consensus.vOraclePublicKeys.push_back("028a52c7a3e8f22c44e356dcda43a0e24ed5e8e284c53c902599f0947763113c");  // 10: ChopperBrian
        consensus.vOraclePublicKeys.push_back("dfcb956f9e6f8ceea00b067176baa118ba8f0fbdb171a821a362af19234e64bd");  // 11: hallvardo
        // Slot 12: DaPunzy real key — operator-supplied and live at oracle_id=12.
        consensus.vOraclePublicKeys.push_back("0f84e9bacc11c6b3f58d979adf3b0e6899b69d9f6ecd3e8d48bee2a7ac5a8560");  // 12: DaPunzy (RC30)
        consensus.vOraclePublicKeys.push_back("a3758e484fe8d46ecd2f3c0a56cfc2365464d229737c18e75d97f8893134fab9");  // 13: DigiByteForce
        consensus.vOraclePublicKeys.push_back("f3ab098eb0ceff8259c280cf5a3682e78b298dc300e26cdb4694d8efcd6a11a2");  // 14: Neel (RC30)
        consensus.vOraclePublicKeys.push_back("447153bcec341f2dad94541104f4e8c0a8b19e342dada1b2204ae56cf2b960a6");  // 15: DigiSwarm (RC30)
        consensus.vOraclePublicKeys.push_back("83b9c6d229a6347370517fc11329abebeae511055702b0408158f2205035adc7");  // 16: GTO90 (RC30)

        LogPrintf("Oracle: Mainnet Phase 3 (MuSig2) at block %d, %d-of-%d quorum\n",
                 consensus.nDigiDollarPhase3Height, consensus.nOracleConsensusRequired,
                 consensus.nOraclePubkeyCount);
    }

private:
    void InitializeOracleNodes() {
        // DigiDollar Oracle Nodes - 30 hardcoded trusted providers
        // These use compressed public keys (33 bytes) and unique endpoints
        vOracleNodes = {
            // Oracle 0-9: Primary Tier 1 providers
            // Oracle 0: Testnet primary oracle (uses G point key for Phase One testing)
            {0,  ParsePubKey("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"), "oracle1.digibyte.io:12028", true},
            {1,  ParsePubKey("02d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35"), "oracle2.digidollar.org:9002", true},
            {2,  ParsePubKey("034e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce"), "oracle3.digidollar.org:9003", true},
            {3,  ParsePubKey("024b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a"), "oracle4.digidollar.org:9004", true},
            {4,  ParsePubKey("03ef2d127de37b942baad06145e54b0c619a1f22327b2ebbcfbec78f5564afe39d"), "oracle5.digidollar.org:9005", true},
            {5,  ParsePubKey("02e7f6c011776e8db7cd330b54174fd76f7d0216b612387a5ffcfb81e6f0919683"), "oracle6.digidollar.org:9006", true},
            {6,  ParsePubKey("037902699be42c8a8e46fbbb4501726517e86b22c56a189f7625a6da49081b2451"), "oracle7.digidollar.org:9007", true},
            {7,  ParsePubKey("022c624232cdd221771294dfbb310aca000a0df6ac8b66b696d90ef06fdefb64a3"), "oracle8.digidollar.org:9008", true},
            {8,  ParsePubKey("0319581e27de7ced00ff1ce50b2047e7a567c76b1cbaebabe5ef03f7c3017bb5b7"), "oracle9.digidollar.org:9009", true},
            {9,  ParsePubKey("024a44dc15364204a80fe80e9039455cc1608281820fe2b24f1e5233ade6af1dd5"), "oracle10.digidollar.org:9010", true},

            // Oracle 10-19: Secondary Tier 2 providers
            {10,  ParsePubKey("034fc82b26aecb47d2868c4efbe3581732a3e7cbcc6c2efb32062c08170a05eeb8"), "oracle11.digidollar.org:9011", true},
            {11,  ParsePubKey("026b51d431df5d7f141cbececcf79edf3dd861c3b4069f0b11661a3eefacbba918"), "oracle12.digidollar.org:9012", true},
            {12,  ParsePubKey("033fdba35f04dc8c462986c992bcf875546257113072a909c162f7e470e581e278"), "oracle13.digidollar.org:9013", true},
            {13,  ParsePubKey("028527a891e224136950ff32ca212b45bc93f69fbb801c3b1ebedac52775f99e61"), "oracle14.digidollar.org:9014", true},
            {14,  ParsePubKey("03e629fa6598d732768f7c726b4b621285f9c3b85303900aa912017db7617d8bdb"), "oracle15.digidollar.org:9015", true},
            {15,  ParsePubKey("02b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9"), "oracle16.digidollar.org:9016", true},
            {16,  ParsePubKey("034523540f1504cd17100c4835e85b7eefd49911580f8efff0599a8f283be6b9e3"), "oracle17.digidollar.org:9017", true},
            {17,  ParsePubKey("024ec9599fc203d176a301536c2e091a19bc852759b255bd6818810a42c5fed14a"), "oracle18.digidollar.org:9018", true},
            {18,  ParsePubKey("039400f1b21cb527d7fa3d3eabba93557a18ebe7a2ca4e471cfe5e4c5b4ca7f767"), "oracle19.digidollar.org:9019", true},
            {19,  ParsePubKey("02f5ca38f748a1d6eaf726b8a42fb575c3c71f1864a8143301782de13da2d9202b"), "oracle20.digidollar.org:9020", true},

            // Oracle 20-29: Backup Tier 3 providers
            {20,  ParsePubKey("026b17dedd3346cf0ee1a1edd41d00f6ad3e5e43f628582f59624821682a1c28e4"), "oracle21.digidollar.org:9021", true},
            {21,  ParsePubKey("03d8c8994c3f8c5f9f6ff8f9f6c6a8e5e4a8c8e7f6a5d4c3b2a1f0e9d8c7b6a5f4"), "oracle22.digidollar.org:9022", true},
            {22,  ParsePubKey("02356a192b7913b04c54574d18c28d46e6395428ab3e43ad63e5e5c4cf9ef18c0c"), "oracle23.digidollar.org:9023", true},
            {23,  ParsePubKey("03da4b9237bacccdf19c0760cab7aec4a8359010b04d69a05fb6ed5a5e9cdb37f6"), "oracle24.digidollar.org:9024", true},
            {24,  ParsePubKey("0277de68daecd823babbb58edb1c8e14d7106e83bb6d6ba7c1c6e0e476c6de5273"), "oracle25.digidollar.org:9025", true},
            {25,  ParsePubKey("031b6453892473a467d07372d45eb05abc20316478a8f89a0e74023c3c05a8e1e7"), "oracle26.digidollar.org:9026", true},
            {26,  ParsePubKey("02ac3478d69a3c81fa62e60f5c3696165a4e5e6ac4cda4d41d92ee258b91e23e38"), "oracle27.digidollar.org:9027", true},
            {27,  ParsePubKey("03c1dfd96eea8cc2b62785275bca38ac261256e27864cf4f74db1c3bb5d8d08c42"), "oracle28.digidollar.org:9028", true},
            {28,  ParsePubKey("02902ba3cda1883801594b6e1b452790cc53948fda6c45e47c74e0fb4e8a8088b9"), "oracle29.digidollar.org:9029", true},
            {29,  ParsePubKey("03fe5dbbcea5ce7e2988b8c69bcfdfde8904aabc1f79c7b2bf6cd5dbec0d93c26c"), "oracle30.digidollar.org:9030", true}
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    explicit CTestNetParams(const CChainParams::TestNetOptions& options) {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 300;
        consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256S("0x00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105"), SCRIPT_VERIFY_NONE);
        consensus.BIP34Height = 1; // BIP34 activated on testnet (Testnet reset 2025)
        consensus.BIP34Hash = uint256S("0x0");
        consensus.BIP65Height = 1; // BIP65 activated on testnet (Testnet reset 2025)
        consensus.BIP66Height = 1; // BIP66 activated on testnet (Testnet reset 2025)
        consensus.CSVHeight = 1; // CSV activated on testnet (Used in rpc activation tests)
        consensus.SegwitHeight = 0; // SEGWIT is always activated on testnet unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = ArithToUint256(~arith_uint256(0) >> 20);

        consensus.initialTarget[ALGO_SHA256D] = ArithToUint256(~arith_uint256(0) >> 31); // production testnet (2048x powLimit)

        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60 / 4;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fEasyPow = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 140; // 140 - 70% of 200
        consensus.nMinerConfirmationWindow = 200; // 200 blocks for fast BIP9 testing
        consensus.fRbfEnabled = false;
        
        // DigiByte Specific Consensus Code from v8.22.2
        consensus.nTargetTimespan =  0.10 * 24 * 60 * 60; // 2.4 hours
        consensus.nTargetSpacing = 60; // 60 seconds
        consensus.nInterval = consensus.nTargetTimespan / consensus.nTargetSpacing;
        // Emission schedule heights (compressed for testnet, similar to regtest)
        // These control BOTH DigiShield activation AND emission period boundaries
        consensus.nDiffChangeTarget = 67; // DigiShield + Period III/IV boundary (mainnet: 67200)
        consensus.patchBlockRewardDuration = 10; // Weekly reward decay blocks (same as regtest)
        consensus.patchBlockRewardDuration2 = 80; // Monthly reward decay blocks (same as regtest)
        consensus.nTargetTimespanRe = 1*60; // 60 Seconds
        consensus.nTargetSpacingRe = 1*60; // 60 seconds
        consensus.nIntervalRe = consensus.nTargetTimespanRe / consensus.nTargetSpacingRe; // 1 block
        consensus.nAveragingInterval = 10; // 10 blocks
        consensus.multiAlgoTargetSpacing = 30*5; // NUM_ALGOS * 30 seconds
        consensus.multiAlgoTargetSpacingV4 = 15*5; // NUM_ALGOS * 15 seconds
        consensus.nAveragingTargetTimespan = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacing;
        consensus.nAveragingTargetTimespanV4 = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacingV4;
        consensus.nMaxAdjustDown = 40; // 40% adjustment down
        consensus.nMaxAdjustUp = 20; // 20% adjustment up
        consensus.nMaxAdjustDownV3 = 16; // 16% adjustment down
        consensus.nMaxAdjustUpV3 = 8; // 8% adjustment up
        consensus.nMaxAdjustDownV4 = 16;
        consensus.nMaxAdjustUpV4 = 8;
        consensus.nMinActualTimespan = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUp) / 100;
        consensus.nMaxActualTimespan = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDown) / 100;
        consensus.nMinActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUpV3) / 100;
        consensus.nMaxActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustUpV3) / 100;
        consensus.nMinActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 - consensus.nMaxAdjustUpV4) / 100;
        consensus.nMaxActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 + consensus.nMaxAdjustUpV4) / 100;
        consensus.nLocalTargetAdjustment = 4; // target adjustment per algo
        consensus.nLocalDifficultyAdjustment = 4; // difficulty adjustment per algo

        // DigiByte Hard Fork Block Heights for testnet (compressed like regtest)
        // Multi-algo is enabled immediately on testnet23 so automated mining can
        // bootstrap the chain quickly from genesis.
        consensus.multiAlgoDiffChangeTarget = 0; // Height 0 MultiAlgo activation
        consensus.alwaysUpdateDiffChangeTarget = 200; // Block 200 MultiShield Hard Fork
        consensus.workComputationChangeTarget = 400; // Block 400 DigiSpeed Hard Fork
        consensus.algoSwapChangeTarget = 500; // Block 500 Odo PoW Hard Fork
        consensus.OdoHeight = 500; // Odocrypt activation at height 500
        consensus.ReserveAlgoBitsHeight = 0;
        consensus.nOdoShapechangeInterval = 1*24*60*60; // 1 day
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342) - Always active for testnet
        // DigiDollar requires P2TR (Taproot) scripts, so Taproot must be active
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        // Deployment of DigiDollar stablecoin features (testnet - real BIP9 signaling)
        // Miners signal bit 23, 70% threshold (140/200 blocks)
        // BIP9 activation sequence with nMinerConfirmationWindow=200:
        //   Window 0 (blocks 0-199):   DEFINED
        //   Window 1 (blocks 200-399): STARTED  — miners begin signaling bit 23
        //   Window 2 (blocks 400-599): LOCKED_IN — if 140/200 blocks signaled
        //   Window 3 (blocks 600+):    ACTIVE   — min_activation_height=600 satisfied
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].bit = 23;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = 1763932527; // Genesis timestamp (already past, signaling starts at first window boundary)
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = 1830297600; // Jan 1, 2028
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 600; // Activation delayed until block 600

        consensus.nMinimumChainWork = uint256S("0x00");
        consensus.defaultAssumeValid = uint256S("0x00"); //1079274

        // NEW TESTNET MAGIC BYTES (2026) - DigiDollar Reset
        pchMessageStart[0] = 0xfd;
        pchMessageStart[1] = 0xd2;
        pchMessageStart[2] = 0xb9;
        pchMessageStart[3] = 0xe4;
        nDefaultPort = 12030;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 40;
        m_assumed_chain_state_size = 2;

        // DigiDollar testnet23 genesis (RC30, 2026) — 9-of-17 MuSig2 oracle consensus
        const char* pszTimestamp = "DigiDollar Testnet23: 9-of-17 MuSig2 Oracle Consensus on DigiByte";
        const CScript genesisOutputScript = CScript() << 0x0 << OP_CHECKSIG;
        genesis = CreateGenesisBlock(pszTimestamp, genesisOutputScript, 1776456000, 561075, 0x1e0ffff0, 1, 8000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0xa19e809bb060f7f50c05a9bec7fdefedd8497aa0bd6ccca6f55c86090963e4ca"));
        assert(genesis.hashMerkleRoot == uint256S("0x14d23080efd3f7ff071e1c250b6fa57d7a3be839c19f05d70d7d04b1341bba97"));

        vFixedSeeds.clear();
        vSeeds.clear();

        // DigiByte TESTNET DNS Seed Servers:
        vSeeds.emplace_back("testnetseed.digibyte.io"); // Jared Tate @JaredTate
        vSeeds.emplace_back("testnetseed.digibyte.link"); // Bastian Driessen @bastiandriessen
        vSeeds.emplace_back("testnetseed.digibyte.services"); // Craig Donnachie @cdonnachie

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,126);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,140);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,254);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "dgbt";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                // testnet23: fresh chain, no checkpoints yet
            }
        };

        m_assumeutxo_data = {
            // TODO to be specified in a future patch.
        };

        chainTxData = ChainTxData{
            // testnet23 starts from a fresh RC30 reset genesis
            .nTime    = 1776456000,
            .nTxCount = 1,
            .dTxRate  = 0.0,
        };

        // DigiDollar consensus parameters (testnet)
        digidollarParams = DigiDollar::ConsensusParams();
        // Testnet specific adjustments for easier testing
        // NOTE: DD amounts are in CENTS, not satoshis. 100 cents = $1.00
        digidollarParams.minMintAmount = 10000;            // 10,000 cents = $100 minimum
        digidollarParams.minMintAmountActivationHeight = 1;  // Activate $100 min at height 150000
        digidollarParams.maxMintAmount = 1000000;          // 1,000,000 cents = $10k maximum
        digidollarParams.oracleThreshold = 9;                         // 9-of-17 consensus for Phase Two (strict majority, RC30)
        digidollarParams.activeOracles = 17;                          // 17 active oracles (RC30)
        digidollarParams.oracleCount = 17;                            // Total 17 oracles defined (RC30)

        // Initialize DigiDollar Oracle Nodes (same as mainnet for compatibility)
        InitializeOracleNodes();

        // Oracle system parameters — Testnet 9-of-17 consensus (RC30)
        // Oracle activation matches DigiDollar BIP9 activation — everything at block 600
        consensus.nOracleActivationHeight = 600;      // Same as nDDActivationHeight
        consensus.nOracleEpochLength = 1440;          // 24 hours (1440 blocks * 15 seconds)
        consensus.nOracleRequiredMessages = 9;        // Phase Two: 9-of-17 consensus (RC30)
        consensus.nOracleTotalOracles = 17;           // 17 oracles active (RC30)
        // Multi-oracle activates at same height as DigiDollar (no separate phases)
        // Everything — DD minting, oracle consensus, multi-oracle — activates together
        consensus.nDigiDollarPhase2Height = 600;  // Same as nDDActivationHeight
        consensus.nDigiDollarPhase3Height = 0;  // Phase Three (MuSig2): active immediately on testnet

        // Phase 3 MuSig2 oracle configuration — 9-of-17 quorum (RC30)
        consensus.nOraclePubkeyCount = 17;
        consensus.nOracleConsensusRequired = 9;

        // Testnet oracle public keys (x-only, 32 bytes) — ordered by oracle slot (0-16)
        // for deterministic MuSig2 key aggregation.
        //
        // Production testnet oracle public keys (9-of-17, RC30):
        // 17 active operators, including DigiSwarm at slot 15.
        consensus.vOraclePublicKeys.clear();
        consensus.vOraclePublicKeys.push_back("e1dce189a530c1fb39dcd9282cf5f9de0e4eb257344be9fd94ce27c06005e8c7");  // 0: Jared
        consensus.vOraclePublicKeys.push_back("3dfb7a36ab40fa6fbc69b4b499eaa17bfa1958aa89ec248efc24b4c18694f990");  // 1: Green Candle
        consensus.vOraclePublicKeys.push_back("172755a320cec96c981d46c86d79a03578d73406a25e89d8edc616a8f361cb5c");  // 2: Bastian
        consensus.vOraclePublicKeys.push_back("546c07ee9d21640c4b4e96e6954bd49c3ab5bcf36c6a512603ebf75f8609da0c");  // 3: DanGB
        consensus.vOraclePublicKeys.push_back("9cef021f841794c1afc4e84d678f3c70dbe3a972330b2b6329852898443deb4f");  // 4: Shenger
        consensus.vOraclePublicKeys.push_back("85016758856ed27388501a54031fa3a678df705bf811fb8bc9abd2d7cfb6d9f7");  // 5: Ycagel
        consensus.vOraclePublicKeys.push_back("7a858e055099e4a9cf8273e9171da148d4fd00afd4376b60dc1cd09974731b51");  // 6: Aussie
        consensus.vOraclePublicKeys.push_back("2d8c9f054d7087e263016c0800ad1c2f8106859e772766b9f8179042d1792a09");  // 7: LookInto
        consensus.vOraclePublicKeys.push_back("89d5c588c8e0d311028f2f7e0db6df1a9fb0319c5e3b2cfc32efaee86538d250");  // 8: JohnnyLawDGB
        consensus.vOraclePublicKeys.push_back("d2f9b0e00ed2fb0a93d04f12eb250b4adf2e4ac8335692c7942e4cba6e462484");  // 9: Ogilvie
        consensus.vOraclePublicKeys.push_back("028a52c7a3e8f22c44e356dcda43a0e24ed5e8e284c53c902599f0947763113c");  // 10: ChopperBrian
        consensus.vOraclePublicKeys.push_back("dfcb956f9e6f8ceea00b067176baa118ba8f0fbdb171a821a362af19234e64bd");  // 11: hallvardo
        consensus.vOraclePublicKeys.push_back("0f84e9bacc11c6b3f58d979adf3b0e6899b69d9f6ecd3e8d48bee2a7ac5a8560");  // 12: DaPunzy (RC30)
        consensus.vOraclePublicKeys.push_back("a3758e484fe8d46ecd2f3c0a56cfc2365464d229737c18e75d97f8893134fab9");  // 13: DigiByteForce
        consensus.vOraclePublicKeys.push_back("f3ab098eb0ceff8259c280cf5a3682e78b298dc300e26cdb4694d8efcd6a11a2");  // 14: Neel (RC30)
        consensus.vOraclePublicKeys.push_back("447153bcec341f2dad94541104f4e8c0a8b19e342dada1b2204ae56cf2b960a6");  // 15: DigiSwarm (RC30)
        consensus.vOraclePublicKeys.push_back("83b9c6d229a6347370517fc11329abebeae511055702b0408158f2205035adc7");  // 16: GTO90 (RC30)
        //
        // LOCAL MINI-TESTNET TESTING (disabled; retained for future use).
        // Test keys derived from SHA256("digibyte_testnet_oracle_N"), N=0..16.
        // Enable this block (and the matching vOracleNodes block in
        // InitializeOracleNodes()) when running test_multi_oracle_testnet.sh.
        //
        // consensus.vOraclePublicKeys.clear();
        // consensus.vOraclePublicKeys.push_back("a69d02b2e39684deacead23e3b34191af99a124411465b19bc0a6e1384f70dee");  //  0: test oracle 0
        // consensus.vOraclePublicKeys.push_back("56baa94b6a787502146177802820189113029d31c4d99b0a5ad59bbb776efc88");  //  1: test oracle 1
        // consensus.vOraclePublicKeys.push_back("df3f298148c79840218722a4dfc81413d3dd4597a1aa3b97f12b3810bb21e4e9");  //  2: test oracle 2
        // consensus.vOraclePublicKeys.push_back("692857a4e171ce477d16afc8c4f225d479f139eaa4baee796f195f7d67927d81");  //  3: test oracle 3
        // consensus.vOraclePublicKeys.push_back("977e8e7355bf6718adf79e3e0cc874f9c98893fa450bfe9d433ae7643b6f6440");  //  4: test oracle 4
        // consensus.vOraclePublicKeys.push_back("ccdca14f48d2e2c7225e378d3d0b40f6aebbc0aef783a6971479cae4d105fc5e");  //  5: test oracle 5
        // consensus.vOraclePublicKeys.push_back("f3c51b026ce02416d806c76e046fb8c32dce4e7ab82a1bb163a1a852f521a7ac");  //  6: test oracle 6
        // consensus.vOraclePublicKeys.push_back("b003867d046e3ea6876e1d33fca9018fd9b3f130f9fcb8a0904d9f58e8cf6141");  //  7: test oracle 7
        // consensus.vOraclePublicKeys.push_back("75a81280ef4e7d977a3234ca9c6860e15243accb7565ff133fb02eda174df34e");  //  8: test oracle 8
        // consensus.vOraclePublicKeys.push_back("a31a0d23ab9d2b2e9fdea406ccc902ff6812b2455985edd54ddba37402283cbb");  //  9: test oracle 9
        // consensus.vOraclePublicKeys.push_back("9f802e732aa1b71e6ebf2385a21d70945c04d7a79adaaf1a72b8358e7d9c8466");  // 10: test oracle 10
        // consensus.vOraclePublicKeys.push_back("ab2ca9a4d43ea036eab82375b3eebe6a4422cb8c327bcdd144813f0d111603fd");  // 11: test oracle 11
        // consensus.vOraclePublicKeys.push_back("bc7117d979782809de3012f2946a91a38af36176c171c1f214282c6958387990");  // 12: test oracle 12
        // consensus.vOraclePublicKeys.push_back("954543733113170ff2fafad02b372ed14ed1692640e0299d7c5d2701f21c5d32");  // 13: test oracle 13
        // consensus.vOraclePublicKeys.push_back("07ad71f90ef3eac0f599a1e233c2b1cb79f10d547ef5615c73935a7e25a30c27");  // 14: test oracle 14
        // consensus.vOraclePublicKeys.push_back("c8a52a79f57a6682396f8cc2589f8cd639434128a96e0bf245987d12e8896016");  // 15: test oracle 15
        // consensus.vOraclePublicKeys.push_back("b2f5c1f7dba8484c08a1b6d8a1b47320586edfdfbd53aedb59522f8154ed6f89");  // 16: test oracle 16

        if (options.easy_pow) {
            EnableLocalMiniTestnetMode();
        }

        LogPrintf("Oracle: Testnet oracle activation height: %d\n", consensus.nOracleActivationHeight);
        LogPrintf("Oracle: %d oracles configured, %d-of-%d consensus, Phase Two at height %d\n",
                 (int)consensus.vOraclePublicKeys.size(), consensus.nOracleRequiredMessages,
                 consensus.nOracleTotalOracles, consensus.nDigiDollarPhase2Height);

        // Testnet-specific oracle and activation settings
        consensus.nDDOracleEpochBlocks = 50;       // Rotate oracles every 50 blocks (~12.5 minutes)
        consensus.nDDOracleUpdateInterval = 2;     // Update price every 2 blocks (~30 seconds)
        consensus.nDDActivationHeight = 600;       // DigiDollar BIP9 activation at block 600 (DEFINED→STARTED→LOCKED_IN→ACTIVE)
    }

private:
    void EnableLocalMiniTestnetMode() {
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fEasyPow = true;

        consensus.vOraclePublicKeys.clear();
        consensus.vOraclePublicKeys.push_back("a69d02b2e39684deacead23e3b34191af99a124411465b19bc0a6e1384f70dee");  //  0: test oracle 0
        consensus.vOraclePublicKeys.push_back("56baa94b6a787502146177802820189113029d31c4d99b0a5ad59bbb776efc88");  //  1: test oracle 1
        consensus.vOraclePublicKeys.push_back("df3f298148c79840218722a4dfc81413d3dd4597a1aa3b97f12b3810bb21e4e9");  //  2: test oracle 2
        consensus.vOraclePublicKeys.push_back("692857a4e171ce477d16afc8c4f225d479f139eaa4baee796f195f7d67927d81");  //  3: test oracle 3
        consensus.vOraclePublicKeys.push_back("977e8e7355bf6718adf79e3e0cc874f9c98893fa450bfe9d433ae7643b6f6440");  //  4: test oracle 4
        consensus.vOraclePublicKeys.push_back("ccdca14f48d2e2c7225e378d3d0b40f6aebbc0aef783a6971479cae4d105fc5e");  //  5: test oracle 5
        consensus.vOraclePublicKeys.push_back("f3c51b026ce02416d806c76e046fb8c32dce4e7ab82a1bb163a1a852f521a7ac");  //  6: test oracle 6
        consensus.vOraclePublicKeys.push_back("b003867d046e3ea6876e1d33fca9018fd9b3f130f9fcb8a0904d9f58e8cf6141");  //  7: test oracle 7
        consensus.vOraclePublicKeys.push_back("75a81280ef4e7d977a3234ca9c6860e15243accb7565ff133fb02eda174df34e");  //  8: test oracle 8
        consensus.vOraclePublicKeys.push_back("a31a0d23ab9d2b2e9fdea406ccc902ff6812b2455985edd54ddba37402283cbb");  //  9: test oracle 9
        consensus.vOraclePublicKeys.push_back("9f802e732aa1b71e6ebf2385a21d70945c04d7a79adaaf1a72b8358e7d9c8466");  // 10: test oracle 10
        consensus.vOraclePublicKeys.push_back("ab2ca9a4d43ea036eab82375b3eebe6a4422cb8c327bcdd144813f0d111603fd");  // 11: test oracle 11
        consensus.vOraclePublicKeys.push_back("bc7117d979782809de3012f2946a91a38af36176c171c1f214282c6958387990");  // 12: test oracle 12
        consensus.vOraclePublicKeys.push_back("954543733113170ff2fafad02b372ed14ed1692640e0299d7c5d2701f21c5d32");  // 13: test oracle 13
        consensus.vOraclePublicKeys.push_back("07ad71f90ef3eac0f599a1e233c2b1cb79f10d547ef5615c73935a7e25a30c27");  // 14: test oracle 14
        consensus.vOraclePublicKeys.push_back("c8a52a79f57a6682396f8cc2589f8cd639434128a96e0bf245987d12e8896016");  // 15: test oracle 15
        consensus.vOraclePublicKeys.push_back("b2f5c1f7dba8484c08a1b6d8a1b47320586edfdfbd53aedb59522f8154ed6f89");  // 16: test oracle 16

        vOracleNodes = {
            { 0, ParsePubKey("03a69d02b2e39684deacead23e3b34191af99a124411465b19bc0a6e1384f70dee"), "localhost:9001", true},
            { 1, ParsePubKey("0356baa94b6a787502146177802820189113029d31c4d99b0a5ad59bbb776efc88"), "localhost:9002", true},
            { 2, ParsePubKey("03df3f298148c79840218722a4dfc81413d3dd4597a1aa3b97f12b3810bb21e4e9"), "localhost:9003", true},
            { 3, ParsePubKey("03692857a4e171ce477d16afc8c4f225d479f139eaa4baee796f195f7d67927d81"), "localhost:9004", true},
            { 4, ParsePubKey("03977e8e7355bf6718adf79e3e0cc874f9c98893fa450bfe9d433ae7643b6f6440"), "localhost:9005", true},
            { 5, ParsePubKey("02ccdca14f48d2e2c7225e378d3d0b40f6aebbc0aef783a6971479cae4d105fc5e"), "localhost:9006", true},
            { 6, ParsePubKey("02f3c51b026ce02416d806c76e046fb8c32dce4e7ab82a1bb163a1a852f521a7ac"), "localhost:9007", true},
            { 7, ParsePubKey("03b003867d046e3ea6876e1d33fca9018fd9b3f130f9fcb8a0904d9f58e8cf6141"), "localhost:9008", true},
            { 8, ParsePubKey("0275a81280ef4e7d977a3234ca9c6860e15243accb7565ff133fb02eda174df34e"), "localhost:9009", true},
            { 9, ParsePubKey("02a31a0d23ab9d2b2e9fdea406ccc902ff6812b2455985edd54ddba37402283cbb"), "localhost:9010", true},
            {10, ParsePubKey("039f802e732aa1b71e6ebf2385a21d70945c04d7a79adaaf1a72b8358e7d9c8466"), "localhost:9011", true},
            {11, ParsePubKey("02ab2ca9a4d43ea036eab82375b3eebe6a4422cb8c327bcdd144813f0d111603fd"), "localhost:9012", true},
            {12, ParsePubKey("02bc7117d979782809de3012f2946a91a38af36176c171c1f214282c6958387990"), "localhost:9013", true},
            {13, ParsePubKey("02954543733113170ff2fafad02b372ed14ed1692640e0299d7c5d2701f21c5d32"), "localhost:9014", true},
            {14, ParsePubKey("0207ad71f90ef3eac0f599a1e233c2b1cb79f10d547ef5615c73935a7e25a30c27"), "localhost:9015", true},
            {15, ParsePubKey("02c8a52a79f57a6682396f8cc2589f8cd639434128a96e0bf245987d12e8896016"), "localhost:9016", true},
            {16, ParsePubKey("02b2f5c1f7dba8484c08a1b6d8a1b47320586edfdfbd53aedb59522f8154ed6f89"), "localhost:9017", true},
        };

        LogPrintf("Testnet local mini-testnet mode enabled via -easypow: easy PoW + local oracle set\n");
    }

    void InitializeOracleNodes() {
        // DigiDollar Oracle Nodes - Testnet
        // 17 oracles active (9-of-17 consensus, RC30)
        //
        // Production testnet oracle nodes.
        vOracleNodes = {
            { 0, ParsePubKey("03e1dce189a530c1fb39dcd9282cf5f9de0e4eb257344be9fd94ce27c06005e8c7"), "oracle1.digibyte.io:12030",  true},  // Jared
            { 1, ParsePubKey("033dfb7a36ab40fa6fbc69b4b499eaa17bfa1958aa89ec248efc24b4c18694f990"), "oracle2.digibyte.io:12030",  true},  // Green Candle
            { 2, ParsePubKey("03172755a320cec96c981d46c86d79a03578d73406a25e89d8edc616a8f361cb5c"), "oracle3.digibyte.io:12030",  true},  // Bastian
            { 3, ParsePubKey("03546c07ee9d21640c4b4e96e6954bd49c3ab5bcf36c6a512603ebf75f8609da0c"), "oracle4.digibyte.io:12030",  true},  // DanGB
            { 4, ParsePubKey("039cef021f841794c1afc4e84d678f3c70dbe3a972330b2b6329852898443deb4f"), "oracle5.digibyte.io:12030",  true},  // Shenger
            { 5, ParsePubKey("0285016758856ed27388501a54031fa3a678df705bf811fb8bc9abd2d7cfb6d9f7"), "oracle6.digibyte.io:12030",  true},  // Ycagel
            { 6, ParsePubKey("037a858e055099e4a9cf8273e9171da148d4fd00afd4376b60dc1cd09974731b51"), "oracle7.digibyte.io:12030",  true},  // Aussie
            { 7, ParsePubKey("032d8c9f054d7087e263016c0800ad1c2f8106859e772766b9f8179042d1792a09"), "oracle8.digibyte.io:12030",  true},  // LookInto
            { 8, ParsePubKey("0389d5c588c8e0d311028f2f7e0db6df1a9fb0319c5e3b2cfc32efaee86538d250"), "oracle9.digibyte.io:12030",  true},  // JohnnyLawDGB
            { 9, ParsePubKey("02d2f9b0e00ed2fb0a93d04f12eb250b4adf2e4ac8335692c7942e4cba6e462484"), "129.212.182.152:12030",      true},  // Ogilvie
            {10, ParsePubKey("02028a52c7a3e8f22c44e356dcda43a0e24ed5e8e284c53c902599f0947763113c"), "oracle11.digibyte.io:12030", true},  // ChopperBrian
            {11, ParsePubKey("024ef063a67b35295e9eaaa9251bc7f0effbceaedc8e9bc92504b0da832744ca08"), "oracle12.digibyte.io:12030", true},  // hallvardo (key rotated for RC31)
            {12, ParsePubKey("020f84e9bacc11c6b3f58d979adf3b0e6899b69d9f6ecd3e8d48bee2a7ac5a8560"), "oracle13.digibyte.io:12030", true},  // DaPunzy (RC30)
            {13, ParsePubKey("03a3758e484fe8d46ecd2f3c0a56cfc2365464d229737c18e75d97f8893134fab9"), "oracle14.digibyte.io:12030", true},  // DigiByteForce
            {14, ParsePubKey("03f3ab098eb0ceff8259c280cf5a3682e78b298dc300e26cdb4694d8efcd6a11a2"), "oracle15.digibyte.io:12030", true},  // Neel (RC30)
            {15, ParsePubKey("03447153bcec341f2dad94541104f4e8c0a8b19e342dada1b2204ae56cf2b960a6"), "oracle16.digibyte.io:12030", true},  // DigiSwarm (RC30)
            {16, ParsePubKey("0283b9c6d229a6347370517fc11329abebeae511055702b0408158f2205035adc7"), "oracle17.digibyte.io:12030", true},  // GTO90 (RC30)
        };
        //
        // LOCAL MINI-TESTNET TESTING (disabled; retained for future use).
        // Test keys derived from SHA256("digibyte_testnet_oracle_N"), N=0..16.
        // Enable this block (and the matching vOraclePublicKeys block in CTestNetParams)
        // when running test_multi_oracle_testnet.sh.
        //
        // vOracleNodes = {
        //     { 0, ParsePubKey("03a69d02b2e39684deacead23e3b34191af99a124411465b19bc0a6e1384f70dee"), "localhost:9001", true},   // test oracle 0
        //     { 1, ParsePubKey("0356baa94b6a787502146177802820189113029d31c4d99b0a5ad59bbb776efc88"), "localhost:9002", true},   // test oracle 1
        //     { 2, ParsePubKey("03df3f298148c79840218722a4dfc81413d3dd4597a1aa3b97f12b3810bb21e4e9"), "localhost:9003", true},   // test oracle 2
        //     { 3, ParsePubKey("03692857a4e171ce477d16afc8c4f225d479f139eaa4baee796f195f7d67927d81"), "localhost:9004", true},   // test oracle 3
        //     { 4, ParsePubKey("03977e8e7355bf6718adf79e3e0cc874f9c98893fa450bfe9d433ae7643b6f6440"), "localhost:9005", true},   // test oracle 4
        //     { 5, ParsePubKey("02ccdca14f48d2e2c7225e378d3d0b40f6aebbc0aef783a6971479cae4d105fc5e"), "localhost:9006", true},   // test oracle 5
        //     { 6, ParsePubKey("02f3c51b026ce02416d806c76e046fb8c32dce4e7ab82a1bb163a1a852f521a7ac"), "localhost:9007", true},   // test oracle 6
        //     { 7, ParsePubKey("03b003867d046e3ea6876e1d33fca9018fd9b3f130f9fcb8a0904d9f58e8cf6141"), "localhost:9008", true},   // test oracle 7
        //     { 8, ParsePubKey("0275a81280ef4e7d977a3234ca9c6860e15243accb7565ff133fb02eda174df34e"), "localhost:9009", true},   // test oracle 8
        //     { 9, ParsePubKey("02a31a0d23ab9d2b2e9fdea406ccc902ff6812b2455985edd54ddba37402283cbb"), "localhost:9010", true},   // test oracle 9
        //     {10, ParsePubKey("039f802e732aa1b71e6ebf2385a21d70945c04d7a79adaaf1a72b8358e7d9c8466"), "localhost:9011", true},   // test oracle 10
        //     {11, ParsePubKey("02ab2ca9a4d43ea036eab82375b3eebe6a4422cb8c327bcdd144813f0d111603fd"), "localhost:9012", true},   // test oracle 11
        //     {12, ParsePubKey("02bc7117d979782809de3012f2946a91a38af36176c171c1f214282c6958387990"), "localhost:9013", true},   // test oracle 12
        //     {13, ParsePubKey("02954543733113170ff2fafad02b372ed14ed1692640e0299d7c5d2701f21c5d32"), "localhost:9014", true},   // test oracle 13
        //     {14, ParsePubKey("0207ad71f90ef3eac0f599a1e233c2b1cb79f10d547ef5615c73935a7e25a30c27"), "localhost:9015", true},   // test oracle 14
        //     {15, ParsePubKey("02c8a52a79f57a6682396f8cc2589f8cd639434128a96e0bf245987d12e8896016"), "localhost:9016", true},   // test oracle 15
        //     {16, ParsePubKey("02b2f5c1f7dba8484c08a1b6d8a1b47320586edfdfbd53aedb59522f8154ed6f89"), "localhost:9017", true},   // test oracle 16
        // };
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vSeeds.clear();

        if (!options.challenge) {
            bin = ParseHex("512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae");
            vSeeds.emplace_back("seed.signet.digibyte.sprovoost.nl.");

            // Hardcoded nodes can be removed once there are more DNS seeds
            vSeeds.emplace_back("178.128.221.177");
            vSeeds.emplace_back("v7ajjeirttkbnt32wpy3c6w3emwnfr3fkla7hpxcfokr3ysd3kqtzmqd.onion:38333");

            consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000001ad46be4862");
            consensus.defaultAssumeValid = uint256S("0x0000013d778ba3f914530f11f6b69869c9fab54acff85acd7b8201d111f19b7f"); // 150000
            m_assumed_blockchain_size = 1;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                // Data from RPC: getchaintxstats 4096 0000013d778ba3f914530f11f6b69869c9fab54acff85acd7b8201d111f19b7f
                .nTime    = 1688366339,
                .nTxCount = 2262750,
                .dTxRate  = 0.003414084572046456,
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogPrintf("Signet with challenge %s\n", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 300; // DigiByte halving interval for signet (same as testnet)
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60 / 4; // 15 seconds (DigiByte)
        consensus.fPowAllowMinDifficultyBlocks = true; // DigiByte allows min difficulty blocks
        consensus.fEasyPow = false; // DigiByte setting
        consensus.fPowNoRetargeting = false;
        consensus.fRbfEnabled = false; // DigiByte RBF disabled
        
        // DigiByte Specific Consensus Code for signet (same as testnet)
        consensus.nOdoShapechangeInterval = 1*24*60*60; // 1 day
        consensus.nTargetTimespan =  0.10 * 24 * 60 * 60; // 2.4 hours
        consensus.nTargetSpacing = 60; // 60 seconds
        consensus.nInterval = consensus.nTargetTimespan / consensus.nTargetSpacing;
        consensus.nDiffChangeTarget = 67; // DigiShield Hard Fork Block
        consensus.patchBlockRewardDuration = 10; 
        consensus.patchBlockRewardDuration2 = 80;
        consensus.nTargetTimespanRe = 1*60; // 60 Seconds
        consensus.nTargetSpacingRe = 1*60; // 60 seconds
        consensus.nIntervalRe = consensus.nTargetTimespanRe / consensus.nTargetSpacingRe; // 1 block
        consensus.nAveragingInterval = 10; // 10 blocks
        consensus.multiAlgoTargetSpacing = 30*5; // NUM_ALGOS * 30 seconds
        consensus.multiAlgoTargetSpacingV4 = 15*5; // NUM_ALGOS * 15 seconds
        consensus.nAveragingTargetTimespan = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacing;
        consensus.nAveragingTargetTimespanV4 = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacingV4;
        consensus.nMaxAdjustDown = 40; // 40% adjustment down
        consensus.nMaxAdjustUp = 20; // 20% adjustment up
        consensus.nMaxAdjustDownV3 = 16; // 16% adjustment down
        consensus.nMaxAdjustUpV3 = 8; // 8% adjustment up
        consensus.nMaxAdjustDownV4 = 16;
        consensus.nMaxAdjustUpV4 = 8;
        consensus.nMinActualTimespan = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUp) / 100;
        consensus.nMaxActualTimespan = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDown) / 100;
        consensus.nMinActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUpV3) / 100;
        consensus.nMaxActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustUpV3) / 100;
        consensus.nMinActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 - consensus.nMaxAdjustUpV4) / 100;
        consensus.nMaxActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 + consensus.nMaxAdjustUpV4) / 100;
        consensus.nLocalTargetAdjustment = 4; // target adjustment per algo
        consensus.nLocalDifficultyAdjustment = 4; // difficulty adjustment per algo
        
        // DigiByte Hard Fork Block Heights for signet (same as testnet)
        consensus.multiAlgoDiffChangeTarget = 100; // Block 100 MultiAlgo Hard Fork
        consensus.alwaysUpdateDiffChangeTarget = 400; // Block 400 MultiShield Hard Fork
        consensus.workComputationChangeTarget = 1430; // Block 1,430 DigiSpeed Hard Fork
        consensus.algoSwapChangeTarget = 20000; // Block 20,000 Odo PoW Hard Fork
        consensus.OdoHeight = 600;
        consensus.ReserveAlgoBitsHeight = 0;
        consensus.initialTarget[ALGO_ODO] = ArithToUint256(~arith_uint256(0) >> 20); // Same as other algos
        
        consensus.nRuleChangeActivationThreshold = 1815; // 90% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("00000377ae000000000000000000000000000000000000000000000000000000");
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Activation of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        // Activation of DigiDollar stablecoin features (signet - always active)
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].bit = 23;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 0; // No activation delay for ALWAYS_ACTIVE

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 38443;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1598918400, 52613770, 0x1e0377ae, 1, 8000);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x9cf8c097b1afc5a37d2d050d2b423b052c2da2856acf0e41c40af1da334fcbf7"));
        assert(genesis.hashMerkleRoot == uint256S("0x72ddd9496b004221ed0557358846d9248ecd4c440ebd28ed901efc18757d0fad"));

        vFixedSeeds.clear();

        m_assumeutxo_data = {
            {
                .height = 160'000,
                .hash_serialized = AssumeutxoHash{uint256S("0xfe0a44309b74d6b5883d246cb419c6221bcccf0b308c9b59b7d70783dbdf928a")},
                .nChainTx = 2289496,
                .blockhash = uint256S("0x0000003ca3c99aff040f2563c2ad8f8ec88bd0fd6b8f0895cfaf1ef90353a62c")
            }
        };

        // DigiByte: Use same prefixes as testnet for signet
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,126);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,140);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,254);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "dgbt";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 300; // DigiByte halving interval for regtest
        consensus.BIP34Height = 1; // Always active unless overridden
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;  // Always active unless overridden
        consensus.BIP66Height = 1;  // Always active unless overridden
        consensus.CSVHeight = 1;    // Always active unless overridden
        consensus.SegwitHeight = 0; // Always active unless overridden
        consensus.ReserveAlgoBitsHeight = 0; // DigiByte ReserveAlgoBits
        consensus.OdoHeight = 600; // DigiByte Odocrypt height
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // Set initial targets for all algorithms (easy difficulty for regtest)
        consensus.initialTarget[ALGO_SHA256D] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_SCRYPT] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_GROESTL] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_SKEIN] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_QUBIT] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_ODO] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // Odocrypt initial target
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60 / 4; // 15 seconds (DigiByte)
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fEasyPow = true; // DigiByte setting - allow easy pow for regtest
        consensus.fPowNoRetargeting = true; // No retargeting in regtest for faster mining
        consensus.fRbfEnabled = false; // DigiByte RBF disabled
        
        // DigiByte Specific Consensus Code
        consensus.nOdoShapechangeInterval = 10*24*60*60; // 10 days
        consensus.nTargetTimespan = 0.10 * 48 * 60 * 60; // 4.8 hours
        consensus.nTargetSpacing = 60; // 60 seconds
        consensus.nInterval = consensus.nTargetTimespan / consensus.nTargetSpacing;
        consensus.nDiffChangeTarget = 334; // DigiShield Hard Fork Block
        consensus.patchBlockRewardDuration = 10; // DigiByte reward duration
        consensus.patchBlockRewardDuration2 = 80; // DigiByte reward duration 2
        consensus.nTargetTimespanRe = 1*60; // 60 Seconds
        consensus.nTargetSpacingRe = 1*60; // 60 seconds
        consensus.nIntervalRe = consensus.nTargetTimespanRe / consensus.nTargetSpacingRe; // 1 block
        consensus.nAveragingInterval = 10; // 10 blocks
        consensus.multiAlgoTargetSpacing = 30*5; // NUM_ALGOS * 30 seconds
        consensus.multiAlgoTargetSpacingV4 = 15*5; // NUM_ALGOS * 15 seconds
        consensus.nAveragingTargetTimespan = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacing; // 10* NUM_ALGOS * 30
        consensus.nAveragingTargetTimespanV4 = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacingV4; // 10 * NUM_ALGOS * 15
        consensus.nMaxAdjustDown = 40; // 40% adjustment down
        consensus.nMaxAdjustUp = 20; // 20% adjustment up
        consensus.nMaxAdjustDownV3 = 16; // 16% adjustment down
        consensus.nMaxAdjustUpV3 = 8; // 8% adjustment up
        consensus.nMaxAdjustDownV4 = 16;
        consensus.nMaxAdjustUpV4 = 8;
        consensus.nMinActualTimespan = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUp) / 100;
        consensus.nMaxActualTimespan = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDown) / 100;
        consensus.nMinActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUpV3) / 100;
        consensus.nMaxActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustUpV3) / 100;
        consensus.nMinActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 - consensus.nMaxAdjustUpV4) / 100;
        consensus.nMaxActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 + consensus.nMaxAdjustUpV4) / 100;
        consensus.nLocalTargetAdjustment = 4; // target adjustment per algo
        consensus.nLocalDifficultyAdjustment = 4; // difficulty adjustment per algo
        
        // DigiByte Hard Fork Block Heights for regtest
        consensus.multiAlgoDiffChangeTarget = 100; // Block 100 MultiAlgo Hard Fork
        consensus.alwaysUpdateDiffChangeTarget = 200; // Block 200 MultiShield Hard Fork
        consensus.workComputationChangeTarget = 400; // Block 400 DigiSpeed Hard Fork
        consensus.algoSwapChangeTarget = 600; // Block 600 Odo PoW Hard Fork
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].bit = 23;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 0; // No activation delay for ALWAYS_ACTIVE

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18444;
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_NVERSIONBIPS:
                // Handle NVERSIONBIPS deployment
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_RESERVEALGO:
                // Handle ReserveAlgo deployment
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_ODO:
                // Handle Odo deployment
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        genesis = CreateGenesisBlock(1519460922, 4, 0x207fffff, 1, 8000);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x4598a0f2b823aaf9e77ee6d5e46f1edb824191dcd48b08437b7cec17e6ae6e26"));
        assert(genesis.hashMerkleRoot == uint256S("0x72ddd9496b004221ed0557358846d9248ecd4c440ebd28ed901efc18757d0fad"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
                {0, uint256S("4598a0f2b823aaf9e77ee6d5e46f1edb824191dcd48b08437b7cec17e6ae6e26")},
            }
        };

        m_assumeutxo_data = {
            {
                // DigiByte regtest values at height 110
                .height = 110,
                .hash_serialized = AssumeutxoHash{uint256S("0x2da005f8e675e4c37ea7d7266d11d2d9c2485c095fff21692ef299fbba42f87c")}, // TODO: Generate actual UTXO hash
                .nChainTx = 111,
                .blockhash = uint256S("0x56b2d1cd24ac6d9c74d3f06867eb2d1b1ca1d455f635dac3e1a450da96ed6374")
            },
            {
                // For use by test/functional/feature_assumeutxo.py
                // DigiByte regtest values at height 299
                .height = 299,
                .hash_serialized = AssumeutxoHash{uint256S("0x0c3eb8c1b150495afa0aa96879243937ae989b45b9f8cd14947f5eec8ba7a103")}, // TODO: Generate actual UTXO hash
                .nChainTx = 300,
                .blockhash = uint256S("0x2294ffc34eb5504fd3a497ea00b0acc5cf54bad0d17d48e0fc3463ddbae016ca")
            },
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,126);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,140);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,254);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "dgbrt";

        // DigiDollar consensus parameters (regtest)
        digidollarParams = DigiDollar::ConsensusParams();
        // Regtest specific adjustments for rapid testing
        // NOTE: DD amounts are in CENTS, not satoshis. 100 cents = $1.00
        digidollarParams.minMintAmount = 1;              // 1 cent = $0.01 minimum for testing
        digidollarParams.maxMintAmount = 100000;         // 100,000 cents = $1000 maximum for testing
        digidollarParams.oracleThreshold = 1;                             // 1-of-1 consensus for testing
        digidollarParams.activeOracles = 1;                               // Only 1 active oracle for testing
        digidollarParams.oracleCount = 1;                                 // Total 1 oracle for testing
        digidollarParams.priceValidBlocks = 5;                            // Price valid for 5 blocks only

        // Initialize DigiDollar Oracle Nodes (minimal set for regtest)
        InitializeOracleNodes();

        // Regtest-specific oracle and activation settings
        consensus.nDDOracleEpochBlocks = 10;       // Rotate oracles every 10 blocks
        consensus.nDDOracleUpdateInterval = 1;     // Update price every block
        consensus.nDDActivationHeight = 650;       // DigiDollar active from height 650 (after Odocrypt at 600)

        // Oracle system parameters (RegTest uses MockOracleManager)
        consensus.nOracleActivationHeight = 650;   // Same as nDDActivationHeight — everything activates together
        consensus.nOracleEpochLength = 144;        // 2.4 hours (144 blocks * 15 seconds)
        consensus.nOracleRequiredMessages = 4;     // Phase Two: 4-of-7 (matches testnet)
        consensus.nOracleTotalOracles = 7;         // Phase Two: 7 oracles (matches testnet)
        consensus.nDigiDollarPhase2Height = 650;   // Same as nDDActivationHeight — everything activates together
        consensus.nDigiDollarPhase3Height = 0;      // Phase Three: MuSig2 active immediately on regtest

        // Phase 3 MuSig2 oracle configuration — 4-of-7 quorum (lower for testing)
        consensus.nOraclePubkeyCount = 7;
        consensus.nOracleConsensusRequired = 4;

        // RegTest: Oracle public keys for all 7 oracles — sorted lexicographically
        // Deterministic keys derived from SHA256("digibyte_regtest_oracle_N")
        // Matching private keys are in MockOracleManager for test signing
        consensus.vOraclePublicKeys.clear();
        consensus.vOraclePublicKeys.push_back("584d30f3650d998b0b5f8be52f46164aee5f10d332421e63f070da8a38693a96");  // oracle 6
        consensus.vOraclePublicKeys.push_back("8849d466503c5bb3875ada6bff3d6eec6299265b7b0a4e31bb58b7cecf6dd08f");  // oracle 0
        consensus.vOraclePublicKeys.push_back("9991f9e0c61dfe10896ee797f271f2f6d4e2545daf9a22732622c02ce39f43f0");  // oracle 1
        consensus.vOraclePublicKeys.push_back("aa1ebe314382eb820a040acf604a6617db3d063e9ee0640780774650649aaf47");  // oracle 4
        consensus.vOraclePublicKeys.push_back("be6ad50e0bf26af3798af09a2824142fa79ff37f8730b7a54ba34c9fe6f1e8ec");  // oracle 5
        consensus.vOraclePublicKeys.push_back("d2292678e5c549e90420dc4c6311ef01bf014fb0f4b34d07e02b7b98bbc35e9a");  // oracle 2
        consensus.vOraclePublicKeys.push_back("f24df57d6ab29241f0f00dd87d7ce1418852684dc41d186650a452179618234f");  // oracle 3

        LogPrintf("Oracle: RegTest Phase 3 (MuSig2) at block %d, %d-of-%d quorum\n",
                 consensus.nDigiDollarPhase3Height, consensus.nOracleConsensusRequired,
                 consensus.nOraclePubkeyCount);
    }

private:
    void InitializeOracleNodes() {
        // DigiDollar Oracle Nodes - 7 oracles for regtest (matches testnet 4-of-7)
        // Deterministic keys from SHA256("digibyte_regtest_oracle_N") - matching privkeys in MockOracleManager
        vOracleNodes = {
            {0, ParsePubKey("038849d466503c5bb3875ada6bff3d6eec6299265b7b0a4e31bb58b7cecf6dd08f"), "localhost:9001", true},
            {1, ParsePubKey("029991f9e0c61dfe10896ee797f271f2f6d4e2545daf9a22732622c02ce39f43f0"), "localhost:9002", true},
            {2, ParsePubKey("02d2292678e5c549e90420dc4c6311ef01bf014fb0f4b34d07e02b7b98bbc35e9a"), "localhost:9003", true},
            {3, ParsePubKey("02f24df57d6ab29241f0f00dd87d7ce1418852684dc41d186650a452179618234f"), "localhost:9004", true},
            {4, ParsePubKey("02aa1ebe314382eb820a040acf604a6617db3d063e9ee0640780774650649aaf47"), "localhost:9005", true},
            {5, ParsePubKey("03be6ad50e0bf26af3798af09a2824142fa79ff37f8730b7a54ba34c9fe6f1e8ec"), "localhost:9006", true},
            {6, ParsePubKey("03584d30f3650d998b0b5f8be52f46164aee5f10d332421e63f070da8a38693a96"), "localhost:9007", true}
        };
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return TestNet(TestNetOptions{});
}

std::unique_ptr<const CChainParams> CChainParams::TestNet(const TestNetOptions& options)
{
    return std::make_unique<const CTestNetParams>(options);
}

// Helper function implementations
const OracleNodeInfo* CChainParams::GetOracleNode(uint32_t id) const {
    for (const auto& oracle : vOracleNodes) {
        if (oracle.id == id) return &oracle;
    }
    return nullptr;
}

uint32_t CChainParams::GetActiveOracleCount() const {
    // Return network-specific active oracle count based on digidollarParams
    return digidollarParams.activeOracles;
}
