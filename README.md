# OBS Plugin Manager

> A native, modern C++/Qt6 plugin manager for [OBS Studio](https://obsproject.com/). Browse curated community plugins, discover releases directly from GitHub, and install them with a single click—no administrative privileges required.

[![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue.svg)](https://obsproject.com)
[![OBS Studio](https://img.shields.io/badge/OBS%20Studio-30%2B-orange.svg)](https://obsproject.com)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-green.svg)](LICENSE)

---

##  Features

- **Native OBS Studio Integration:** Embedded right into the OBS menu: **Werkzeuge (Tools) &rarr; Plugin Manager...**.
- **Empfohlen (Curated Plugins):** One-click installation for tested, popular plugins:
  - *Move Transition* (Exeldro)
  - *Source Clone* (Exeldro)
  - *Aitum Vertical Canvas* (Aitum)
  - *Shader Filter* (Exeldro)
  - *Source Record* (Exeldro)
  - *Downstream Keyer* (Exeldro)
  - *3D Effect* (Exeldro)
  - *Audio Monitor* (Exeldro)
  - *Advanced Masks* (FiniteSingularity)
  - *Background Removal AI* (occ-ai)
  - *Spectralizer* (univrsal)
  - *Transition Table* (Exeldro)
- **GitHub Entdecken (GitHub Discovery):** Search any OBS Studio plugin on GitHub with live star ratings, repository descriptions, and automatic asset selection.
- **Smart Archive Extraction:** Automatically normalizes diverse release archive structures (`-programdata.zip`, standard `-windows.zip`, and flat DLL packages) into the official OBS folder structure (`bin/64bit/` and `data/`).
- **No Admin Rights Required:** Installs safely into the user directory (`%APPDATA%/obs-studio/plugins/`) or `%ProgramData%/obs-studio/plugins/`.
- **Zero External DLL Dependencies:** Bundles the high-performance `miniz` ZIP decompression library directly into the plugin binary and utilizes native OS network TLS (Windows Schannel / curl) for 100% reliable HTTPS transfers.

---

##  Installation (Für Benutzer / For Users)

### Windows (64-Bit)

1. Lade das neueste Release (`obs-plugin-manager-windows-x64.zip`) unter **Releases** herunter.
2. Entpacke den Inhalt nach:
   ```text
   C:\ProgramData\obs-studio\plugins\obs-plugin-manager\
   ```
   *oder alternativ in dein Benutzerverzeichnis:*
   ```text
   %APPDATA%\obs-studio\plugins\obs-plugin-manager\
   ```
3. Die Ordnerstruktur sollte wie folgt aussehen:
   ```text
   obs-plugin-manager/
   └── bin/
       └── 64bit/
           └── obs-plugin-manager.dll
   ```
4. Starte OBS Studio.
5. Klicke im Menü auf **Werkzeuge** &rarr; **Plugin Manager...**.

---

## 🛠️ Build-Anleitung (Für Entwickler / For Developers)

### Voraussetzungen
- **Betriebssystem:** Windows 10 / 11 (64-Bit)
- **Compiler:** Visual Studio 2022 / 2026 (MSVC mit C++17)
- **Build-Tool:** CMake 3.16+
- **Qt SDK:** Qt 6.6+ (MSVC 64-Bit mit Modulen: Core, Gui, Widgets, Network)
- **OBS Studio:** OBS Studio 30+ x64

### Kompilieren mit CMake

```powershell
# 1. Repository klonen
git clone https://github.com/<your-username>/obs-plugin-manager.git
cd obs-plugin-manager

# 2. CMake konfigurieren
cmake -B build -S . `
  -DCMAKE_BUILD_TYPE=Release `
  -DQt6_DIR="A:/Qt/6.6.3/msvc2019_64/lib/cmake/Qt6"

# 3. Release kompilieren
cmake --build build --config Release
```

Die fertige DLL wird in `build/Release/obs-plugin-manager.dll` erstellt.

---

##  Projektstruktur

```text
obs-plugin-manager/
├── CMakeLists.txt              # CMake Build-Konfiguration
├── LICENSE                     # GNU General Public License v2.0
├── README.md                   # Dokumentation
├── resources.qrc               # Qt-Ressourcendatei
├── 3rdparty/
│   ├── miniz.h                 # Integrierter ZIP-Dekompressor
│   └── miniz.c
├── cmake/
│   ├── Findlibobs.cmake        # Modulsuche für libobs
│   └── Findobs-frontend-api.cmake
├── src/
│   ├── plugin-main.cpp         # OBS Plugin-Einstiegspunkt & Menü-Aktion
│   ├── core/
│   │   ├── github_client.hpp   # GitHub REST API Client & OS-Asset-Scoring
│   │   ├── github_client.cpp
│   │   ├── installer.hpp       # Asynchroner Download & miniz-Extraktion
│   │   └── installer.cpp
│   ├── ui/
│   │   ├── manager_dialog.hpp  # Qt6 Dialog (Empfohlen, Entdecken, Status)
│   │   └── manager_dialog.cpp
│   └── data/
│       └── curated.json        # Liste verifizierter Community-Plugins
```

---

##  Lizenz

Dieses Projekt ist unter der **GNU General Public License v2.0 (GPL-2.0)** lizenziert – passend zur Lizenz von OBS Studio. Details siehe [LICENSE](LICENSE).
