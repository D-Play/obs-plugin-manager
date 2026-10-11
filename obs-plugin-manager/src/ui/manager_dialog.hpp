#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QVector>
#include <QMap>
#include "../core/github_client.hpp"
#include "../core/installer.hpp"

class ManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ManagerDialog(QWidget *parent = nullptr);
    ~ManagerDialog() override = default;

private slots:
    // Suche & GitHub
    void onSearchClicked();
    void onSearchResultsReady(const QVector<PluginInfo> &results);
    void onSearchFailed(const QString &errorMessage);

    // Release Details
    void onReleaseInfoReady(const QString &repo, const ReleaseInfo &release,
                            const QString &bestAssetUrl, const QString &bestAssetName,
                            qint64 bestAssetSize);
    void onReleaseInfoFailed(const QString &repo, const QString &errorMessage);

    // Filter für kuratierte Plugins
    void onCuratedFilterChanged(const QString &filterText);

    // Tab & Zeilenauswahl
    void onCuratedTableSelectionChanged();
    void onGitHubTableSelectionChanged();
    void onTabChanged(int index);

    // Aktionen
    void onInstallClicked();
    void onUninstallClicked();
    void onOpenPluginFolderClicked();

    // Installer-Rückmeldungen
    void onInstallStatusUpdated(const QString &status);
    void onInstallProgressUpdated(int percent);
    void onInstallSuccess(const PluginInfo &plugin, const QString &installedPath);
    void onInstallFailed(const PluginInfo &plugin, const QString &errorMessage);

private:
    void setupUi();
    void populateCuratedTable();
    void updateButtonsState();
    PluginInfo getSelectedPlugin(bool *hasSelection = nullptr);
    void updatePluginStatusInTables(const QString &pluginId, bool isInstalled);

    // UI-Elemente
    QTabWidget *m_tabWidget = nullptr;

    // Tab 1: Empfohlen
    QLineEdit *m_curatedSearchEdit = nullptr;
    QTableWidget *m_curatedTable = nullptr;

    // Tab 2: GitHub Entdecken
    QLineEdit *m_githubSearchEdit = nullptr;
    QPushButton *m_githubSearchBtn = nullptr;
    QTableWidget *m_githubTable = nullptr;
    QLabel *m_securityBanner = nullptr;

    // Fußzeile
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_installBtn = nullptr;
    QPushButton *m_uninstallBtn = nullptr;
    QPushButton *m_openFolderBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;

    // Core-Komponenten
    GitHubClient *m_githubClient = nullptr;
    Installer *m_installer = nullptr;

    // Daten
    QVector<PluginInfo> m_curatedPlugins;
    QVector<PluginInfo> m_githubPlugins;
    QMap<QString, QString> m_resolvedAssetUrls;  // Repo -> Asset URL
    QMap<QString, QString> m_resolvedAssetNames; // Repo -> Asset Name
    QMap<QString, qint64>  m_resolvedAssetSizes; // Repo -> Asset Size
    bool m_isInstalling = false;
    PluginInfo m_pendingInstallPlugin;
};
