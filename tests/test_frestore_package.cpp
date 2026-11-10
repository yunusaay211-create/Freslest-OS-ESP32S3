#include "frestore_package.h"
#include "frestore_starter.h"
#include <mbedtls/sha256.h>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

static std::string read(const char *file) {
    std::ifstream f(file, std::ios::binary);
    assert(f.good());
    return {std::istreambuf_iterator<char>(f), {}};
}

int main(int argc, char **argv) {
    assert(argc == 14);
    const auto key = read(argv[1]), catalog = read(argv[2]), package = read(argv[3]);
    const auto wrongTarget = read(argv[4]), wrongAbi = read(argv[5]), badDeps = read(argv[6]);
    frestore::App apps[frestore::MaxApps];
    unsigned count = 0;
    char fingerprint[17];
    assert(frestore::fingerprint(key.c_str(), fingerprint));
    assert(!frestore::fingerprint("broken", fingerprint));
    assert(frestore::parseCatalog(catalog.data(), catalog.size(), key.c_str(), apps, count) && count == 1);
    assert(std::string(apps[0].id) == "merhaba" && apps[0].version == 1);
    assert(frestore::matches(package.data(), package.size(), apps[0]));
    frestore::Card card;
    assert(frestore::parseCard(package.data(), package.size(), key.c_str(), card) && card.count == 6);
    assert(!apps[0].assetSize && !card.assetSize);
    assert(std::string(card.lines[0]) == "Freslest OS 1.5'e hos geldin!");
    auto damaged = package;
    damaged[damaged.size()-3] ^= 1;
    assert(!frestore::matches(damaged.data(), damaged.size(), apps[0]));
    assert(!frestore::parseCard(damaged.data(), damaged.size(), key.c_str(), card));
    assert(!frestore::parseCard(package.data(), package.size()-1, key.c_str(), card));
    assert(!frestore::parseCard(package.data(), package.size(), "", card));
    assert(!frestore::parseCard(wrongTarget.data(), wrongTarget.size(), key.c_str(), card));
    assert(!frestore::parseCard(wrongAbi.data(), wrongAbi.size(), key.c_str(), card));
    assert(!frestore::parseCatalog(badDeps.data(), badDeps.size(), key.c_str(), apps, count) && count == 1);
    assert(!frestore::validId("../sd") && !frestore::validId("X64") && frestore::validId("dosya-2"));
    const auto badLayout = read(argv[7]), manifest = read(argv[8]), image = read(argv[9]);
    frestore::OsUpdate update{};
    assert(frestore::parseOsUpdate(manifest.data(), manifest.size(), key.c_str(), "ili9488", update));
    uint8_t digest[32];
    assert(mbedtls_sha256(reinterpret_cast<const uint8_t *>(image.data()), image.size(), digest, 0) == 0);
    assert(update.size == image.size() && std::string(update.version) == "1.5.1" &&
           !memcmp(update.digest, digest, sizeof(digest)));
    assert(!frestore::parseOsUpdate(manifest.data(), manifest.size(), key.c_str(), "vga", update));
    assert(!frestore::parseOsUpdate(badLayout.data(), badLayout.size(), key.c_str(), "ili9488", update));
    auto changedManifest = manifest;
    changedManifest.back() ^= 1;
    assert(!frestore::parseOsUpdate(changedManifest.data(), changedManifest.size(), key.c_str(), "ili9488", update));
    assert(frestore::newerOsVersion("1.5.1", "1.5") && frestore::newerOsVersion("1.10", "1.9"));
    assert(!frestore::newerOsVersion("1.5", "1.5.0") && !frestore::newerOsVersion("1.4.9", "1.5") &&
           !frestore::newerOsVersion("1.05", "1.5"));

    const std::string starter = argv[10];
    assert(read((starter + "/publisher.pub").c_str()) == FreStoreStarter.key);
    const auto starterCatalog = read((starter + "/catalog").c_str());
    assert(frestore::parseCatalog(starterCatalog.data(), starterCatalog.size(), FreStoreStarter.key, apps, count) && count == 3);
    for (unsigned i = 0; i < count; ++i) {
        const auto &app = apps[i];
        const auto asset = read((starter + "/packages/" + app.id + "/" + std::to_string(app.version)).c_str());
        assert(frestore::matches(asset.data(), asset.size(), app));
        assert(frestore::parseCard(asset.data(), asset.size(), FreStoreStarter.key, card));
        assert(std::string(card.id) == app.id && std::string(card.name) == app.name && card.version == app.version);
        assert(!frestore::parseCard(asset.data(), asset.size(), key.c_str(), card));
    }
    const auto largeCatalog = read(argv[11]), largeCard = read(argv[12]), data = read(argv[13]);
    assert(frestore::parseCatalog(largeCatalog.data(), largeCatalog.size(), key.c_str(), apps, count) && count == 1);
    assert(apps[0].assetSize == data.size() && data.size() > frestore::InternalPackageLimit);
    assert(frestore::matches(largeCard.data(), largeCard.size(), apps[0]));
    assert(frestore::parseCard(largeCard.data(), largeCard.size(), key.c_str(), card));
    assert(card.assetSize == apps[0].assetSize);
    assert(mbedtls_sha256(reinterpret_cast<const uint8_t *>(data.data()), data.size(), digest, 0) == 0);
    assert(!memcmp(digest, card.assetDigest, sizeof(digest)));
}
