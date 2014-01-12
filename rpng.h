/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RPNG_H__
#define RPNG_H__

#include <stdint.h>
#include "shared.hpp"

// Modified version of RetroArch's PNG loader.
// Uses bottom-left origin rather than top-left.
// Also outputs RGBA byte order to work on GLES without extensions (GL_RGBA, GL_UNSIGNED_BYTE).

#ifdef __cplusplus
extern "C" {
#endif

bool rpng_load_image_rgba(const char *path, uint8_t **data, unsigned *width, unsigned *height);

#ifdef __cplusplus
}
#endif

#endif

