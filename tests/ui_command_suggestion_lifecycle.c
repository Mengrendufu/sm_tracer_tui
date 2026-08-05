#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <notcurses/notcurses.h>
#include "widgets/command_suggestion.h"

static bool cellEquals_(struct ncplane * const plane,
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

static uint64_t cellChannels_(struct ncplane * const plane,
                              int const y,
                              int const x)
{
    uint64_t channels = 0U;
    char * const contents = ncplane_at_yx(
        plane, y, x, (uint16_t *)0, &channels);
    free(contents);
    return channels;
}

static bool cellIsBlank_(struct ncplane * const plane,
                         int const y,
                         int const x)
{
    char * const contents = ncplane_at_yx(
        plane, y, x, (uint16_t *)0, (uint64_t *)0);
    bool const blank = contents != (char *)0
                       && (contents[0] == '\0'
                           || strcmp(contents, " ") == 0);
    free(contents);
    return blank;
}

static bool geometryEquals_(struct ncplane * const plane,
                            int const expectedY,
                            int const expectedX,
                            unsigned const expectedRows,
                            unsigned const expectedCols)
{
    int y;
    int x;
    unsigned rows;
    unsigned cols;
    ncplane_yx(plane, &y, &x);
    ncplane_dim_yx(plane, &rows, &cols);
    return y == expectedY && x == expectedX
           && rows == expectedRows && cols == expectedCols;
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

    struct CommandSuggestion suggestion;
    CommandSuggestion_init(&suggestion);
    if (suggestion.plane != (struct ncplane *)0) {
        notcurses_stop(nc);
        fclose(output);
        return 3;
    }

    CommandSuggestion_create(&suggestion, notcurses_stdplane(nc));
    if (suggestion.plane == (struct ncplane *)0
        || ncplane_below(suggestion.plane) != (struct ncplane *)0)
    {
        CommandSuggestion_destroy(&suggestion);
        notcurses_stop(nc);
        fclose(output);
        return 4;
    }

    char const * const candidates[] = {
        "connect", "disconnect", "refresh", "send",
        "settings", "status", "trace"
    };
    CommandSuggestion_show(&suggestion, 8, 20U, candidates,
                           sizeof(candidates) / sizeof(candidates[0]), 2U);

    if (!geometryEquals_(suggestion.plane, 2, 2, 6U, 12U)
        || !cellEquals_(suggestion.plane, 0, 1, "c")
        || !cellEquals_(suggestion.plane, 0, 7, "t")
        || !cellEquals_(suggestion.plane, 5, 1, "s")
        || cellChannels_(suggestion.plane, 1, 0)
           == cellChannels_(suggestion.plane, 2, 0)
        || ncplane_above(suggestion.plane) != (struct ncplane *)0)
    {
        CommandSuggestion_destroy(&suggestion);
        notcurses_stop(nc);
        fclose(output);
        return 5;
    }

    CommandSuggestion_show(&suggestion, 8, 20U, candidates,
                           sizeof(candidates) / sizeof(candidates[0]), 6U);
    CommandSuggestion_show(&suggestion, 8, 20U, candidates,
                           sizeof(candidates) / sizeof(candidates[0]), 5U);
    if (!cellEquals_(suggestion.plane, 0, 1, "d")
        || !cellEquals_(suggestion.plane, 4, 1, "s")
        || !cellEquals_(suggestion.plane, 5, 1, "t")
        || cellChannels_(suggestion.plane, 3, 0)
           == cellChannels_(suggestion.plane, 4, 0)
        || cellChannels_(suggestion.plane, 4, 0)
           == cellChannels_(suggestion.plane, 5, 0))
    {
        CommandSuggestion_destroy(&suggestion);
        notcurses_stop(nc);
        fclose(output);
        return 6;
    }

    char const * const narrow[] = {"open", "ports"};
    CommandSuggestion_show(&suggestion, 5, 6U, narrow, 2U, 1U);
    if (!geometryEquals_(suggestion.plane, 3, 2, 2U, 6U)
        || !cellEquals_(suggestion.plane, 0, 1, "o")
        || !cellEquals_(suggestion.plane, 0, 4, "n")
        || !cellEquals_(suggestion.plane, 1, 1, "p")
        || !cellEquals_(suggestion.plane, 1, 5, "s")
        || cellChannels_(suggestion.plane, 0, 0)
           == cellChannels_(suggestion.plane, 1, 0))
    {
        CommandSuggestion_destroy(&suggestion);
        notcurses_stop(nc);
        fclose(output);
        return 7;
    }

    CommandSuggestion_hide(&suggestion);
    if (ncplane_below(suggestion.plane) != (struct ncplane *)0
        || !cellIsBlank_(suggestion.plane, 0, 0))
    {
        CommandSuggestion_destroy(&suggestion);
        notcurses_stop(nc);
        fclose(output);
        return 8;
    }

    CommandSuggestion_show(&suggestion, 0, 6U, narrow, 2U, 0U);
    if (ncplane_below(suggestion.plane) != (struct ncplane *)0) {
        CommandSuggestion_destroy(&suggestion);
        notcurses_stop(nc);
        fclose(output);
        return 9;
    }

    CommandSuggestion_destroy(&suggestion);
    if (suggestion.plane != (struct ncplane *)0) {
        notcurses_stop(nc);
        fclose(output);
        return 10;
    }

    int const result = notcurses_stop(nc);
    fclose(output);
    return result == 0 ? 0 : 11;
}
