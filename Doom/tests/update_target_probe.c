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
    assert(argc == 3 || argc == 4);
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 10;
    freslest_update_package package;
    char error[160] = {0};
    if (!freslest_update_inspect(file, &package, error, sizeof(error))) {
        fprintf(stderr, "REJECT INSPECT: %s\n", error);
        fclose(file);
        return 2;
    }
#if defined(FRESLEST_UPDATE_LEGACY_PROBE)
    const char *target = "os";
#else
    const char *target = package.target == FRESLEST_UPDATE_DOOM ? "doom" : "os";
    if (argc == 4 && !strcmp(argv[3], "--invalid-target"))
        package.target = (enum freslest_update_target)255;
#endif
    if (strcmp(target, argv[2])) {
        fprintf(stderr, "WRONG TARGET: got %s expected %s\n", target, argv[2]);
        fclose(file);
        return 4;
    }
    if (argc == 4 && !strcmp(argv[3], "--inspect")) {
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
