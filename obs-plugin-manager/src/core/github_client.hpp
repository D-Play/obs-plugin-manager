#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>

struct ReleaseAsset {
    QString name;
    QString downloadUrl;
    qint64 size = 0;
    QString contentType;
};

struct ReleaseInfo {
    QString tagName;
    QString name;
    QString body;
    QDateTime publishedAt;
    QVector<ReleaseAsset> assets;
};

struct PluginInfo {
    QString id;
    QString name;
    QString author;
    QString description;
    QString repo;        // Format: "owner/repo"
    QString homepage;
    int stars = 0;
    bool isCurated = false;
    QString latestVersion;
    QString downloadUrl;
    QString assetName;
    qint64 assetSize = 0;
    bool isInstalled = false;
};

class GitHubClient : public QObject {
    Q_OBJECT

public:
    explicit GitHubClient(QObject *parent = nullptr);
    ~GitHubClient() override = default;

    /**
     * @brief Sucht Repositories auf GitHub
     * @param query Suchbegriff des Benutzers
     */
    void searchPlugins(const QString &query);

    /**
     * @brief Ruft das neueste Release für ein bestimmtes GitHub-Repository ab
     * @param repoFullName z. B. "exeldro/obs-move-transition"
     */
    void fetchLatestRelease(const QString &repoFullName);

    /**
     * @brief Lädt die kuratierte Plugin-Liste aus der lokalen JSON-Ressource/Datei
     * @return Liste der kuratierten Plugins
     */
    QVector<PluginInfo> loadCuratedPlugins();

    /**
     * @brief Erkennt die aktuelle Betriebssystem-Plattform ("windows", "linux", "macos")
     */
    static QString currentPlatformName();

    /**
     * @brief Wählt das am besten passende ZIP-Asset für das aktuelle OS aus
     */
    static QString findBestAssetUrl(const QVector<ReleaseAsset> &assets,
                                    QString *outAssetName = nullptr,
                                    qint64 *outAssetSize = nullptr);

signals:
    void searchFinished(const QVector<PluginInfo> &results);
    void searchFailed(const QString &errorMessage);

    void releaseInfoFinished(const QString &repo, const ReleaseInfo &release,
                             const QString &compatibleAssetUrl, const QString &compatibleAssetName,
                             qint64 assetSize);
    void releaseInfoFailed(const QString &repo, const QString &errorMessage);

private slots:
    void onSearchReplyFinished(QNetworkReply *reply);

private:
    void processSearchJson(const QByteArray &data);
    void processReleaseJson(const QString &repoFullName, const QByteArray &data);
    void fetchUrlViaCurl(const QUrl &url, std::function<void(bool success, int statusCode, const QByteArray &data, const QString &error)> callback);

    QNetworkRequest createApiRequest(const QUrl &url) const;
    static int scoreAssetForPlatform(const QString &assetName, const QString &platform);

    QNetworkAccessManager *m_networkManager;
};
