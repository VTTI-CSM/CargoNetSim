#include "Backend/BackendInit.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/LogMessageHandler.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "GUI/MainWindow.h"
#include "GUI/Utils/ApplicationLogger.h"
#include "GUI/Utils/ErrorHandlers.h"
#include "GUI/Widgets/SplashScreen.h"
#include <QApplication>
#include <QLoggingCategory>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QObject>
#include <QSplashScreen>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <signal.h>

using namespace CargoNetSim::GUI;

bool isAnotherInstanceRunning(const QString &serverName)
{
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(100))
    {
        return true; // Another instance is already running
    }
    return false; // No instance running
}

void createLocalServer(const QString &serverName)
{
    // Reclaim a socket file orphaned by a previous crash or
    // SIGKILL. Safe here: isAnotherInstanceRunning() already
    // confirmed no live listener exists.
    QLocalServer::removeServer(serverName);

    QLocalServer *localServer = new QLocalServer(qApp);
    localServer->setSocketOptions(
        QLocalServer::WorldAccessOption);
    if (!localServer->listen(serverName))
    {
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical);
        errorBox.setWindowTitle("CargoNetSim Error");
        errorBox.setText("Failed to create local server.");
        errorBox.setDetailedText(
            localServer->errorString());
        errorBox.setStandardButtons(QMessageBox::Ok);
        errorBox.exec();
        exit(EXIT_FAILURE);
    }
}

// Signal handler for SIGINT (Ctrl+C). Re-entry guard ensures
// quit() is triggered exactly once if the signal fires repeatedly.
void signalHandler(int signal)
{
    static std::atomic<bool> handled{false};
    bool                     expected = false;
    if (!handled.compare_exchange_strong(expected, true))
    {
        return;
    }

    qCInfo(lcInit) << "Received signal:" << signal;
    QApplication::quit();
}

int main(int argc, char *argv[])
{
    // Configure logging: debug enabled in debug builds only.
    // Override at runtime: QT_LOGGING_RULES="cargonetsim.*=true"
#ifdef QT_DEBUG
    QLoggingCategory::setFilterRules(QStringLiteral(
        "cargonetsim.*.debug=true\n"
        "cargonetsim.*.info=true\n"
        "cargonetsim.*.warning=true\n"
        "cargonetsim.*.critical=true\n"));
#else
    QLoggingCategory::setFilterRules(QStringLiteral(
        "cargonetsim.*.debug=false\n"
        "cargonetsim.*.info=true\n"
        "cargonetsim.*.warning=true\n"
        "cargonetsim.*.critical=true\n"));
#endif

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    // Install after QApplication: the qtMessageHandler calls
    // applicationDirPath(), which requires QApplication to
    // exist. Installing earlier causes any qWarning to recurse
    // through the handler via applicationDirPath()'s own
    // qWarning call.
    installExceptionHandlers();

    // Unique name for the local server
    const QString uniqueServerName =
        "CargoNetSimServerInstance";

    // Check if another instance is already running
    if (isAnotherInstanceRunning(uniqueServerName))
    {
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical);
        errorBox.setWindowTitle("CargoNetSim Error");
        errorBox.setText("Another instance of CargoNetSim "
                         "is already running.");
        errorBox.setStandardButtons(QMessageBox::Ok);
        errorBox.exec();
        return EXIT_FAILURE;
    }

    // Create the local server to mark this instance as the
    // active one
    createLocalServer(uniqueServerName);

    // Track initialization time
    QElapsedTimer initTimer;
    initTimer.start();

    // Initialize the logger first
    CargoNetSim::Backend::LoggerInterface *logger =
        ApplicationLogger::getInstance();
    ApplicationLogger::getInstance()->start();

    // Bridge Qt categorized logging to GUI log tabs.
    // Info/warning/critical from cargonetsim.* categories
    // appear in the Servers Log tab for the user.
    CargoNetSim::Backend::installCargoNetSimLogHandler(
        ApplicationLogger::getInstance());

    // Set application info
    app.setApplicationName("CargoNetSim");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CargoNetSim Org");

    // Initialize backend metatypes
    CargoNetSim::Backend::initializeBackend("", logger);

    // Set up signal handling for graceful shutdown on Ctrl+C.
    // Intentionally NOT connected to aboutToQuit: calling
    // CargoNetSimControllerCleanup::cleanup() during shutdown
    // deletes the singleton while GUI widgets still hold pointers
    // to it, causing heap corruption during later destruction.
    // The OS reclaims memory on process exit; ~QApplication
    // cleans up the QLocalServer socket file via parent ownership.
    signal(SIGINT, signalHandler);

    // Create splash screen
    SplashScreen splash;
    splash.show();

    // Minimum splash screen display time (in ms)
    const int MINIMUM_SPLASH_TIME = 3000; // 3 seconds

    // Create and initialize main window in background
    MainWindow *mainWindow = nullptr;
    QTimer::singleShot(500, [&]() {
        // Create and initialize the main window
        mainWindow = MainWindow::getInstance();

        // TODO: Pre-load any necessary resources or perform
        // initialization here
        splash.showMessage(
            "Loading application...",
            Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
        QApplication::processEvents();

        // Calculate remaining time to display splash
        int elapsedMs   = initTimer.elapsed();
        int remainingMs = MINIMUM_SPLASH_TIME - elapsedMs;

        // Show "Ready" message
        splash.showMessage(
            "Ready...", Qt::AlignBottom | Qt::AlignHCenter,
            Qt::black);
        QApplication::processEvents();

        // Only proceed after minimum splash time has
        // elapsed
        if (remainingMs > 0)
        {
            QTimer::singleShot(remainingMs, [&]() {
                splash.finish(mainWindow);
                mainWindow->showMaximized();
                // Signal initialization complete
                ApplicationLogger::signalInitComplete();
            });
        }
        else
        {
            splash.finish(mainWindow);
            mainWindow->showMaximized();
            // Signal initialization complete
            ApplicationLogger::signalInitComplete();
        }
    });

    return app.exec();
}
