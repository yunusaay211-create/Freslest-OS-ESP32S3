#include "freslest_update_package.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static unsigned previous;
static void progress(unsigned value, void *context) {
    (void)context;
    assert(value >= previous && value <= 100);
    previous = value;
}

int main(int argc, char **argv) {
    assert(argc >= 3 && argc <= 6);
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 10;
    freslest_update_package package;
    char error[160] = {0};
    bool selected = false, output = false, vga = false, inspect_only = false;
    bool invalid_target = false, invalid_request = false;
    for (int i = 3; i < argc; ++i) {
        if (!strcmp(argv[i], "--select")) selected = true;
        else if (!strcmp(argv[i], "--lcd")) output = true;
        else if (!strcmp(argv[i], "--vga")) output = vga = true;
        else if (!strcmp(argv[i], "--inspect")) inspect_only = true;
        else if (!strcmp(argv[i], "--invalid-target")) invalid_target = true;
        else if (!strcmp(argv[i], "--invalid-request")) invalid_request = true;
        else assert(!"Unknown probe option");
    }
#if defined(FRESLEST_UPDATE_LEGACY_PROBE)
    (void)selected; (void)output; (void)vga; (void)invalid_target; (void)invalid_request;
    bool compatible = freslest_update_inspect(file, &package, error, sizeof(error));
#else
    enum freslest_update_target requested = !strcmp(argv[2], "os") ? FRESLEST_UPDATE_OS : FRESLEST_UPDATE_DOOM;
    if (invalid_request) requested = (enum freslest_update_target)255;
    bool compatible = output ? freslest_update_inspect_target_for_output(file, &package, requested, vga, error, sizeof(error)) :
        (selected || invalid_request) ? freslest_update_inspect_target(file, &package, requested, error, sizeof(error)) :
        freslest_update_inspect(file, &package, error, sizeof(error));
#endif
    if (!compatible) {
        fprintf(stderr, "REJECT INSPECT: %s\n", error);
        fclose(file);
        return 2;
    }
#if defined(FRESLEST_UPDATE_LEGACY_PROBE)
    const char *target = "os";
#else
    const char *target = package.target == FRESLEST_UPDATE_DOOM ? "doom" : "os";
    if (invalid_target)
        package.target = (enum freslest_update_target)255;
#endif
    if (strcmp(target, argv[2])) {
        fprintf(stderr, "WRONG TARGET: got %s expected %s\n", target, argv[2]);
        fclose(file);
        return 4;
    }
    if (inspect_only) {
        printf("COMPATIBLE %s %s\n", target, package.version);
        fclose(file);
        return 0;
    }
    uint8_t *image = malloc(package.image_size + 32);
    assert(image);
    memset(image, 0xa5, package.image_size + 32);
    bool ok = freslest_update_load(file, &package, image, package.image_size,
                                   progress, NULL, error, sizeof(error));
    for (unsigned i = 0; i < 32; ++i) assert(image[package.image_size + i] == 0xa5);
    fclose(file);
    free(image);
    if (!ok) {
        fprintf(stderr, "REJECT LOAD: %s\n", error);
        return 3;
    }
    assert(previous == 100);
    printf("VERIFIED %s %s %u\n", target, package.version, package.image_size);
    return 0;
}
