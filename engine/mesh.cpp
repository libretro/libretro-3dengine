/*
 *  Scenewalker Tech demo
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

#include "mesh.hpp"

using namespace glm;
using namespace std;
using namespace std1;

namespace GL
{
   Mesh::Mesh() : 
      vertex_type(GL_TRIANGLES),
      light_pos(0, 10, 0),
      light_ambient(0.25f, 0.25f, 0.25f),
      model(mat4(1.0)),
      view(mat4(1.0)),
      projection(mat4(1.0))
   {
      SYM(glGenBuffers)(1, &vbo);
      mvp = projection * view * model;
   }

   Mesh::~Mesh()
   {
      if (dead_state)
         return;

      SYM(glDeleteBuffers)(1, &vbo);
   }

   void Mesh::set_vertices(vector<Vertex> vertex)
   {
      set_vertices(std1::shared_ptr<vector<Vertex> >(new vector<Vertex>(vertex)));
   }

   void Mesh::set_vertex_type(GLenum type)
   {
      vertex_type = type;
   }

   void Mesh::set_light_pos(const glm::vec3& light_pos)
   {
      this->light_pos = light_pos;
   }

   void Mesh::set_eye(const vec3& eye_pos)
   {
      this->eye_pos = eye_pos;
   }

   void Mesh::set_vertices(const std1::shared_ptr<vector<Vertex> >& vertex)
   {
      this->vertex = vertex;

      SYM(glBindBuffer)(GL_ARRAY_BUFFER, vbo);
      SYM(glBufferData)(GL_ARRAY_BUFFER, vertex->size() * sizeof(Vertex),
            &(*vertex)[0], GL_STATIC_DRAW);
      SYM(glBindBuffer)(GL_ARRAY_BUFFER, 0);
   }

   void Mesh::set_material(const Material& material)
   {
      this->material = material;
   }

   void Mesh::set_blank(const std1::shared_ptr<Texture>& blank)
   {
      this->blank = blank;
   }

   void Mesh::set_shader(const std1::shared_ptr<Shader>& shader)
   {
      this->shader = shader;
   }

   void Mesh::set_model(const mat4& model)
   {
      this->model = model;
      mvp = projection * view * model;
   }

   void Mesh::set_view(const mat4& view)
   {
      this->view = view;
      mvp = projection * view * model;
   }

   void Mesh::set_projection(const mat4& projection)
   {
      this->projection = projection;
      mvp = projection * view * model;
   }

   void Mesh::render()
   {
      if (!vertex || !shader)
         return;

      if (material.diffuse_map)
         material.diffuse_map->bind(0);
      else if (blank)
         blank->bind(0);

      if (material.ambient_map)
         material.ambient_map->bind(1);
      else if (material.diffuse_map)
         material.diffuse_map->bind(1);
      else if (blank)
         blank->bind(1);

      shader->use();

      SYM(glUniform1i)(shader->uniform("sDiffuse"), 0);
      SYM(glUniform1i)(shader->uniform("sAmbient"), 1);

      SYM(glUniformMatrix4fv)(shader->uniform("uModel"),
            1, GL_FALSE, value_ptr(model));
      SYM(glUniformMatrix4fv)(shader->uniform("uMVP"),
            1, GL_FALSE, value_ptr(mvp));
      SYM(glUniform3fv)(shader->uniform("uEyePos"),
            1, value_ptr(eye_pos));

      SYM(glUniform3fv)(shader->uniform("uMTLAmbient"),
            1, value_ptr(material.ambient));
      SYM(glUniform3fv)(shader->uniform("uMTLDiffuse"),
            1, value_ptr(material.diffuse));
      SYM(glUniform3fv)(shader->uniform("uMTLSpecular"),
            1, value_ptr(material.specular));
      SYM(glUniform1f)(shader->uniform("uMTLSpecularPower"),
            material.specular_power);
      SYM(glUniform1f)(shader->uniform("uMTLAlphaMod"),
            material.alpha_mod);

      SYM(glUniform3fv)(shader->uniform("uLightPos"),
            1, value_ptr(light_pos));

      SYM(glUniform3fv)(shader->uniform("uLightAmbient"),
            1, value_ptr(light_ambient));


      GLint aVertex = shader->attrib("aVertex");
      GLint aNormal = shader->attrib("aNormal");
      GLint aTex    = shader->attrib("aTex");

      SYM(glBindBuffer)(GL_ARRAY_BUFFER, vbo);

      if (aVertex >= 0)
      {
         SYM(glEnableVertexAttribArray)(aVertex);
         SYM(glVertexAttribPointer)(aVertex, 3, GL_FLOAT,
               GL_FALSE, sizeof(Vertex),
               reinterpret_cast<const GLvoid*>(offsetof(Vertex, vert)));
      }

      if (aNormal >= 0)
      {
         SYM(glEnableVertexAttribArray)(aNormal);
         SYM(glVertexAttribPointer)(aNormal, 3, GL_FLOAT,
               GL_FALSE, sizeof(Vertex),
               reinterpret_cast<const GLvoid*>(offsetof(Vertex, normal)));
      }

      if (aTex >= 0)
      {
         SYM(glEnableVertexAttribArray)(aTex);
         SYM(glVertexAttribPointer)(aTex, 2, GL_FLOAT,
               GL_FALSE, sizeof(Vertex),
               reinterpret_cast<const GLvoid*>(offsetof(Vertex, tex)));
      }

      SYM(glDrawArrays)(vertex_type, 0, vertex->size());

      if (aVertex >= 0)
         SYM(glDisableVertexAttribArray)(aVertex);
      if (aNormal >= 0)
         SYM(glDisableVertexAttribArray)(aNormal);
      if (aTex >= 0)
         SYM(glDisableVertexAttribArray)(aTex);

      SYM(glBindBuffer)(GL_ARRAY_BUFFER, 0);

      Texture::unbind(0);
      Texture::unbind(1);
      Shader::unbind();
   }
}

