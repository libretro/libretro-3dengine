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

#ifndef TEXTURE_HPP__
#define TEXTURE_HPP__

#include "gl.hpp"

namespace GL
{
   class Texture
   {
      public:
         Texture(const std::string& path);
         Texture();
         ~Texture();

         void bind(unsigned unit = 0);
         static void unbind(unsigned unit = 0);

         void load_dds(const std::string& path);

         static std1::shared_ptr<Texture> blank();
         void upload_data(const void* data, unsigned width, unsigned height,
               bool generate_mipmap);

      private:
         GLuint tex;
   };
}

#endif

