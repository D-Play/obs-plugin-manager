#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
#include <QPointer>
#include "ui/manager_dialog.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-plugin-manager", "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
    return "OBS Plugin Manager";
}

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Nativer Plugin-Manager für OBS Studio zum Durchsuchen, Herunterladen und Installieren von Plugins.";
}

static QPointer<ManagerDialog> s_managerDialog = nullptr;

static void on_open_plugin_manager()
{
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!s_managerDialog) {
        s_managerDialog = new ManagerDialog(mainWindow);
    }
    s_managerDialog->show();
    s_managerDialog->raise();
    s_managerDialog->activateWindow();
}

#include <QCoreApplication>
#include <QDir>
#include "core/installer.hpp"

bool obs_module_load(void)
{
    // Füge Plugin-Pfade zu QCoreApplication::libraryPaths hinzu, damit Qt das TLS-Backend (qschannelbackend) findet
    QString baseDir = Installer::getPluginsBaseDir();
    QCoreApplication::addLibraryPath(baseDir + QStringLiteral("/obs-plugin-manager/bin/64bit"));
    QCoreApplication::addLibraryPath(baseDir + QStringLiteral("/obs-plugin-manager"));
    QCoreApplication::addLibraryPath(QStringLiteral("C:/ProgramData/obs-studio/plugins/obs-plugin-manager/bin/64bit"));
    QCoreApplication::addLibraryPath(QStringLiteral("C:/ProgramData/obs-studio/plugins/obs-plugin-manager"));

    // Registriere Menüeintrag im OBS-Hauptmenü unter "Werkzeuge" / "Tools"
    auto *action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction("Plugin Manager..."));
    if (action) {
        QAction::connect(action, &QAction::triggered, []() {
            on_open_plugin_manager();
        });
    }

    return true;
}

void obs_module_unload(void)
{
    if (s_managerDialog) {
        delete s_managerDialog;
        s_managerDialog = nullptr;
    }
}
