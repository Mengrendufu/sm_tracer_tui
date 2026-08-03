#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <notcurses/notcurses.h>
#include "widgets/input_composer.h"

static int resizeCb_(struct ncplane * const plane) {
    (void)plane;
    return 0;
}

static bool cellEqualsYx_(struct ncplane * const plane,
                          int const y,
                          int const x,
                          char const * const expected)
{
    char * const contents = ncplane_at_yx(
        plane, y, x, (uint16_t *)0, (uint64_t *)0);
    bool const equals = contents != (char *)0
                        && strcmp(contents, expected) == 0;
    free(contents);
    return equals;
}

static bool cellEquals_(struct ncplane * const plane,
                        int const x,
                        char const * const expected)
{
    return cellEqualsYx_(plane, 0, x, expected);
}

static bool cellHasCursorChannels_(struct ncplane * const plane,
                                   int const x)
{
    uint64_t channels = 0U;
    char * const contents = ncplane_at_yx(
        plane, 0, x, (uint16_t *)0, &channels);
    free(contents);
    return channels == ncchannels_reverse(ncplane_channels(plane));
}

static bool cellHasCursorChannelsYx_(struct ncplane * const plane,
                                     int const y,
                                     int const x)
{
    uint64_t channels = 0U;
    char * const contents = ncplane_at_yx(
        plane, y, x, (uint16_t *)0, &channels);
    free(contents);
    return channels == ncchannels_reverse(ncplane_channels(plane));
}

static uint64_t cellChannelsYx_(struct ncplane * const plane,
                                int const y,
                                int const x)
{
    uint64_t channels = 0U;
    char * const contents = ncplane_at_yx(
        plane, y, x, (uint16_t *)0, &channels);
    free(contents);
    return channels;
}

static bool cellFgEqualsYx_(struct ncplane * const plane,
                            int const y,
                            int const x,
                            unsigned const expectedR,
                            unsigned const expectedG,
                            unsigned const expectedB)
{
    uint64_t const channels = cellChannelsYx_(plane, y, x);
    unsigned r;
    unsigned g;
    unsigned b;
    (void)ncchannels_fg_rgb8(channels, &r, &g, &b);
    return ncchannels_fg_rgb_p(channels)
           && r == expectedR && g == expectedG && b == expectedB;
}

static bool cellBgEqualsYx_(struct ncplane * const plane,
                            int const y,
                            int const x,
                            unsigned const expectedR,
                            unsigned const expectedG,
                            unsigned const expectedB)
{
    uint64_t const channels = cellChannelsYx_(plane, y, x);
    unsigned r;
    unsigned g;
    unsigned b;
    (void)ncchannels_bg_rgb8(channels, &r, &g, &b);
    return ncchannels_bg_rgb_p(channels)
           && r == expectedR && g == expectedG && b == expectedB;
}

static bool cellIsWideLeft_(struct ncplane * const plane, int const x) {
    nccell cell = NCCELL_TRIVIAL_INITIALIZER;
    int const loaded = ncplane_at_yx_cell(plane, 0, x, &cell);
    bool const isWideLeft = loaded > 0 && nccell_wide_left_p(&cell);
    nccell_release(plane, &cell);
    return isWideLeft;
}

static bool cellIsWideRight_(struct ncplane * const plane, int const x) {
    nccell cell = NCCELL_TRIVIAL_INITIALIZER;
    int const loaded = ncplane_at_yx_cell(plane, 0, x, &cell);
    bool const isWideRight = loaded >= 0 && nccell_wide_right_p(&cell);
    nccell_release(plane, &cell);
    return isWideRight;
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

    InputComposer_showCursor(&composer, "", 0U, 0U);
    if (notcurses_render(nc) != 0
        || !composer.cursorVisible
        || !cellHasCursorChannels_(composer.plane, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 3;
    }

    InputComposer_projectFrom(&composer, "a", 1U, 1U, 0U);

    if (!cellEquals_(composer.plane, 2, "a")
        || cellHasCursorChannels_(composer.plane, 2)
        || !cellHasCursorChannels_(composer.plane, 3))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 4;
    }

    InputComposer_projectAll(&composer, "", 0U, 0U);
    if (!cellEquals_(composer.plane, 2, " ")
        || !cellHasCursorChannels_(composer.plane, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 5;
    }

    InputComposer_projectAll(&composer, "AB", 2U, 1U);
    if (!cellEquals_(composer.plane, 2, "A")
        || !cellEquals_(composer.plane, 3, "B")
        || cellHasCursorChannels_(composer.plane, 2)
        || !cellHasCursorChannels_(composer.plane, 3))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 6;
    }
    InputComposer_hideCursor(&composer, "AB", 2U, 1U);
    if (composer.cursorVisible
        || !cellEquals_(composer.plane, 3, "B")
        || cellHasCursorChannels_(composer.plane, 3))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 7;
    }
    InputComposer_showCursor(&composer, "AB", 2U, 1U);

    InputComposer_projectFrom(&composer, "AXB", 3U, 2U, 1U);
    if (!cellEquals_(composer.plane, 2, "A")
        || !cellEquals_(composer.plane, 3, "X")
        || !cellEquals_(composer.plane, 4, "B")
        || !cellHasCursorChannels_(composer.plane, 4))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 8;
    }

    InputComposer_projectFrom(&composer, "AB", 2U, 1U, 1U);
    if (!cellEquals_(composer.plane, 2, "A")
        || !cellEquals_(composer.plane, 3, "B")
        || !cellEquals_(composer.plane, 4, " ")
        || !cellHasCursorChannels_(composer.plane, 3))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 9;
    }

    InputComposer_resize(&composer, 0, 2U, 8U);
    InputComposer_projectAll(&composer, "abcdef", 6U, 6U);
    if (composer.viewRow != 0U
        || composer.cursorRow != 1U
        || composer.cursorCol != 0U
        || !cellEquals_(composer.plane, 2, "a")
        || !cellEquals_(composer.plane, 7, "f")
        || !cellHasCursorChannelsYx_(composer.plane, 1, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 10;
    }

    InputComposer_moveCursor(&composer, "abcdef", 6U, 0U);
    if (composer.viewRow != 0U
        || composer.cursorRow != 0U
        || composer.cursorCol != 0U
        || !cellEquals_(composer.plane, 2, "a")
        || !cellHasCursorChannels_(composer.plane, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 11;
    }

    InputComposer_projectAll(&composer, "abcdefghij", 10U, 0U);
    if (composer.viewRow != 0U
        || composer.cursorRow != 0U
        || composer.cursorCol != 0U
        || !cellEquals_(composer.plane, 2, "a")
        || !cellEquals_(composer.plane, 7, "f")
        || !cellHasCursorChannels_(composer.plane, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 12;
    }

    char const wideText[] = "\xE4\xBD\xA0" "a";
    InputComposer_projectAll(&composer, wideText,
                             sizeof(wideText) - 1U, 0U);
    if (!cellIsWideLeft_(composer.plane, 2)
        || !cellIsWideRight_(composer.plane, 3)
        || !cellHasCursorChannels_(composer.plane, 2)
        || !cellHasCursorChannels_(composer.plane, 3))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 13;
    }
    InputComposer_hideCursor(&composer, wideText,
                             sizeof(wideText) - 1U, 0U);
    if (!cellIsWideLeft_(composer.plane, 2)
        || !cellIsWideRight_(composer.plane, 3)
        || cellHasCursorChannels_(composer.plane, 2)
        || cellHasCursorChannels_(composer.plane, 3))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 14;
    }
    InputComposer_showCursor(&composer, wideText,
                             sizeof(wideText) - 1U, 0U);
    InputComposer_projectAll(&composer, wideText,
                             sizeof(wideText) - 1U, 3U);
    if (composer.viewRow != 0U
        || composer.cursorRow != 0U
        || composer.cursorCol != 2U
        || !cellHasCursorChannels_(composer.plane, 4))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 15;
    }

    char const wideEdge[] = "a" "\xE4\xBD\xA0";
    InputComposer_resize(&composer, 0, 2U, 5U);
    InputComposer_projectAll(&composer, wideEdge,
                             sizeof(wideEdge) - 1U,
                             sizeof(wideEdge) - 1U);
    if (composer.viewRow != 0U
        || composer.cursorRow != 1U
        || composer.cursorCol != 0U
        || !cellEquals_(composer.plane, 3, "\xE4\xBD\xA0")
        || !cellHasCursorChannelsYx_(composer.plane, 1, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 16;
    }

    InputComposer_hideCursor(&composer, wideEdge,
                             sizeof(wideEdge) - 1U,
                             sizeof(wideEdge) - 1U);
    if (composer.cursorVisible
        || cellHasCursorChannelsYx_(composer.plane, 1, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 17;
    }

    InputComposer_showCursor(&composer, wideText,
                             sizeof(wideText) - 1U, 0U);
    InputComposer_resize(&composer, 0, 2U, 3U);
    InputComposer_projectAll(&composer, wideText,
                             sizeof(wideText) - 1U, 0U);
    if (!cellEquals_(composer.plane, 2, " ")
        || !cellHasCursorChannels_(composer.plane, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 18;
    }

    char const combiningText[] = "e\xCC\x81";
    InputComposer_resize(&composer, 0, 1U, 8U);
    InputComposer_projectAll(&composer, combiningText,
                             sizeof(combiningText) - 1U, 1U);
    if (composer.cursorCol != 1U
        || !cellEquals_(composer.plane, 3, " ")
        || !cellHasCursorChannels_(composer.plane, 3))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 19;
    }

    char const combiningEdge[] = "abcde" "e\xCC\x81";
    InputComposer_resize(&composer, 0, 2U, 8U);
    InputComposer_projectAll(&composer, combiningEdge,
                             sizeof(combiningEdge) - 1U, 6U);
    if (InputComposer_preferredRows(&composer, combiningEdge,
                                    sizeof(combiningEdge) - 1U,
                                    6U, 8U) != 2U
        || composer.cursorRow != 1U
        || composer.cursorCol != 0U
        || !cellHasCursorChannelsYx_(composer.plane, 1, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 20;
    }

    char const wrappedText[] =
        "abcdefghijklmnopqrstuvwxyz1234";
    if (InputComposer_preferredRows(&composer, wrappedText,
                                    sizeof(wrappedText) - 1U,
                                    sizeof(wrappedText) - 1U,
                                    8U) != 4U
        || InputComposer_preferredRows(&composer, wrappedText,
                                       sizeof(wrappedText) - 1U,
                                       sizeof(wrappedText) - 1U,
                                       14U) != 3U)
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 21;
    }

    InputComposer_resize(&composer, 0, 4U, 8U);
    InputComposer_projectAll(&composer, wrappedText,
                             sizeof(wrappedText) - 1U,
                             sizeof(wrappedText) - 1U);
    if (composer.viewRow != 2U
        || composer.cursorRow != 3U
        || composer.cursorCol != 0U
        || !cellEqualsYx_(composer.plane, 0, 0, " ")
        || !cellEqualsYx_(composer.plane, 0, 2, "m")
        || !cellHasCursorChannelsYx_(composer.plane, 3, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 22;
    }

    InputComposer_setLimitReached(&composer, true);
    InputComposer_projectAll(&composer, wrappedText,
                             sizeof(wrappedText) - 1U,
                             sizeof(wrappedText) - 1U);
    if (composer.viewRow != 2U
        || !cellEqualsYx_(composer.plane, 0, 0, "!"))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 23;
    }
    InputComposer_setLimitReached(&composer, false);

    InputComposer_moveCursor(&composer, wrappedText,
                             sizeof(wrappedText) - 1U, 0U);
    if (composer.viewRow != 0U
        || composer.cursorRow != 0U
        || composer.cursorCol != 0U
        || !cellEqualsYx_(composer.plane, 0, 2, "a")
        || !cellHasCursorChannelsYx_(composer.plane, 0, 2))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 24;
    }

    InputComposer_resize(&composer, 0, 1U, 12U);
    InputComposer_projectAll(&composer, "abc", 3U, 3U);
    uint64_t const normalCursorChannels =
        cellChannelsYx_(composer.plane, 0, 5);
    InputComposer_setLimitReached(&composer, true);
    InputComposer_projectAll(&composer, "abc", 3U, 3U);
    uint64_t const warningCursorChannels =
        cellChannelsYx_(composer.plane, 0, 5);
    if (!cellEquals_(composer.plane, 0, "!")
        || warningCursorChannels == normalCursorChannels)
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 25;
    }

    InputComposer_setLimitReached(&composer, false);
    InputComposer_projectAll(&composer, "abc", 3U, 3U);
    if (!cellEquals_(composer.plane, 0, ">")
        || cellChannelsYx_(composer.plane, 0, 5)
               != normalCursorChannels)
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 26;
    }

    char const tokenText[] = "$connect plain";
    InputComposer_resize(&composer, 0, 2U, 16U);
    InputComposer_projectAll(&composer, tokenText,
                             sizeof(tokenText) - 1U,
                             sizeof(tokenText) - 1U);
    uint64_t const plainTokenChannels =
        cellChannelsYx_(composer.plane, 0, 2);
    uint64_t const plainTextChannels =
        cellChannelsYx_(composer.plane, 0, 11);
    uint64_t const separatorChannels =
        cellChannelsYx_(composer.plane, 0, 10);
    uint64_t const cursorChannels = cellChannelsYx_(
        composer.plane, (int)composer.cursorRow,
        (int)(2U + composer.cursorCol));
    InputComposer_highlightToken(&composer, tokenText,
                                 sizeof(tokenText) - 1U,
                                 sizeof(tokenText) - 1U,
                                 0U, 8U);
    if (cellChannelsYx_(composer.plane, 0, 2) == plainTokenChannels
        || !cellFgEqualsYx_(composer.plane, 0, 2,
                            95U, 165U, 255U)
        || cellChannelsYx_(composer.plane, 0, 10)
               != separatorChannels
        || cellChannelsYx_(composer.plane, 0, 11)
               != plainTextChannels
        || cellChannelsYx_(composer.plane, (int)composer.cursorRow,
                           (int)(2U + composer.cursorCol))
               != cursorChannels)
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 27;
    }

    InputComposer_projectAll(&composer, tokenText,
                             sizeof(tokenText) - 1U, 9U);
    uint64_t const embeddedCursorChannels =
        cellChannelsYx_(composer.plane, 0, 11);
    InputComposer_highlightToken(&composer, tokenText,
                                 sizeof(tokenText) - 1U, 9U,
                                 0U, 8U);
    if (cellChannelsYx_(composer.plane, 0, 11)
            != embeddedCursorChannels)
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 28;
    }

    InputComposer_projectAll(&composer, tokenText,
                             sizeof(tokenText) - 1U, 0U);
    InputComposer_highlightToken(&composer, tokenText,
                                 sizeof(tokenText) - 1U, 0U,
                                 0U, 8U);
    if (!cellBgEqualsYx_(composer.plane, 0, 2,
                         95U, 165U, 255U))
    {
        InputComposer_destroy(&composer);
        notcurses_stop(nc);
        fclose(output);
        return 29;
    }

    InputComposer_destroy(&composer);

    int const result = notcurses_stop(nc);
    fclose(output);
    return result == 0 ? 0 : 30;
}
