#!/bin/bash

# Script to resolve remaining conflicts in src/rpc/util.cpp
# This handles the systematic patterns in the merge conflicts

FILE="/Users/jt/Code/digibyte/src/rpc/util.cpp"

# Function to replace patterns
replace_pattern() {
    local pattern="$1"
    local replacement="$2"
    perl -i -0pe "s/\Q$pattern\E/$replacement/gs" "$FILE"
}

echo "Resolving remaining conflicts in src/rpc/util.cpp..."

# Handle HandleRequest function conflicts
replace_pattern '<<<<<<< HEAD
    const UniValue ret = m_fun(*this, request);
    CHECK_NONFATAL(std::any_of(m_results.m_results.begin(), m_results.m_results.end(), [ret](const RPCResult& res) { return res.MatchesType(ret); }));
    return ret;
}

=======
    UniValue arg_mismatch{UniValue::VOBJ};
    for (size_t i{0}; i < m_args.size(); ++i) {
        const auto& arg{m_args.at(i)};
        UniValue match{arg.MatchesType(request.params[i])};
        if (!match.isTrue()) {
            arg_mismatch.pushKV(strprintf("Position %s (%s)", i + 1, arg.m_names), std::move(match));
        }
    }
    if (!arg_mismatch.empty()) {
        throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Wrong type passed:\n%s", arg_mismatch.write(4)));
    }
    CHECK_NONFATAL(m_req == nullptr);
    m_req = &request;
    UniValue ret = m_fun(*this, request);
    m_req = nullptr;
    if (gArgs.GetBoolArg("-rpcdoccheck", DEFAULT_RPC_DOC_CHECK)) {
        UniValue mismatch{UniValue::VARR};
        for (const auto& res : m_results.m_results) {
            UniValue match{res.MatchesType(ret)};
            if (match.isTrue()) {
                mismatch.setNull();
                break;
            }
            mismatch.push_back(match);
        }
        if (!mismatch.isNull()) {
            std::string explain{
                mismatch.empty() ? "no possible results defined" :
                mismatch.size() == 1 ? mismatch[0].write(4) :
                mismatch.write(4)};
            throw std::runtime_error{
                strprintf("Internal bug detected: RPC call \"%s\" returned incorrect type:\n%s\n%s %s\nPlease report this issue here: %s\n",
                          m_name, explain,
                          PACKAGE_NAME, FormatFullVersion(),
                          PACKAGE_BUGREPORT)};
        }
    }
    return ret;
}

using CheckFn = void(const RPCArg&);
static const UniValue* DetailMaybeArg(CheckFn* check, const std::vector<RPCArg>& params, const JSONRPCRequest* req, size_t i)
{
    CHECK_NONFATAL(i < params.size());
    const UniValue& arg{CHECK_NONFATAL(req)->params[i]};
    const RPCArg& param{params.at(i)};
    if (check) check(param);

    if (!arg.isNull()) return &arg;
    if (!std::holds_alternative<RPCArg::Default>(param.m_fallback)) return nullptr;
    return &std::get<RPCArg::Default>(param.m_fallback);
}

static void CheckRequiredOrDefault(const RPCArg& param)
{
    // Must use `Arg<Type>(i)` to get the argument or its default value.
    const bool required{
        std::holds_alternative<RPCArg::Optional>(param.m_fallback) && RPCArg::Optional::NO == std::get<RPCArg::Optional>(param.m_fallback),
    };
    CHECK_NONFATAL(required || std::holds_alternative<RPCArg::Default>(param.m_fallback));
}

#define TMPL_INST(check_param, ret_type, return_code)       \
    template <>                                             \
    ret_type RPCHelpMan::ArgValue<ret_type>(size_t i) const \
    {                                                       \
        const UniValue* maybe_arg{                          \
            DetailMaybeArg(check_param, m_args, m_req, i),  \
        };                                                  \
        return return_code                                  \
    }                                                       \
    void force_semicolon(ret_type)

// Optional arg (without default). Can also be called on required args, if needed.
TMPL_INST(nullptr, std::optional<double>, maybe_arg ? std::optional{maybe_arg->get_real()} : std::nullopt;);
TMPL_INST(nullptr, std::optional<bool>, maybe_arg ? std::optional{maybe_arg->get_bool()} : std::nullopt;);
TMPL_INST(nullptr, const std::string*, maybe_arg ? &maybe_arg->get_str() : nullptr;);

// Required arg or optional arg with default value.
TMPL_INST(CheckRequiredOrDefault, bool, CHECK_NONFATAL(maybe_arg)->get_bool(););
TMPL_INST(CheckRequiredOrDefault, int, CHECK_NONFATAL(maybe_arg)->getInt<int>(););
TMPL_INST(CheckRequiredOrDefault, uint64_t, CHECK_NONFATAL(maybe_arg)->getInt<uint64_t>(););
TMPL_INST(CheckRequiredOrDefault, const std::string&, CHECK_NONFATAL(maybe_arg)->get_str(););

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion' 'UniValue arg_mismatch{UniValue::VOBJ};
    for (size_t i{0}; i < m_args.size(); ++i) {
        const auto& arg{m_args.at(i)};
        UniValue match{arg.MatchesType(request.params[i])};
        if (!match.isTrue()) {
            arg_mismatch.pushKV(strprintf("Position %s (%s)", i + 1, arg.m_names), std::move(match));
        }
    }
    if (!arg_mismatch.empty()) {
        throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Wrong type passed:\n%s", arg_mismatch.write(4)));
    }
    CHECK_NONFATAL(m_req == nullptr);
    m_req = &request;
    UniValue ret = m_fun(*this, request);
    m_req = nullptr;
    if (gArgs.GetBoolArg("-rpcdoccheck", DEFAULT_RPC_DOC_CHECK)) {
        UniValue mismatch{UniValue::VARR};
        for (const auto& res : m_results.m_results) {
            UniValue match{res.MatchesType(ret)};
            if (match.isTrue()) {
                mismatch.setNull();
                break;
            }
            mismatch.push_back(match);
        }
        if (!mismatch.isNull()) {
            std::string explain{
                mismatch.empty() ? "no possible results defined" :
                mismatch.size() == 1 ? mismatch[0].write(4) :
                mismatch.write(4)};
            throw std::runtime_error{
                strprintf("Internal bug detected: RPC call \"%s\" returned incorrect type:\n%s\n%s %s\nPlease report this issue here: %s\n",
                          m_name, explain,
                          PACKAGE_NAME, FormatFullVersion(),
                          PACKAGE_BUGREPORT)};
        }
    }
    return ret;
}

using CheckFn = void(const RPCArg&);
static const UniValue* DetailMaybeArg(CheckFn* check, const std::vector<RPCArg>& params, const JSONRPCRequest* req, size_t i)
{
    CHECK_NONFATAL(i < params.size());
    const UniValue& arg{CHECK_NONFATAL(req)->params[i]};
    const RPCArg& param{params.at(i)};
    if (check) check(param);

    if (!arg.isNull()) return &arg;
    if (!std::holds_alternative<RPCArg::Default>(param.m_fallback)) return nullptr;
    return &std::get<RPCArg::Default>(param.m_fallback);
}

static void CheckRequiredOrDefault(const RPCArg& param)
{
    // Must use `Arg<Type>(i)` to get the argument or its default value.
    const bool required{
        std::holds_alternative<RPCArg::Optional>(param.m_fallback) && RPCArg::Optional::NO == std::get<RPCArg::Optional>(param.m_fallback),
    };
    CHECK_NONFATAL(required || std::holds_alternative<RPCArg::Default>(param.m_fallback));
}

#define TMPL_INST(check_param, ret_type, return_code)       \
    template <>                                             \
    ret_type RPCHelpMan::ArgValue<ret_type>(size_t i) const \
    {                                                       \
        const UniValue* maybe_arg{                          \
            DetailMaybeArg(check_param, m_args, m_req, i),  \
        };                                                  \
        return return_code                                  \
    }                                                       \
    void force_semicolon(ret_type)

// Optional arg (without default). Can also be called on required args, if needed.
TMPL_INST(nullptr, std::optional<double>, maybe_arg ? std::optional{maybe_arg->get_real()} : std::nullopt;);
TMPL_INST(nullptr, std::optional<bool>, maybe_arg ? std::optional{maybe_arg->get_bool()} : std::nullopt;);
TMPL_INST(nullptr, const std::string*, maybe_arg ? &maybe_arg->get_str() : nullptr;);

// Required arg or optional arg with default value.
TMPL_INST(CheckRequiredOrDefault, bool, CHECK_NONFATAL(maybe_arg)->get_bool(););
TMPL_INST(CheckRequiredOrDefault, int, CHECK_NONFATAL(maybe_arg)->getInt<int>(););
TMPL_INST(CheckRequiredOrDefault, uint64_t, CHECK_NONFATAL(maybe_arg)->getInt<uint64_t>(););
TMPL_INST(CheckRequiredOrDefault, const std::string&, CHECK_NONFATAL(maybe_arg)->get_str(););'

echo "Conflict resolution script completed."
echo "Remaining conflicts: $(grep -c '<<<<<<< HEAD' "$FILE")"