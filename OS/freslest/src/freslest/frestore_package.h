#pragma once
#include <stddef.h>
#include <stdint.h>

namespace frestore {
constexpr unsigned MaxApps = 16, CatalogBytes = 12288, PackageBytes = 4096;
constexpr uint32_t MaxAssetBytes = UINT32_MAX - PackageBytes;
constexpr uint32_t InternalPackageLimit = 500 * 1024;
constexpr unsigned OsManifestBytes = 768;
constexpr unsigned MaxLines = 20, LineBytes = 45;
struct App {
    char id[25], name[35], developer[30], description[97];
    uint32_t version, size, assetSize;
    uint8_t category, digest[32];
};
struct Card {
    char id[25], name[35];
    uint32_t version;
    uint32_t assetSize;
    uint8_t assetDigest[32];
    unsigned count;
    char lines[MaxLines][LineBytes + 1];
};
struct OsUpdate {
    char version[32];
    uint32_t size;
    uint8_t digest[32];
};
enum Category : uint8_t { Tools, Education, Media, CategoryCount };

bool parseCatalog(const char *data, size_t size, const char *key, App (&apps)[MaxApps], unsigned &count);
bool parseCard(const char *data, size_t size, const char *key, Card &card);
bool matches(const char *data, size_t size, const App &app);
bool validId(const char *id);
bool fingerprint(const char *key, char (&hex)[17]);
bool parseOsUpdate(const char *data, size_t size, const char *key,
                   const char *profile, OsUpdate &update);
bool newerOsVersion(const char *candidate, const char *running);
}
