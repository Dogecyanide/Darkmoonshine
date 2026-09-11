#ifndef SUSAMUNE_LM_COLOUR_CODEC_H
#define SUSAMUNE_LM_COLOUR_CODEC_H

static inline unsigned int LmColour565(unsigned int colour,
    unsigned int red, unsigned int green, unsigned int blue) {
    unsigned int r = (colour >> 11) * 255 / 31;
    unsigned int g = ((colour >> 5) & 63) * 255 / 63;
    unsigned int b = (colour & 31) * 255 / 31;
    if (g <= 8 || g * 5 <= r * 6 || g * 5 <= b * 6) return colour;
    r = red * g / 255;
    b = blue * g / 255;
    g = green * g / 255;
    return ((r * 31 + 127) / 255 << 11) |
           ((g * 63 + 127) / 255 << 5) | ((b * 31 + 127) / 255);
}

/* GX CMPR stores two BE RGB565 endpoints, then sixteen two-bit selectors.
 * Endpoint order selects opaque-four-colour versus transparent-three-colour.
 * Keep that mode and remap selectors whenever the endpoints are exchanged. */
static inline void LmColourBlock(unsigned char *out, const unsigned char *source,
    unsigned int red, unsigned int green, unsigned int blue) {
    unsigned int old0 = (unsigned int)source[0] << 8 | source[1];
    unsigned int old1 = (unsigned int)source[2] << 8 | source[3];
    unsigned int first = LmColour565(old0, red, green, blue);
    unsigned int second = LmColour565(old1, red, green, blue);
    unsigned int selectors = (unsigned int)source[4] << 24 |
        (unsigned int)source[5] << 16 | (unsigned int)source[6] << 8 | source[7];
    if (old0 > old1) {
        if (first < second) {
            unsigned int swap = first; first = second; second = swap;
            selectors ^= 0x55555555u;
        } else if (first == second) {
            if (first < 65535u) {
                ++first;
                selectors = 0x55555555u;
            } else {
                --second;
                selectors = 0u;
            }
        }
    } else if (first > second) {
        unsigned int swap = first; first = second; second = swap;
        selectors ^= (~selectors >> 1) & 0x55555555u;
    }
    out[0] = (unsigned char)(first >> 8);
    out[1] = (unsigned char)first;
    out[2] = (unsigned char)(second >> 8);
    out[3] = (unsigned char)second;
    out[4] = (unsigned char)(selectors >> 24);
    out[5] = (unsigned char)(selectors >> 16);
    out[6] = (unsigned char)(selectors >> 8);
    out[7] = (unsigned char)selectors;
}

#endif
