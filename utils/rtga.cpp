/*
 *  Libretro 3DEngine
 *  Copyright (C) 2013-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2013-2014 - Daniel De Matteis
 *
 *  Libretro 3DEngine is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  Libretro 3DEngine is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with Libretro 3DEngine.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rtga.h"
#include "shared.hpp"

bool texture_image_load_tga(const char *path,
      uint8_t*& data, unsigned& width, unsigned& height)
{
   FILE *file = fopen(path, "rb");
   if (!file)
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "Failed to open image: %s.\n", path);
      return false;
   }

   fseek(file, 0, SEEK_END);
   long len = ftell(file);
   rewind(file);

   uint8_t* buffer = (uint8_t*)malloc(len);
   if (!buffer)
   {
      fclose(file);
      return false;
   }

   fread(buffer, 1, len, file);
   fclose(file);

   if (buffer[2] != 2) // Uncompressed RGB
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "TGA image is not uncompressed RGB.\n");
      free(buffer);
      return false;
   }

   uint8_t info[6];
   memcpy(info, buffer + 12, 6);

   width = info[0] + ((unsigned)info[1] * 256);
   height = info[2] + ((unsigned)info[3] * 256);
   unsigned bits = info[4];

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Loaded TGA: (%ux%u @ %u bpp)\n", width, height, bits);

   unsigned size = width * height * sizeof(uint32_t);
   data = (uint8_t*)malloc(size);
   if (!data)
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "Failed to allocate TGA pixels.\n");
      free(buffer);
      return false;
   }

   const uint8_t *tmp = buffer + 18;
   if (bits == 32)
   {
      for (unsigned i = 0; i < width * height; i++)
      {
         data[i * 4 + 2] = tmp[i * 4 + 0];
         data[i * 4 + 1] = tmp[i * 4 + 1];
         data[i * 4 + 0] = tmp[i * 4 + 2];
         data[i * 4 + 3] = tmp[i * 4 + 3];
      }
   }
   else if (bits == 24)
   {
      for (unsigned i = 0; i < width * height; i++)
      {
         data[i * 4 + 2] = tmp[i * 3 + 0];
         data[i * 4 + 1] = tmp[i * 3 + 1];
         data[i * 4 + 0] = tmp[i * 3 + 2];
         data[i * 4 + 3] = 0xff;
      }
   }
   else
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "Bit depth of TGA image is wrong. Only 32-bit and 24-bit supported.\n");
      free(buffer);
      free(data);
      return false;
   }

   free(buffer);
   return true;
}
