// Copyright (c) 2011-2022 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <meowcoin-build-config.h> // IWYU pragma: keep

#include <qt/splashscreen.h>

#include <clientversion.h>
#include <common/system.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <qt/guiutil.h>
#include <qt/networkstyle.h>
#include <qt/walletmodel.h>
#include <util/translation.h>

#include <functional>

#include <QApplication>
#include <QCloseEvent>
#include <QPainter>
#include <QLinearGradient>
#include <QScreen>


SplashScreen::SplashScreen(const NetworkStyle* networkStyle)
    : QWidget()
{
    float devicePixelRatio      = 1.0;
    devicePixelRatio = static_cast<QGuiApplication*>(QCoreApplication::instance())->devicePixelRatio();

    // define text to place
    QString titleText       = CLIENT_NAME;
    QString versionText     = QString::fromStdString(FormatFullVersion());
    QString taglineText     = QString("Blockchain built for animal welfare");
    QString copyrightText   = QString::fromUtf8(CopyrightHolders(strprintf("\xc2\xA9 %u-%u ", 2009, COPYRIGHT_YEAR)).c_str());
    const QString& titleAddText = networkStyle->getTitleAddText();

    QString font            = QApplication::font().toString();

    // create a bitmap according to device pixelratio
    int w = 480;
    int h = 400;
    QSize splashSize(w*devicePixelRatio, h*devicePixelRatio);
    pixmap = QPixmap(splashSize);
    pixmap.setDevicePixelRatio(devicePixelRatio);

    QPainter pixPaint(&pixmap);
    pixPaint.setRenderHint(QPainter::Antialiasing, true);
    pixPaint.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // --- Background: warm white-to-cream gradient ---
    QLinearGradient bgGrad(0, 0, 0, h);
    bgGrad.setColorAt(0.0, QColor(255, 255, 255));
    bgGrad.setColorAt(1.0, QColor(252, 249, 245));
    pixPaint.fillRect(QRect(0, 0, w, h), bgGrad);

    // --- Accent bar at the very top ---
    QLinearGradient accentGrad(0, 0, w, 0);
    accentGrad.setColorAt(0.0, QColor(194, 154, 76));  // Meowcoin gold
    accentGrad.setColorAt(1.0, QColor(168, 128, 56));
    pixPaint.fillRect(QRect(0, 0, w, 4), accentGrad);

    // --- Icon: centered horizontally, near top ---
    int iconSize = 140;
    int iconX = (w - iconSize) / 2;
    int iconY = 20;
    QRect rectIcon(iconX, iconY, iconSize, iconSize);
    const QSize requiredSize(1024, 1024);
    QPixmap icon(networkStyle->getAppIcon().pixmap(requiredSize));
    pixPaint.drawPixmap(rectIcon, icon);

    // --- Title: centered below icon ---
    int textY = iconY + iconSize + 28;
    QFont titleFont(font, 22);
    titleFont.setWeight(QFont::DemiBold);
    pixPaint.setFont(titleFont);
    pixPaint.setPen(QColor(50, 50, 50));
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth = GUIUtil::TextWidth(fm, titleText);
    pixPaint.drawText((w - titleTextWidth) / 2, textY, titleText);

    // --- Version: right below title ---
    textY += 6;
    QFont versionFont(font, 10);
    pixPaint.setFont(versionFont);
    pixPaint.setPen(QColor(140, 140, 140));
    fm = pixPaint.fontMetrics();
    int versionTextWidth = GUIUtil::TextWidth(fm, versionText);
    pixPaint.drawText((w - versionTextWidth) / 2, textY + fm.height(), versionText);

    // --- Tagline: animal welfare mission ---
    textY += fm.height() + 14;
    QFont tagFont(font, 10);
    tagFont.setItalic(true);
    pixPaint.setFont(tagFont);
    pixPaint.setPen(QColor(140, 116, 62));  // Warm gold-brown
    fm = pixPaint.fontMetrics();
    int tagWidth = GUIUtil::TextWidth(fm, taglineText);
    pixPaint.drawText((w - tagWidth) / 2, textY + fm.height(), taglineText);

    // --- Thin separator line ---
    textY += fm.height() + 14;
    pixPaint.setPen(QPen(QColor(220, 215, 205), 1));
    pixPaint.drawLine(w / 4, textY, w - w / 4, textY);

    // --- Copyright: small, centered, near bottom ---
    {
        QFont copyrightFont(font, 7);
        pixPaint.setFont(copyrightFont);
        pixPaint.setPen(QColor(170, 170, 170));
        int copyrightY = h - 90;
        QRect copyrightRect(20, copyrightY, w - 40, 40);
        pixPaint.drawText(copyrightRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, copyrightText);
    }

    // --- Network label (testnet/signet) in top-right corner ---
    if(!titleAddText.isEmpty()) {
        QFont netFont(font, 9);
        netFont.setWeight(QFont::Bold);
        pixPaint.setFont(netFont);
        pixPaint.setPen(QColor(200, 100, 50));
        fm = pixPaint.fontMetrics();
        int netTextWidth = GUIUtil::TextWidth(fm, titleAddText);
        pixPaint.drawText(w - netTextWidth - 12, 18, titleAddText);
    }

    pixPaint.end();

    // Set window title
    setWindowTitle(titleText + " " + titleAddText);

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(pixmap.size().width()/devicePixelRatio,pixmap.size().height()/devicePixelRatio));
    resize(r.size());
    setFixedSize(r.size());
    move(QGuiApplication::primaryScreen()->geometry().center() - r.center());

    installEventFilter(this);

    GUIUtil::handleCloseWindowShortcut(this);
}

SplashScreen::~SplashScreen()
{
    if (m_node) unsubscribeFromCoreSignals();
}

void SplashScreen::setNode(interfaces::Node& node)
{
    assert(!m_node);
    m_node = &node;
    subscribeToCoreSignals();
    if (m_shutdown) m_node->startShutdown();
}

void SplashScreen::shutdown()
{
    m_shutdown = true;
    if (m_node) m_node->startShutdown();
}

bool SplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if (keyEvent->key() == Qt::Key_Q) {
            shutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    bool invoked = QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(55,55,55)));
    assert(invoked);
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    InitMessage(splash, title + std::string("\n") +
            (resume_possible ? SplashScreen::tr("(press q to shutdown and continue later)").toStdString()
                                : SplashScreen::tr("press q to shutdown").toStdString()) +
            strprintf("\n%d", nProgress) + "%");
}

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_init_message = m_node->handleInitMessage(std::bind(InitMessage, this, std::placeholders::_1));
    m_handler_show_progress = m_node->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    m_handler_init_wallet = m_node->handleInitWallet([this]() { handleLoadWallet(); });
}

void SplashScreen::handleLoadWallet()
{
#ifdef ENABLE_WALLET
    if (!WalletModel::isWalletEnabled()) return;
    m_handler_load_wallet = m_node->walletLoader().handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) {
        m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, false)));
        m_connected_wallets.emplace_back(std::move(wallet));
    });
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
    for (const auto& handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    QRect r = rect().adjusted(5, 5, -5, -5);
    painter.setPen(curColor);
    painter.drawText(r, curAlignment, curMessage);
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    shutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
