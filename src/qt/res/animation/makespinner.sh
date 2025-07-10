#!/usr/bin/env bash
#
<<<<<<< HEAD
# Copyright (c) 2014-2020 The DigiByte Core developers
=======
# Copyright (c) 2014-2021 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
<<<<<<< HEAD
FRAMEDIR=$(dirname $0)
for i in {0..35}
do
    frame=$(printf "%03d" $i)
    angle=$((i * 10))
    convert $FRAMEDIR/../src/spinner.png -background "rgba(0,0,0,0.0)" -distort SRT $angle $FRAMEDIR/spinner-$frame.png
=======
FRAMEDIR=$(dirname "$0")
for i in {0..35}
do
    frame=$(printf "%03d" "$i")
    angle=$((i * 10))
    convert "${FRAMEDIR}/../src/spinner.png" -background "rgba(0,0,0,0.0)" -distort SRT $angle "${FRAMEDIR}/spinner-${frame}.png"
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
done
