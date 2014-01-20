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
      vector<string> vertices = String::split(data, " ");
      if (vertices.size() > 3)
         vertices.resize(3);

      vector<vector<string> > verts;
      for (unsigned i = 0; i < vertices.size(); i++)
      {
         Vertex out_vertex;

         vector<string> coords = String::split(vertices[i], "/", true);
         if (coords.size() == 1) // Vertex only
         {
            size_t coord = translate_index(String::stoi(coords[0]), vertex.size());

            if (coord && vertex.size() >= coord)
               out_vertex.vert = vertex[coord - 1];
         }
         else if (coords.size() == 2) // Vertex/Texcoord
         {
            size_t coord_vert = translate_index(String::stoi(coords[0]), vertex.size());
            size_t coord_tex  = translate_index(String::stoi(coords[1]), tex.size());

            if (coord_vert && vertex.size() >= coord_vert)
               out_vertex.vert = vertex[coord_vert - 1];
            if (coord_tex && tex.size() >= coord_tex)
               out_vertex.tex = tex[coord_tex - 1];
         }
         else if (coords.size() == 3 && coords[1].size()) // Vertex/Texcoord/Normal
         {
            size_t coord_vert   = translate_index(String::stoi(coords[0]), vertex.size());
            size_t coord_tex    = translate_index(String::stoi(coords[1]), tex.size());
            size_t coord_normal = translate_index(String::stoi(coords[2]), normal.size());

            if (coord_vert && vertex.size() >= coord_vert)
               out_vertex.vert = vertex[coord_vert - 1];
            if (coord_tex && tex.size() >= coord_tex)
               out_vertex.tex = tex[coord_tex - 1];
            if (coord_normal && normal.size() >= coord_normal)
               out_vertex.normal = normal[coord_normal - 1];
         }
         else if (coords.size() == 3 && !coords[1].size()) // Vertex//Normal
         {
            size_t coord_vert   = translate_index(String::stoi(coords[0]), vertex.size());
            size_t coord_normal = translate_index(String::stoi(coords[2]), normal.size());

            if (coord_vert && vertex.size() >= coord_vert)
               out_vertex.vert = vertex[coord_vert - 1];
            if (coord_normal && normal.size() >= coord_normal)
               out_vertex.normal = normal[coord_normal - 1];
         }

         vertices_buffer.push_back(out_vertex);
      }
   }

   static map<string, Material> parse_mtllib(const string& path, map<string, std1::shared_ptr<Texture> >& textures)
   {
      map<string, Material> materials;

      ifstream file(path.c_str(), ios::in);
      if (!file.is_open())
         return materials;

      Material current;
      string current_mtl;

      for (string line; getline(file, line); )
      {
         line = String::strip(line);

         size_t split_point = line.find_first_of(' ');
         string type = line.substr(0, split_point);
         string data = split_point != string::npos ? line.substr(split_point + 1) : string();

         if (type == "newmtl")
         {
            if (current_mtl.size())
               materials[current_mtl] = current;

            current = Material();
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
         {
            if (!textures[data])
            {
               string diffuse_path = Path::join(Path::basedir(path), data);
               textures[data] = std1::shared_ptr<Texture>(new Texture(diffuse_path));
            }

            current.diffuse_map = textures[data];
         }
         else if (type == "map_Ka")
         {
            if (!textures[data])
            {
               string ambient_path = Path::join(Path::basedir(path), data);
               textures[data] = std1::shared_ptr<Texture>(new Texture(ambient_path));
            }

            current.ambient_map = textures[data];
         }
      }

      materials[current_mtl] = current;

      return materials;
   }

   vector<std1::shared_ptr<Mesh> > load_from_file(const string& path)
   {
      ifstream file(path.c_str(), ios::in);
      vector<std1::shared_ptr<Mesh> > meshes;
      if (!file.is_open())
         return meshes;

      vector<vec3> vertex;
      vector<vec3> normal;
      vector<vec2> tex;

      vector<Vertex> vertices;

      // Texture cache.
      map<string, std1::shared_ptr<Texture> > textures;
      Material current_material;

      map<string, Material> materials;

      for (string line; getline(file, line); )
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
            if (vertices.size()) // Different texture, new mesh.
            {
               std1::shared_ptr<Mesh> mesh(new Mesh());
               mesh->set_vertices(vertices);
               vertices.clear();

               mesh->set_material(current_material);
               meshes.push_back(mesh);
            }

            if (!textures[data])
            {
               string texture_path = Path::join(Path::basedir(path), data + ".png");
               textures[data] = std1::shared_ptr<Texture>(new Texture(texture_path));
            }

            current_material = Material();
            current_material.diffuse_map = textures[data];
            current_material.ambient_map = textures[data];
         }
         else if (type == "usemtl")
         {
            if (vertices.size()) // Different texture, new mesh.
            {
               std1::shared_ptr<Mesh> mesh(new Mesh());
               mesh->set_vertices(vertices);
               vertices.clear();

               mesh->set_material(current_material);
               meshes.push_back(mesh);
            }

            current_material = materials[data];
         }
         else if (type == "mtllib")
            materials = parse_mtllib(Path::join(Path::basedir(path), data), textures);
      }

      if (vertices.size())
      {
         std1::shared_ptr<Mesh> mesh(new Mesh());
         mesh->set_vertices(vertices);
         vertices.clear();

         mesh->set_material(current_material);
         meshes.push_back(mesh);
      }

      return meshes;
   }
}

