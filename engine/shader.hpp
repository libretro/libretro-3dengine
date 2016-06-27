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

#ifndef SHADER_HPP__
#define SHADER_HPP__

#include "gl.hpp"
#include <map>

namespace GL
{
   class Shader
   {
      public:
         Shader(const std::string& vertex, const std::string& fragment);
         ~Shader();
         void use();

         static void unbind();

         GLint uniform(const char* sym);
         GLint attrib(const char* sym);

      private:
         GLuint prog;
         std::map<std::string, GLint> map;

         GLuint compile_shader(GLenum type, const std::string& source);
   };
}

#endif

