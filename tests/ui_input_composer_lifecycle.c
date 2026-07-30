#include <stdio.h>
#include <notcurses/notcurses.h>
#include "widgets/input_composer.h"

static int resizeCb_(struct ncplane * const plane) {
    (void)plane;
    return 0;
}

int main(void) {
    struct notcurses_options const opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS
               | NCOPTION_NO_ALTERNATE_SCREEN
               | NCOPTION_NO_FONT_CHANGES
    };
    FILE * const output = tmpfile();
    if (output == (FILE *)0) {
        return 1;
    }

    struct notcurses * const nc = notcurses_core_init(&opts, output);
    if (nc == (struct notcurses *)0) {
        fclose(output);
        return 2;
    }

    struct InputComposer composer;
    InputComposer_init(&composer);
    InputComposer_create(&composer, notcurses_stdplane(nc), &composer,
                         0, 32U, &resizeCb_);
    InputComposer_destroy(&composer);

    int const result = notcurses_stop(nc);
    fclose(output);
    return result == 0 ? 0 : 3;
}
