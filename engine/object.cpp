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

#include "object.hpp"
#include "util.hpp"
#include <fstream>
#include <string>
#include <map>

using namespace GL;
using namespace glm;
using namespace std;
using namespace std1;

namespace OBJ
{
   template<typename T>
   inline T parse_line(const string& data);

   template<>
   inline vec2 parse_line(const string& data)
   {
      float x = 0, y = 0;
      vector<string> split = String::split(data, " ");
      if (split.size() >= 2)
      {
         x = String::stof(split[0]);
         y = String::stof(split[1]);
      }

      return vec2(x, y);
   }

   template<>
   inline vec3 parse_line(const string& data)
   {
      float x = 0, y = 0, z = 0;
      vector<string> split = String::split(data, " ");
      if (split.size() >= 3)
      {
         x = String::stof(split[0]);
         y = String::stof(split[1]);
         z = String::stof(split[2]);
      }
      return vec3(x, y, z);
   }

   inline size_t translate_index(int index, size_t size)
   {
      return index < 0 ? size + index + 1 : index;
   }

   static void parse_vertex(const string& data,
         vector<Vertex>& vertices_buffer,
         const vector<vec3>& vertex,
         const vector<vec3>& normal,
         const vector<vec2>& tex)
   {
      unsigned i;
      vector<vector<string> > verts;
      vector<string> vertices = String::split(data, " ");

      if (vertices.size() > 3)
         vertices.resize(3);

      for (i = 0; i < vertices.size(); i++)
      {
         Vertex out_vertex;

         vector<string> coords = String::split(vertices[i], "/", true);
         size_t coord_vert     = translate_index(String::stoi(coords[0]), vertex.size());

         switch (coords.size())
         {
            case 1: /* Vertex only */
               if (coord_vert && vertex.size() >= coord_vert)
                  out_vertex.vert = vertex[coord_vert - 1];
               break;
            case 2: /* Vertex/Texcoord */
               {
                  size_t coord_tex  = translate_index(String::stoi(coords[1]), tex.size());

                  if (coord_vert && vertex.size() >= coord_vert)
                     out_vertex.vert = vertex[coord_vert - 1];
                  if (coord_tex && tex.size() >= coord_tex)
                     out_vertex.tex = tex[coord_tex - 1];
               }
               break;
            case 3:
               if (coords[1].size()) /* Vertex/Texcoord/Normal */
               {
                  size_t coord_tex    = translate_index(String::stoi(coords[1]), tex.size());
                  size_t coord_normal = translate_index(String::stoi(coords[2]), normal.size());

                  if (coord_vert && vertex.size() >= coord_vert)
                     out_vertex.vert = vertex[coord_vert - 1];
                  if (coord_tex && tex.size() >= coord_tex)
                     out_vertex.tex = tex[coord_tex - 1];
                  if (coord_normal && normal.size() >= coord_normal)
                     out_vertex.normal = normal[coord_normal - 1];
               }
               else /* Vertex//Normal */
               {
                  size_t coord_normal = translate_index(String::stoi(coords[2]), normal.size());

                  if (coord_vert && vertex.size() >= coord_vert)
                     out_vertex.vert = vertex[coord_vert - 1];
                  if (coord_normal && normal.size() >= coord_normal)
                     out_vertex.normal = normal[coord_normal - 1];
               }
               break;
            default:
               break;
         }

         vertices_buffer.push_back(out_vertex);
      }
   }

   static map<string, RawMaterial> parse_mtllib(const string& path)
   {
      map<string, RawMaterial> materials;
      RawMaterial current;
      string current_mtl;
      string line;

      ifstream file(path.c_str(), ios::in);
      if (!file.is_open())
         return materials;

      for (; getline(file, line); )
      {
         line = String::strip(line);

         size_t split_point = line.find_first_of(' ');
         string type = line.substr(0, split_point);
         string data = split_point != string::npos ? line.substr(split_point + 1) : string();

         if (type == "newmtl")
         {
            if (current_mtl.size())
               materials[current_mtl] = current;

            current = RawMaterial();
            current_mtl = data;
         }
         else if (type == "Ka")
            current.ambient = parse_line<vec3>(data);
         else if (type == "Kd")
            current.diffuse = parse_line<vec3>(data);
         else if (type == "Ks")
            current.specular = parse_line<vec3>(data);
         else if (type == "Ns")
            current.specular_power = String::stof(data);
         else if (type == "d")
            current.alpha_mod = String::stof(data);
         else if (type == "Tr")
            current.alpha_mod = 1.0f - String::stof(data);
         else if (type == "map_Kd")
            current.diffuse_map = Path::join(Path::basedir(path), data);
         else if (type == "map_Ka")
            current.ambient_map = Path::join(Path::basedir(path), data);
      }

      materials[current_mtl] = current;

      return materials;
   }

   static void flush_part(vector<Part>& parts, vector<Vertex>& vertices,
         const RawMaterial& material)
   {
      if (!vertices.size())
         return;

      Part part;
      part.vertices.swap(vertices);
      part.material = material;
      parts.push_back(part);
   }

   vector<Part> load_parts(const string& path)
   {
      vector<vec3> vertex;
      vector<vec3> normal;
      vector<vec2> tex;

      vector<Vertex> vertices;
      vector<Part> parts;

      RawMaterial current_material;
      string line;

      map<string, RawMaterial> materials;
      ifstream file(path.c_str(), ios::in);

      if (!file.is_open())
         return parts;

      for (; getline(file, line); )
      {
         line = String::strip(line);

         size_t split_point = line.find_first_of(' ');
         string type = line.substr(0, split_point);
         string data = split_point != string::npos ? line.substr(split_point + 1) : string();

         if (type == "v")
            vertex.push_back(parse_line<vec3>(data));
         else if (type == "vn")
            normal.push_back(parse_line<vec3>(data));
         else if (type == "vt")
            tex.push_back(parse_line<vec2>(data));
         else if (type == "f")
            parse_vertex(data, vertices, vertex, normal, tex);
         else if (type == "texture") // Not standard OBJ, but do it like this for simplicity ...
         {
            flush_part(parts, vertices, current_material); // Different texture, new part.

            current_material = RawMaterial();
            current_material.diffuse_map = Path::join(Path::basedir(path), data + ".png");
            current_material.ambient_map = current_material.diffuse_map;
         }
         else if (type == "usemtl")
         {
            flush_part(parts, vertices, current_material); // Different material, new part.
            current_material = materials[data];
         }
         else if (type == "mtllib")
            materials = parse_mtllib(Path::join(Path::basedir(path), data));
      }

      flush_part(parts, vertices, current_material);
      return parts;
   }

   static std1::shared_ptr<Texture> cached_texture(const string& path,
         map<string, std1::shared_ptr<Texture> >& cache)
   {
      if (!path.size())
         return std1::shared_ptr<Texture>();

      std1::shared_ptr<Texture>& slot = cache[path];
      if (!slot)
         slot = std1::shared_ptr<Texture>(new Texture(path));
      return slot;
   }

   vector<std1::shared_ptr<Mesh> > load_from_file(const string& path)
   {
      unsigned i;
      vector<Part> parts = load_parts(path);
      vector<std1::shared_ptr<Mesh> > meshes;
      map<string, std1::shared_ptr<Texture> > textures; /* Texture cache. */

      for (i = 0; i < parts.size(); i++)
      {
         const RawMaterial& raw = parts[i].material;
         Material material;

         material.ambient        = raw.ambient;
         material.diffuse        = raw.diffuse;
         material.specular       = raw.specular;
         material.specular_power = raw.specular_power;
         material.alpha_mod      = raw.alpha_mod;
         material.diffuse_map    = cached_texture(raw.diffuse_map, textures);
         material.ambient_map    = cached_texture(raw.ambient_map, textures);

         std1::shared_ptr<Mesh> mesh(new Mesh());
         mesh->set_vertices(parts[i].vertices);
         mesh->set_material(material);
         meshes.push_back(mesh);
      }

      return meshes;
   }
}
