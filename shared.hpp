/*
 *  Libretro 3DEngine
 *  Copyright (C) 2013-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2013-2014 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - Michael Lelli
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

#ifndef SHARED_HPP__
#define SHARED_HPP__

#if defined(_MSC_VER)
#include <memory>
#ifdef _MSC_VER
#define snprintf _snprintf
#endif
#else
#include <tr1/memory>
#endif

#if defined(__QNX__) || defined(IOS) || defined(OSX) || defined(EMSCRIPTEN)
namespace std1 = compat;
#else
namespace std1 = std::tr1;
#endif

#include <libretro.h>

extern retro_log_printf_t log_cb;
extern struct retro_sensor_interface sensor_cb;

#endif

