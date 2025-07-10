<<<<<<< HEAD
// Copyright (c) 2019-2020 The DigiByte Core developers
=======
// Copyright (c) 2019-2021 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_WALLETCONTROLLER_H
#define DIGIBYTE_QT_WALLETCONTROLLER_H

#include <qt/sendcoinsrecipient.h>
#include <support/allocators/secure.h>
#include <sync.h>
#include <util/translation.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QMessageBox>
#include <QMutex>
#include <QProgressDialog>
#include <QThread>
#include <QTimer>
#include <QString>

class ClientModel;
class OptionsModel;
class PlatformStyle;
class WalletModel;

namespace interfaces {
class Handler;
class Node;
class Wallet;
} // namespace interfaces

<<<<<<< HEAD
class AskPassphraseDialog;
class CreateWalletActivity;
class CreateWalletDialog;
=======
namespace fs {
class path;
}

class AskPassphraseDialog;
class CreateWalletActivity;
class CreateWalletDialog;
class MigrateWalletActivity;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
class OpenWalletActivity;
class WalletControllerActivity;

/**
 * Controller between interfaces::Node, WalletModel instances and the GUI.
 */
class WalletController : public QObject
{
    Q_OBJECT

    void removeAndDeleteWallet(WalletModel* wallet_model);

public:
    WalletController(ClientModel& client_model, const PlatformStyle* platform_style, QObject* parent);
    ~WalletController();

<<<<<<< HEAD
    //! Returns wallet models currently open.
    std::vector<WalletModel*> getOpenWallets() const;

=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    WalletModel* getOrCreateWallet(std::unique_ptr<interfaces::Wallet> wallet);

    //! Returns all wallet names in the wallet dir mapped to whether the wallet
    //! is loaded.
    std::map<std::string, bool> listWalletDir() const;

    void closeWallet(WalletModel* wallet_model, QWidget* parent = nullptr);
    void closeAllWallets(QWidget* parent = nullptr);

<<<<<<< HEAD
=======
    void migrateWallet(WalletModel* wallet_model, QWidget* parent = nullptr);

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
Q_SIGNALS:
    void walletAdded(WalletModel* wallet_model);
    void walletRemoved(WalletModel* wallet_model);

    void coinsSent(WalletModel* wallet_model, SendCoinsRecipient recipient, QByteArray transaction);

private:
    QThread* const m_activity_thread;
    QObject* const m_activity_worker;
    ClientModel& m_client_model;
    interfaces::Node& m_node;
    const PlatformStyle* const m_platform_style;
    OptionsModel* const m_options_model;
    mutable QMutex m_mutex;
    std::vector<WalletModel*> m_wallets;
    std::unique_ptr<interfaces::Handler> m_handler_load_wallet;

    friend class WalletControllerActivity;
<<<<<<< HEAD
=======
    friend class MigrateWalletActivity;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
};

class WalletControllerActivity : public QObject
{
    Q_OBJECT

public:
    WalletControllerActivity(WalletController* wallet_controller, QWidget* parent_widget);
<<<<<<< HEAD
    virtual ~WalletControllerActivity();
=======
    virtual ~WalletControllerActivity() = default;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

Q_SIGNALS:
    void finished();

protected:
    interfaces::Node& node() const { return m_wallet_controller->m_node; }
    QObject* worker() const { return m_wallet_controller->m_activity_worker; }

<<<<<<< HEAD
    void showProgressDialog(const QString& label_text);
    void destroyProgressDialog();

    WalletController* const m_wallet_controller;
    QWidget* const m_parent_widget;
    QProgressDialog* m_progress_dialog{nullptr};
=======
    void showProgressDialog(const QString& title_text, const QString& label_text, bool show_minimized=false);

    WalletController* const m_wallet_controller;
    QWidget* const m_parent_widget;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    WalletModel* m_wallet_model{nullptr};
    bilingual_str m_error_message;
    std::vector<bilingual_str> m_warning_message;
};


class CreateWalletActivity : public WalletControllerActivity
{
    Q_OBJECT

public:
    CreateWalletActivity(WalletController* wallet_controller, QWidget* parent_widget);
    virtual ~CreateWalletActivity();

    void create();

Q_SIGNALS:
    void created(WalletModel* wallet_model);

private:
    void askPassphrase();
    void createWallet();
    void finish();

    SecureString m_passphrase;
    CreateWalletDialog* m_create_wallet_dialog{nullptr};
    AskPassphraseDialog* m_passphrase_dialog{nullptr};
};

class OpenWalletActivity : public WalletControllerActivity
{
    Q_OBJECT

public:
    OpenWalletActivity(WalletController* wallet_controller, QWidget* parent_widget);

    void open(const std::string& path);

Q_SIGNALS:
    void opened(WalletModel* wallet_model);

private:
    void finish();
};

<<<<<<< HEAD
=======
class LoadWalletsActivity : public WalletControllerActivity
{
    Q_OBJECT

public:
    LoadWalletsActivity(WalletController* wallet_controller, QWidget* parent_widget);

    void load(bool show_loading_minimized);
};

class RestoreWalletActivity : public WalletControllerActivity
{
    Q_OBJECT

public:
    RestoreWalletActivity(WalletController* wallet_controller, QWidget* parent_widget);

    void restore(const fs::path& backup_file, const std::string& wallet_name);

Q_SIGNALS:
    void restored(WalletModel* wallet_model);

private:
    void finish();
};

class MigrateWalletActivity : public WalletControllerActivity
{
    Q_OBJECT

public:
    MigrateWalletActivity(WalletController* wallet_controller, QWidget* parent) : WalletControllerActivity(wallet_controller, parent) {}

    void migrate(WalletModel* wallet_model);

Q_SIGNALS:
    void migrated(WalletModel* wallet_model);

private:
    QString m_success_message;

    void finish();
};

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#endif // DIGIBYTE_QT_WALLETCONTROLLER_H
