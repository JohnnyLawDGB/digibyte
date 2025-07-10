<<<<<<< HEAD
// Copyright (c) 2014-2018 The DigiByte Core developers
=======
// Copyright (c) 2014-2022 The Bitcoin Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <zmq/zmqutil.h>

#include <logging.h>
#include <zmq.h>

#include <cerrno>
#include <string>

void zmqError(const std::string& str)
{
<<<<<<< HEAD
    LogPrint(BCLog::ZMQ, "zmq: Error: %s, msg: %s\n", str, zmq_strerror(errno));
=======
    LogPrint(BCLog::ZMQ, "Error: %s, msg: %s\n", str, zmq_strerror(errno));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}
