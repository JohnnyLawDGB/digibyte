#!/usr/bin/env bash
set -euo pipefail

# Five-node isolated mainnet-PRE oracle rehearsal.
#
# This intentionally runs chain=main with the PRE chainparams from this tree:
# same mainnet genesis and magic bytes, but isolated localhost ports/datadirs,
# no seeds/discovery, and fast DigiDollar/MuSig2 activation at height 600.
#
# Defaults leave all Qt nodes and datadirs in place for manual inspection.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_BIN="${QT_BIN:-$SCRIPT_DIR/src/qt/digibyte-qt}"
CLI_BIN="${CLI_BIN:-$SCRIPT_DIR/src/digibyte-cli}"

NODE_NAMES=(bob alice charlie dave eve)
P2P_PORTS=(12046 12047 12048 12049 12050)
RPC_PORTS=(14046 14047 14048 14049 14050)
NODE_ALGOS=(scrypt sha256d skein qubit groestl)

BASE_DIR="${BASE_DIR:-$(mktemp -d /tmp/dgb-mainnet-pre-oracle.XXXXXX)}"
TARGET_HEIGHT="${TARGET_HEIGHT:-600}"
ACTIVATION_HEIGHT="${ACTIVATION_HEIGHT:-600}"
KEEP_QT_OPEN="${KEEP_QT_OPEN:-1}"
STOP_ON_SUCCESS="${STOP_ON_SUCCESS:-0}"
START_ONLY="${START_ONLY:-0}"
SELF_TEST="${SELF_TEST:-0}"
MAX_TRIES="${MAX_TRIES:-2000000000}"
FUND_AMOUNT="${FUND_AMOUNT:-100}"
RPC_USER="${RPC_USER:-preminer}"
RPC_PASSWORD="${RPC_PASSWORD:-preminerpass}"
CPUMINER_BIN="${CPUMINER_BIN:-$HOME/Code/cpuminer-multi/cpuminer}"
CPUMINER_THREADS="${CPUMINER_THREADS:-$(command -v nproc >/dev/null && nproc || printf '4')}"
CPUMINER_BLOCK_TIMEOUT="${CPUMINER_BLOCK_TIMEOUT:-900}"
USE_CPUMINER="${USE_CPUMINER:-0}"
CPUMINER_DIGIDOLLAR="${CPUMINER_DIGIDOLLAR:-1}"
CORE_RACE_TRIES="${CORE_RACE_TRIES:-50000}"
CORE_RACE_TRIES_SCRYPT="${CORE_RACE_TRIES_SCRYPT:-50000}"
CORE_RACE_TRIES_SHA256D="${CORE_RACE_TRIES_SHA256D:-500000}"
CORE_RACE_TRIES_SKEIN="${CORE_RACE_TRIES_SKEIN:-500000}"
CORE_RACE_TRIES_QUBIT="${CORE_RACE_TRIES_QUBIT:-500000}"
CORE_RACE_TRIES_GROESTL="${CORE_RACE_TRIES_GROESTL:-500000}"
CORE_RACE_TRIES_ODO="${CORE_RACE_TRIES_ODO:-250000}"
CORE_RACE_WORKERS_PER_NODE="${CORE_RACE_WORKERS_PER_NODE:-8}"
CORE_RACE_BLOCK_TIMEOUT="${CORE_RACE_BLOCK_TIMEOUT:-3600}"

DIGISWARM_ORACLE_ID="${DIGISWARM_ORACLE_ID:-15}"
DIGISWARM_WALLET_NAME="${DIGISWARM_WALLET_NAME:-DigiSwarmOracle}"
DIGISWARM_EXPECTED_PUBKEY="${DIGISWARM_EXPECTED_PUBKEY:-030f809ccbeea32bcca9c817dbf674e38e2fbc234f4462b88612876287489197a9}"
DIGISWARM_ORACLE_WALLET_DIR="${DIGISWARM_ORACLE_WALLET_DIR:-$HOME/.digibyte-digiswarm-mainnet-oracle-slot15-20260623/DigiSwarmOracle}"
DIGISWARM_ORACLE_KEY_FILE="${DIGISWARM_ORACLE_KEY_FILE:-}"
DIGISWARM_ORACLE_KEY="${DIGISWARM_ORACLE_KEY:-}"
DIGISWARM_WALLET_PASSPHRASE="${DIGISWARM_WALLET_PASSPHRASE:-}"

declare -a DATADIRS
declare -a PIDS
declare -A MINED_BY_ALGO=()

die() {
    echo "ERROR: $*" >&2
    echo "BASE_DIR=$BASE_DIR" >&2
    exit 1
}

log() {
    printf '[%s] %s\n' "$(date '+%H:%M:%S')" "$*"
}

require_file() {
    [ -x "$1" ] || die "missing executable: $1"
}

port_is_listening() {
    python3 - "$1" <<'PY'
import socket
import sys

port = int(sys.argv[1])
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(0.2)
try:
    sock.connect(("127.0.0.1", port))
except OSError:
    sys.exit(1)
else:
    sys.exit(0)
finally:
    sock.close()
PY
}

cli() {
    local idx="$1"
    shift
    "$CLI_BIN" -datadir="${DATADIRS[$idx]}" -rpcport="${RPC_PORTS[$idx]}" -rpcuser="$RPC_USER" -rpcpassword="$RPC_PASSWORD" "$@"
}

wcli() {
    local idx="$1"
    local wallet="$2"
    shift 2
    "$CLI_BIN" -datadir="${DATADIRS[$idx]}" -rpcport="${RPC_PORTS[$idx]}" -rpcuser="$RPC_USER" -rpcpassword="$RPC_PASSWORD" -rpcwallet="$wallet" "$@"
}

node_chain_dir() {
    local idx="$1"
    printf '%s/mainnet-pre' "${DATADIRS[$idx]}"
}

write_run_info() {
    {
        printf 'BASE_DIR=%q\n' "$BASE_DIR"
        printf 'TARGET_HEIGHT=%q\n' "$TARGET_HEIGHT"
        printf 'ACTIVATION_HEIGHT=%q\n' "$ACTIVATION_HEIGHT"
        printf 'KEEP_QT_OPEN=%q\n' "$KEEP_QT_OPEN"
        for i in "${!NODE_NAMES[@]}"; do
            printf '%s_DATADIR=%q\n' "${NODE_NAMES[$i]^^}" "${DATADIRS[$i]}"
            printf '%s_P2P_PORT=%q\n' "${NODE_NAMES[$i]^^}" "${P2P_PORTS[$i]}"
            printf '%s_RPC_PORT=%q\n' "${NODE_NAMES[$i]^^}" "${RPC_PORTS[$i]}"
            if [ -n "${PIDS[$i]:-}" ]; then
                printf '%s_QT_PID=%q\n' "${NODE_NAMES[$i]^^}" "${PIDS[$i]}"
            fi
        done
    } > "$BASE_DIR/test_oracle_mainnet.env"
}

init_dirs() {
    mkdir -p "$BASE_DIR"
    for i in "${!NODE_NAMES[@]}"; do
        DATADIRS[$i]="$BASE_DIR/${NODE_NAMES[$i]}"
        mkdir -p "${DATADIRS[$i]}"
    done
    write_run_info
}

copy_digiswarm_wallet() {
    local wallet_target
    wallet_target="$(node_chain_dir 0)/wallets/$DIGISWARM_WALLET_NAME"

    if [ -d "$wallet_target" ]; then
        log "DigiSwarm wallet already exists in Bob PRE datadir: $wallet_target"
        return
    fi

    if [ -d "$DIGISWARM_ORACLE_WALLET_DIR" ]; then
        log "Copying DigiSwarm oracle wallet into Bob PRE datadir"
        mkdir -p "$(dirname "$wallet_target")"
        cp -a "$DIGISWARM_ORACLE_WALLET_DIR" "$wallet_target"
        return
    fi

    log "No DigiSwarm wallet directory found at $DIGISWARM_ORACLE_WALLET_DIR"
    log "The script will try DIGISWARM_ORACLE_KEY/DIGISWARM_ORACLE_KEY_FILE later if provided."
}

check_ports_available_for_new_run() {
    if [ -f "$BASE_DIR/test_oracle_mainnet.env" ]; then
        return
    fi

    local port
    for port in "${P2P_PORTS[@]}" "${RPC_PORTS[@]}"; do
        if port_is_listening "$port"; then
            die "localhost port $port is already in use; stop the old PRE run or set BASE_DIR/ports explicitly"
        fi
    done
}

start_node() {
    local idx="$1"
    local name="${NODE_NAMES[$idx]}"
    local datadir="${DATADIRS[$idx]}"
    local p2p="${P2P_PORTS[$idx]}"
    local rpc="${RPC_PORTS[$idx]}"
    local log_file="$BASE_DIR/$name.qt.log"
    local connect_arg

    if cli "$idx" getblockchaininfo >/dev/null 2>&1; then
        log "$name RPC is already available; reusing running node"
        return
    fi

    if [ "$idx" -eq 0 ]; then
        connect_arg="-connect=0"
    else
        connect_arg="-connect=127.0.0.1:${P2P_PORTS[0]}"
    fi

    log "Starting $name Qt: datadir=$datadir p2p=$p2p rpc=$rpc"
    setsid "$QT_BIN" \
        -datadir="$datadir" \
        -server=1 \
        -listen=1 \
        -bind="127.0.0.1:$p2p" \
        -port="$p2p" \
        -rpcbind=127.0.0.1 \
        -rpcallowip=127.0.0.1 \
        -rpcport="$rpc" \
        -rpcuser="$RPC_USER" \
        -rpcpassword="$RPC_PASSWORD" \
        -rpcthreads=16 \
        -dnsseed=0 \
        -fixedseeds=0 \
        -discover=0 \
        -listenonion=0 \
        -onion=0 \
        -upnp=0 \
        -natpmp=0 \
        -maxconnections=64 \
        -whitelist=127.0.0.1 \
        -maxtipage=9999999999 \
        -txindex=1 \
        -digidollar=1 \
        -algo="${NODE_ALGOS[$idx]}" \
        -fallbackfee=0.0001 \
        -debug=net \
        -debug=validation \
        -debug=digidollar \
        "$connect_arg" \
        > "$log_file" 2>&1 &

    PIDS[$idx]="$!"
    printf '%s\n' "${PIDS[$idx]}" > "$datadir/digibyte-qt.pid"
    write_run_info
}

wait_for_rpc() {
    local idx="$1"
    local name="${NODE_NAMES[$idx]}"
    local attempt
    for attempt in $(seq 1 180); do
        if cli "$idx" getblockchaininfo >/dev/null 2>&1; then
            local chain
            chain="$(cli "$idx" getblockchaininfo | jq -r '.chain')"
            [ "$chain" = "main" ] || die "$name started on chain=$chain, expected main"
            log "$name RPC ready on isolated mainnet-PRE datadir"
            return
        fi
        sleep 1
    done
    die "$name RPC did not become ready; see $BASE_DIR/$name.qt.log"
}

ensure_wallet() {
    local idx="$1"
    local wallet="$2"

    if cli "$idx" listwallets | jq -e --arg wallet "$wallet" 'index($wallet) != null' >/dev/null; then
        return
    fi

    if cli "$idx" loadwallet "$wallet" >/dev/null 2>&1; then
        log "Loaded wallet $wallet on ${NODE_NAMES[$idx]}"
        return
    fi

    log "Creating wallet $wallet on ${NODE_NAMES[$idx]}"
    cli "$idx" createwallet "$wallet" >/dev/null
}

ensure_wallets() {
    ensure_wallet 0 "$DIGISWARM_WALLET_NAME"
    for i in "${!NODE_NAMES[@]}"; do
        ensure_wallet "$i" "${NODE_NAMES[$i]}"
    done
}

refresh_local_links() {
    local i
    for i in 1 2 3 4; do
        cli "$i" addnode "127.0.0.1:${P2P_PORTS[0]}" onetry >/dev/null 2>&1 || true
    done
}

wait_for_links() {
    local attempt
    for attempt in $(seq 1 120); do
        if [ $((attempt % 5)) -eq 1 ]; then
            refresh_local_links
        fi
        local bob_count
        bob_count="$(cli 0 getconnectioncount 2>/dev/null || echo 0)"
        local ok=1
        [ "$bob_count" -ge 4 ] || ok=0
        for i in 1 2 3 4; do
            local count
            count="$(cli "$i" getconnectioncount 2>/dev/null || echo 0)"
            [ "$count" -ge 1 ] || ok=0
        done
        if [ "$ok" -eq 1 ]; then
            log "All five nodes are connected over localhost"
            return
        fi
        sleep 1
    done
    die "nodes did not establish localhost links"
}

assert_no_public_peers() {
    log "Checking peer isolation: only localhost peers, no public mainnet port 12024"
    local i
    for i in "${!NODE_NAMES[@]}"; do
        local peers
        peers="$(cli "$i" getpeerinfo)"
        echo "$peers" | jq -e '
          all(.[]; ((.addr | test("^(127[.]0[.]0[.]1|localhost|\\[::1\\])[:\\]]")) and ((.addr | contains(":12024")) | not)))
        ' >/dev/null || {
            echo "$peers" | jq '[.[] | {id, addr, inbound, connection_type}]' >&2
            die "${NODE_NAMES[$i]} has a non-local or public-mainnet peer"
        }
        if grep -E '(trying connection|connected|Added connection).*:12024' "$(node_chain_dir "$i")/debug.log" >/dev/null 2>&1; then
            die "${NODE_NAMES[$i]} debug.log shows a public mainnet port 12024 connection attempt"
        fi
    done
}

active_chain_block_algo() {
    local height="$1"
    local hash
    hash="$(cli 0 getblockhash "$height")"
    cli 0 getblock "$hash" | jq -r '.pow_algo // (.pow_algo_id | tostring)'
}

new_legacy_address() {
    local idx="$1"
    local wallet="$2"
    local attempt addr valid
    for attempt in $(seq 1 5); do
        addr="$(wcli "$idx" "$wallet" getnewaddress "" legacy 2>/dev/null | tr -d '[:space:]' || true)"
        if [ -z "$addr" ]; then
            continue
        fi
        valid="$(cli "$idx" validateaddress "$addr" | jq -r '.isvalid // false')"
        if [ "$valid" = "true" ]; then
            printf '%s\n' "$addr"
            return
        fi
    done
    die "failed to generate a valid legacy address for ${NODE_NAMES[$idx]}/$wallet"
}

sync_all_nodes() {
    local attempt
    for attempt in $(seq 1 180); do
        local base_height base_hash ok
        base_height="$(cli 0 getblockcount)"
        base_hash="$(cli 0 getbestblockhash)"
        ok=1
        local i
        for i in 1 2 3 4; do
            local h hash
            h="$(cli "$i" getblockcount 2>/dev/null || echo -1)"
            hash="$(cli "$i" getbestblockhash 2>/dev/null || echo missing)"
            if [ "$h" != "$base_height" ] || [ "$hash" != "$base_hash" ]; then
                ok=0
                break
            fi
        done
        if [ "$ok" -eq 1 ]; then
            return
        fi
        sleep 1
    done
    die "nodes failed to sync to the same best block"
}

deployment_status() {
    cli 0 getdigidollardeploymentinfo | jq -r '.status // "unknown"'
}

print_deployment() {
    local label="$1"
    local info
    info="$(cli 0 getdigidollardeploymentinfo)"
    log "$label: height=$(cli 0 getblockcount) status=$(echo "$info" | jq -r '.status') enabled=$(echo "$info" | jq -r '.enabled') signal=$(echo "$info" | jq -r '.signaling_blocks // "n/a"') activation=$(echo "$info" | jq -r '.activation_height // "n/a"')"
}

assert_deployment_status() {
    local expected="$1"
    local actual
    actual="$(deployment_status)"
    if [ "$actual" != "$expected" ]; then
        cli 0 getdigidollardeploymentinfo | jq . >&2
        die "DigiDollar deployment status is $actual, expected $expected at height $(cli 0 getblockcount)"
    fi
}

assert_pre_activation_params() {
    local info
    info="$(cli 0 getdigidollardeploymentinfo)"
    echo "$info" | jq -e --argjson h "$ACTIVATION_HEIGHT" '
      .min_activation_height == $h and
      .oracle_activation_height == $h and
      .musig2_format_activation_height == $h and
      .oracle_pubkey_count == 35 and
      .oracle_consensus_required == 7 and
      .oracle_total_slots == 35
    ' >/dev/null || {
        echo "$info" | jq . >&2
        die "PRE DigiDollar/MuSig2/oracle params are not the expected mainnet rehearsal values"
    }
}

algo_for_next_height() {
    local next_height="$1"
    if [ "$next_height" -le 100 ]; then
        printf 'scrypt\n'
        return
    fi

    if [ "$next_height" -lt 500 ]; then
        local pre_algos=(sha256d skein scrypt qubit groestl)
        local idx=$(( ((next_height - 101) / 50) % 5 ))
        printf '%s\n' "${pre_algos[$idx]}"
        return
    fi

    local post_algos=(odo sha256d skein scrypt qubit)
    local idx=$(( ((next_height - 500) / 50) % 5 ))
    printf '%s\n' "${post_algos[$idx]}"
}

template_has_signal_bit() {
    local algo="$1"
    local template
    if ! template="$(cli 0 getblocktemplate '{"rules":["segwit"]}' "$algo" 2>/dev/null)"; then
        printf 'unknown\n'
        return
    fi
    echo "$template" | jq -r '((.version / 8388608 | floor) % 2) == 1'
}

core_race_tries_for_algo() {
    local algo="$1"
    case "$algo" in
        scrypt) printf '%s\n' "$CORE_RACE_TRIES_SCRYPT" ;;
        sha256d) printf '%s\n' "$CORE_RACE_TRIES_SHA256D" ;;
        skein) printf '%s\n' "$CORE_RACE_TRIES_SKEIN" ;;
        qubit) printf '%s\n' "$CORE_RACE_TRIES_QUBIT" ;;
        groestl) printf '%s\n' "$CORE_RACE_TRIES_GROESTL" ;;
        odo) printf '%s\n' "$CORE_RACE_TRIES_ODO" ;;
        *) printf '%s\n' "$CORE_RACE_TRIES" ;;
    esac
}

miner_node_for_algo() {
    local algo="$1"
    case "$algo" in
        scrypt) printf '0\n' ;;
        sha256d) printf '1\n' ;;
        skein) printf '2\n' ;;
        qubit) printf '3\n' ;;
        groestl) printf '4\n' ;;
        *) return 1 ;;
    esac
}

cpuminer_digidollar_args() {
    if [ "$CPUMINER_DIGIDOLLAR" = "1" ]; then
        printf '%s\n' '--digidollar'
    fi
}

mine_one_block_with_cpuminer() {
    local miner_addr="$1"
    local old_height="$2"
    local next_height="$3"
    local algo="$4"
    local miner_idx miner_name log_file pid started elapsed current_height actual_algo

    [ "$USE_CPUMINER" = "1" ] || return 1
    [ -x "$CPUMINER_BIN" ] || return 1
    miner_idx="$(miner_node_for_algo "$algo")" || return 1
    miner_name="${NODE_NAMES[$miner_idx]}"
    log_file="$BASE_DIR/cpuminer-height-${next_height}-${algo}.log"

    log "Mining block $next_height with cpuminer: algo=$algo node=$miner_name threads=$CPUMINER_THREADS"
    started="$(date +%s)"
    "$CPUMINER_BIN" \
        -a "$algo" \
        -o "http://127.0.0.1:${RPC_PORTS[$miner_idx]}/" \
        -O "$RPC_USER:$RPC_PASSWORD" \
        --coinbase-addr="$miner_addr" \
        --no-stratum \
        --no-getwork \
        --no-longpoll \
        --no-color \
        $(cpuminer_digidollar_args) \
        --threads="$CPUMINER_THREADS" \
        > "$log_file" 2>&1 &
    pid="$!"

    while kill -0 "$pid" >/dev/null 2>&1; do
        current_height="$(cli "$miner_idx" getblockcount 2>/dev/null || echo "$old_height")"
        if [ "$current_height" -gt "$old_height" ]; then
            kill "$pid" >/dev/null 2>&1 || true
            wait "$pid" >/dev/null 2>&1 || true
            sync_all_nodes
            elapsed=$(( $(date +%s) - started ))
            actual_algo="$(active_chain_block_algo "$current_height")"
            MINED_BY_ALGO[$actual_algo]=$(( ${MINED_BY_ALGO[$actual_algo]:-0} + current_height - old_height ))
            log "Mined block $current_height with active-chain algo=$actual_algo via cpuminer in ${elapsed}s"
            if [ "$actual_algo" != "$algo" ]; then
                tail -80 "$log_file" >&2 || true
                die "expected cpuminer algo $algo, but active-chain block $current_height is $actual_algo"
            fi
            return 0
        fi

        if [ $(( $(date +%s) - started )) -ge "$CPUMINER_BLOCK_TIMEOUT" ]; then
            kill "$pid" >/dev/null 2>&1 || true
            wait "$pid" >/dev/null 2>&1 || true
            tail -80 "$log_file" >&2 || true
            die "cpuminer timed out mining block $next_height with $algo after ${CPUMINER_BLOCK_TIMEOUT}s"
        fi
        sleep 1
    done

    tail -80 "$log_file" >&2 || true
    die "cpuminer exited before mining block $next_height with $algo"
}

mine_one_block_with_rpc() {
    local miner_addr="$1"
    local height next_height algo started elapsed signal
    height="$(cli 0 getblockcount)"
    next_height=$((height + 1))
    algo="$(algo_for_next_height "$next_height")"
    signal="$(template_has_signal_bit "$algo")"

    started="$(date +%s)"
    log "Mining block $next_height with $algo (template bit23=$signal, real PoW, maxtries=$MAX_TRIES)"
    wcli 0 bob generatetoaddress 1 "$miner_addr" "$MAX_TRIES" "$algo" >/dev/null
    elapsed=$(( $(date +%s) - started ))
    MINED_BY_ALGO[$algo]=$(( ${MINED_BY_ALGO[$algo]:-0} + 1 ))
    log "Mined block $next_height with $algo in ${elapsed}s"

    sync_all_nodes
}

mine_one_block_with_core_race() {
    local miner_addr="$1"
    local old_height="$2"
    local next_height="$3"
    local algo="$4"
    local started batch elapsed failed new_height actual_algo batch_tries

    started="$(date +%s)"
    batch=0
    batch_tries="$(core_race_tries_for_algo "$algo")"
    log "Mining block $next_height with Core batch race: algo=$algo nodes=5 workers_per_node=$CORE_RACE_WORKERS_PER_NODE batch_tries=$batch_tries timeout=$CORE_RACE_BLOCK_TIMEOUT"

    while true; do
        batch=$((batch + 1))
        local pids=()
        local logs=()
        local log_labels=()
        local i worker slot
        slot=0
        for i in "${!NODE_NAMES[@]}"; do
            for worker in $(seq 1 "$CORE_RACE_WORKERS_PER_NODE"); do
                local payout_addr
                payout_addr="$(new_legacy_address "$i" "${NODE_NAMES[$i]}")"
                logs[$slot]="$BASE_DIR/coremine-height-${next_height}-${algo}-${NODE_NAMES[$i]}-w${worker}-batch-${batch}.log"
                log_labels[$slot]="${NODE_NAMES[$i]} worker $worker"
                (
                    wcli "$i" "${NODE_NAMES[$i]}" generatetoaddress 1 "$payout_addr" "$batch_tries" "$algo"
                ) > "${logs[$slot]}" 2>&1 &
                pids[$slot]="$!"
                slot=$((slot + 1))
            done
        done

        failed=0
        for i in "${!pids[@]}"; do
            if ! wait "${pids[$i]}"; then
                failed=1
            fi
        done

        new_height="$(cli 0 getblockcount)"
        if [ "$new_height" -gt "$old_height" ]; then
            sync_all_nodes
            new_height="$(cli 0 getblockcount)"
            actual_algo="$(active_chain_block_algo "$new_height")"
            MINED_BY_ALGO[$actual_algo]=$(( ${MINED_BY_ALGO[$actual_algo]:-0} + new_height - old_height ))
            elapsed=$(( $(date +%s) - started ))
            log "Mined block $new_height with active-chain algo=$actual_algo via Core race in ${elapsed}s (${batch} batch(es))"
            if [ "$actual_algo" != "$algo" ]; then
                die "expected Core race algo $algo, but active-chain block $new_height is $actual_algo"
            fi
            return 0
        fi

        if [ "$failed" -ne 0 ]; then
            for i in "${!logs[@]}"; do
                echo "== ${log_labels[$i]} coremine log ==" >&2
                tail -80 "${logs[$i]}" >&2 || true
            done
            die "Core batch mining failed for block $next_height with $algo before any node advanced height"
        fi

        elapsed=$(( $(date +%s) - started ))
        if [ "$elapsed" -ge "$CORE_RACE_BLOCK_TIMEOUT" ]; then
            die "Core batch race timed out mining block $next_height with $algo after ${CORE_RACE_BLOCK_TIMEOUT}s"
        fi
        if [ $((batch % 5)) -eq 0 ]; then
            log "Still mining block $next_height with $algo after ${elapsed}s (${batch} batches)"
        fi
    done
}

mine_one_block() {
    local miner_addr="$1"
    local height next_height algo
    height="$(cli 0 getblockcount)"
    next_height=$((height + 1))
    algo="$(algo_for_next_height "$next_height")"

    if [ "$algo" != "odo" ] && mine_one_block_with_cpuminer "$miner_addr" "$height" "$next_height" "$algo"; then
        return
    fi

    if mine_one_block_with_core_race "$miner_addr" "$height" "$next_height" "$algo"; then
        return
    fi

    mine_one_block_with_rpc "$miner_addr"
}

mine_to_height() {
    local target="$1"
    local miner_addr="$2"
    while [ "$(cli 0 getblockcount)" -lt "$target" ]; do
        local before_h
        before_h="$(cli 0 getblockcount)"
        mine_one_block "$miner_addr"

        local h after_h
        after_h="$(cli 0 getblockcount)"
        for h in $(seq $((before_h + 1)) "$after_h"); do
            case "$h" in
                99)
                    print_deployment "BIP9 STARTED checkpoint"
                    assert_deployment_status started
                    ;;
                100)
                    print_deployment "First 100 scrypt blocks complete"
                    ;;
                199)
                    print_deployment "BIP9 LOCKED_IN checkpoint"
                    assert_deployment_status locked_in
                    ;;
                500)
                    log "Odo height reached; post-Odo rotation now uses odo/sha256d/skein/scrypt/qubit"
                    ;;
                599)
                    print_deployment "BIP9 ACTIVE checkpoint before height 600 block"
                    assert_deployment_status active
                    ;;
                600)
                    print_deployment "DigiDollar/MuSig2 activation height"
                    assert_deployment_status active
                    assert_pre_activation_params
                    start_digiswarm_oracle
                    ;;
            esac
        done

        if [ $((after_h % 25)) -eq 0 ]; then
            assert_no_public_peers
        fi
    done
}

read_key_from_file() {
    local file="$1"
    [ -n "$file" ] || return 1
    [ -f "$file" ] || return 1
    tr -d '[:space:]' < "$file"
}

ensure_digiswarm_key_ready() {
    local list_info pubkey key

    if list_info="$(wcli 0 "$DIGISWARM_WALLET_NAME" listoracle 2>/dev/null)"; then
        pubkey="$(echo "$list_info" | jq -r '.pubkey // empty')"
        if [ "$pubkey" = "$DIGISWARM_EXPECTED_PUBKEY" ]; then
            log "DigiSwarm wallet already contains the authorized slot $DIGISWARM_ORACLE_ID key"
            return
        fi
    fi

    if [ -n "$DIGISWARM_ORACLE_KEY" ]; then
        key="$DIGISWARM_ORACLE_KEY"
    elif key="$(read_key_from_file "$DIGISWARM_ORACLE_KEY_FILE" 2>/dev/null)"; then
        :
    else
        die "DigiSwarm oracle key is not available in wallet $DIGISWARM_WALLET_NAME; set DIGISWARM_ORACLE_WALLET_DIR, DIGISWARM_ORACLE_KEY_FILE, or DIGISWARM_ORACLE_KEY"
    fi

    log "Importing DigiSwarm oracle key into Bob wallet without printing the secret"
    wcli 0 "$DIGISWARM_WALLET_NAME" importoracleprivkey "$DIGISWARM_ORACLE_ID" "$key" true | jq -e --arg pub "$DIGISWARM_EXPECTED_PUBKEY" '
      .authorized == true and .pubkey == $pub
    ' >/dev/null || die "imported DigiSwarm oracle key does not match chainparams slot $DIGISWARM_ORACLE_ID"
}

start_digiswarm_oracle() {
    local result running pubkey

    log "Starting only the DigiSwarm oracle slot $DIGISWARM_ORACLE_ID on Bob"
    ensure_digiswarm_key_ready

    if [ -n "$DIGISWARM_WALLET_PASSPHRASE" ]; then
        wcli 0 "$DIGISWARM_WALLET_NAME" walletpassphrase "$DIGISWARM_WALLET_PASSPHRASE" 600 >/dev/null
    fi

    if ! result="$(wcli 0 "$DIGISWARM_WALLET_NAME" startoracle "$DIGISWARM_ORACLE_ID" 2>&1)"; then
        echo "$result" >&2
        die "startoracle $DIGISWARM_ORACLE_ID failed"
    fi
    echo "$result" | jq .

    sleep 3
    result="$(wcli 0 "$DIGISWARM_WALLET_NAME" listoracle)"
    running="$(echo "$result" | jq -r '.running // false')"
    pubkey="$(echo "$result" | jq -r '.pubkey // empty')"
    if [ "$running" != "true" ] || [ "$pubkey" != "$DIGISWARM_EXPECTED_PUBKEY" ]; then
        echo "$result" | jq . >&2
        die "DigiSwarm oracle is not running with the expected authorized pubkey"
    fi
    echo "$result" | jq '{running, configured, oracle_id, name, pubkey, authorized, wallet_name, price_source, enabled, last_heartbeat_time, musig2_context_version, message}'

    log "DigiSwarm oracle started. A single oracle proves local key/runtime health; 7-of-35 price quorum requires more operators."
}

fund_manual_wallets() {
    local miner_addr="$1"
    log "Funding Alice/Charlie/Dave/Eve wallets for manual Qt transactions"
    local i addr txid
    for i in 1 2 3 4; do
        addr="$(wcli "$i" "${NODE_NAMES[$i]}" getnewaddress)"
        txid="$(wcli 0 bob sendtoaddress "$addr" "$FUND_AMOUNT")"
        log "Sent $FUND_AMOUNT DGB from Bob to ${NODE_NAMES[$i]} ($txid)"
    done
    mine_one_block "$miner_addr"
}

print_manual_summary() {
    echo
    echo "Five Qt nodes are ready for manual testing."
    echo "BASE_DIR=$BASE_DIR"
    echo "Run info: $BASE_DIR/test_oracle_mainnet.env"
    echo
    for i in "${!NODE_NAMES[@]}"; do
        printf '  %-7s datadir=%s rpc=%s p2p=%s height=%s peers=%s\n' \
            "${NODE_NAMES[$i]}" \
            "${DATADIRS[$i]}" \
            "${RPC_PORTS[$i]}" \
            "${P2P_PORTS[$i]}" \
            "$(cli "$i" getblockcount)" \
            "$(cli "$i" getconnectioncount)"
    done
    echo
    echo "Mined block counts by algorithm:"
    for algo in scrypt sha256d skein qubit groestl odo; do
        printf '  %-7s %s\n' "$algo" "${MINED_BY_ALGO[$algo]:-0}"
    done
    echo
    echo "Useful commands:"
    echo "  $CLI_BIN -datadir=${DATADIRS[0]} -rpcport=${RPC_PORTS[0]} -rpcuser=$RPC_USER -rpcpassword=$RPC_PASSWORD getdigidollardeploymentinfo"
    echo "  $CLI_BIN -datadir=${DATADIRS[0]} -rpcport=${RPC_PORTS[0]} -rpcuser=$RPC_USER -rpcpassword=$RPC_PASSWORD -rpcwallet=$DIGISWARM_WALLET_NAME listoracle"
    echo "  $CLI_BIN -datadir=${DATADIRS[0]} -rpcport=${RPC_PORTS[0]} -rpcuser=$RPC_USER -rpcpassword=$RPC_PASSWORD getpeerinfo"
    echo
    if [ "$KEEP_QT_OPEN" = "1" ] && [ "$STOP_ON_SUCCESS" = "0" ]; then
        echo "KEEP_QT_OPEN=1: leaving all Qt nodes and datadirs intact."
    fi
}

assert_equal() {
    local expected="$1"
    local actual="$2"
    local label="$3"
    if [ "$expected" != "$actual" ]; then
        die "self-test failed: $label expected=$expected actual=$actual"
    fi
}

run_self_tests() {
    assert_equal scrypt "$(algo_for_next_height 1)" "height 1 algo"
    assert_equal scrypt "$(algo_for_next_height 100)" "height 100 algo"
    assert_equal sha256d "$(algo_for_next_height 101)" "height 101 algo"
    assert_equal sha256d "$(algo_for_next_height 150)" "height 150 algo"
    assert_equal skein "$(algo_for_next_height 151)" "height 151 algo"
    assert_equal scrypt "$(algo_for_next_height 201)" "height 201 algo"
    assert_equal qubit "$(algo_for_next_height 251)" "height 251 algo"
    assert_equal groestl "$(algo_for_next_height 301)" "height 301 algo"
    assert_equal odo "$(algo_for_next_height 500)" "height 500 algo"
    assert_equal sha256d "$(algo_for_next_height 550)" "height 550 algo"
    assert_equal "$CORE_RACE_TRIES_SHA256D" "$(core_race_tries_for_algo sha256d)" "sha256d try window"
    assert_equal "$CORE_RACE_TRIES_SKEIN" "$(core_race_tries_for_algo skein)" "skein try window"
    assert_equal "$CORE_RACE_TRIES_QUBIT" "$(core_race_tries_for_algo qubit)" "qubit try window"
    assert_equal "$CORE_RACE_TRIES_GROESTL" "$(core_race_tries_for_algo groestl)" "groestl try window"
    assert_equal "$CORE_RACE_TRIES_ODO" "$(core_race_tries_for_algo odo)" "odo try window"
    assert_equal "$CORE_RACE_TRIES_SCRYPT" "$(core_race_tries_for_algo scrypt)" "scrypt try window"
    assert_equal "--digidollar" "$(cpuminer_digidollar_args)" "cpuminer DD-aware GBT flag"
    log "SELF_TEST=1: schedule and mining-window checks passed"
}

stop_nodes() {
    log "Stopping Qt nodes; datadirs are still preserved under $BASE_DIR"
    local i
    for i in "${!NODE_NAMES[@]}"; do
        cli "$i" stop >/dev/null 2>&1 || true
    done
}

main() {
    if [ "$SELF_TEST" = "1" ]; then
        run_self_tests
        return
    fi

    require_file "$QT_BIN"
    require_file "$CLI_BIN"
    command -v jq >/dev/null || die "jq is required"
    command -v python3 >/dev/null || die "python3 is required"

    check_ports_available_for_new_run
    init_dirs
    copy_digiswarm_wallet

    log "Starting isolated five-node mainnet-PRE rehearsal"
    log "Same mainnet genesis/magic; local PRE datadirs and ports only. BASE_DIR=$BASE_DIR"
    for i in "${!NODE_NAMES[@]}"; do
        start_node "$i"
    done
    for i in "${!NODE_NAMES[@]}"; do
        wait_for_rpc "$i"
    done

    ensure_wallets
    wait_for_links
    assert_no_public_peers
    sync_all_nodes
    assert_pre_activation_params
    print_deployment "Initial deployment state"

    if [ "$START_ONLY" = "1" ]; then
        log "START_ONLY=1: startup/isolation checks complete; skipping mining"
        print_manual_summary
        return
    fi

    local miner_addr
    miner_addr="$(new_legacy_address 0 bob)"
    mine_to_height "$TARGET_HEIGHT" "$miner_addr"
    assert_no_public_peers
    fund_manual_wallets "$miner_addr"
    assert_no_public_peers
    sync_all_nodes

    print_deployment "Final deployment state"
    print_manual_summary

    if [ "$KEEP_QT_OPEN" != "1" ] || [ "$STOP_ON_SUCCESS" = "1" ]; then
        stop_nodes
    fi
}

main "$@"
