<<<<<<< HEAD
// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2011-2020 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_DIGIBYTEADDRESSVALIDATOR_H
#define DIGIBYTE_QT_DIGIBYTEADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
<<<<<<< HEAD
class DigiByteAddressEntryValidator : public QValidator
=======
class BitcoinAddressEntryValidator : public QValidator
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    Q_OBJECT

public:
<<<<<<< HEAD
    explicit DigiByteAddressEntryValidator(QObject *parent);
=======
    explicit BitcoinAddressEntryValidator(QObject *parent);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    State validate(QString &input, int &pos) const override;
};

/** DigiByte address widget validator, checks for a valid digibyte address.
 */
<<<<<<< HEAD
class DigiByteAddressCheckValidator : public QValidator
=======
class BitcoinAddressCheckValidator : public QValidator
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    Q_OBJECT

public:
<<<<<<< HEAD
    explicit DigiByteAddressCheckValidator(QObject *parent);
=======
    explicit BitcoinAddressCheckValidator(QObject *parent);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    State validate(QString &input, int &pos) const override;
};

#endif // DIGIBYTE_QT_DIGIBYTEADDRESSVALIDATOR_H
