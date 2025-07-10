// Copyright (c) 2009-2010 Satoshi Nakamoto
<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2009-2022 The Bitcoin Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_POLICY_FEERATE_H
#define DIGIBYTE_POLICY_FEERATE_H

#include <consensus/amount.h>
#include <serialize.h>


<<<<<<< HEAD
=======
#include <cstdint>
#include <string>
#include <type_traits>

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
const std::string CURRENCY_UNIT = "DGB"; // One formatted unit
const std::string CURRENCY_ATOM = "sat"; // One indivisible minimum value unit

/* Used to determine type of fee estimation requested */
enum class FeeEstimateMode {
    UNSET,        //!< Use default settings based on other criteria
    ECONOMICAL,   //!< Force estimateSmartFee to use non-conservative estimates
    CONSERVATIVE, //!< Force estimateSmartFee to use conservative estimates
    DGB_KVB,      //!< Use DGB/kvB fee rate unit
    SAT_VB,       //!< Use sat/vB fee rate unit
};

/**
 * Fee rate in satoshis per kilovirtualbyte: CAmount / kvB
 */
class CFeeRate
{
private:
    /** Fee rate in sat/kvB (satoshis per 1000 virtualbytes) */
    CAmount nSatoshisPerK;

public:
    /** Fee rate of 0 satoshis per kvB */
    CFeeRate() : nSatoshisPerK(0) { }
    template<typename I>
    explicit CFeeRate(const I _nSatoshisPerK): nSatoshisPerK(_nSatoshisPerK) {
        // We've previously had bugs creep in from silent double->int conversion...
        static_assert(std::is_integral<I>::value, "CFeeRate should be used without floats");
    }
<<<<<<< HEAD
    /** Constructor for a fee rate in satoshis per kvB (sat/kvB).
     *
     *  Passing a num_bytes value of COIN (1e8) returns a fee rate in satoshis per vB (sat/vB),
     *  e.g. (nFeePaid * 1e8 / 1e3) == (nFeePaid / 1e5),
     *  where 1e5 is the ratio to convert from DGB/kvB to sat/vB.
     */
    CFeeRate(const CAmount& nFeePaid, uint32_t num_bytes);
    /**
     * Return the fee in satoshis for the given size in bytes.
     * If the calculated fee would have fractional satoshis, then the returned fee will always be rounded up to the nearest satoshi.
     */
    CAmount GetFee(uint32_t num_bytes) const;
=======

    /**
     * Construct a fee rate from a fee in satoshis and a vsize in vB.
     *
     * param@[in]   nFeePaid    The fee paid by a transaction, in satoshis
     * param@[in]   num_bytes   The vsize of a transaction, in vbytes
     */
    CFeeRate(const CAmount& nFeePaid, uint32_t num_bytes);

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    /**
     * Return the fee in satoshis for the given vsize in vbytes.
     * If the calculated fee would have fractional satoshis, then the
     * returned fee will always be rounded up to the nearest satoshi.
     */
    CAmount GetFee(uint32_t num_bytes) const;

    /**
     * Return the fee in satoshis for a vsize of 1000 vbytes
     */
    CAmount GetFeePerK() const { return nSatoshisPerK; }
    friend bool operator<(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK < b.nSatoshisPerK; }
    friend bool operator>(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK > b.nSatoshisPerK; }
    friend bool operator==(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK == b.nSatoshisPerK; }
    friend bool operator<=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK <= b.nSatoshisPerK; }
    friend bool operator>=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK >= b.nSatoshisPerK; }
    friend bool operator!=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK != b.nSatoshisPerK; }
    CFeeRate& operator+=(const CFeeRate& a) { nSatoshisPerK += a.nSatoshisPerK; return *this; }
    std::string ToString(const FeeEstimateMode& fee_estimate_mode = FeeEstimateMode::DGB_KVB) const;

    SERIALIZE_METHODS(CFeeRate, obj) { READWRITE(obj.nSatoshisPerK); }
};

<<<<<<< HEAD
#endif //  DIGIBYTE_POLICY_FEERATE_H
=======
#endif // DIGIBYTE_POLICY_FEERATE_H
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
