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

#ifndef GL_HPP__
#define GL_HPP__

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif

#define GL_GLEXT_PROTOTYPES
#if defined(GLES)
#ifdef IOS
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <map>
#include <stdio.h>
#include <string>
#include "libretro.h"
#include "shared.hpp"

#if defined(__GNUC__) && !defined(__clang__) || defined(__clang__) && defined(OSX)
#define decltype(type) typeof(type)
#endif

#ifdef GLES
#define SYM(sym) sym
#else
#define SYM(sym) (::GL::symbol<decltype(&sym)>(#sym))
#endif

namespace GL
{
   // If true, GL context has been reset and all
   // objects are invalid. Do not free their resources
   // in destructors.
   extern bool dead_state;

   typedef std::map<std::string, retro_proc_address_t> SymMap;

   SymMap& symbol_map();
   void init_symbol_map();

   // Discover and cache GL symbols on-the-fly.
   // Avoids things like GLEW, and avoids typing out a billion symbol declarations.
   void set_function_cb(retro_hw_get_proc_address_t);
   retro_proc_address_t get_symbol(const std::string& str);

   template<typename Func>
   inline Func symbol(const std::string& sym)
   {
      std::map<std::string, retro_proc_address_t>& map = symbol_map();
      
      retro_proc_address_t func = map[sym];
      if (!func)
      {
         func = get_symbol(sym);
         if (!func)
            retro_stderr_print("Didn't find GL symbol: %s\n", sym.c_str());
      }

      return reinterpret_cast<Func>(func);
   }
}

#endif

