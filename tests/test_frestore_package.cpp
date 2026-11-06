#include "frestore_package.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

static std::string read(const char *file) {
    std::ifstream f(file, std::ios::binary);
    assert(f.good());
    return {std::istreambuf_iterator<char>(f), {}};
}

int main(int argc, char **argv) {
    assert(argc == 7);
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
}
