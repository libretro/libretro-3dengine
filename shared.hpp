/*
 *  InstancingViewer Camera Tech demo
 *  Copyright (C) 2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2013 - Daniel De Matteis
 *  Copyright (C) 2013 - Michael Lelli
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

#ifndef SHARED_HPP__
#define SHARED_HPP__

#if defined(_MSC_VER) || defined(EMSCRIPTEN)
#include <memory>
#ifdef _MSC_VER
#define snprintf _snprintf
#endif
#else
#include <tr1/memory>
#endif

#if defined(__QNX__) || defined(__CELLOS_LV2__) || defined(IOS) || defined(OSX)
namespace std1 = compat;
#elif !defined(EMSCRIPTEN)
namespace std1 = std::tr1;
#endif

void retro_stderr(const char *str);
void retro_stderr_print(const char *fmt, ...);

#endif

