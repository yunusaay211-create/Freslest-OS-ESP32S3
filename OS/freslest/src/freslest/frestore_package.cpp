#include "frestore_package.h"
#include "freslest_update_package.h"
#include <cJSON.h>
#include <cstring>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

namespace frestore {
namespace {
const cJSON *field(const cJSON *obj, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(obj, name);
}
bool is(const cJSON *obj, const char *name, const char *expected) {
    const char *value = cJSON_GetStringValue(field(obj, name));
    return value && !strcmp(value, expected);
}
bool number(const cJSON *obj, const char *name, uint32_t max, uint32_t &out) {
    const auto *value = field(obj, name);
    if (!cJSON_IsNumber(value) || !(value->valuedouble >= 1 && value->valuedouble <= max) ||
        value->valuedouble != value->valueint) return false;
    out = value->valueint;
    return true;
}
bool printable(const char *s, char *out, size_t capacity, bool empty = false) {
    if (!s) return false;
    size_t n = strlen(s);
    if (n >= capacity || (!n && !empty)) return false;
    for (size_t i = 0; i < n; ++i)
        if (s[i] < 32 || s[i] > 126) return false;
    memcpy(out, s, n + 1);
    return true;
}
bool text(const cJSON *obj, const char *name, char *out, size_t capacity) {
    return printable(cJSON_GetStringValue(field(obj, name)), out, capacity);
}
int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
bool hashField(const cJSON *obj, uint8_t (&out)[32]) {
    const char *s = cJSON_GetStringValue(field(obj, "sha256"));
    if (!s || strlen(s) != 64) return false;
    for (unsigned i = 0; i < 32; ++i) {
        int a = hexDigit(s[i * 2]), b = hexDigit(s[i * 2 + 1]);
        if (a < 0 || b < 0) return false;
        out[i] = (a << 4) | b;
    }
    return true;
}
bool publicKey(mbedtls_pk_context &pk, const char *key, uint8_t (&der)[128], size_t &length) {
    return !mbedtls_base64_decode(der, sizeof(der), &length,
               reinterpret_cast<const uint8_t *>(key), strlen(key)) &&
        !mbedtls_pk_parse_public_key(&pk, der, length) &&
        mbedtls_pk_can_do(&pk, MBEDTLS_PK_ECDSA) && mbedtls_pk_get_bitlen(&pk) == 256;
}
cJSON *verified(const char *data, size_t size, const char *key, const char *format) {
    const char *separator = static_cast<const char *>(memchr(data, '\n', size));
    if (!separator || separator == data || separator - data > 100 || !key[0]) return nullptr;
    const char *body = separator + 1;
    size_t bodyBytes = size - (body - data);
    if (!bodyBytes || memchr(body, 0, bodyBytes)) return nullptr;
    // Keep cJSON's C strings and the signed field bytes in agreement.
    for (size_t i = 0; i + 5 < bodyBytes; ++i)
        if (body[i] == '\\' && !memcmp(body + i, "\\u0000", 6)) return nullptr;
    uint8_t sig[80], pub[128], digest[32];
    size_t sigBytes = 0, pubBytes = 0;
    if (mbedtls_base64_decode(sig, sizeof(sig), &sigBytes,
            reinterpret_cast<const uint8_t *>(data), separator - data) ||
        mbedtls_sha256(reinterpret_cast<const uint8_t *>(body), bodyBytes, digest, 0)) return nullptr;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    const bool valid = publicKey(pk, key, pub, pubBytes) &&
        !mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, digest, sizeof(digest), sig, sigBytes);
    mbedtls_pk_free(&pk);
    if (!valid) return nullptr;
    const char *end = nullptr;
    cJSON *doc = cJSON_ParseWithLengthOpts(body, bodyBytes, &end, false);
    while (end && end < body + bodyBytes && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t')) ++end;
    uint32_t abi;
    if (!doc || end != body + bodyBytes || !is(doc, "format", format) ||
        !is(doc, "target", "esp32s3") || !number(doc, "abi", 1, abi)) {
        cJSON_Delete(doc);
        return nullptr;
    }
    return doc;
}
bool parseApp(const cJSON *item, App &app) {
    uint32_t category;
    const auto *dependencies = field(item, "dependencies");
    if (!cJSON_IsArray(dependencies) || cJSON_GetArraySize(dependencies)) return false;
    if (!text(item, "id", app.id, sizeof(app.id)) || !validId(app.id) ||
        !text(item, "name", app.name, sizeof(app.name)) ||
        !text(item, "developer", app.developer, sizeof(app.developer)) ||
        !text(item, "description", app.description, sizeof(app.description)) ||
        !number(item, "version", 999999, app.version) ||
        !number(item, "size", PackageBytes, app.size) ||
        !number(item, "category", CategoryCount, category) || !hashField(item, app.digest)) return false;
    app.category = category - 1;
    return true;
}
bool versionNumber(const char *text, uint64_t &value) {
    uint64_t parsed = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (*text < '0' || *text > '9' || (*text == '0' && text[1] >= '0' && text[1] <= '9'))
            return false;
        unsigned component = 0, digits = 0;
        while (*text >= '0' && *text <= '9') {
            if (++digits > 4) return false;
            component = component * 10 + (*text++ - '0');
        }
        parsed = parsed * 10000 + component;
        if (!*text) {
            if (i == 0) return false;
            value = i == 1 ? parsed * 10000 : parsed;
            return true;
        }
        if (*text++ != '.') return false;
    }
    return false;
}
} // namespace

bool parseOsUpdate(const char *data, size_t size, const char *key,
                   const char *profile, OsUpdate &update) {
    if (!data || !size || size > OsManifestBytes) return false;
    cJSON *doc = verified(data, size, key, "frestore-os-update-1");
    if (!doc) return false;
    OsUpdate parsed{};
    uint64_t release;
    const bool valid = is(doc, "layout", FRESLEST_UPDATE_LAYOUT) && is(doc, "profile", profile) &&
        text(doc, "version", parsed.version, sizeof(parsed.version)) && versionNumber(parsed.version, release) &&
        number(doc, "size", FRESLEST_OS_SLOT_SIZE, parsed.size) && parsed.size >= 288 &&
        hashField(doc, parsed.digest);
    cJSON_Delete(doc);
    if (valid) update = parsed;
    return valid;
}
bool newerOsVersion(const char *candidate, const char *running) {
    uint64_t next, current;
    return versionNumber(candidate, next) && versionNumber(running, current) && next > current;
}

bool validId(const char *id) {
    if (!id || !id[0] || strlen(id) > 24) return false;
    for (const char *p = id; *p; ++p)
        if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') && (*p != '-' || p == id)) return false;
    return true;
}
bool fingerprint(const char *key, char (&hex)[17]) {
    uint8_t der[128], digest[32];
    size_t length = 0;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    bool valid = key && *key && publicKey(pk, key, der, length) &&
                 !mbedtls_sha256(der, length, digest, 0);
    mbedtls_pk_free(&pk);
    if (!valid) return false;
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < 8; ++i) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 15];
    }
    hex[16] = 0;
    return true;
}
bool parseCatalog(const char *data, size_t size, const char *key, App (&apps)[MaxApps], unsigned &count) {
    if (!data || size > CatalogBytes) return false;
    cJSON *doc = verified(data, size, key, "frestore-catalog-1");
    if (!doc) return false;
    const auto *items = field(doc, "apps");
    int n = cJSON_GetArraySize(items);
    bool valid = cJSON_IsArray(items) && n <= (int)MaxApps;
    App parsed[MaxApps];
    const cJSON *item = items ? items->child : nullptr;
    for (int i = 0; valid && i < n; ++i, item = item->next) {
        valid = parseApp(item, parsed[i]);
        for (int j = 0; valid && j < i; ++j) valid = strcmp(parsed[i].id, parsed[j].id) != 0;
    }
    cJSON_Delete(doc);
    if (valid) {
        count = n;
        memcpy(apps, parsed, count * sizeof(App));
    }
    return valid;
}
bool parseCard(const char *data, size_t size, const char *key, Card &card) {
    if (!data || size > PackageBytes) return false;
    cJSON *doc = verified(data, size, key, "frestore-card-1");
    if (!doc) return false;
    const auto *lines = field(doc, "lines");
    int n = cJSON_GetArraySize(lines);
    bool valid = cJSON_IsArray(lines) && n > 0 && n <= (int)MaxLines &&
        text(doc, "id", card.id, sizeof(card.id)) && validId(card.id) &&
        text(doc, "name", card.name, sizeof(card.name)) &&
        number(doc, "version", 999999, card.version);
    const cJSON *line = lines ? lines->child : nullptr;
    for (int i = 0; valid && i < n; ++i, line = line->next)
        valid = printable(cJSON_GetStringValue(line), card.lines[i], sizeof(card.lines[i]), true);
    cJSON_Delete(doc);
    card.count = valid ? n : 0;
    return valid;
}
bool matches(const char *data, size_t size, const App &app) {
    uint8_t digest[32];
    return data && size == app.size &&
        !mbedtls_sha256(reinterpret_cast<const uint8_t *>(data), size, digest, 0) &&
        !memcmp(digest, app.digest, sizeof(digest));
}
} // namespace frestore
