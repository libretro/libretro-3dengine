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

#include "shader.hpp"
#include <vector>

namespace GL
{
   Shader::Shader(const std::string& vertex_src, const std::string& fragment_src)
   {
      prog = SYM(glCreateProgram)();

      GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_src);
      GLuint frag = compile_shader(GL_FRAGMENT_SHADER, fragment_src);

      if (vertex)
         SYM(glAttachShader)(prog, vertex);
      if (frag)
         SYM(glAttachShader)(prog, frag);

      SYM(glLinkProgram)(prog);
      GLint status = 0;
      SYM(glGetProgramiv)(prog, GL_LINK_STATUS, &status);
      if (!status)
      {
         GLint len = 0;
         SYM(glGetProgramiv)(prog, GL_INFO_LOG_LENGTH, &len);

         if (len > 0)
         {
            std::vector<char> buf(len + 1);
            GLsizei out_len;
            SYM(glGetProgramInfoLog)(prog, len, &out_len, &buf[0]);

            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "Link error: %s\n", &buf[0]);
         }
      }
   }

   GLuint Shader::compile_shader(GLenum type, const std::string& source)
   {
      GLuint shader = SYM(glCreateShader)(type);

      const char* src = source.c_str();
      SYM(glShaderSource)(shader, 1, &src, NULL);
      SYM(glCompileShader)(shader);

      GLint status = 0;
      SYM(glGetShaderiv)(shader, GL_COMPILE_STATUS, &status);
      if (!status)
      {
         GLint len = 0;
         SYM(glGetShaderiv)(shader, GL_INFO_LOG_LENGTH, &len);

         if (len > 0)
         {
            std::vector<char> buf(len + 1);
            GLsizei out_len;
            SYM(glGetShaderInfoLog)(shader, len, &out_len, &buf[0]);

            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "Shader error: %s\n", &buf[0]);
         }

         SYM(glDeleteShader)(shader);
         return 0;
      }

      return shader;
   }

   Shader::~Shader()
   {
      if (dead_state)
         return;

      GLsizei count;
      GLuint shaders[2];

      SYM(glGetAttachedShaders)(prog, 2, &count, shaders);
      for (GLsizei i = 0; i < count; i++)
      {
         SYM(glDetachShader)(prog, shaders[i]);
         SYM(glDeleteShader)(shaders[i]);
      }

      SYM(glDeleteProgram)(prog);
   }

   void Shader::use()
   {
      SYM(glUseProgram)(prog);
   }

   void Shader::unbind()
   {
      SYM(glUseProgram)(0);
   }

   GLint Shader::uniform(const char* sym)
   {
      GLint ret = -1;

      std::map<std::string, GLint>::iterator itr = map.find(sym);
      if (itr == map.end())
         map[sym] = ret = SYM(glGetUniformLocation)(prog, sym);
      else
         ret = itr->second;

      return ret;
   }

   GLint Shader::attrib(const char* sym)
   {
      GLint ret = -1;

      std::map<std::string, GLint>::iterator itr = map.find(sym);
      if (itr == map.end())
         map[sym] = ret = SYM(glGetAttribLocation)(prog, sym);
      else
         ret = itr->second;

      return ret;
   }
}

