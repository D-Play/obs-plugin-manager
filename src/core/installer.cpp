#include "installer.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QDebug>
#include <QSslSocket>
#include "miniz.h"

Installer::Installer(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this))
{
}

Installer::~Installer()
{
    cancel();
}

QString Installer::getPluginsBaseDir()
{
#if defined(Q_OS_WIN)
    QString appData = QString::fromLocal8Bit(qgetenv("APPDATA"));
    if (appData.isEmpty()) {
        appData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    }
    return QDir(appData).filePath(QStringLiteral("obs-studio/plugins"));
#elif defined(Q_OS_MACOS)
    QString home = QDir::homePath();
    return QDir(home).filePath(QStringLiteral("Library/Application Support/obs-studio/plugins"));
#else // Linux
    QString xdgConfig = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
    if (xdgConfig.isEmpty()) {
        xdgConfig = QDir(QDir::homePath()).filePath(QStringLiteral(".config"));
    }
    return QDir(xdgConfig).filePath(QStringLiteral("obs-studio/plugins"));
#endif
}

QString Installer::getPluginInstallDir(const QString &pluginId)
{
    QString sanitized = pluginId.trimmed().toLower();
    sanitized.remove(QRegularExpression(QStringLiteral("[^a-z0-9_-]")));
    if (sanitized.isEmpty()) {
        sanitized = QStringLiteral("unknown-plugin");
    }
    return QDir(getPluginsBaseDir()).filePath(sanitized);
}

bool Installer::isPluginInstalled(const QString &pluginId)
{
    QString sanitized = pluginId.trimmed().toLower();
    sanitized.remove(QRegularExpression(QStringLiteral("[^a-z0-9_-]")));
    if (sanitized.isEmpty()) {
        return false;
    }

    // Check APPDATA
    QString userPath = getPluginInstallDir(sanitized);
    QDir userDir(userPath);
    if (userDir.exists() && !userDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty()) {
        return true;
    }

#if defined(Q_OS_WIN)
    // Also check ProgramData
    QString programData = QString::fromLocal8Bit(qgetenv("ProgramData"));
    if (!programData.isEmpty()) {
        QString pdPath = QDir(programData).filePath(QStringLiteral("obs-studio/plugins/%1").arg(sanitized));
        QDir pdDir(pdPath);
        if (pdDir.exists() && !pdDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty()) {
            return true;
        }
    }
#endif

    return false;
}

bool Installer::uninstallPlugin(const QString &pluginId)
{
    QString sanitized = pluginId.trimmed().toLower();
    sanitized.remove(QRegularExpression(QStringLiteral("[^a-z0-9_-]")));
    if (sanitized.isEmpty()) {
        return false;
    }

    QString userPath = getPluginInstallDir(sanitized);
    QDir userDir(userPath);
    bool removed = false;
    if (userDir.exists()) {
        removed = userDir.removeRecursively();
    }

#if defined(Q_OS_WIN)
    QString programData = QString::fromLocal8Bit(qgetenv("ProgramData"));
    if (!programData.isEmpty()) {
        QString pdPath = QDir(programData).filePath(QStringLiteral("obs-studio/plugins/%1").arg(sanitized));
        QDir pdDir(pdPath);
        if (pdDir.exists()) {
            pdDir.removeRecursively();
        }
    }
#endif

    return removed || !isPluginInstalled(sanitized);
}

void Installer::startInstallation(const PluginInfo &plugin, const QString &downloadUrl)
{
    cancel();

    m_isCancelled = false;
    m_currentPlugin = plugin;

    if (downloadUrl.isEmpty()) {
        emit installationFailed(plugin, QStringLiteral("Keine Download-URL für das Plugin vorhanden."));
        return;
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_tempFilePath = QDir(tempDir).filePath(QStringLiteral("obs_plugin_%1.zip").arg(m_currentPlugin.id));
    if (QFile::exists(m_tempFilePath)) {
        QFile::remove(m_tempFilePath);
    }

    emit statusUpdated(QStringLiteral("Verbinde mit Server..."));
    emit progressUpdated(0);

    if (!QSslSocket::supportsSsl()) {
        startCurlDownload(downloadUrl);
        return;
    }

    QUrl url(downloadUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("obs-plugin-manager/1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &Installer::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_currentReply && !m_isCancelled) {
            QFile file(m_tempFilePath);
            if (file.open(QIODevice::Append)) {
                file.write(m_currentReply->readAll());
                file.close();
            }
        }
    });
    connect(m_currentReply, &QNetworkReply::finished, this, [this, downloadUrl]() {
        if (m_isCancelled) return;
        if (!m_currentReply) return;

        QNetworkReply *reply = m_currentReply;
        m_currentReply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::SslHandshakeFailedError ||
                reply->errorString().contains(QStringLiteral("TLS"), Qt::CaseInsensitive) ||
                reply->errorString().contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
                QFile::remove(m_tempFilePath);
                startCurlDownload(downloadUrl);
                return;
            }

            emit installationFailed(m_currentPlugin,
                QStringLiteral("Download fehlgeschlagen: %1").arg(reply->errorString()));
            if (!m_tempFilePath.isEmpty()) {
                QFile::remove(m_tempFilePath);
            }
            return;
        }

        onDownloadFinished();
    });
}

void Installer::startCurlDownload(const QString &downloadUrl)
{
    QString curlPath = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (curlPath.isEmpty()) {
#if defined(Q_OS_WIN)
        curlPath = QStringLiteral("C:\\Windows\\System32\\curl.exe");
#else
        curlPath = QStringLiteral("/usr/bin/curl");
#endif
    }

    m_curlProcess = new QProcess(this);
    QString nativeTemp = QDir::toNativeSeparators(m_tempFilePath);
    QString encodedUrl = QString::fromUtf8(QUrl(downloadUrl).toEncoded());

    QStringList args;
    args << QStringLiteral("-L")
         << QStringLiteral("-f")
         << QStringLiteral("--max-time") << QStringLiteral("180")
         << QStringLiteral("-H") << QStringLiteral("User-Agent: obs-plugin-manager/1.0")
         << QStringLiteral("-o") << nativeTemp
         << encodedUrl;

    if (!m_progressTimer) {
        m_progressTimer = new QTimer(this);
    }
    m_progressTimer->setInterval(200);
    m_progressTimer->disconnect();
    connect(m_progressTimer, &QTimer::timeout, this, [this]() {
        if (m_tempFilePath.isEmpty() || m_isCancelled) return;
        qint64 currentBytes = QFileInfo(m_tempFilePath).size();
        qint64 totalBytes = m_currentPlugin.assetSize;
        if (totalBytes > 0) {
            int percent = qBound(0, static_cast<int>((currentBytes * 100) / totalBytes), 99);
            emit progressUpdated(percent);
            double mbRecv = currentBytes / (1024.0 * 1024.0);
            double mbTotal = totalBytes / (1024.0 * 1024.0);
            emit statusUpdated(QStringLiteral("Lade herunter: %1% (%2 MB von %3 MB)...")
                .arg(percent)
                .arg(mbRecv, 0, 'f', 1)
                .arg(mbTotal, 0, 'f', 1));
        } else {
            double mbRecv = currentBytes / (1024.0 * 1024.0);
            emit statusUpdated(QStringLiteral("Lade herunter (%1 MB)...").arg(mbRecv, 0, 'f', 1));
        }
    });
    m_progressTimer->start();

    connect(m_curlProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            if (m_progressTimer) m_progressTimer->stop();
            if (m_curlProcess) {
                m_curlProcess->deleteLater();
                m_curlProcess = nullptr;
            }

            if (m_isCancelled) {
                if (!m_tempFilePath.isEmpty()) QFile::remove(m_tempFilePath);
                return;
            }

            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                emit installationFailed(m_currentPlugin,
                    QStringLiteral("Download mit curl fehlgeschlagen (Exit-Code %1).").arg(exitCode));
                if (!m_tempFilePath.isEmpty()) QFile::remove(m_tempFilePath);
                return;
            }

            onDownloadFinished();
        });

    connect(m_curlProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError /*err*/) {
        if (m_progressTimer) m_progressTimer->stop();
        if (m_curlProcess) {
            m_curlProcess->deleteLater();
            m_curlProcess = nullptr;
        }
        if (m_isCancelled) return;
        emit installationFailed(m_currentPlugin, QStringLiteral("Konnte curl nicht ausführen."));
        if (!m_tempFilePath.isEmpty()) QFile::remove(m_tempFilePath);
    });

    m_curlProcess->start(curlPath, args);
}

void Installer::cancel()
{
    m_isCancelled = true;

    if (m_progressTimer) {
        m_progressTimer->stop();
    }

    if (m_curlProcess) {
        m_curlProcess->kill();
        m_curlProcess->waitForFinished(500);
        m_curlProcess->deleteLater();
        m_curlProcess = nullptr;
    }

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (!m_tempFilePath.isEmpty()) {
        QFile::remove(m_tempFilePath);
        m_tempFilePath.clear();
    }
}

void Installer::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_isCancelled) return;

    if (bytesTotal > 0) {
        int percent = static_cast<int>((static_cast<double>(bytesReceived) / bytesTotal) * 100.0);
        emit progressUpdated(percent);
        double mbRecv = bytesReceived / (1024.0 * 1024.0);
        double mbTotal = bytesTotal / (1024.0 * 1024.0);
        emit statusUpdated(QStringLiteral("Lade herunter: %1% (%2 MB von %3 MB)...")
            .arg(percent)
            .arg(mbRecv, 0, 'f', 1)
            .arg(mbTotal, 0, 'f', 1));
    } else {
        double mbRecv = bytesReceived / (1024.0 * 1024.0);
        emit statusUpdated(QStringLiteral("Lade herunter (%1 MB)...").arg(mbRecv, 0, 'f', 1));
    }
}

void Installer::onDownloadFinished()
{
    if (m_isCancelled) return;

    emit statusUpdated(QStringLiteral("Download abgeschlossen. Entpacke Archiv..."));
    emit progressUpdated(0);

    QFileInfo fi(m_tempFilePath);
    if (!fi.exists() || fi.size() == 0) {
        emit installationFailed(m_currentPlugin, QStringLiteral("Die heruntergeladene Datei existiert nicht oder ist leer."));
        if (!m_tempFilePath.isEmpty()) {
            QFile::remove(m_tempFilePath);
            m_tempFilePath.clear();
        }
        return;
    }

    QString targetDir = getPluginInstallDir(m_currentPlugin.id);
    QString extractError;

    bool ok = extractZipArchive(m_tempFilePath, targetDir, extractError);

    // Clean up temporary download file
    if (!m_tempFilePath.isEmpty()) {
        QFile::remove(m_tempFilePath);
        m_tempFilePath.clear();
    }

    if (!ok) {
        emit installationFailed(m_currentPlugin, extractError);
        return;
    }

    emit progressUpdated(100);
    emit statusUpdated(QStringLiteral("Installation erfolgreich abgeschlossen."));
    emit installationSuccess(m_currentPlugin, targetDir);
}

bool Installer::extractZipArchive(const QString &zipFilePath, const QString &targetDirectory, QString &outError)
{
    QString nativeZipPath = QDir::toNativeSeparators(zipFilePath);

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    mz_bool status = mz_zip_reader_init_file(&zip, nativeZipPath.toUtf8().constData(), 0);
    if (!status) {
        mz_zip_error err = mz_zip_get_last_error(&zip);
        outError = QStringLiteral("Konnte das ZIP-Archiv nicht öffnen (beschädigt oder ungültiges Format): %1")
            .arg(QString::fromUtf8(mz_zip_get_error_string(err)));
        return false;
    }

    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
    if (numFiles == 0) {
        outError = QStringLiteral("Das heruntergeladene ZIP-Archiv ist leer.");
        mz_zip_reader_end(&zip);
        return false;
    }

    // Check if all files in the archive share a single root folder prefix
    QString commonPrefix;
    bool hasSingleRoot = true;

    for (mz_uint i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat fileStat;
        if (!mz_zip_reader_file_stat(&zip, i, &fileStat)) continue;

        QString name = QString::fromUtf8(fileStat.m_filename).replace(QLatin1Char('\\'), QLatin1Char('/'));
        int slashIdx = name.indexOf(QLatin1Char('/'));
        if (slashIdx == -1) {
            hasSingleRoot = false;
            break;
        }

        QString prefix = name.left(slashIdx + 1);
        if (commonPrefix.isEmpty()) {
            commonPrefix = prefix;
        } else if (commonPrefix != prefix) {
            hasSingleRoot = false;
            break;
        }
    }

    QDir target(targetDirectory);
    if (!target.exists()) {
        if (!QDir().mkpath(targetDirectory)) {
            outError = QStringLiteral("Konnte Zielverzeichnis nicht erstellen: %1").arg(targetDirectory);
            mz_zip_reader_end(&zip);
            return false;
        }
    }

    for (mz_uint i = 0; i < numFiles; ++i) {
        if (m_isCancelled) {
            outError = QStringLiteral("Installation wurde vom Benutzer abgebrochen.");
            mz_zip_reader_end(&zip);
            return false;
        }

        mz_zip_archive_file_stat fileStat;
        if (!mz_zip_reader_file_stat(&zip, i, &fileStat)) continue;

        QString rawName = QString::fromUtf8(fileStat.m_filename).replace(QLatin1Char('\\'), QLatin1Char('/'));

        // Strip single root folder wrapper if present
        if (hasSingleRoot && !commonPrefix.isEmpty() && rawName.startsWith(commonPrefix)) {
            rawName = rawName.mid(commonPrefix.length());
        }

        if (rawName.isEmpty()) continue;

        // Security: Zip Slip vulnerability prevention
        if (rawName.contains(QStringLiteral("..")) || rawName.startsWith(QLatin1Char('/'))) {
            qWarning() << "Überspringe verdächtigen Pfad in ZIP-Archiv:" << rawName;
            continue;
        }

        // Normalize standard OBS plugin paths:
        // Format 1: "obs-plugins/64bit/<file>" -> "bin/64bit/<file>"
        // Format 2: "data/obs-plugins/<anything>/..." -> "data/..."
        // Format 3: Root ".dll" file -> "bin/64bit/<file>"
        QString normalizedName = rawName;
        if (normalizedName.startsWith(QStringLiteral("obs-plugins/64bit/"), Qt::CaseInsensitive)) {
            normalizedName = QStringLiteral("bin/64bit/") + normalizedName.mid(18);
        } else if (normalizedName.startsWith(QStringLiteral("data/obs-plugins/"), Qt::CaseInsensitive)) {
            int nextSlash = normalizedName.indexOf(QLatin1Char('/'), 17);
            if (nextSlash != -1) {
                normalizedName = QStringLiteral("data/") + normalizedName.mid(nextSlash + 1);
            } else {
                normalizedName = QStringLiteral("data");
            }
        } else if (!normalizedName.contains(QLatin1Char('/')) && normalizedName.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive)) {
            normalizedName = QStringLiteral("bin/64bit/") + normalizedName;
        }

        QString destPath = target.filePath(normalizedName);

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            QDir().mkpath(destPath);
        } else {
            QFileInfo destInfo(destPath);
            QDir().mkpath(destInfo.path());

            QString nativeDestPath = QDir::toNativeSeparators(destPath);
            if (!mz_zip_reader_extract_to_file(&zip, i, nativeDestPath.toUtf8().constData(), 0)) {
                mz_zip_error err = mz_zip_get_last_error(&zip);
                outError = QStringLiteral("Fehler beim Entpacken der Datei '%1': %2")
                    .arg(rawName, QString::fromUtf8(mz_zip_get_error_string(err)));
                mz_zip_reader_end(&zip);
                return false;
            }
        }

        int percent = static_cast<int>((static_cast<double>(i + 1) / numFiles) * 100.0);
        emit progressUpdated(percent);
        emit statusUpdated(QStringLiteral("Entpacke: %1 (%2%)").arg(rawName).arg(percent));
    }

    mz_zip_reader_end(&zip);
    return true;
}
