<<<<<<< HEAD
// Copyright (c) 2021 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

=======
// Copyright (c) 2021-2022 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <consensus/consensus.h>
#include <wallet/receive.h>
#include <wallet/transaction.h>
#include <wallet/wallet.h>

<<<<<<< HEAD
isminetype CWallet::IsMine(const CTxIn &txin) const
{
    AssertLockHeld(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
    if (mi != mapWallet.end())
    {
        const CWalletTx& prev = (*mi).second;
        if (txin.prevout.n < prev.tx->vout.size())
            return IsMine(prev.tx->vout[txin.prevout.n]);
=======
namespace wallet {
isminetype InputIsMine(const CWallet& wallet, const CTxIn& txin)
{
    AssertLockHeld(wallet.cs_wallet);
    const CWalletTx* prev = wallet.GetWalletTx(txin.prevout.hash);
    if (prev && txin.prevout.n < prev->tx->vout.size()) {
        return wallet.IsMine(prev->tx->vout[txin.prevout.n]);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    return ISMINE_NO;
}

<<<<<<< HEAD
bool CWallet::IsAllFromMe(const CTransaction& tx, const isminefilter& filter) const
{
    LOCK(cs_wallet);

    for (const CTxIn& txin : tx.vin)
    {
        auto mi = mapWallet.find(txin.prevout.hash);
        if (mi == mapWallet.end())
            return false; // any unknown inputs can't be from us

        const CWalletTx& prev = (*mi).second;

        if (txin.prevout.n >= prev.tx->vout.size())
            return false; // invalid input!

        if (!(IsMine(prev.tx->vout[txin.prevout.n]) & filter))
            return false;
=======
bool AllInputsMine(const CWallet& wallet, const CTransaction& tx, const isminefilter& filter)
{
    LOCK(wallet.cs_wallet);
    for (const CTxIn& txin : tx.vin) {
        if (!(InputIsMine(wallet, txin) & filter)) return false;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    return true;
}

<<<<<<< HEAD
CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    LOCK(cs_wallet);
    return ((IsMine(txout) & filter) ? txout.nValue : 0);
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter) const
=======
CAmount OutputGetCredit(const CWallet& wallet, const CTxOut& txout, const isminefilter& filter)
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    LOCK(wallet.cs_wallet);
    return ((wallet.IsMine(txout) & filter) ? txout.nValue : 0);
}

CAmount TxGetCredit(const CWallet& wallet, const CTransaction& tx, const isminefilter& filter)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    CAmount nCredit = 0;
    for (const CTxOut& txout : tx.vout)
    {
<<<<<<< HEAD
        nCredit += GetCredit(txout, filter);
=======
        nCredit += OutputGetCredit(wallet, txout, filter);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nCredit;
}

<<<<<<< HEAD
bool CWallet::IsChange(const CScript& script) const
=======
bool ScriptIsChange(const CWallet& wallet, const CScript& script)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a script that is ours, but is not in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
<<<<<<< HEAD
    AssertLockHeld(cs_wallet);
    if (IsMine(script))
=======
    AssertLockHeld(wallet.cs_wallet);
    if (wallet.IsMine(script))
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    {
        CTxDestination address;
        if (!ExtractDestination(script, address))
            return true;
<<<<<<< HEAD
        if (!FindAddressBookEntry(address)) {
=======
        if (!wallet.FindAddressBookEntry(address)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            return true;
        }
    }
    return false;
}

<<<<<<< HEAD
bool CWallet::IsChange(const CTxOut& txout) const
{
    return IsChange(txout.scriptPubKey);
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    AssertLockHeld(cs_wallet);
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    LOCK(cs_wallet);
    CAmount nChange = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nChange += GetChange(txout);
=======
bool OutputIsChange(const CWallet& wallet, const CTxOut& txout)
{
    return ScriptIsChange(wallet, txout.scriptPubKey);
}

CAmount OutputGetChange(const CWallet& wallet, const CTxOut& txout)
{
    AssertLockHeld(wallet.cs_wallet);
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return (OutputIsChange(wallet, txout) ? txout.nValue : 0);
}

CAmount TxGetChange(const CWallet& wallet, const CTransaction& tx)
{
    LOCK(wallet.cs_wallet);
    CAmount nChange = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nChange += OutputGetChange(wallet, txout);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        if (!MoneyRange(nChange))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nChange;
}

<<<<<<< HEAD
CAmount CWalletTx::GetCachableAmount(AmountType type, const isminefilter& filter, bool recalculate) const
{
    auto& amount = m_amounts[type];
    if (recalculate || !amount.m_cached[filter]) {
        amount.Set(filter, type == DEBIT ? pwallet->GetDebit(*tx, filter) : pwallet->GetCredit(*tx, filter));
        m_is_cache_empty = false;
=======
static CAmount GetCachableAmount(const CWallet& wallet, const CWalletTx& wtx, CWalletTx::AmountType type, const isminefilter& filter)
{
    auto& amount = wtx.m_amounts[type];
    if (!amount.m_cached[filter]) {
        amount.Set(filter, type == CWalletTx::DEBIT ? wallet.GetDebit(*wtx.tx, filter) : TxGetCredit(wallet, *wtx.tx, filter));
        wtx.m_is_cache_empty = false;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    return amount.m_value[filter];
}

<<<<<<< HEAD
CAmount CWalletTx::GetCredit(const isminefilter& filter) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsImmatureCoinBase())
        return 0;

    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE) {
        // GetBalance can assume transactions in mapWallet won't change
        credit += GetCachableAmount(CREDIT, ISMINE_SPENDABLE);
    }
    if (filter & ISMINE_WATCH_ONLY) {
        credit += GetCachableAmount(CREDIT, ISMINE_WATCH_ONLY);
=======
CAmount CachedTxGetCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (wallet.IsTxImmatureCoinBase(wtx))
        return 0;

    CAmount credit = 0;
    const isminefilter get_amount_filter{filter & ISMINE_ALL};
    if (get_amount_filter) {
        // GetBalance can assume transactions in mapWallet won't change
        credit += GetCachableAmount(wallet, wtx, CWalletTx::CREDIT, get_amount_filter);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    return credit;
}

<<<<<<< HEAD
CAmount CWalletTx::GetDebit(const isminefilter& filter) const
{
    if (tx->vin.empty())
        return 0;

    CAmount debit = 0;
    if (filter & ISMINE_SPENDABLE) {
        debit += GetCachableAmount(DEBIT, ISMINE_SPENDABLE);
    }
    if (filter & ISMINE_WATCH_ONLY) {
        debit += GetCachableAmount(DEBIT, ISMINE_WATCH_ONLY);
=======
CAmount CachedTxGetDebit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    if (wtx.tx->vin.empty())
        return 0;

    CAmount debit = 0;
    const isminefilter get_amount_filter{filter & ISMINE_ALL};
    if (get_amount_filter) {
        debit += GetCachableAmount(wallet, wtx, CWalletTx::DEBIT, get_amount_filter);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    return debit;
}

<<<<<<< HEAD
CAmount CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*tx);
    fChangeCached = true;
    return nChangeCached;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache) const
{
    if (IsImmatureCoinBase() && IsInMainChain()) {
        return GetCachableAmount(IMMATURE_CREDIT, ISMINE_SPENDABLE, !fUseCache);
=======
CAmount CachedTxGetChange(const CWallet& wallet, const CWalletTx& wtx)
{
    if (wtx.fChangeCached)
        return wtx.nChangeCached;
    wtx.nChangeCached = TxGetChange(wallet, *wtx.tx);
    wtx.fChangeCached = true;
    return wtx.nChangeCached;
}

CAmount CachedTxGetImmatureCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);

    if (wallet.IsTxImmatureCoinBase(wtx) && wallet.IsTxInMainChain(wtx)) {
        return GetCachableAmount(wallet, wtx, CWalletTx::IMMATURE_CREDIT, filter);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    return 0;
}

<<<<<<< HEAD
CAmount CWalletTx::GetImmatureWatchOnlyCredit(const bool fUseCache) const
{
    if (IsImmatureCoinBase() && IsInMainChain()) {
        return GetCachableAmount(IMMATURE_CREDIT, ISMINE_WATCH_ONLY, !fUseCache);
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(bool fUseCache, const isminefilter& filter) const
{
    if (pwallet == nullptr)
        return 0;
=======
CAmount CachedTxGetAvailableCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // Avoid caching ismine for NO or ALL cases (could remove this check and simplify in the future).
    bool allow_cache = (filter & ISMINE_ALL) && (filter & ISMINE_ALL) != ISMINE_ALL;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
<<<<<<< HEAD
    if (IsImmatureCoinBase())
        return 0;

    if (fUseCache && allow_cache && m_amounts[AVAILABLE_CREDIT].m_cached[filter]) {
        return m_amounts[AVAILABLE_CREDIT].m_value[filter];
    }

    bool allow_used_addresses = (filter & ISMINE_USED) || !pwallet->IsWalletFlagSet(WALLET_FLAG_AVOID_REUSE);
    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(hashTx, i) && (allow_used_addresses || !pwallet->IsSpentKey(hashTx, i))) {
            const CTxOut &txout = tx->vout[i];
            nCredit += pwallet->GetCredit(txout, filter);
=======
    if (wallet.IsTxImmatureCoinBase(wtx))
        return 0;

    if (allow_cache && wtx.m_amounts[CWalletTx::AVAILABLE_CREDIT].m_cached[filter]) {
        return wtx.m_amounts[CWalletTx::AVAILABLE_CREDIT].m_value[filter];
    }

    bool allow_used_addresses = (filter & ISMINE_USED) || !wallet.IsWalletFlagSet(WALLET_FLAG_AVOID_REUSE);
    CAmount nCredit = 0;
    uint256 hashTx = wtx.GetHash();
    for (unsigned int i = 0; i < wtx.tx->vout.size(); i++) {
        const CTxOut& txout = wtx.tx->vout[i];
        if (!wallet.IsSpent(COutPoint(hashTx, i)) && (allow_used_addresses || !wallet.IsSpentKey(txout.scriptPubKey))) {
            nCredit += OutputGetCredit(wallet, txout, filter);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            if (!MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + " : value out of range");
        }
    }

    if (allow_cache) {
<<<<<<< HEAD
        m_amounts[AVAILABLE_CREDIT].Set(filter, nCredit);
        m_is_cache_empty = false;
=======
        wtx.m_amounts[CWalletTx::AVAILABLE_CREDIT].Set(filter, nCredit);
        wtx.m_is_cache_empty = false;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    return nCredit;
}

<<<<<<< HEAD
void CWalletTx::GetAmounts(std::list<COutputEntry>& listReceived,
                           std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter) const
=======
void CachedTxGetAmounts(const CWallet& wallet, const CWalletTx& wtx,
                  std::list<COutputEntry>& listReceived,
                  std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter,
                  bool include_change)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();

    // Compute fee:
<<<<<<< HEAD
    CAmount nDebit = GetDebit(filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = tx->GetValueOut();
        nFee = nDebit - nValueOut;
    }

    LOCK(pwallet->cs_wallet);
    // Sent/received.
    for (unsigned int i = 0; i < tx->vout.size(); ++i)
    {
        const CTxOut& txout = tx->vout[i];
        isminetype fIsMine = pwallet->IsMine(txout);
=======
    CAmount nDebit = CachedTxGetDebit(wallet, wtx, filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = wtx.tx->GetValueOut();
        nFee = nDebit - nValueOut;
    }

    LOCK(wallet.cs_wallet);
    // Sent/received.
    for (unsigned int i = 0; i < wtx.tx->vout.size(); ++i)
    {
        const CTxOut& txout = wtx.tx->vout[i];
        isminetype fIsMine = wallet.IsMine(txout);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (nDebit > 0)
        {
<<<<<<< HEAD
            // Don't report 'change' txouts
            if (pwallet->IsChange(txout))
=======
            if (!include_change && OutputIsChange(wallet, txout))
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                continue;
        }
        else if (!(fIsMine & filter))
            continue;

        // In either case, we need to get the destination address
        CTxDestination address;

        if (!ExtractDestination(txout.scriptPubKey, address) && !txout.scriptPubKey.IsUnspendable())
        {
<<<<<<< HEAD
            pwallet->WalletLogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                                    this->GetHash().ToString());
=======
            wallet.WalletLogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                                    wtx.GetHash().ToString());
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            address = CNoDestination();
        }

        COutputEntry output = {address, txout.nValue, (int)i};

        // If we are debited by the transaction, add the output as a "sent" entry
        if (nDebit > 0)
            listSent.push_back(output);

        // If we are receiving the output, add it as a "received" entry
        if (fIsMine & filter)
            listReceived.push_back(output);
    }

}

<<<<<<< HEAD
bool CWallet::IsTrusted(const CWalletTx& wtx, std::set<uint256>& trusted_parents) const
{
    AssertLockHeld(cs_wallet);
    // Quick answer in most cases
    if (!chain().checkFinalTx(*wtx.tx)) return false;
    int nDepth = wtx.GetDepthInMainChain();
    if (nDepth >= 1) return true;
    if (nDepth < 0) return false;
    // using wtx's cached debit
    if (!m_spend_zero_conf_change || !wtx.IsFromMe(ISMINE_ALL)) return false;
=======
bool CachedTxIsFromMe(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    return (CachedTxGetDebit(wallet, wtx, filter) > 0);
}

bool CachedTxIsTrusted(const CWallet& wallet, const CWalletTx& wtx, std::set<uint256>& trusted_parents)
{
    AssertLockHeld(wallet.cs_wallet);
    int nDepth = wallet.GetTxDepthInMainChain(wtx);
    if (nDepth >= 1) return true;
    if (nDepth < 0) return false;
    // using wtx's cached debit
    if (!wallet.m_spend_zero_conf_change || !CachedTxIsFromMe(wallet, wtx, ISMINE_ALL)) return false;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // Don't trust unconfirmed transactions from us unless they are in the mempool.
    if (!wtx.InMempool()) return false;

    // Trusted if all inputs are from us and are in the mempool:
    for (const CTxIn& txin : wtx.tx->vin)
    {
        // Transactions not sent by us: not trusted
<<<<<<< HEAD
        const CWalletTx* parent = GetWalletTx(txin.prevout.hash);
        if (parent == nullptr) return false;
        const CTxOut& parentOut = parent->tx->vout[txin.prevout.n];
        // Check that this specific input being spent is trusted
        if (IsMine(parentOut) != ISMINE_SPENDABLE) return false;
        // If we've already trusted this parent, continue
        if (trusted_parents.count(parent->GetHash())) continue;
        // Recurse to check that the parent is also trusted
        if (!IsTrusted(*parent, trusted_parents)) return false;
=======
        const CWalletTx* parent = wallet.GetWalletTx(txin.prevout.hash);
        if (parent == nullptr) return false;
        const CTxOut& parentOut = parent->tx->vout[txin.prevout.n];
        // Check that this specific input being spent is trusted
        if (wallet.IsMine(parentOut) != ISMINE_SPENDABLE) return false;
        // If we've already trusted this parent, continue
        if (trusted_parents.count(parent->GetHash())) continue;
        // Recurse to check that the parent is also trusted
        if (!CachedTxIsTrusted(wallet, *parent, trusted_parents)) return false;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        trusted_parents.insert(parent->GetHash());
    }
    return true;
}

<<<<<<< HEAD
bool CWalletTx::IsTrusted() const
{
    std::set<uint256> trusted_parents;
    LOCK(pwallet->cs_wallet);
    return pwallet->IsTrusted(*this, trusted_parents);
}

CWallet::Balance CWallet::GetBalance(const int min_depth, bool avoid_reuse) const
=======
bool CachedTxIsTrusted(const CWallet& wallet, const CWalletTx& wtx)
{
    std::set<uint256> trusted_parents;
    LOCK(wallet.cs_wallet);
    return CachedTxIsTrusted(wallet, wtx, trusted_parents);
}

Balance GetBalance(const CWallet& wallet, const int min_depth, bool avoid_reuse)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    Balance ret;
    isminefilter reuse_filter = avoid_reuse ? ISMINE_NO : ISMINE_USED;
    {
<<<<<<< HEAD
        LOCK(cs_wallet);
        std::set<uint256> trusted_parents;
        for (const auto& entry : mapWallet)
        {
            const CWalletTx& wtx = entry.second;
            const bool is_trusted{IsTrusted(wtx, trusted_parents)};
            const int tx_depth{wtx.GetDepthInMainChain()};
            const CAmount tx_credit_mine{wtx.GetAvailableCredit(/* fUseCache */ true, ISMINE_SPENDABLE | reuse_filter)};
            const CAmount tx_credit_watchonly{wtx.GetAvailableCredit(/* fUseCache */ true, ISMINE_WATCH_ONLY | reuse_filter)};
=======
        LOCK(wallet.cs_wallet);
        std::set<uint256> trusted_parents;
        for (const auto& entry : wallet.mapWallet)
        {
            const CWalletTx& wtx = entry.second;
            const bool is_trusted{CachedTxIsTrusted(wallet, wtx, trusted_parents)};
            const int tx_depth{wallet.GetTxDepthInMainChain(wtx)};
            const CAmount tx_credit_mine{CachedTxGetAvailableCredit(wallet, wtx, ISMINE_SPENDABLE | reuse_filter)};
            const CAmount tx_credit_watchonly{CachedTxGetAvailableCredit(wallet, wtx, ISMINE_WATCH_ONLY | reuse_filter)};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            if (is_trusted && tx_depth >= min_depth) {
                ret.m_mine_trusted += tx_credit_mine;
                ret.m_watchonly_trusted += tx_credit_watchonly;
            }
            if (!is_trusted && tx_depth == 0 && wtx.InMempool()) {
                ret.m_mine_untrusted_pending += tx_credit_mine;
                ret.m_watchonly_untrusted_pending += tx_credit_watchonly;
            }
<<<<<<< HEAD
            ret.m_mine_immature += wtx.GetImmatureCredit();
            ret.m_watchonly_immature += wtx.GetImmatureWatchOnlyCredit();
=======
            ret.m_mine_immature += CachedTxGetImmatureCredit(wallet, wtx, ISMINE_SPENDABLE);
            ret.m_watchonly_immature += CachedTxGetImmatureCredit(wallet, wtx, ISMINE_WATCH_ONLY);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        }
    }
    return ret;
}

<<<<<<< HEAD
std::map<CTxDestination, CAmount> CWallet::GetAddressBalances() const
=======
std::map<CTxDestination, CAmount> GetAddressBalances(const CWallet& wallet)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    std::map<CTxDestination, CAmount> balances;

    {
<<<<<<< HEAD
        LOCK(cs_wallet);
        std::set<uint256> trusted_parents;
        for (const auto& walletEntry : mapWallet)
        {
            const CWalletTx& wtx = walletEntry.second;

            if (!IsTrusted(wtx, trusted_parents))
                continue;

            if (wtx.IsImmatureCoinBase())
                continue;

            int nDepth = wtx.GetDepthInMainChain();
            if (nDepth < (wtx.IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < wtx.tx->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(wtx.tx->vout[i]))
                    continue;
                if(!ExtractDestination(wtx.tx->vout[i].scriptPubKey, addr))
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : wtx.tx->vout[i].nValue;
=======
        LOCK(wallet.cs_wallet);
        std::set<uint256> trusted_parents;
        for (const auto& walletEntry : wallet.mapWallet)
        {
            const CWalletTx& wtx = walletEntry.second;

            if (!CachedTxIsTrusted(wallet, wtx, trusted_parents))
                continue;

            if (wallet.IsTxImmatureCoinBase(wtx))
                continue;

            int nDepth = wallet.GetTxDepthInMainChain(wtx);
            if (nDepth < (CachedTxIsFromMe(wallet, wtx, ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < wtx.tx->vout.size(); i++) {
                const auto& output = wtx.tx->vout[i];
                CTxDestination addr;
                if (!wallet.IsMine(output))
                    continue;
                if(!ExtractDestination(output.scriptPubKey, addr))
                    continue;

                CAmount n = wallet.IsSpent(COutPoint(walletEntry.first, i)) ? 0 : output.nValue;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                balances[addr] += n;
            }
        }
    }

    return balances;
}

<<<<<<< HEAD
std::set< std::set<CTxDestination> > CWallet::GetAddressGroupings() const
{
    AssertLockHeld(cs_wallet);
    std::set< std::set<CTxDestination> > groupings;
    std::set<CTxDestination> grouping;

    for (const auto& walletEntry : mapWallet)
=======
std::set< std::set<CTxDestination> > GetAddressGroupings(const CWallet& wallet)
{
    AssertLockHeld(wallet.cs_wallet);
    std::set< std::set<CTxDestination> > groupings;
    std::set<CTxDestination> grouping;

    for (const auto& walletEntry : wallet.mapWallet)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    {
        const CWalletTx& wtx = walletEntry.second;

        if (wtx.tx->vin.size() > 0)
        {
            bool any_mine = false;
            // group all input addresses with each other
            for (const CTxIn& txin : wtx.tx->vin)
            {
                CTxDestination address;
<<<<<<< HEAD
                if(!IsMine(txin)) /* If this input isn't mine, ignore it */
                    continue;
                if(!ExtractDestination(mapWallet.at(txin.prevout.hash).tx->vout[txin.prevout.n].scriptPubKey, address))
=======
                if(!InputIsMine(wallet, txin)) /* If this input isn't mine, ignore it */
                    continue;
                if(!ExtractDestination(wallet.mapWallet.at(txin.prevout.hash).tx->vout[txin.prevout.n].scriptPubKey, address))
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine)
            {
               for (const CTxOut& txout : wtx.tx->vout)
<<<<<<< HEAD
                   if (IsChange(txout))
=======
                   if (OutputIsChange(wallet, txout))
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                   {
                       CTxDestination txoutAddr;
                       if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                           continue;
                       grouping.insert(txoutAddr);
                   }
            }
            if (grouping.size() > 0)
            {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        // group lone addrs by themselves
        for (const auto& txout : wtx.tx->vout)
<<<<<<< HEAD
            if (IsMine(txout))
=======
            if (wallet.IsMine(txout))
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            {
                CTxDestination address;
                if(!ExtractDestination(txout.scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    std::set< std::set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    std::map< CTxDestination, std::set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
<<<<<<< HEAD
    for (std::set<CTxDestination> _grouping : groupings)
=======
    for (const std::set<CTxDestination>& _grouping : groupings)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    {
        // make a set of all the groups hit by this new group
        std::set< std::set<CTxDestination>* > hits;
        std::map< CTxDestination, std::set<CTxDestination>* >::iterator it;
        for (const CTxDestination& address : _grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        std::set<CTxDestination>* merged = new std::set<CTxDestination>(_grouping);
        for (std::set<CTxDestination>* hit : hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        for (const CTxDestination& element : *merged)
            setmap[element] = merged;
    }

    std::set< std::set<CTxDestination> > ret;
    for (const std::set<CTxDestination>* uniqueGrouping : uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}
<<<<<<< HEAD
=======
} // namespace wallet
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
