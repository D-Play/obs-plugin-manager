#include "github_client.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QSysInfo>
#include <QDebug>
#include <QSslSocket>
#include <QProcess>
#include <QStandardPaths>

GitHubClient::GitHubClient(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this))
{
}

QNetworkRequest GitHubClient::createApiRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("obs-plugin-manager/1.0 (OBS Studio Plugin)"));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    // Enable redirects
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString GitHubClient::currentPlatformName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QStringLiteral("unknown");
#endif
}

QVector<PluginInfo> GitHubClient::loadCuratedPlugins()
{
    QVector<PluginInfo> result;

    QStringList candidates = {
        QStringLiteral(":/src/data/curated.json"),
        QStringLiteral(":/data/curated.json"),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/curated.json")),
        QDir::current().filePath(QStringLiteral("src/data/curated.json")),
        QStringLiteral("src/data/curated.json")
    };

    QByteArray jsonData;
    bool found = false;

    for (const QString &path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            jsonData = file.readAll();
            file.close();
            found = true;
            break;
        }
    }

    if (!found) {
        qWarning() << "curated.json could not be located in standard paths.";
        return result;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Error parsing curated.json:" << parseError.errorString();
        return result;
    }

    QJsonObject root = doc.object();
    QJsonArray pluginsArray = root.value(QStringLiteral("plugins")).toArray();

    for (const QJsonValue &val : pluginsArray) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        PluginInfo info;
        info.id = obj.value(QStringLiteral("id")).toString();
        info.name = obj.value(QStringLiteral("name")).toString();
        info.author = obj.value(QStringLiteral("author")).toString();
        info.description = obj.value(QStringLiteral("description")).toString();
        info.repo = obj.value(QStringLiteral("repo")).toString();
        info.homepage = obj.value(QStringLiteral("homepage")).toString();
        info.isCurated = true;

        if (!info.name.isEmpty()) {
            result.append(info);
        }
    }

    return result;
}

void GitHubClient::fetchUrlViaCurl(const QUrl &url, std::function<void(bool success, int statusCode, const QByteArray &data, const QString &error)> callback)
{
    QString curlPath = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (curlPath.isEmpty()) {
#if defined(Q_OS_WIN)
        curlPath = QStringLiteral("C:\\Windows\\System32\\curl.exe");
#else
        curlPath = QStringLiteral("/usr/bin/curl");
#endif
    }

    QProcess *proc = new QProcess(this);
    QStringList args;
    args << QStringLiteral("-s")
         << QStringLiteral("-L")
         << QStringLiteral("--max-time") << QStringLiteral("15")
         << QStringLiteral("-H") << QStringLiteral("User-Agent: obs-plugin-manager/1.0 (OBS Studio Plugin)")
         << QStringLiteral("-H") << QStringLiteral("Accept: application/vnd.github.v3+json")
         << QStringLiteral("-w") << QStringLiteral("\n__HTTP_STATUS__:%{http_code}")
         << QString::fromUtf8(url.toEncoded());

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [proc, callback](int exitCode, QProcess::ExitStatus exitStatus) {
            proc->deleteLater();
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                callback(false, 0, QByteArray(), QStringLiteral("Netzwerkfehler (curl Exit %1)").arg(exitCode));
                return;
            }

            QByteArray output = proc->readAllStandardOutput();
            int sep = output.lastIndexOf("\n__HTTP_STATUS__:");
            int code = 200;
            QByteArray body = output;
            if (sep != -1) {
                code = output.mid(sep + 17).trimmed().toInt();
                body = output.left(sep);
            }

            if (code >= 200 && code < 300) {
                callback(true, code, body, QString());
            } else if (code == 403) {
                callback(false, code, body, QStringLiteral("GitHub API Rate-Limit erreicht. Bitte warte einige Minuten oder versuche es später erneut."));
            } else if (code == 404) {
                callback(false, code, body, QStringLiteral("Nicht gefunden (HTTP 404)."));
            } else {
                callback(false, code, body, QStringLiteral("Server-Fehler (HTTP %1)").arg(code));
            }
        });

    connect(proc, &QProcess::errorOccurred, this, [proc, callback](QProcess::ProcessError /*err*/) {
        proc->deleteLater();
        callback(false, 0, QByteArray(), QStringLiteral("Konnte curl nicht ausführen: %1").arg(proc->errorString()));
    });

    proc->start(curlPath, args);
}

void GitHubClient::processSearchJson(const QByteArray &data)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit searchFailed(QStringLiteral("Ungültige Antwort von GitHub: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray items = root.value(QStringLiteral("items")).toArray();

    QVector<PluginInfo> results;
    for (const QJsonValue &itemVal : items) {
        if (!itemVal.isObject()) continue;
        QJsonObject repoObj = itemVal.toObject();

        PluginInfo info;
        info.repo = repoObj.value(QStringLiteral("full_name")).toString();
        info.name = repoObj.value(QStringLiteral("name")).toString();
        info.description = repoObj.value(QStringLiteral("description")).toString();
        info.homepage = repoObj.value(QStringLiteral("html_url")).toString();
        info.stars = repoObj.value(QStringLiteral("stargazers_count")).toInt();
        info.id = repoObj.value(QStringLiteral("name")).toString().toLower();

        QJsonObject ownerObj = repoObj.value(QStringLiteral("owner")).toObject();
        info.author = ownerObj.value(QStringLiteral("login")).toString();

        info.isCurated = false;
        results.append(info);
    }

    emit searchFinished(results);
}

void GitHubClient::processReleaseJson(const QString &repoFullName, const QByteArray &data)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit releaseInfoFailed(repoFullName, QStringLiteral("Ungültiges JSON-Format im Release."));
        return;
    }

    QJsonObject root = doc.object();
    ReleaseInfo release;
    release.tagName = root.value(QStringLiteral("tag_name")).toString();
    release.name = root.value(QStringLiteral("name")).toString();
    release.body = root.value(QStringLiteral("body")).toString();
    QString publishedStr = root.value(QStringLiteral("published_at")).toString();
    release.publishedAt = QDateTime::fromString(publishedStr, Qt::ISODate);

    QJsonArray assetsArr = root.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &assetVal : assetsArr) {
        if (!assetVal.isObject()) continue;
        QJsonObject assetObj = assetVal.toObject();

        ReleaseAsset asset;
        asset.name = assetObj.value(QStringLiteral("name")).toString();
        asset.downloadUrl = assetObj.value(QStringLiteral("browser_download_url")).toString();
        asset.size = static_cast<qint64>(assetObj.value(QStringLiteral("size")).toDouble());
        asset.contentType = assetObj.value(QStringLiteral("content_type")).toString();
        release.assets.append(asset);
    }

    QString bestAssetName;
    qint64 bestAssetSize = 0;
    QString bestUrl = findBestAssetUrl(release.assets, &bestAssetName, &bestAssetSize);

    if (bestUrl.isEmpty()) {
        emit releaseInfoFailed(repoFullName,
            QStringLiteral("Kein passendes ZIP-Paket für %1 gefunden.")
                .arg(currentPlatformName().toUpper()));
        return;
    }

    emit releaseInfoFinished(repoFullName, release, bestUrl, bestAssetName, bestAssetSize);
}

void GitHubClient::searchPlugins(const QString &query)
{
    QUrl url(QStringLiteral("https://api.github.com/search/repositories"));
    QUrlQuery urlQuery;

    QString trimmed = query.trimmed();
    QString searchQuery;
    if (trimmed.isEmpty()) {
        searchQuery = QStringLiteral("topic:obs-studio-plugin");
    } else {
        if (trimmed.contains(QStringLiteral("obs"), Qt::CaseInsensitive)) {
            searchQuery = QStringLiteral("%1 fork:false").arg(trimmed);
        } else {
            searchQuery = QStringLiteral("%1 obs fork:false").arg(trimmed);
        }
    }

    urlQuery.addQueryItem(QStringLiteral("q"), searchQuery);
    urlQuery.addQueryItem(QStringLiteral("sort"), QStringLiteral("stars"));
    urlQuery.addQueryItem(QStringLiteral("order"), QStringLiteral("desc"));
    urlQuery.addQueryItem(QStringLiteral("per_page"), QStringLiteral("30"));
    url.setQuery(urlQuery);

    if (!QSslSocket::supportsSsl()) {
        fetchUrlViaCurl(url, [this](bool ok, int /*code*/, const QByteArray &data, const QString &err) {
            if (!ok) {
                emit searchFailed(err);
                return;
            }
            processSearchJson(data);
        });
        return;
    }

    QNetworkRequest request = createApiRequest(url);
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        onSearchReplyFinished(reply);
    });
}

void GitHubClient::onSearchReplyFinished(QNetworkReply *reply)
{
    QUrl requestUrl = reply->url();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() == QNetworkReply::SslHandshakeFailedError ||
            reply->errorString().contains(QStringLiteral("TLS"), Qt::CaseInsensitive) ||
            reply->errorString().contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
            fetchUrlViaCurl(requestUrl, [this](bool ok, int /*code*/, const QByteArray &data, const QString &err) {
                if (!ok) {
                    emit searchFailed(err);
                    return;
                }
                processSearchJson(data);
            });
            return;
        }

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString errorMsg = reply->errorString();
        if (statusCode == 403) {
            errorMsg = QStringLiteral("GitHub API Rate-Limit erreicht. Bitte warte einige Minuten oder versuche es später erneut.");
        }
        emit searchFailed(errorMsg);
        return;
    }

    QByteArray data = reply->readAll();
    processSearchJson(data);
}

void GitHubClient::fetchLatestRelease(const QString &repoFullName)
{
    if (repoFullName.isEmpty()) {
        emit releaseInfoFailed(repoFullName, QStringLiteral("Ungültiger Repository-Name."));
        return;
    }

    QUrl url(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repoFullName));

    if (!QSslSocket::supportsSsl()) {
        fetchUrlViaCurl(url, [this, repoFullName](bool ok, int /*code*/, const QByteArray &data, const QString &err) {
            if (!ok) {
                emit releaseInfoFailed(repoFullName, err);
                return;
            }
            processReleaseJson(repoFullName, data);
        });
        return;
    }

    QNetworkRequest request = createApiRequest(url);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, repoFullName, url]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::SslHandshakeFailedError ||
                reply->errorString().contains(QStringLiteral("TLS"), Qt::CaseInsensitive) ||
                reply->errorString().contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
                fetchUrlViaCurl(url, [this, repoFullName](bool ok, int /*code*/, const QByteArray &data, const QString &err) {
                    if (!ok) {
                        emit releaseInfoFailed(repoFullName, err);
                        return;
                    }
                    processReleaseJson(repoFullName, data);
                });
                return;
            }

            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString errorMsg = reply->errorString();
            if (statusCode == 404) {
                errorMsg = QStringLiteral("Kein offizielles Release für dieses Repository gefunden.");
            } else if (statusCode == 403) {
                errorMsg = QStringLiteral("GitHub API Rate-Limit erreicht.");
            }
            emit releaseInfoFailed(repoFullName, errorMsg);
            return;
        }

        QByteArray data = reply->readAll();
        processReleaseJson(repoFullName, data);
    });
}

int GitHubClient::scoreAssetForPlatform(const QString &assetName, const QString &platform)
{
    QString lower = assetName.toLower();

    // Must be a zip file
    if (!lower.endsWith(QStringLiteral(".zip"))) {
        return -1000;
    }

    // Penalize source bundles or checksums
    if (lower.contains(QStringLiteral("source")) ||
        lower.contains(QStringLiteral("symbols")) ||
        lower.contains(QStringLiteral("pdb")) ||
        lower.contains(QStringLiteral("debug"))) {
        return -500;
    }

    int score = 50; // base score for being a zip

    if (platform == QStringLiteral("windows")) {
        if (lower.contains(QStringLiteral("linux")) || lower.contains(QStringLiteral("ubuntu")) ||
            lower.contains(QStringLiteral("macos")) || lower.contains(QStringLiteral("darwin")) ||
            lower.contains(QStringLiteral("osx"))) {
            return -200;
        }
        if (lower.contains(QStringLiteral("programdata"))) score += 60;
        if (lower.contains(QStringLiteral("win64")) || lower.contains(QStringLiteral("windows-x64")) || lower.contains(QStringLiteral("win-x64"))) score += 50;
        else if (lower.contains(QStringLiteral("windows")) || lower.contains(QStringLiteral("win"))) score += 30;
        if (lower.contains(QStringLiteral("x64")) || lower.contains(QStringLiteral("x86_64"))) score += 20;
    } else if (platform == QStringLiteral("linux")) {
        if (lower.contains(QStringLiteral("win")) || lower.contains(QStringLiteral("windows")) ||
            lower.contains(QStringLiteral("macos")) || lower.contains(QStringLiteral("darwin")) ||
            lower.contains(QStringLiteral("osx"))) {
            return -200;
        }
        if (lower.contains(QStringLiteral("linux"))) score += 50;
        if (lower.contains(QStringLiteral("ubuntu"))) score += 30;
        if (lower.contains(QStringLiteral("x86_64")) || lower.contains(QStringLiteral("x64"))) score += 20;
    } else if (platform == QStringLiteral("macos")) {
        if (lower.contains(QStringLiteral("win")) || lower.contains(QStringLiteral("windows")) ||
            lower.contains(QStringLiteral("linux")) || lower.contains(QStringLiteral("ubuntu"))) {
            return -200;
        }
        if (lower.contains(QStringLiteral("macos")) || lower.contains(QStringLiteral("mac")) ||
            lower.contains(QStringLiteral("osx")) || lower.contains(QStringLiteral("darwin"))) score += 50;
        if (lower.contains(QStringLiteral("universal"))) score += 30;
        if (lower.contains(QStringLiteral("arm64")) || lower.contains(QStringLiteral("x86_64"))) score += 20;
    }

    return score;
}

QString GitHubClient::findBestAssetUrl(const QVector<ReleaseAsset> &assets, QString *outAssetName, qint64 *outAssetSize)
{
    QString platform = currentPlatformName();
    int bestScore = -100;
    QString bestUrl;
    QString bestName;
    qint64 bestSize = 0;

    for (const ReleaseAsset &asset : assets) {
        int score = scoreAssetForPlatform(asset.name, platform);
        if (score > bestScore) {
            bestScore = score;
            bestUrl = asset.downloadUrl;
            bestName = asset.name;
            bestSize = asset.size;
        }
    }

    // Fallback: If only one ZIP asset exists and score is > 0
    if (bestUrl.isEmpty()) {
        for (const ReleaseAsset &asset : assets) {
            if (asset.name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                bestUrl = asset.downloadUrl;
                bestName = asset.name;
                bestSize = asset.size;
                break;
            }
        }
    }

    if (outAssetName && !bestName.isEmpty()) {
        *outAssetName = bestName;
    }
    if (outAssetSize) {
        *outAssetSize = bestSize;
    }

    return bestUrl;
}
