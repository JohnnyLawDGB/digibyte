<<<<<<< HEAD
// Copyright (c) 2011-2020 The DigiByte Core developers
// Copyright (c) 2013-2021 The DigiByte Core developers
=======
// Copyright (c) 2011-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_DIGIBYTE_H
#define DIGIBYTE_QT_DIGIBYTE_H

#if defined(HAVE_CONFIG_H)
#include <config/digibyte-config.h>
#endif

#include <interfaces/node.h>
#include <qt/initexecutor.h>

#include <assert.h>
#include <memory>
#include <optional>

#include <QApplication>

class DigiByteGUI;
class ClientModel;
class NetworkStyle;
class OptionsModel;
class PaymentServer;
class PlatformStyle;
class SplashScreen;
class WalletController;
class WalletModel;
<<<<<<< HEAD
=======
namespace interfaces {
class Init;
} // namespace interfaces
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion


/** Main DigiByte application object */
class DigiByteApplication: public QApplication
{
    Q_OBJECT
public:
    explicit DigiByteApplication();
    ~DigiByteApplication();

#ifdef ENABLE_WALLET
    /// Create payment server
    void createPaymentServer();
#endif
    /// parameter interaction/setup based on rules
    void parameterSetup();
    /// Create options model
<<<<<<< HEAD
    void createOptionsModel(bool resetSettings);
=======
    [[nodiscard]] bool createOptionsModel(bool resetSettings);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    /// Initialize prune setting
    void InitPruneSetting(int64_t prune_MiB);
    /// Create main window
    void createWindow(const NetworkStyle *networkStyle);
    /// Create splash screen
    void createSplashScreen(const NetworkStyle *networkStyle);
<<<<<<< HEAD
=======
    /// Create or spawn node
    void createNode(interfaces::Init& init);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    /// Basic initialization, before starting initialization/shutdown thread. Return true on success.
    bool baseInitialize();

    /// Request core initialization
    void requestInitialize();
<<<<<<< HEAD
    /// Request core shutdown
    void requestShutdown();

    /// Get process return value
    int getReturnValue() const { return returnValue; }
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    /// Get window identifier of QMainWindow (DigiByteGUI)
    WId getMainWinId() const;

    /// Setup platform style
    void setupPlatformStyle();

    interfaces::Node& node() const { assert(m_node); return *m_node; }
<<<<<<< HEAD
    void setNode(interfaces::Node& node);

public Q_SLOTS:
    void initializeResult(bool success, interfaces::BlockAndHeaderTipInfo tip_info);
    void shutdownResult();
=======

public Q_SLOTS:
    void initializeResult(bool success, interfaces::BlockAndHeaderTipInfo tip_info);
    /// Request core shutdown
    void requestShutdown();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    /// Handle runaway exceptions. Shows a message box with the problem and quits the program.
    void handleRunawayException(const QString &message);

    /**
     * A helper function that shows a message box
     * with details about a non-fatal exception.
     */
    void handleNonFatalException(const QString& message);

Q_SIGNALS:
    void requestedInitialize();
    void requestedShutdown();
<<<<<<< HEAD
    void splashFinished();
    void windowShown(DigiByteGUI* window);

private:
    std::optional<InitExecutor> m_executor;
    OptionsModel *optionsModel;
    ClientModel *clientModel;
    DigiByteGUI *window;
    QTimer *pollShutdownTimer;
=======
    void windowShown(DigiByteGUI* window);

protected:
    bool event(QEvent* e) override;

private:
    std::optional<InitExecutor> m_executor;
    OptionsModel* optionsModel{nullptr};
    ClientModel* clientModel{nullptr};
    DigiByteGUI* window{nullptr};
    QTimer* pollShutdownTimer{nullptr};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#ifdef ENABLE_WALLET
    PaymentServer* paymentServer{nullptr};
    WalletController* m_wallet_controller{nullptr};
#endif
<<<<<<< HEAD
    int returnValue;
    const PlatformStyle *platformStyle;
    std::unique_ptr<QWidget> shutdownWindow;
    SplashScreen* m_splash = nullptr;
    interfaces::Node* m_node = nullptr;
=======
    const PlatformStyle* platformStyle{nullptr};
    std::unique_ptr<QWidget> shutdownWindow;
    SplashScreen* m_splash = nullptr;
    std::unique_ptr<interfaces::Node> m_node;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    void startThread();
};

int GuiMain(int argc, char* argv[]);

<<<<<<< HEAD
#endif // DIGIBYTE_QT_DIGIBYTE_H
=======
#endif // DIGIBYTE_QT_DIGIBYTE_H
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
