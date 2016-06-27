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

#include "shader.hpp"
#include <vector>

namespace GL
{
   Shader::Shader(const std::string& vertex_src, const std::string& fragment_src)
   {
      prog          = glCreateProgram();

      GLint status  = 0;
      GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_src);
      GLuint frag   = compile_shader(GL_FRAGMENT_SHADER, fragment_src);

      if (vertex)
         glAttachShader(prog, vertex);
      if (frag)
         glAttachShader(prog, frag);

      glLinkProgram(prog);
      glGetProgramiv(prog, GL_LINK_STATUS, &status);
      if (!status)
      {
         GLint len = 0;
         glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);

         if (len > 0)
         {
            std::vector<char> buf(len + 1);
            GLsizei out_len;
            glGetProgramInfoLog(prog, len, &out_len, &buf[0]);

            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "Link error: %s\n", &buf[0]);
         }
      }
   }

   GLuint Shader::compile_shader(GLenum type, const std::string& source)
   {
      GLint status    = 0;
      GLuint shader   = glCreateShader(type);
      const char* src = source.c_str();

      glShaderSource(shader, 1, &src, NULL);
      glCompileShader(shader);

      glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
      if (!status)
      {
         GLint len = 0;
         glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

         if (len > 0)
         {
            GLsizei out_len;
            std::vector<char> buf(len + 1);

            glGetShaderInfoLog(shader, len, &out_len, &buf[0]);

            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "Shader error: %s\n", &buf[0]);
         }

         glDeleteShader(shader);
         return 0;
      }

      return shader;
   }

   Shader::~Shader()
   {
      GLsizei count;
      GLsizei i;
      GLuint shaders[2];

      if (renderer_dead_state)
         return;

      glGetAttachedShaders(prog, 2, &count, shaders);

      for (i = 0; i < count; i++)
      {
         glDetachShader(prog, shaders[i]);
         glDeleteShader(shaders[i]);
      }

      glDeleteProgram(prog);
   }

   void Shader::use()
   {
      glUseProgram(prog);
   }

   void Shader::unbind()
   {
      glUseProgram(0);
   }

   GLint Shader::uniform(const char* sym)
   {
      std::map<std::string, GLint>::iterator itr = map.find(sym);

      if (itr == map.end())
      {
         GLint ret = glGetUniformLocation(prog, sym);
         map[sym]  = ret;
         return ret;
      }

      return itr->second;
   }

   GLint Shader::attrib(const char* sym)
   {
      std::map<std::string, GLint>::iterator itr = map.find(sym);

      if (itr == map.end())
      {
         GLint ret = glGetAttribLocation(prog, sym);
         map[sym]  = ret;
         return ret;
      }

      return itr->second;
   }
}

