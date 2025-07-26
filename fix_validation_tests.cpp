// DigiByte validation test fix
// This program generates the correct assumeutxo values for DigiByte's regtest chain

#include <chainparams.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util/system.h>
#include <pow.h>

#include <iostream>
#include <iomanip>

// DigiByte-specific block generation for regtest
static CBlock CreateDigiByteRegtestBlock(const CBlock& prev, const CScript& scriptPubKey)
{
    CBlock block;
    
    // Set version
    block.nVersion = 1;
    
    // Set previous block hash
    block.hashPrevBlock = prev.GetHash();
    
    // Set timestamp (must be greater than previous)
    block.nTime = prev.nTime + 1;
    
    // Set bits (regtest difficulty)
    block.nBits = 0x207fffff; // DigiByte regtest difficulty
    
    // Set algo (DigiByte uses ALGO_SCRYPT for regtest)
    block.SetAlgo(ALGO_SCRYPT);
    
    // Create coinbase transaction
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << CScriptNum(0) << OP_0;
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = GetBlockSubsidy(0, Params().GetConsensus());
    coinbase.vout[0].scriptPubKey = scriptPubKey;
    
    // Add coinbase to block
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    
    // Calculate merkle root
    block.hashMerkleRoot = BlockMerkleRoot(block);
    
    // Mine the block (find valid nonce)
    while (!CheckProofOfWork(block.GetPoWHash(block.GetAlgo()), block.nBits, block.GetAlgo(), Params().GetConsensus())) {
        ++block.nNonce;
    }
    
    return block;
}

int main()
{
    // Initialize DigiByte regtest parameters
    SelectParams(ChainType::REGTEST);
    
    const auto& params = Params();
    
    std::cout << "Generating DigiByte regtest assumeutxo values..." << std::endl;
    std::cout << std::endl;
    
    // Start with genesis block
    CBlock block = params.GenesisBlock();
    
    // Generate blocks up to height 110
    CScript scriptPubKey = CScript() << OP_TRUE;
    
    for (int height = 1; height <= 110; ++height) {
        block = CreateDigiByteRegtestBlock(block, scriptPubKey);
        
        if (height == 110) {
            std::cout << "Block 110 hash: " << block.GetHash().ToString() << std::endl;
        }
    }
    
    // Continue to height 299
    for (int height = 111; height <= 299; ++height) {
        block = CreateDigiByteRegtestBlock(block, scriptPubKey);
        
        if (height == 299) {
            std::cout << "Block 299 hash: " << block.GetHash().ToString() << std::endl;
        }
    }
    
    std::cout << std::endl;
    std::cout << "Update these values in src/kernel/chainparams.cpp:" << std::endl;
    std::cout << std::endl;
    std::cout << "For height 110:" << std::endl;
    std::cout << "  .hash_serialized = AssumeutxoHash{uint256{\"<GET_FROM_DUMPTXOUTSET>\"}}" << std::endl;
    std::cout << "  .m_chain_tx_count = 111" << std::endl;
    std::cout << std::endl;
    std::cout << "For height 299:" << std::endl;
    std::cout << "  .hash_serialized = AssumeutxoHash{uint256{\"<GET_FROM_DUMPTXOUTSET>\"}}" << std::endl;
    std::cout << "  .m_chain_tx_count = 300" << std::endl;
    
    return 0;
}