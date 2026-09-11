#include "susamune/lm_colour_codec.h"

__declspec(dllexport) void colour_block(unsigned char *out,
    const unsigned char *source, unsigned int r, unsigned int g, unsigned int b) {
    LmColourBlock(out, source, r, g, b);
}
