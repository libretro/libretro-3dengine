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

#ifndef OBJECT_HPP__
#define OBJECT_HPP__

#include "mesh.hpp"
#include <string>
#include <vector>
#include <memory>
#include "shared.hpp"

namespace OBJ
{
   /* Renderer-agnostic material. Texture maps are full file paths
    * (empty if none) so both the GL and Vulkan back-ends can load them. */
   struct RawMaterial
   {
      RawMaterial() :
         ambient(0, 0, 0),
         diffuse(0, 0, 0),
         specular(0, 0, 0),
         specular_power(60.0f),
         alpha_mod(1.0f)
      {}

      glm::vec3 ambient;
      glm::vec3 diffuse;
      glm::vec3 specular;
      float specular_power;
      float alpha_mod;
      std::string diffuse_map;
      std::string ambient_map;
   };

   /* One draw call's worth of triangles that share a material. */
   struct Part
   {
      std::vector<GL::Vertex> vertices;
      RawMaterial material;
   };

   /* Pure CPU parse. Needs no GL/Vulkan context. */
   std::vector<Part> load_parts(const std::string& path);

   /* GL back-end: load_parts() + create GL::Mesh/GL::Texture objects. */
   std::vector<std1::shared_ptr<GL::Mesh> > load_from_file(const std::string& path);
}

#endif
