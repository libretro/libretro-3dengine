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

#ifndef UTIL_HPP__
#define UTIL_HPP__

#include <string>
#include <cstdlib>
#include <vector>

#define DIR_BACK(string) (string[string.length()-1])

namespace Path
{
   inline std::string basedir(const std::string& path)
   {
      size_t last = path.find_last_of("/\\");
      if (last != std::string::npos)
         return path.substr(0, last);
      else
         return ".";
   }

   inline std::string join(const std::string& dir, const std::string& path)
   {
      char last = dir.size() ? DIR_BACK(dir) : '\0';
      std::string sep;
      if (last != '/' && last != '\\')
         sep = "/";
      return dir + sep + path;
   }

   inline std::string ext(const std::string& path)
   {
      size_t last = path.find_last_of('.');
      if (last == std::string::npos)
         return "";

      last++;
      if (last == path.size())
         return "";

      return path.substr(last);
   }
}

namespace String
{
   inline std::vector<std::string> split(const std::string& str, const std::string& splitter, bool keep_empty = false)
   {
      std::vector<std::string> list;

      for (std::size_t pos = 0, endpos = 0;
            endpos != std::string::npos; pos = endpos + 1)
      {
         endpos = str.find_first_of(splitter, pos);

         if (endpos - pos)
            list.push_back(str.substr(pos, endpos - pos));
         else if (keep_empty)
            list.push_back("");
      }

      return list;
   }

   inline int stoi(const std::string& str)
   {
      return std::strtol(str.c_str(), NULL, 0);
   }

   inline float stof(const std::string& str)
   {
      return static_cast<float>(std::strtod(str.c_str(), NULL));
   }

   inline std::string strip(const std::string& str)
   {
      size_t first_non_space = str.find_first_not_of(" \t\r");
      if (first_non_space == std::string::npos)
         return "";

      return str.substr(first_non_space,
            str.find_last_not_of(" \t\r") - first_non_space + 1);
   }
}

#endif

