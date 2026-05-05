#include "SplashScreen.h"

#include <QApplication>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace GUI
{

SplashScreen::SplashScreen()
    : QSplashScreen()
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_progress(0)
    , m_statusMessage("Loading...")
    , m_isFinished(false)
{
    qCDebug(lcGui) << "SplashScreen::SplashScreen: constructing";
    // Load splash image
    QString imagePath = ":/Splash";
    m_originalPixmap  = QPixmap(imagePath);

    if (m_originalPixmap.isNull())
    {
        qCWarning(lcGuiUtil) << "Failed to load splash image:"
                             << imagePath;
        // Create a default splash pixmap
        m_originalPixmap = QPixmap(600, 400);
        m_originalPixmap.fill(Qt::white);

        QPainter painter(&m_originalPixmap);
        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 20, QFont::Bold));
        painter.drawText(m_originalPixmap.rect(),
                         Qt::AlignCenter, "CargoNetSim");
    }

    // Scale splash image for high DPI screens
    qreal dpiScale = qApp->devicePixelRatio();
    if (dpiScale > 1.0)
    {
        m_originalPixmap.setDevicePixelRatio(dpiScale);
    }

    // Set the splash pixmap
    setPixmap(m_originalPixmap);

    // Initialize UI components
    initUI();

    // Center on screen
    QScreen *primaryScreen =
        QGuiApplication::primaryScreen();
    if (primaryScreen)
    {
        QRect screenGeometry =
            primaryScreen->availableGeometry();
        move(screenGeometry.center() - rect().center());
    }

    // Set window flag to stay on top
    setWindowFlags(windowFlags()
                   | Qt::WindowStaysOnTopHint);
}

SplashScreen::~SplashScreen()
{
    // Clean up owned widgets if not already deleted
    if (m_progressBar && !m_progressBar->parent())
    {
        delete m_progressBar;
    }

    if (m_statusLabel && !m_statusLabel->parent())
    {
        delete m_statusLabel;
    }
}

void SplashScreen::initUI()
{
    // Create progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setAlignment(Qt::AlignCenter);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "   border: 1px solid gray;"
        "   border-radius: 3px;"
        "   background: white;"
        "   padding: 1px;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #4CAF50;"
        "   width: 10px;"
        "   margin: 0.5px;"
        "}");

    // Create status label
    m_statusLabel = new QLabel(m_statusMessage, this);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "   color: black;"
        "   background-color: rgba(255, 255, 255, 180);"
        "   border-radius: 3px;"
        "   padding: 3px;"
        "}");

    // Position the widgets
    updateLayout();
}

void SplashScreen::updateLayout()
{
    if (!m_progressBar || !m_statusLabel)
        return;

    // Get dimensions
    int width  = pixmap().width();
    int height = pixmap().height();

    // Set progress bar position and size (near bottom)
    int barWidth  = width * 0.8; // 80% of splash width
    int barHeight = 20;
    m_progressBar->setGeometry((width - barWidth) / 2,
                               height - barHeight
                                   - 40, // 40px from bottom
                               barWidth, barHeight);

    // Set status label position and size (above progress
    // bar)
    m_statusLabel->setGeometry(
        (width - barWidth) / 2,
        height - barHeight - 70, // 30px above progress bar
        barWidth, 20);
}

void SplashScreen::drawContents(QPainter *painter)
{
    // First let the base class draw the pixmap
    QSplashScreen::drawContents(painter);

    // Additional custom drawing if needed
}

void SplashScreen::resizeEvent(QResizeEvent *event)
{
    QSplashScreen::resizeEvent(event);
    updateLayout();
}

void SplashScreen::setProgress(int progress)
{
    if (m_progress != progress)
    {
        m_progress = qBound(0, progress, 100);
        if (m_progressBar)
        {
            m_progressBar->setValue(m_progress);
        }

        emit progressChanged(m_progress);

        // If progress reaches 100%, emit loading complete
        if (m_progress == 100 && !m_isFinished)
        {
            m_isFinished = true;
            qCInfo(lcGui) << "SplashScreen::setProgress:"
                          << "loading complete";
            emit loadingComplete();
        }

        // Ensure the splash screen is repainted
        repaint();
    }
}

void SplashScreen::setStatusMessage(const QString &message)
{
    if (m_statusMessage != message)
    {
        m_statusMessage = message;
        if (m_statusLabel)
        {
            m_statusLabel->setText(message);
        }

        emit statusMessageChanged(message);

        // Ensure the splash screen is repainted
        repaint();
    }
}

void SplashScreen::showMessage(const QString &message,
                               int            alignment,
                               const QColor  &color)
{
    qCDebug(lcGui) << "SplashScreen::showMessage:"
                   << message;
    // Update internal status message
    setStatusMessage(message);

    // Call base class method
    QSplashScreen::showMessage(message, alignment, color);
}

} // namespace GUI
} // namespace CargoNetSim
