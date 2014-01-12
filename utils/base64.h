#ifndef _LIBRETRO_BASE64_H
#define _LIBRETRO_BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

#include "boolean.h"

bool base64_encode(char *output, const uint8_t* input, unsigned inlength);
bool base64_decode(uint8_t *output, unsigned *outlength, const char *input);

#ifdef __cplusplus
}
#endif

#endif
