#pragma once

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QFile>

namespace opendriver::installer {

class InstallerWindow : public QMainWindow {
    Q_OBJECT

public:
    InstallerWindow(QWidget* parent = nullptr);
    ~InstallerWindow();

private slots:
    void onInstallClicked();
    void checkForUpdates();
    void onCheckUpdatesFinished(QNetworkReply* reply);
    void fetchReleaseInfo();
    void onReleaseInfoFinished(QNetworkReply* reply);
    void downloadRelease(const QString& url);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(QNetworkReply* reply);
    void extractAndInstall();
    
private:
    void setupUI();
    void logMessage(const QString& msg, bool error = false);
    QString getInstalledVersion(bool stable);
    void saveInstalledVersion(bool stable, const QString& version);
    
    QComboBox* m_versionCombo;
    QPushButton* m_installBtn;
    QLabel* m_versionStatusLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_logView;
    
    QNetworkAccessManager* m_networkManager;
    QNetworkAccessManager* m_updateCheckManager;
    QFile* m_downloadFile;
    QString m_downloadPath;
    
    // Config
    QString m_installDir;
    QString m_latestFetchedVersion;
};

} // namespace opendriver::installer
