<<<<<<< HEAD
// Copyright (c) 2019 The DigiByte Core developers
=======
// Copyright (c) 2019-2021 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_CREATEWALLETDIALOG_H
#define DIGIBYTE_QT_CREATEWALLETDIALOG_H

#include <QDialog>

<<<<<<< HEAD
class ExternalSigner;
=======
#include <memory>

namespace interfaces {
class ExternalSigner;
} // namespace interfaces

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
class WalletModel;

namespace Ui {
    class CreateWalletDialog;
}

/** Dialog for creating wallets
 */
class CreateWalletDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateWalletDialog(QWidget* parent);
    virtual ~CreateWalletDialog();

<<<<<<< HEAD
    void setSigners(const std::vector<ExternalSigner>& signers);
=======
    void setSigners(const std::vector<std::unique_ptr<interfaces::ExternalSigner>>& signers);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    QString walletName() const;
    bool isEncryptWalletChecked() const;
    bool isDisablePrivateKeysChecked() const;
    bool isMakeBlankWalletChecked() const;
<<<<<<< HEAD
    bool isDescriptorWalletChecked() const;
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    bool isExternalSignerChecked() const;

private:
    Ui::CreateWalletDialog *ui;
<<<<<<< HEAD
=======
    bool m_has_signers = false;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
};

#endif // DIGIBYTE_QT_CREATEWALLETDIALOG_H
