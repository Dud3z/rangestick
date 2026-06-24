#pragma once

// Firmware version + GitHub repo coordinates for the OTA update check (see OtaUpdater).
// For every release: FW_VERSION here AND the git tag (e.g. "v1.0.1") must match -- the GitHub
// Action writes the tag name (without the leading "v") to ota/version.txt, which is how the
// stick detects via string comparison against FW_VERSION whether a new version is available.
namespace Version {
constexpr const char* FW_VERSION = "1.0.0";

constexpr const char* OTA_REPO_OWNER = "Dud3z";
constexpr const char* OTA_REPO_NAME = "rangestick";
constexpr const char* OTA_REPO_BRANCH = "main";
} // namespace Version
