#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QTimer>
#include <memory>
#include "github_client.hpp"

class Installer : public QObject {
    Q_OBJECT

public:
    explicit Installer(QObject *parent = nullptr);
    ~Installer() override;

    /**
     * @brief Startet den asynchronen Download und die Installation eines Plugins
     * @param plugin Metadaten des Plugins
     * @param downloadUrl Direkte Download-URL des ZIP-Assets
     */
    void startInstallation(const PluginInfo &plugin, const QString &downloadUrl);

    /**
     * @brief Bricht eine laufende Installation ab
     */
    void cancel();

    /**
     * @brief Gibt das Basisverzeichnis für Benutzer-Plugins zurück
     * Windows: %APPDATA%/obs-studio/plugins
     * Linux: ~/.config/obs-studio/plugins
     * macOS: ~/Library/Application Support/obs-studio/plugins
     */
    static QString getPluginsBaseDir();

    /**
     * @brief Gibt den Zielpfad für ein bestimmtes Plugin zurück
     */
    static QString getPluginInstallDir(const QString &pluginId);

    /**
     * @brief Prüft, ob ein bestimmtes Plugin bereits lokal installiert ist
     */
    static bool isPluginInstalled(const QString &pluginId);

    /**
     * @brief Deinstalliert ein vorhandenes Plugin (löscht dessen Ordner)
     */
    static bool uninstallPlugin(const QString &pluginId);

signals:
    void statusUpdated(const QString &message);
    void progressUpdated(int percent);
    void installationSuccess(const PluginInfo &plugin, const QString &installedPath);
    void installationFailed(const PluginInfo &plugin, const QString &errorMessage);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void startCurlDownload(const QString &downloadUrl);
    bool extractZipArchive(const QString &zipFilePath, const QString &targetDirectory, QString &outError);

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply = nullptr;
    QProcess *m_curlProcess = nullptr;
    QTimer *m_progressTimer = nullptr;
    QString m_tempFilePath;
    PluginInfo m_currentPlugin;
    bool m_isCancelled = false;
};
