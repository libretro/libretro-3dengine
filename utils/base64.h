/*
 *  Libretro 3DEngine
 *  Copyright (C) 2013-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2013-2014 - Daniel De Matteis
 *
 *  InstancingViewer is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  InstancingViewer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with InstancingViewer.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
