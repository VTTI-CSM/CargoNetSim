#include "Backend/BackendInit.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/LogMessageHandler.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "GUI/MainWindow.h"
#include "GUI/Utils/ApplicationLogger.h"
#include "GUI/Utils/ErrorHandlers.h"
#include "GUI/Widgets/SplashScreen.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
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

    // Tier 1 lifetime (Option E): the controller is a stack variable
    // owned by main(). It is declared BEFORE MainWindow (below) so
    // stack unwinding destroys MainWindow first, then the controller,
    // then QApplication - the required teardown order.
    //
    // It is also declared BEFORE initializeBackend because
    // initializeBackend registers metatypes AND calls initialize() /
    // startAll(), which spawn worker threads that will immediately
    // begin reading those metatypes. The controller must exist before
    // those reads happen - and its constructor must have finished
    // setting s_instance so workers can find it via instance().
    CargoNetSim::CargoNetSimController controller(logger);

    // Initialize backend metatypes and bring the controller online.
    CargoNetSim::Backend::initializeBackend("", logger);

    // Ctrl+C graceful exit. No aboutToQuit connection: Option E
    // stack ownership and the controller's s_instance lifetime
    // already ensure deterministic teardown - main() unwinds
    // ~MainWindow, ~CargoNetSimController, ~QApplication in
    // that order on normal return.
    signal(SIGINT, signalHandler);

    // Splash screen: show immediately, construct MainWindow eagerly
    // (required for stack allocation), enforce a minimum display
    // time via a local event loop before switching to the main window.
    SplashScreen splash;
    splash.show();
    QApplication::processEvents();

    // Timer scope is "splash visible" rather than "whole startup".
    // The old timer was started before logger/controller init and
    // often elapsed before the user ever saw the splash; this one
    // starts after the splash is visible, so the user always sees
    // the splash for at least MINIMUM_SPLASH_TIME_MS.
    QElapsedTimer splashTimer;
    splashTimer.start();
    constexpr int MINIMUM_SPLASH_TIME_MS = 3000;

    // Tier 1 Option E: MainWindow is a stack variable owned by main(),
    // declared AFTER the controller (Task 3). Stack unwind destroys
    // MainWindow first, then the controller, then QApplication -
    // the required teardown order.
    MainWindow mw;

    splash.showMessage(
        "Loading application...",
        Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
    QApplication::processEvents();

    splash.showMessage(
        "Ready...", Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
    QApplication::processEvents();

    // Enforce minimum splash time with a local event loop rather
    // than a cascade of QTimer::singleShot lambdas.
    const int remainingMs =
        MINIMUM_SPLASH_TIME_MS - static_cast<int>(splashTimer.elapsed());
    if (remainingMs > 0)
    {
        QEventLoop splashLoop;
        QTimer::singleShot(remainingMs, &splashLoop, &QEventLoop::quit);
        splashLoop.exec();
    }
    splash.finish(&mw);
    mw.showMaximized();
    ApplicationLogger::signalInitComplete();

    return app.exec();
}
