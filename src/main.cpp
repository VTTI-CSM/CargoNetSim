#include "Backend/Bootstrap/BackendBootstrapService.h"
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
#include <QSocketNotifier>
#include <QSplashScreen>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

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

// Self-pipe for SIGINT handling.
//
// POSIX signal handlers may only call async-signal-safe functions
// (not qCInfo, not QApplication::quit). Standard Qt workaround:
// the handler writes one byte to a socketpair; a QSocketNotifier
// on the main thread picks it up and performs the real shutdown
// work in normal Qt event-loop context.
//
// s_sigIntFd[0] = write end (used by the signal handler)
// s_sigIntFd[1] = read  end (used by the QSocketNotifier)
static int s_sigIntFd[2] = {-1, -1};

extern "C" void sigIntHandler(int sig)
{
    const char byte = static_cast<char>(sig);
    (void)::write(s_sigIntFd[0], &byte, sizeof(byte));
}

// Main-thread slot invoked by QSocketNotifier when a signal byte
// is available. Attempts a graceful quit; if the event loop
// doesn't exit within a short grace period (because worker
// threads block in librabbitmq and keep the loop busy draining
// their queued messages), force-exits the process.
//
// Force-exit is acceptable for Ctrl+C semantics: the user asked
// for immediate termination, memory is reclaimed by the OS, and
// the QLocalServer socket file is cleaned on next launch by
// QLocalServer::removeServer().
static void onSigIntReceived(QSocketNotifier *notifier)
{
    notifier->setEnabled(false);
    char byte = 0;
    if (::read(s_sigIntFd[1], &byte, sizeof(byte)) == sizeof(byte))
    {
        qCInfo(lcInit)
            << "Received signal:" << static_cast<int>(byte);
    }

    QApplication::quit();

    // Safety net: if graceful quit doesn't return from app.exec()
    // within the grace period, force-exit. In practice this always
    // fires under the current backend design because the RabbitMQ
    // worker threads block in amqp_consume_message and their
    // messageReceived signal emissions keep the main loop busy
    // enough to defer the exit-flag check indefinitely.
    constexpr int kGracePeriodMs = 1500;
    QTimer::singleShot(kGracePeriodMs, [] {
        qCInfo(lcInit)
            << "Graceful shutdown timed out; forcing process exit.";
        std::_Exit(0);
    });
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
    // It is also declared BEFORE backend bootstrap because bootstrap
    // registers metatypes AND calls initialize() / startAll(), which
    // spawn worker threads that will immediately begin reading those
    // metatypes. The controller must exist before those reads happen -
    // and its constructor must have finished setting s_instance so
    // workers can find it via instance().
    CargoNetSim::CargoNetSimController controller(logger);

    // Initialize backend metatypes and bring the controller online
    // through the explicit bootstrap contract.
    const CargoNetSim::Backend::BackendBootstrapService
        backendBootstrap;
    const auto bootstrapResult =
        backendBootstrap.initializeAndStartController("");
    if (!bootstrapResult.succeeded())
    {
        qFatal("main: %s",
               qPrintable(bootstrapResult.message.isEmpty()
                              ? QStringLiteral(
                                    "backend bootstrap failed")
                              : bootstrapResult.message));
    }

    // Ctrl+C graceful exit via the self-pipe pattern. The POSIX
    // handler writes one byte; a QSocketNotifier on the main
    // thread picks it up in Qt event-loop context and issues
    // QApplication::quit() with a force-exit safety net.
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_sigIntFd) != 0)
    {
        qFatal("main: failed to create SIGINT self-pipe (errno=%d)",
               errno);
    }
    auto *sigIntNotifier = new QSocketNotifier(
        s_sigIntFd[1], QSocketNotifier::Read, &app);
    QObject::connect(sigIntNotifier, &QSocketNotifier::activated,
                     &app, [sigIntNotifier]() {
                         onSigIntReceived(sigIntNotifier);
                     });
    ::signal(SIGINT, sigIntHandler);

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
