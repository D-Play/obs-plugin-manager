#include "manager_dialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QColor>
#include <QFrame>
#include <QCoreApplication>

ManagerDialog::ManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    QString baseDir = Installer::getPluginsBaseDir();
    QCoreApplication::addLibraryPath(baseDir + QStringLiteral("/obs-plugin-manager/bin/64bit"));
    QCoreApplication::addLibraryPath(QStringLiteral("C:/ProgramData/obs-studio/plugins/obs-plugin-manager/bin/64bit"));
    QCoreApplication::addLibraryPath(QStringLiteral("C:/ProgramData/obs-studio/plugins/obs-plugin-manager"));

    m_githubClient = new GitHubClient(this);
    m_installer = new Installer(this);

    setupUi();

    // Signalverbindungen für GitHubClient
    connect(m_githubClient, &GitHubClient::searchFinished, this, &ManagerDialog::onSearchResultsReady);
    connect(m_githubClient, &GitHubClient::searchFailed, this, &ManagerDialog::onSearchFailed);
    connect(m_githubClient, &GitHubClient::releaseInfoFinished, this, &ManagerDialog::onReleaseInfoReady);
    connect(m_githubClient, &GitHubClient::releaseInfoFailed, this, &ManagerDialog::onReleaseInfoFailed);

    // Signalverbindungen für Installer
    connect(m_installer, &Installer::statusUpdated, this, &ManagerDialog::onInstallStatusUpdated);
    connect(m_installer, &Installer::progressUpdated, this, &ManagerDialog::onInstallProgressUpdated);
    connect(m_installer, &Installer::installationSuccess, this, &ManagerDialog::onInstallSuccess);
    connect(m_installer, &Installer::installationFailed, this, &ManagerDialog::onInstallFailed);

    // Initiale Daten laden
    m_curatedPlugins = m_githubClient->loadCuratedPlugins();
    populateCuratedTable();
    updateButtonsState();
}

void ManagerDialog::setupUi()
{
    setWindowTitle(QStringLiteral("OBS Studio - Plugin Manager"));
    resize(940, 620);
    setMinimumSize(800, 520);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    // Haupt-Tabs
    m_tabWidget = new QTabWidget(this);

    // -------------------------------------------------------------
    // Tab 1: Empfohlen (Kuratierte Plugins)
    // -------------------------------------------------------------
    auto *curatedTab = new QWidget(this);
    auto *curatedLayout = new QVBoxLayout(curatedTab);
    curatedLayout->setContentsMargins(10, 10, 10, 10);
    curatedLayout->setSpacing(8);

    // Filter-Eingabe
    auto *curatedFilterLayout = new QHBoxLayout();
    auto *curatedFilterLabel = new QLabel(QStringLiteral("Filter:"), curatedTab);
    m_curatedSearchEdit = new QLineEdit(curatedTab);
    m_curatedSearchEdit->setPlaceholderText(QStringLiteral("In der kuratierten Liste filtern (z. B. Transition, Exeldro)..."));
    m_curatedSearchEdit->setClearButtonEnabled(true);
    connect(m_curatedSearchEdit, &QLineEdit::textChanged, this, &ManagerDialog::onCuratedFilterChanged);

    curatedFilterLayout->addWidget(curatedFilterLabel);
    curatedFilterLayout->addWidget(m_curatedSearchEdit);
    curatedLayout->addLayout(curatedFilterLayout);

    // Tabelle für kuratierte Plugins
    m_curatedTable = new QTableWidget(0, 4, curatedTab);
    m_curatedTable->setHorizontalHeaderLabels({
        QStringLiteral("Name"),
        QStringLiteral("Autor"),
        QStringLiteral("Beschreibung"),
        QStringLiteral("Status")
    });
    m_curatedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_curatedTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_curatedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_curatedTable->setAlternatingRowColors(true);
    m_curatedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_curatedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_curatedTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_curatedTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_curatedTable->verticalHeader()->setVisible(false);

    connect(m_curatedTable, &QTableWidget::itemSelectionChanged, this, &ManagerDialog::onCuratedTableSelectionChanged);
    connect(m_curatedTable, &QTableWidget::cellDoubleClicked, this, &ManagerDialog::onInstallClicked);

    curatedLayout->addWidget(m_curatedTable);
    m_tabWidget->addTab(curatedTab, QStringLiteral("Empfohlen"));

    // -------------------------------------------------------------
    // Tab 2: GitHub Entdecken
    // -------------------------------------------------------------
    auto *githubTab = new QWidget(this);
    auto *githubLayout = new QVBoxLayout(githubTab);
    githubLayout->setContentsMargins(10, 10, 10, 10);
    githubLayout->setSpacing(8);

    // Sicherheitsbanner / Warnhinweis
    auto *bannerFrame = new QFrame(githubTab);
    bannerFrame->setObjectName(QStringLiteral("securityBanner"));
    bannerFrame->setStyleSheet(
        QStringLiteral("#securityBanner { background-color: rgba(234, 179, 8, 0.15); "
                       "border: 1px solid #eab308; border-radius: 6px; padding: 8px; }")
    );
    auto *bannerLayout = new QHBoxLayout(bannerFrame);
    bannerLayout->setContentsMargins(8, 6, 8, 6);

    m_securityBanner = new QLabel(bannerFrame);
    m_securityBanner->setWordWrap(true);
    m_securityBanner->setText(
        QStringLiteral("<b>⚠️ Sicherheitshinweis:</b> Plugins aus der GitHub-Suche sind nicht offiziell getestet "
                       "oder verifiziert. Die Installation erfolgt auf eigene Gefahr!")
    );
    bannerLayout->addWidget(m_securityBanner);
    githubLayout->addWidget(bannerFrame);

    // Suchbereich
    auto *searchLayout = new QHBoxLayout();
    m_githubSearchEdit = new QLineEdit(githubTab);
    m_githubSearchEdit->setPlaceholderText(QStringLiteral("GitHub nach OBS-Plugins durchsuchen (z. B. move, transition, filter)..."));
    m_githubSearchEdit->setClearButtonEnabled(true);
    connect(m_githubSearchEdit, &QLineEdit::returnPressed, this, &ManagerDialog::onSearchClicked);

    m_githubSearchBtn = new QPushButton(QStringLiteral("Suchen"), githubTab);
    connect(m_githubSearchBtn, &QPushButton::clicked, this, &ManagerDialog::onSearchClicked);

    searchLayout->addWidget(m_githubSearchEdit);
    searchLayout->addWidget(m_githubSearchBtn);
    githubLayout->addLayout(searchLayout);

    // GitHub-Ergebnistabelle
    m_githubTable = new QTableWidget(0, 5, githubTab);
    m_githubTable->setHorizontalHeaderLabels({
        QStringLiteral("Repository"),
        QStringLiteral("Autor"),
        QStringLiteral("Stars"),
        QStringLiteral("Beschreibung"),
        QStringLiteral("Status")
    });
    m_githubTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_githubTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_githubTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_githubTable->setAlternatingRowColors(true);
    m_githubTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_githubTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_githubTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_githubTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_githubTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_githubTable->verticalHeader()->setVisible(false);

    connect(m_githubTable, &QTableWidget::itemSelectionChanged, this, &ManagerDialog::onGitHubTableSelectionChanged);
    connect(m_githubTable, &QTableWidget::cellDoubleClicked, this, &ManagerDialog::onInstallClicked);

    githubLayout->addWidget(m_githubTable);
    m_tabWidget->addTab(githubTab, QStringLiteral("GitHub Entdecken"));

    mainLayout->addWidget(m_tabWidget);

    // -------------------------------------------------------------
    // Fußzeile (Status, Fortschritt und Buttons)
    // -------------------------------------------------------------
    auto *footerLayout = new QVBoxLayout();
    footerLayout->setSpacing(6);

    // Statuszeile & Fortschrittsbalken
    m_statusLabel = new QLabel(QStringLiteral("Bereit."), this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(16);
    m_progressBar->setTextVisible(true);

    footerLayout->addWidget(m_statusLabel);
    footerLayout->addWidget(m_progressBar);

    // Aktions-Buttons
    auto *btnLayout = new QHBoxLayout();

    m_openFolderBtn = new QPushButton(QStringLiteral("Plugin-Ordner öffnen"), this);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &ManagerDialog::onOpenPluginFolderClicked);
    btnLayout->addWidget(m_openFolderBtn);

    btnLayout->addStretch();

    m_uninstallBtn = new QPushButton(QStringLiteral("Deinstallieren"), this);
    m_uninstallBtn->setEnabled(false);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &ManagerDialog::onUninstallClicked);
    btnLayout->addWidget(m_uninstallBtn);

    m_installBtn = new QPushButton(QStringLiteral("Installieren"), this);
    m_installBtn->setDefault(true);
    m_installBtn->setEnabled(false);
    m_installBtn->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; }"));
    connect(m_installBtn, &QPushButton::clicked, this, &ManagerDialog::onInstallClicked);
    btnLayout->addWidget(m_installBtn);

    m_closeBtn = new QPushButton(QStringLiteral("Schließen"), this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_closeBtn);

    footerLayout->addLayout(btnLayout);
    mainLayout->addLayout(footerLayout);

    // Erst verbinden, wenn alle Tabs, Tabellen und Buttons initialisiert sind
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &ManagerDialog::onTabChanged);
}

void ManagerDialog::populateCuratedTable()
{
    if (!m_curatedTable) return;

    m_curatedTable->blockSignals(true);
    m_curatedTable->setRowCount(0);

    for (int i = 0; i < m_curatedPlugins.size(); ++i) {
        const auto &plugin = m_curatedPlugins[i];
        bool installed = Installer::isPluginInstalled(plugin.id);

        int row = m_curatedTable->rowCount();
        m_curatedTable->insertRow(row);

        auto *nameItem = new QTableWidgetItem(plugin.name);
        QFont boldFont = nameItem->font();
        boldFont.setBold(true);
        nameItem->setFont(boldFont);
        nameItem->setData(Qt::UserRole, plugin.id);

        auto *authorItem = new QTableWidgetItem(plugin.author);
        auto *descItem = new QTableWidgetItem(plugin.description);

        auto *statusItem = new QTableWidgetItem(installed ? QStringLiteral("Installiert") : QStringLiteral("Verfügbar"));
        if (installed) {
            statusItem->setForeground(QColor(46, 160, 67)); // Grün
        } else {
            statusItem->setForeground(QColor(120, 120, 120)); // Grau
        }

        m_curatedTable->setItem(row, 0, nameItem);
        m_curatedTable->setItem(row, 1, authorItem);
        m_curatedTable->setItem(row, 2, descItem);
        m_curatedTable->setItem(row, 3, statusItem);
    }

    m_curatedTable->blockSignals(false);
}

void ManagerDialog::onCuratedFilterChanged(const QString &filterText)
{
    QString trimmed = filterText.trimmed().toLower();
    for (int row = 0; row < m_curatedTable->rowCount(); ++row) {
        bool match = false;
        if (trimmed.isEmpty()) {
            match = true;
        } else {
            for (int col = 0; col < 3; ++col) {
                QTableWidgetItem *item = m_curatedTable->item(row, col);
                if (item && item->text().toLower().contains(trimmed)) {
                    match = true;
                    break;
                }
            }
        }
        m_curatedTable->setRowHidden(row, !match);
    }
}

void ManagerDialog::onSearchClicked()
{
    QString query = m_githubSearchEdit->text().trimmed();
    m_githubSearchBtn->setEnabled(false);
    m_statusLabel->setText(QStringLiteral("Durchsuche GitHub nach OBS-Plugins..."));
    m_progressBar->setValue(0);

    m_githubClient->searchPlugins(query);
}

void ManagerDialog::onSearchResultsReady(const QVector<PluginInfo> &results)
{
    m_githubSearchBtn->setEnabled(true);
    m_githubPlugins = results;
    m_githubTable->setRowCount(0);

    for (int i = 0; i < m_githubPlugins.size(); ++i) {
        const auto &plugin = m_githubPlugins[i];
        bool installed = Installer::isPluginInstalled(plugin.id);

        int row = m_githubTable->rowCount();
        m_githubTable->insertRow(row);

        auto *repoItem = new QTableWidgetItem(plugin.repo);
        QFont boldFont = repoItem->font();
        boldFont.setBold(true);
        repoItem->setFont(boldFont);
        repoItem->setData(Qt::UserRole, plugin.id);

        auto *authorItem = new QTableWidgetItem(plugin.author);
        auto *starsItem = new QTableWidgetItem(QStringLiteral("⭐ %1").arg(plugin.stars));
        auto *descItem = new QTableWidgetItem(plugin.description);

        auto *statusItem = new QTableWidgetItem(installed ? QStringLiteral("Installiert") : QStringLiteral("Verfügbar"));
        if (installed) {
            statusItem->setForeground(QColor(46, 160, 67));
        } else {
            statusItem->setForeground(QColor(120, 120, 120));
        }

        m_githubTable->setItem(row, 0, repoItem);
        m_githubTable->setItem(row, 1, authorItem);
        m_githubTable->setItem(row, 2, starsItem);
        m_githubTable->setItem(row, 3, descItem);
        m_githubTable->setItem(row, 4, statusItem);
    }

    m_statusLabel->setText(QStringLiteral("%1 Plugins auf GitHub gefunden.").arg(results.size()));
    updateButtonsState();
}

void ManagerDialog::onSearchFailed(const QString &errorMessage)
{
    m_githubSearchBtn->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("Fehler bei der Suche: %1").arg(errorMessage));
    QMessageBox::warning(this, QStringLiteral("Suche fehlgeschlagen"), errorMessage);
}

void ManagerDialog::onCuratedTableSelectionChanged()
{
    updateButtonsState();
    bool hasSel = false;
    PluginInfo plugin = getSelectedPlugin(&hasSel);
    if (hasSel && !plugin.repo.isEmpty() && !m_resolvedAssetUrls.contains(plugin.repo)) {
        m_statusLabel->setText(QStringLiteral("Prüfe neuestes Release für %1...").arg(plugin.name));
        m_githubClient->fetchLatestRelease(plugin.repo);
    }
}

void ManagerDialog::onGitHubTableSelectionChanged()
{
    updateButtonsState();
    bool hasSel = false;
    PluginInfo plugin = getSelectedPlugin(&hasSel);
    if (hasSel && !plugin.repo.isEmpty() && !m_resolvedAssetUrls.contains(plugin.repo)) {
        m_statusLabel->setText(QStringLiteral("Prüfe Release-Paket für %1...").arg(plugin.repo));
        m_githubClient->fetchLatestRelease(plugin.repo);
    }
}

void ManagerDialog::onTabChanged(int /*index*/)
{
    if (!m_tabWidget || !m_curatedTable || !m_githubTable) {
        return;
    }
    updateButtonsState();
}

PluginInfo ManagerDialog::getSelectedPlugin(bool *hasSelection)
{
    if (hasSelection) *hasSelection = false;

    if (!m_tabWidget || !m_curatedTable || !m_githubTable) {
        return PluginInfo();
    }

    int currentTab = m_tabWidget->currentIndex();
    QTableWidget *activeTable = (currentTab == 0) ? m_curatedTable : m_githubTable;
    const QVector<PluginInfo> &activeList = (currentTab == 0) ? m_curatedPlugins : m_githubPlugins;

    if (!activeTable) {
        return PluginInfo();
    }

    int selectedRow = activeTable->currentRow();
    if (selectedRow < 0 || selectedRow >= activeTable->rowCount()) {
        return PluginInfo();
    }

    QTableWidgetItem *item = activeTable->item(selectedRow, 0);
    if (!item) {
        return PluginInfo();
    }

    QString pluginId = item->data(Qt::UserRole).toString();

    for (const auto &p : activeList) {
        if (p.id == pluginId) {
            if (hasSelection) *hasSelection = true;
            return p;
        }
    }

    return PluginInfo();
}

void ManagerDialog::updateButtonsState()
{
    if (!m_installBtn || !m_uninstallBtn || !m_githubSearchBtn) {
        return;
    }

    if (m_isInstalling) {
        m_installBtn->setEnabled(false);
        m_uninstallBtn->setEnabled(false);
        m_githubSearchBtn->setEnabled(false);
        return;
    }

    m_githubSearchBtn->setEnabled(true);

    bool hasSel = false;
    PluginInfo plugin = getSelectedPlugin(&hasSel);

    if (!hasSel) {
        m_installBtn->setEnabled(false);
        m_uninstallBtn->setEnabled(false);
        return;
    }

    bool isInstalled = Installer::isPluginInstalled(plugin.id);
    m_installBtn->setEnabled(true);
    m_installBtn->setText(isInstalled ? QStringLiteral("Aktualisieren / Neu installieren") : QStringLiteral("Installieren"));
    m_uninstallBtn->setEnabled(isInstalled);
}

void ManagerDialog::onReleaseInfoReady(const QString &repo, const ReleaseInfo &release,
                                       const QString &bestAssetUrl, const QString &bestAssetName,
                                       qint64 bestAssetSize)
{
    m_resolvedAssetUrls.insert(repo, bestAssetUrl);
    m_resolvedAssetNames.insert(repo, bestAssetName);
    m_resolvedAssetSizes.insert(repo, bestAssetSize);

    bool hasSel = false;
    PluginInfo selected = getSelectedPlugin(&hasSel);
    if (hasSel && selected.repo == repo) {
        double mbSize = bestAssetSize / (1024.0 * 1024.0);
        m_statusLabel->setText(QStringLiteral("Release %1 verfügbar: %2 (%3 MB)")
            .arg(release.tagName, bestAssetName)
            .arg(mbSize, 0, 'f', 1));
    }

    // Wenn auf diesen Download gewartet wurde:
    if (m_pendingInstallPlugin.repo == repo) {
        PluginInfo pluginToInstall = m_pendingInstallPlugin;
        pluginToInstall.assetSize = bestAssetSize;
        m_pendingInstallPlugin = PluginInfo();
        m_isInstalling = true;
        updateButtonsState();
        m_installer->startInstallation(pluginToInstall, bestAssetUrl);
    }
}

void ManagerDialog::onReleaseInfoFailed(const QString &repo, const QString &errorMessage)
{
    if (m_pendingInstallPlugin.repo == repo) {
        m_pendingInstallPlugin = PluginInfo();
        m_isInstalling = false;
        updateButtonsState();
        m_statusLabel->setText(QStringLiteral("Release-Prüfung fehlgeschlagen."));
        QMessageBox::critical(this, QStringLiteral("Installation nicht möglich"),
            QStringLiteral("Für das Repository '%1' konnte kein kompatibles Paket gefunden werden:\n%2")
                .arg(repo, errorMessage));
    } else {
        m_statusLabel->setText(errorMessage);
    }
}

void ManagerDialog::onInstallClicked()
{
    if (m_isInstalling) return;

    bool hasSel = false;
    PluginInfo plugin = getSelectedPlugin(&hasSel);
    if (!hasSel) return;

    if (plugin.repo.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Fehler"),
            QStringLiteral("Kein GitHub-Repository für dieses Plugin hinterlegt."));
        return;
    }

    if (m_resolvedAssetUrls.contains(plugin.repo)) {
        QString downloadUrl = m_resolvedAssetUrls.value(plugin.repo);
        plugin.assetSize = m_resolvedAssetSizes.value(plugin.repo, 0);
        m_isInstalling = true;
        updateButtonsState();
        m_installer->startInstallation(plugin, downloadUrl);
    } else {
        // Zuerst Release-Informationen von GitHub anfordern
        m_pendingInstallPlugin = plugin;
        m_isInstalling = true;
        updateButtonsState();
        m_statusLabel->setText(QStringLiteral("Ermittle neuestes Release für %1...").arg(plugin.name));
        m_githubClient->fetchLatestRelease(plugin.repo);
    }
}

void ManagerDialog::onUninstallClicked()
{
    bool hasSel = false;
    PluginInfo plugin = getSelectedPlugin(&hasSel);
    if (!hasSel) return;

    auto answer = QMessageBox::question(this,
        QStringLiteral("Plugin deinstallieren"),
        QStringLiteral("Möchtest du das Plugin \"%1\" wirklich entfernen?").arg(plugin.name),
        QMessageBox::Yes | QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    if (Installer::uninstallPlugin(plugin.id)) {
        updatePluginStatusInTables(plugin.id, false);
        m_statusLabel->setText(QStringLiteral("Plugin \"%1\" wurde erfolgreich entfernt.").arg(plugin.name));
        updateButtonsState();
        QMessageBox::information(this,
            QStringLiteral("Deinstallation erfolgreich"),
            QStringLiteral("Das Plugin \"%1\" wurde entfernt.\nBitte starte OBS Studio neu.")
                .arg(plugin.name));
    } else {
        QMessageBox::critical(this,
            QStringLiteral("Fehler"),
            QStringLiteral("Das Plugin-Verzeichnis konnte nicht gelöscht werden. Eventuell ist OBS aktiv und blockiert Dateien."));
    }
}

void ManagerDialog::onOpenPluginFolderClicked()
{
    QString pluginsDir = Installer::getPluginsBaseDir();
    QDir dir(pluginsDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(pluginsDir));
}

void ManagerDialog::onInstallStatusUpdated(const QString &status)
{
    m_statusLabel->setText(status);
}

void ManagerDialog::onInstallProgressUpdated(int percent)
{
    m_progressBar->setValue(percent);
}

void ManagerDialog::onInstallSuccess(const PluginInfo &plugin, const QString &installedPath)
{
    m_isInstalling = false;
    m_progressBar->setValue(100);
    updatePluginStatusInTables(plugin.id, true);
    updateButtonsState();

    QMessageBox::information(this,
        QStringLiteral("Installation erfolgreich"),
        QStringLiteral("Das Plugin \"%1\" wurde erfolgreich installiert!\n\n"
                       "Zielverzeichnis:\n%2\n\n"
                       "WICHTIG: Bitte starte OBS Studio neu, damit das Plugin geladen wird.")
            .arg(plugin.name, installedPath));
}

void ManagerDialog::onInstallFailed(const PluginInfo &plugin, const QString &errorMessage)
{
    m_isInstalling = false;
    m_progressBar->setValue(0);
    m_statusLabel->setText(QStringLiteral("Installation fehlgeschlagen."));
    updateButtonsState();

    QMessageBox::critical(this,
        QStringLiteral("Installationsfehler"),
        QStringLiteral("Fehler bei der Installation von \"%1\":\n\n%2")
            .arg(plugin.name, errorMessage));
}

void ManagerDialog::updatePluginStatusInTables(const QString &pluginId, bool isInstalled)
{
    auto updateTable = [&](QTableWidget *table) {
        for (int r = 0; r < table->rowCount(); ++r) {
            QTableWidgetItem *item = table->item(r, 0);
            if (item && item->data(Qt::UserRole).toString() == pluginId) {
                int statusCol = (table == m_curatedTable) ? 3 : 4;
                QTableWidgetItem *statusItem = table->item(r, statusCol);
                if (statusItem) {
                    statusItem->setText(isInstalled ? QStringLiteral("Installiert") : QStringLiteral("Verfügbar"));
                    statusItem->setForeground(isInstalled ? QColor(46, 160, 67) : QColor(120, 120, 120));
                }
            }
        }
    };

    updateTable(m_curatedTable);
    updateTable(m_githubTable);
}
