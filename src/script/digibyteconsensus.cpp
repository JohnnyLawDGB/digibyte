// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2009-2022 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/digibyteconsensus.h>

#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/interpreter.h>
#include <version.h>

namespace {

/** A class that deserializes a single CTransaction one time. */
class TxInputStream
{
public:
    TxInputStream(int nVersionIn, const unsigned char *txTo, size_t txToLen) :
    m_version(nVersionIn),
    m_data(txTo),
    m_remaining(txToLen)
    {}

<<<<<<< HEAD
    void read(char* pch, size_t nSize)
    {
        if (nSize > m_remaining)
            throw std::ios_base::failure(std::string(__func__) + ": end of data");

        if (pch == nullptr)
            throw std::ios_base::failure(std::string(__func__) + ": bad destination buffer");

        if (m_data == nullptr)
            throw std::ios_base::failure(std::string(__func__) + ": bad source buffer");

        memcpy(pch, m_data, nSize);
        m_remaining -= nSize;
        m_data += nSize;
=======
    void read(Span<std::byte> dst)
    {
        if (dst.size() > m_remaining) {
            throw std::ios_base::failure(std::string(__func__) + ": end of data");
        }

        if (dst.data() == nullptr) {
            throw std::ios_base::failure(std::string(__func__) + ": bad destination buffer");
        }

        if (m_data == nullptr) {
            throw std::ios_base::failure(std::string(__func__) + ": bad source buffer");
        }

        memcpy(dst.data(), m_data, dst.size());
        m_remaining -= dst.size();
        m_data += dst.size();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    template<typename T>
    TxInputStream& operator>>(T&& obj)
    {
        ::Unserialize(*this, obj);
        return *this;
    }

    int GetVersion() const { return m_version; }
private:
    const int m_version;
    const unsigned char* m_data;
    size_t m_remaining;
};

inline int set_error(digibyteconsensus_error* ret, digibyteconsensus_error serror)
{
    if (ret)
        *ret = serror;
    return 0;
}

<<<<<<< HEAD
struct ECCryptoClosure
{
    ECCVerifyHandle handle;
};

ECCryptoClosure instance_of_eccryptoclosure;
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
} // namespace

/** Check that all specified flags are part of the libconsensus interface. */
static bool verify_flags(unsigned int flags)
{
    return (flags & ~(digibyteconsensus_SCRIPT_FLAGS_VERIFY_ALL)) == 0;
}

static int verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, CAmount amount,
                                    const unsigned char *txTo        , unsigned int txToLen,
<<<<<<< HEAD
=======
                                    const UTXO *spentOutputs, unsigned int spentOutputsLen,
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                                    unsigned int nIn, unsigned int flags, digibyteconsensus_error* err)
{
    if (!verify_flags(flags)) {
        return set_error(err, digibyteconsensus_ERR_INVALID_FLAGS);
    }
<<<<<<< HEAD
    try {
        TxInputStream stream(PROTOCOL_VERSION, txTo, txToLen);
        CTransaction tx(deserialize, stream);
=======

    if (flags & digibyteconsensus_SCRIPT_FLAGS_VERIFY_TAPROOT && spentOutputs == nullptr) {
        return set_error(err, digibyteconsensus_ERR_SPENT_OUTPUTS_REQUIRED);
    }

    try {
        TxInputStream stream(PROTOCOL_VERSION, txTo, txToLen);
        CTransaction tx(deserialize, stream);

        std::vector<CTxOut> spent_outputs;
        if (spentOutputs != nullptr) {
            if (spentOutputsLen != tx.vin.size()) {
                return set_error(err, digibyteconsensus_ERR_SPENT_OUTPUTS_MISMATCH);
            }
            for (size_t i = 0; i < spentOutputsLen; i++) {
                CScript spk = CScript(spentOutputs[i].scriptPubKey, spentOutputs[i].scriptPubKey + spentOutputs[i].scriptPubKeySize);
                const CAmount& value = spentOutputs[i].value;
                CTxOut tx_out = CTxOut(value, spk);
                spent_outputs.push_back(tx_out);
            }
        }

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        if (nIn >= tx.vin.size())
            return set_error(err, digibyteconsensus_ERR_TX_INDEX);
        if (GetSerializeSize(tx, PROTOCOL_VERSION) != txToLen)
            return set_error(err, digibyteconsensus_ERR_TX_SIZE_MISMATCH);

        // Regardless of the verification result, the tx did not error.
        set_error(err, digibyteconsensus_ERR_OK);

        PrecomputedTransactionData txdata(tx);
<<<<<<< HEAD
=======

        if (spentOutputs != nullptr && flags & digibyteconsensus_SCRIPT_FLAGS_VERIFY_TAPROOT) {
            txdata.Init(tx, std::move(spent_outputs));
        }

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        return VerifyScript(tx.vin[nIn].scriptSig, CScript(scriptPubKey, scriptPubKey + scriptPubKeyLen), &tx.vin[nIn].scriptWitness, flags, TransactionSignatureChecker(&tx, nIn, amount, txdata, MissingDataBehavior::FAIL), nullptr);
    } catch (const std::exception&) {
        return set_error(err, digibyteconsensus_ERR_TX_DESERIALIZE); // Error deserializing
    }
}

<<<<<<< HEAD
=======
int digibyteconsensus_verify_script_with_spent_outputs(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    const UTXO *spentOutputs, unsigned int spentOutputsLen,
                                    unsigned int nIn, unsigned int flags, digibyteconsensus_error* err)
{
    CAmount am(amount);
    return ::verify_script(scriptPubKey, scriptPubKeyLen, am, txTo, txToLen, spentOutputs, spentOutputsLen, nIn, flags, err);
}

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
int digibyteconsensus_verify_script_with_amount(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    unsigned int nIn, unsigned int flags, digibyteconsensus_error* err)
{
    CAmount am(amount);
<<<<<<< HEAD
    return ::verify_script(scriptPubKey, scriptPubKeyLen, am, txTo, txToLen, nIn, flags, err);
=======
    UTXO *spentOutputs = nullptr;
    unsigned int spentOutputsLen = 0;
    return ::verify_script(scriptPubKey, scriptPubKeyLen, am, txTo, txToLen, spentOutputs, spentOutputsLen, nIn, flags, err);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}


int digibyteconsensus_verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
                                   const unsigned char *txTo        , unsigned int txToLen,
                                   unsigned int nIn, unsigned int flags, digibyteconsensus_error* err)
{
    if (flags & digibyteconsensus_SCRIPT_FLAGS_VERIFY_WITNESS) {
        return set_error(err, digibyteconsensus_ERR_AMOUNT_REQUIRED);
    }

    CAmount am(0);
<<<<<<< HEAD
    return ::verify_script(scriptPubKey, scriptPubKeyLen, am, txTo, txToLen, nIn, flags, err);
=======
    UTXO *spentOutputs = nullptr;
    unsigned int spentOutputsLen = 0;
    return ::verify_script(scriptPubKey, scriptPubKeyLen, am, txTo, txToLen, spentOutputs, spentOutputsLen, nIn, flags, err);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

unsigned int digibyteconsensus_version()
{
    // Just use the API version for now
    return DIGIBYTECONSENSUS_API_VER;
}
