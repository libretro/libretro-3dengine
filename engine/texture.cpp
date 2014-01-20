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

#include "texture.hpp"
#include "rpng.h"
#include "util.hpp"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef GLES
#include "gli/gli.hpp"
#include "gli/gtx/gl_texture2d.hpp"
#endif

using namespace std;
using namespace std1;

static bool texture_image_load_tga(const char *path,
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

namespace GL
{
   Texture::Texture() : tex(0)
   {}

   void Texture::upload_data(const void* data, unsigned width, unsigned height,
         bool generate_mipmap)
   {
      if (!tex)
         SYM(glGenTextures)(1, &tex);

      bind();

      SYM(glTexImage2D)(GL_TEXTURE_2D,
            0, GL_RGBA, width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE,
            data);

      if (generate_mipmap)
      {
         SYM(glGenerateMipmap)(GL_TEXTURE_2D);
         SYM(glTexParameteri)(GL_TEXTURE_2D,
               GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         SYM(glTexParameteri)(GL_TEXTURE_2D,
               GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#ifndef GLES
         GLint max = 0.0f;
         SYM(glGetIntegerv)(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max);
         if (log_cb)
            log_cb(RETRO_LOG_INFO, "Max anisotropy: %d.\n", max);
         SYM(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max);
#endif
      }
      else
      {
         SYM(glTexParameteri)(GL_TEXTURE_2D,
               GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         SYM(glTexParameteri)(GL_TEXTURE_2D,
               GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      }

      unbind();
   }

#ifndef GLES
   void Texture::load_dds(const std::string& path)
   {
      if (!tex)
         SYM(glGenTextures)(1, &tex);

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Loading DDS: %s.\n", path.c_str());
      unsigned levels = 0;
      tex = gli::createTexture2D(path, &levels);

      bind();

      if (levels == 1)
      {
         SYM(glGenerateMipmap)(GL_TEXTURE_2D);
         SYM(glTexParameteri)(GL_TEXTURE_2D,
               GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         SYM(glTexParameteri)(GL_TEXTURE_2D,
               GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      }

      GLint max = 0.0f;
      SYM(glGetIntegerv)(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max);

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Max anisotropy: %d.\n", max);

      SYM(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max);
      unbind();
   }
#endif

   Texture::Texture(const std::string& path) : tex(0)
   {
      uint8_t* data = NULL;
      unsigned width = 0, height = 0;

      string ext = Path::ext(path);

#ifndef GLES
      if (ext == "dds")
         load_dds(path);
      else
#endif
      {
         bool ret = false;
         if (ext == "png")
         {
            ret = rpng_load_image_rgba(path.c_str(),
                  &data, &width, &height);
         }
         else if (ext == "tga")
         {
            ret = texture_image_load_tga(path.c_str(),
                  data, width, height);
         }
         else if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Unrecognized extension: \"%s\"\n", ext.c_str());

         if (ret)
         {
            upload_data(data, width, height, true);
            free(data);
         }
         else if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Failed to load image: %s\n", path.c_str());
      }
   }

   Texture::~Texture()
   {
      if (dead_state)
         return;

      if (tex)
         SYM(glDeleteTextures)(1, &tex);
   }

   void Texture::bind(unsigned unit)
   {
      SYM(glActiveTexture)(GL_TEXTURE0 + unit);
      SYM(glBindTexture)(GL_TEXTURE_2D, tex);
      SYM(glActiveTexture)(GL_TEXTURE0);
   }

   std1::shared_ptr<Texture> Texture::blank()
   {
      std1::shared_ptr<Texture> tex(new Texture);
      uint32_t data = -1;
      tex->upload_data(&data, 1, 1, false);
      return tex;
   }

   void Texture::unbind(unsigned unit)
   {
      SYM(glActiveTexture)(GL_TEXTURE0 + unit);
      SYM(glBindTexture)(GL_TEXTURE_2D, 0);
      SYM(glActiveTexture)(GL_TEXTURE0);
   }
}

