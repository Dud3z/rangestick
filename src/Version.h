#pragma once

// Firmware-Version + GitHub-Repo-Koordinaten fuer den OTA-Update-Check (siehe OtaUpdater).
// Bei jedem Release: FW_VERSION hier UND der Git-Tag (z.B. "v1.0.1") muessen uebereinstimmen --
// die GitHub Action schreibt den Tag-Namen (ohne fuehrendes "v") nach ota/version.txt, womit
// der Stick per Stringvergleich gegen FW_VERSION erkennt, ob eine neue Version vorliegt.
namespace Version {
constexpr const char* FW_VERSION = "1.0.0";

constexpr const char* OTA_REPO_OWNER = "Dud3z";
constexpr const char* OTA_REPO_NAME = "rangestick";
constexpr const char* OTA_REPO_BRANCH = "main";
} // namespace Version
