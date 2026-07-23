#include "installer_window.h"
#include "../ui/theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QIcon>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QMessageBox>
#include <QApplication>

namespace opendriver::installer {

InstallerWindow::InstallerWindow(QWidget* parent) 
    : QMainWindow(parent), m_downloadFile(nullptr) {
    
    m_networkManager = new QNetworkAccessManager(this);
    m_updateCheckManager = new QNetworkAccessManager(this);
    
    // Default install dir
    QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (localAppData.isEmpty()) {
        localAppData = QDir::homePath() + "/AppData/Local";
    }
    m_installDir = QDir::cleanPath(localAppData + "/OpenDriverVR");
    
    setupUI();
}

InstallerWindow::~InstallerWindow() {
    if (m_downloadFile) {
        if (m_downloadFile->isOpen()) {
            m_downloadFile->close();
        }
        delete m_downloadFile;
    }
}

void InstallerWindow::setupUI() {
    setWindowTitle("OpenDriver VR Installer");
    setWindowIcon(QIcon(":/icons/icon.png"));
    resize(600, 450);
    setMinimumSize(500, 400);

    setStyleSheet(opendriver::ui::GetDefaultStylesheet());

    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // ── HEADER ──
    auto* headerBar = new QHBoxLayout();
    auto* logoLabel = new QLabel("◈", this);
    logoLabel->setStyleSheet("font-size: 32px; color: #6c63ff; margin-right: 8px;");
    auto* titleLabel = new QLabel("OpenDriver VR", this);
    titleLabel->setStyleSheet("font-size: 26px; font-weight: 800; color: #e0e4f0;");
    headerBar->addWidget(logoLabel);
    headerBar->addWidget(titleLabel);
    headerBar->addStretch();
    mainLayout->addLayout(headerBar);

    auto* subtitle = new QLabel("Native SteamVR Driver for Custom Headsets", this);
    subtitle->setStyleSheet("font-size: 14px; color: #8b8fa3; margin-bottom: 10px;");
    mainLayout->addWidget(subtitle);

    // ── SETTINGS ──
    auto* group = new QGroupBox("Installation Settings", this);
    auto* groupLayout = new QVBoxLayout(group);
    
    auto* versionLayout = new QHBoxLayout();
    auto* versionLabel = new QLabel("Version Branch:");
    versionLabel->setStyleSheet("font-weight: bold;");
    m_versionCombo = new QComboBox();
    m_versionCombo->addItem("Stable (Recommended)");
    m_versionCombo->addItem("Nightly (Experimental)");
    m_versionCombo->setMinimumWidth(200);
    
    versionLayout->addWidget(versionLabel);
    versionLayout->addWidget(m_versionCombo);
    
    m_versionStatusLabel = new QLabel("Checking for updates...");
    m_versionStatusLabel->setStyleSheet("color: #8b8fa3; font-style: italic;");
    versionLayout->addWidget(m_versionStatusLabel);
    
    versionLayout->addStretch();
    groupLayout->addLayout(versionLayout);
    
    connect(m_versionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InstallerWindow::checkForUpdates);
    
    auto* pathLayout = new QHBoxLayout();
    auto* pathLabel = new QLabel("Install Path:");
    pathLabel->setStyleSheet("font-weight: bold;");
    auto* pathValue = new QLabel(m_installDir);
    pathValue->setStyleSheet("color: #b0b4c8; font-family: monospace;");
    
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(pathValue);
    pathLayout->addStretch();
    groupLayout->addLayout(pathLayout);
    
    mainLayout->addWidget(group);

    // ── PROGRESS ──
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    mainLayout->addWidget(m_progressBar);

    // ── LOGS ──
    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    mainLayout->addWidget(m_logView);

    // ── BUTTONS ──
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    m_installBtn = new QPushButton("Install / Update");
    m_installBtn->setMinimumWidth(150);
    m_installBtn->setMinimumHeight(40);
    connect(m_installBtn, &QPushButton::clicked, this, &InstallerWindow::onInstallClicked);
    
    btnLayout->addWidget(m_installBtn);
    mainLayout->addLayout(btnLayout);

    setCentralWidget(central);
    
    logMessage("Ready to install. Select a branch and click Install.");
    checkForUpdates();
}

QString InstallerWindow::getInstalledVersion(bool stable) {
    QString fileName = stable ? "version_stable.txt" : "version_nightly.txt";
    QFile file(m_installDir + "/" + fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString(file.readAll()).trimmed();
    }
    return "None";
}

void InstallerWindow::saveInstalledVersion(bool stable, const QString& version) {
    QString fileName = stable ? "version_stable.txt" : "version_nightly.txt";
    QFile file(m_installDir + "/" + fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(version.toUtf8());
    }
}

void InstallerWindow::checkForUpdates() {
    m_versionStatusLabel->setText("Checking for updates...");
    m_installBtn->setText("Install / Update");
    
    QString apiUrl;
    if (m_versionCombo->currentIndex() == 0) {
        apiUrl = "https://api.github.com/repos/Rozgaleziacz/OpenDriver-VR/releases/latest";
    } else {
        apiUrl = "https://api.github.com/repos/Dimentt/OpenDriver-VR/releases/tags/Nightly";
    }
    
    QNetworkRequest request((QUrl(apiUrl)));
    request.setRawHeader("User-Agent", "OpenDriver-Installer/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    auto* reply = m_updateCheckManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onCheckUpdatesFinished(reply);
    });
}

void InstallerWindow::onCheckUpdatesFinished(QNetworkReply* reply) {
    reply->deleteLater();
    
    bool stable = (m_versionCombo->currentIndex() == 0);
    QString installed = getInstalledVersion(stable);
    
    if (reply->error() != QNetworkReply::NoError) {
        m_versionStatusLabel->setText(QString("Installed: %1 | Error checking latest").arg(installed));
        m_latestFetchedVersion = "unknown";
        return;
    }
    
    QJsonDocument jsonResponse = QJsonDocument::fromJson(reply->readAll());
    if (jsonResponse.isObject() && jsonResponse.object().contains("tag_name")) {
        QString latest = jsonResponse.object()["tag_name"].toString();
        m_latestFetchedVersion = latest;
        
        if (installed == latest && installed != "None") {
            m_versionStatusLabel->setText(QString("Installed: %1 | Up to date!").arg(installed));
            m_installBtn->setText("Reinstall");
        } else {
            m_versionStatusLabel->setText(QString("Installed: %1 | Latest: %2").arg(installed, latest));
            m_installBtn->setText("Update Now");
        }
    } else {
        m_versionStatusLabel->setText(QString("Installed: %1 | Unknown latest").arg(installed));
        m_latestFetchedVersion = "unknown";
    }
}

void InstallerWindow::logMessage(const QString& msg, bool error) {
    QString color = error ? "#f87171" : "#c8ccd8";
    m_logView->append(QString("<span style='color: %1'>%2</span>").arg(color, msg.toHtmlEscaped()));
    // Auto-scroll
    QTextCursor cursor = m_logView->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logView->setTextCursor(cursor);
}

void InstallerWindow::onInstallClicked() {
    m_installBtn->setEnabled(false);
    m_versionCombo->setEnabled(false);
    m_progressBar->setValue(0);
    m_logView->clear();
    
    fetchReleaseInfo();
}

void InstallerWindow::fetchReleaseInfo() {
    if (m_versionCombo->currentIndex() == 0) {
        // Stable
        QString apiUrl = "https://api.github.com/repos/Rozgaleziacz/OpenDriver-VR/releases/latest";
        logMessage("Fetching latest Stable release info...");
        
        QNetworkRequest request((QUrl(apiUrl)));
        request.setRawHeader("User-Agent", "OpenDriver-Installer/1.0");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        
        auto* reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            onReleaseInfoFinished(reply);
        });
    } else {
        // Nightly - bypass API check for download as requested by user
        logMessage("Downloading latest Nightly directly...");
        QString downloadUrl = "https://github.com/Dimentt/OpenDriver-VR/releases/download/Nightly/opendriver-nightly.zip";
        downloadRelease(downloadUrl);
    }
}

void InstallerWindow::onReleaseInfoFinished(QNetworkReply* reply) {
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        logMessage(QString("Network error: %1").arg(reply->errorString()), true);
        m_installBtn->setEnabled(true);
        m_versionCombo->setEnabled(true);
        return;
    }
    
    QJsonDocument jsonResponse = QJsonDocument::fromJson(reply->readAll());
    if (jsonResponse.isNull() || !jsonResponse.isObject()) {
        logMessage("Invalid JSON response from GitHub API", true);
        m_installBtn->setEnabled(true);
        m_versionCombo->setEnabled(true);
        return;
    }
    
    QJsonObject jsonObj = jsonResponse.object();
    if (!jsonObj.contains("assets") || !jsonObj["assets"].isArray()) {
        logMessage("No assets found in the release.", true);
        m_installBtn->setEnabled(true);
        m_versionCombo->setEnabled(true);
        return;
    }
    
    QJsonArray assets = jsonObj["assets"].toArray();
    QString downloadUrl;
    
    for (const QJsonValue& val : assets) {
        QJsonObject asset = val.toObject();
        QString name = asset["name"].toString();
        
        if (name.startsWith("opendriver-stable") && name.endsWith(".zip")) {
            downloadUrl = asset["browser_download_url"].toString();
            break;
        }
    }
    
    if (downloadUrl.isEmpty()) {
        logMessage("Could not find a suitable .zip artifact in the release.", true);
        m_installBtn->setEnabled(true);
        m_versionCombo->setEnabled(true);
        return;
    }
    
    logMessage(QString("Found asset. URL: %1").arg(downloadUrl));
    downloadRelease(downloadUrl);
}

void InstallerWindow::downloadRelease(const QString& url) {
    m_downloadPath = QDir::tempPath() + "/opendriver_download.zip";
    
    m_downloadFile = new QFile(m_downloadPath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        logMessage("Failed to create temporary file for download.", true);
        m_installBtn->setEnabled(true);
        m_versionCombo->setEnabled(true);
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }
    
    logMessage(QString("Downloading to %1...").arg(m_downloadPath));
    
    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent", "OpenDriver-Installer/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    auto* reply = m_networkManager->get(request);
    
    connect(reply, &QNetworkReply::downloadProgress, this, &InstallerWindow::onDownloadProgress);
    
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (m_downloadFile) {
            m_downloadFile->write(reply->readAll());
        }
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDownloadFinished(reply);
    });
}

void InstallerWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressBar->setValue(percent);
    }
}

void InstallerWindow::onDownloadFinished(QNetworkReply* reply) {
    reply->deleteLater();
    
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        logMessage(QString("Download failed: %1").arg(reply->errorString()), true);
        m_installBtn->setEnabled(true);
        m_versionCombo->setEnabled(true);
        return;
    }
    
    logMessage("Download complete.");
    extractAndInstall();
}

void InstallerWindow::extractAndInstall() {
    logMessage(QString("Extracting archive to %1...").arg(m_installDir));
    
    QDir dir(m_installDir);
    if (dir.exists()) {
        dir.removeRecursively(); // Clean previous installation
    }
    dir.mkpath(".");
    
    // Use tar.exe which is built into Windows 10/11
    QProcess tarProc;
    QStringList args;
    args << "-xf" << m_downloadPath << "-C" << m_installDir;
    
    logMessage("Running tar.exe " + args.join(" "));
    tarProc.start("tar.exe", args);
    tarProc.waitForFinished(30000); // 30 seconds max
    
    if (tarProc.exitStatus() != QProcess::NormalExit || tarProc.exitCode() != 0) {
        logMessage(QString("Extraction failed! Exit code: %1").arg(tarProc.exitCode()), true);
        logMessage(QString(tarProc.readAllStandardError()), true);
        m_installBtn->setEnabled(true);
        m_versionCombo->setEnabled(true);
        return;
    }
    
    logMessage("Extraction successful. Locating SteamVR...");
    
    // Register Driver
    QStringList steamVrPaths = {
        "C:/Program Files (x86)/Steam/steamapps/common/SteamVR",
        "C:/Program Files/Steam/steamapps/common/SteamVR"
    };
    
    // In Qt, qEnvironmentVariable("ProgramFiles(x86)") etc. could be used to make it robust, 
    // but the above usually covers 99% cases.
    
    QString vrPathReg;
    for (const QString& path : steamVrPaths) {
        QString candidate = path + "/bin/win64/vrpathreg.exe";
        if (QFile::exists(candidate)) {
            vrPathReg = candidate;
            break;
        }
    }
    
    if (vrPathReg.isEmpty()) {
        logMessage("Could not find SteamVR installation. Driver files were copied, but you must register it manually using vrpathreg.", true);
        m_installBtn->setEnabled(true);
        m_versionCombo->setEnabled(true);
        return;
    }
    
    // Zwykle repo uzywa struktury np "driver" w srodku zipa, wiec zalezy jak paczka jest pakowana.
    // Zarejestrujemy "m_installDir/driver" jesli istnieje, lub sam m_installDir
    QString driverPathToRegister = m_installDir;
    if (QDir(m_installDir + "/driver").exists()) {
        driverPathToRegister = m_installDir + "/driver";
    }
    
    logMessage(QString("Registering driver at: %1").arg(driverPathToRegister));
    
    // Remove old registration first (just in case)
    QProcess::execute(vrPathReg, QStringList() << "removedriver" << driverPathToRegister);
    
    // Add driver
    int ret = QProcess::execute(vrPathReg, QStringList() << "adddriver" << driverPathToRegister);
    
    if (ret == 0) {
        saveInstalledVersion(m_versionCombo->currentIndex() == 0, m_latestFetchedVersion);
        checkForUpdates(); // refresh status
        logMessage(QString("<b><font color='#6c63ff'>Installation completed successfully!</font></b>"));
        QMessageBox::information(this, "Success", "OpenDriver VR has been installed successfully!\nPlease restart SteamVR.");
    } else {
        logMessage(QString("vrpathreg failed with code %1").arg(ret), true);
    }
    
    m_installBtn->setEnabled(true);
    m_versionCombo->setEnabled(true);
}

} // namespace opendriver::installer
