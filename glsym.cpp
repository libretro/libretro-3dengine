/*
 *  InstancingViewer Camera Tech demo
 *  Copyright (C) 2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2013 - Daniel De Matteis
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

#include "gl.hpp"

namespace GL
{
   bool dead_state;

   static SymMap map;
   SymMap& symbol_map()
   {
      return map;
   }

   void init_symbol_map()
   {
      map.clear();

      // On Windows, you cannot lookup these symbols ... <_<
      struct mapper { const char* sym; retro_proc_address_t proc; };
#define _D(sym) { #sym, reinterpret_cast<retro_proc_address_t>(sym) }
      static const mapper bind_map[] = {
         _D(glEnable),
         _D(glDisable),
         _D(glBlendFunc),
         _D(glClearColor),
         _D(glTexImage2D),
         _D(glViewport),
         _D(glClear),
         _D(glTexParameteri),
         _D(glDeleteTextures),
         _D(glGenTextures),
         _D(glBindTexture),
         _D(glDrawArrays),
         _D(glGetError),
         _D(glFrontFace),
#ifdef ANDROID
         _D(glGetString),
#endif
#if defined(__APPLE__) && !defined(IOS)
         _D(glActiveTexture),
         _D(glCreateProgram),
         _D(glCreateShader),
         _D(glShaderSource),
         _D(glCompileShader),
         _D(glGetShaderiv),
         _D(glAttachShader),
         _D(glLinkProgram),
         _D(glGetProgramiv),
         _D(glGenerateMipmap),
         _D(glGetIntegerv),
         _D(glGenBuffers),
         _D(glBindBuffer),
         _D(glBufferData),
         _D(glBindFramebuffer),
         _D(glUseProgram),
         _D(glUniform1i),
         _D(glGetUniformLocation),
         _D(glUniformMatrix4fv),
         _D(glUniform3fv),
         _D(glUniform1f),
         _D(glGetAttribLocation),
         _D(glEnableVertexAttribArray),
         _D(glVertexAttribPointer),
         _D(glDisableVertexAttribArray),
         _D(glVertexAttribDivisorARB),
         _D(glUniform4fv),
#endif
      };
#undef _D

      for (unsigned i = 0; i < sizeof(bind_map) / sizeof(bind_map[0]); i++)
         map[bind_map[i].sym] = bind_map[i].proc;
   }

   static retro_hw_get_proc_address_t proc;
   void set_function_cb(retro_hw_get_proc_address_t proc_)
   {
      proc = proc_;
   }

   retro_proc_address_t get_symbol(const std::string& str)
   {
      return proc(str.c_str());
   }
}

