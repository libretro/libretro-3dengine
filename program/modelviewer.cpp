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

#include "libretro.h"
#include "program.h"
#include "../engine/mesh.hpp"
#include "../engine/texture.hpp"
#include "../engine/object.hpp"
#include "collision_detection.hpp"
#include "location_math.hpp"

#include <vector>
#include <string.h>

using namespace glm;

static bool first_init = true;
static std::string mesh_path;
static bool discard_hack_enable = false;

static std::vector<std1::shared_ptr<GL::Mesh> > meshes;
static std1::shared_ptr<GL::Texture> blank;

//forward decls
static void scenewalker_reset_mesh_path(void);

static float light_r;
static float light_g;
static float light_b;
static float ambient_light_r;
static float ambient_light_g;
static float ambient_light_b;

static bool update;

static vec3 player_size(0.4f, 0.8f, 0.4f);

enum {
   MODE_MODELVIEWER = 0,
   MODE_SCENEWALKER = 1,
};

unsigned mode_engine = MODE_MODELVIEWER;

static vec3 scenewalker_check_input(void)
{
   static float player_view_deg_x;
   static float player_view_deg_y;
   static vec3 player_pos(0, 2, 0);

   input_poll_cb();

   int analog_x = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);

   int analog_y = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

   int analog_ry = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

   int analog_rx = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);

   static bool old_jump;
   bool new_jump = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_B);

   bool jump = new_jump && !old_jump;
   old_jump = new_jump;

   bool run_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_Y);
   bool mouselook_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_X);

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_LEFT))
      analog_rx = run_pressed ? -32767 : -16384;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_RIGHT))
      analog_rx = run_pressed ? 32767 : 16384;

   if (location_camera_control_enable)
   {
      if (loc_float_greater_than(current_location.longitude, previous_location.longitude))
         analog_y = run_pressed ? -32767 : -16384;
      else if (loc_float_lesser_than(current_location.longitude, previous_location.longitude))
         analog_y = run_pressed ? 32767 : 16384;
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_UP))
   {
      if (mouselook_pressed)
         analog_ry = run_pressed ? -32767 : -16384;
      else
         analog_y = run_pressed ? -32767 : -16384;
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_DOWN))
   {
      if (mouselook_pressed)
         analog_ry = run_pressed ? 32767 : 16384;
      else
         analog_y = run_pressed ? 32767 : 16384;
   }


   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_L))
      analog_x = run_pressed ? -32767 : -16384;

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_R))
      analog_x = run_pressed ? 32767 : 16384;

   if (abs(analog_x) < 10000)
      analog_x = 0;
   if (abs(analog_y) < 10000)
      analog_y = 0;
   if (abs(analog_rx) < 10000)
      analog_rx = 0;
   if (abs(analog_ry) < 10000)
      analog_ry = 0;

#if 0
   if (log_cb)
   {
      log_cb(RETRO_LOG_INFO, "analog_x: %d\n", analog_x);
      log_cb(RETRO_LOG_INFO, "analog_y: %d\n", analog_y);
      log_cb(RETRO_LOG_INFO, "analog_rx: %d\n", analog_rx);
      log_cb(RETRO_LOG_INFO, "analog_ry: %d\n", analog_ry);
   }
#endif

   player_view_deg_y += analog_rx * -0.00008f;
   player_view_deg_x += analog_ry * -0.00005f;

   player_view_deg_x = clamp(player_view_deg_x, -80.0f, 80.0f);

   mat4 rotate_x = rotate(mat4(1.0), player_view_deg_x, vec3(1, 0, 0));
   mat4 rotate_y = rotate(mat4(1.0), player_view_deg_y, vec3(0, 1, 0));
   mat4 rotate_y_right = rotate(mat4(1.0), player_view_deg_y - 90.0f, vec3(0, 1, 0));

   vec3 look_dir = vec3(rotate_y * rotate_x * vec4(0, 0, -1, 1));

   vec3 right_walk_dir = vec3(rotate_y_right * vec4(0, 0, -1, 1));
   vec3 front_walk_dir = vec3(rotate_y * vec4(0, 0, -1, 1));

   vec3 velocity = front_walk_dir * vec3(analog_y * -0.000002f) +
      right_walk_dir * vec3(analog_x * 0.000002f);

   vec3 player_pos_espace = player_pos / player_size;
   vec3 velocity_espace = velocity / player_size;

   coll_detection(player_pos_espace, velocity_espace);
   player_pos_espace += velocity_espace;
   coll_wall_hug_detection(player_pos_espace);

   static vec3 gravity;
   static bool can_jump;
   gravity += vec3(0.0f, -0.01f, 0.0f);
   if (can_jump && jump)
   {
      gravity[1] += 0.3f;
      can_jump = false;
   }
   gravity[1] -= gravity[1] * 0.01f;

   vec3 old_gravity = gravity;
   coll_detection(player_pos_espace, gravity);
   if (old_gravity[1] != gravity[1])
   {
      gravity = vec3(0.0f);
      can_jump = true;
   }

   player_pos_espace += gravity;
   coll_wall_hug_detection(player_pos_espace);

   player_pos = player_pos_espace * player_size;

   mat4 view = lookAt(player_pos, player_pos + look_dir, vec3(0, 1, 0));

   bool start_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
         if (start_pressed)
            light_r -= 0.01;
         else
            ambient_light_r -= 0.01;
      }
      else
      {
         if (start_pressed)
            light_r += 0.01;
         else
            ambient_light_r += 0.01;
      }
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
         if (start_pressed)
            light_g -= 0.01;
         else
            ambient_light_g -= 0.01;
      }
      else
      {
         if (start_pressed)
            light_g += 0.01;
         else
            ambient_light_g += 0.01;
      }
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
         if (start_pressed)
            light_b -= 0.01;
         else
            ambient_light_b -= 0.01;
      }
      else
      {
         if (start_pressed)
            light_b += 0.01;
         else
            ambient_light_b += 0.01;
      }
   }

   for (unsigned i = 0; i < meshes.size(); i++)
   {
      meshes[i]->set_view(view);
      meshes[i]->set_eye(player_pos);
      meshes[i]->set_lighting(light_r, light_g, light_b);
      meshes[i]->set_ambient_lighting(ambient_light_r, ambient_light_g, ambient_light_b);
   }

   return player_size;
}


static vec3 modelviewer_check_input(void)
{
   if (mode_engine == MODE_SCENEWALKER)
      return scenewalker_check_input();

   static float model_rotate_y;
   static float model_rotate_x;
   static float model_scale = 1.0f;

   input_poll_cb();

   int analog_x = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);

   int analog_y = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

   int analog_ry = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

   if (abs(analog_x) < 10000)
      analog_x = 0;
   if (abs(analog_y) < 10000)
      analog_y = 0;

   if (abs(analog_ry) < 10000)
      analog_ry = 0;

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
      analog_x -= 30000;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
      analog_x += 30000;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
      analog_y -= 30000;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
      analog_y += 30000;

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
      analog_ry -= 30000;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
      analog_ry += 30000;

   bool start_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
         if (start_pressed)
            light_r -= 0.01;
         else
            ambient_light_r -= 0.01;
      }
      else
      {
         if (start_pressed)
            light_r += 0.01;
         else
            ambient_light_r += 0.01;
      }
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
         if (start_pressed)
            light_g -= 0.01;
         else
            ambient_light_g -= 0.01;
      }
      else
      {
         if (start_pressed)
            light_g += 0.01;
         else
            ambient_light_g += 0.01;
      }
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
         if (start_pressed)
            light_b -= 0.01;
         else
            ambient_light_b -= 0.01;
      }
      else
      {
         if (start_pressed)
            light_b += 0.01;
         else
            ambient_light_b += 0.01;
      }
   }

   model_scale *= 1.0f - analog_ry * 0.000001f;
   model_scale = clamp(model_scale, 0.0001f, 100.0f);
   model_rotate_x += analog_y * 0.0001f;
   model_rotate_y += analog_x * 0.00015f;

   mat4 translation = translate(mat4(1.0), vec3(0, 0, -40));
   mat4 scaler = scale(mat4(1.0), vec3(model_scale, model_scale, model_scale));
   mat4 rotate_x = rotate(mat4(1.0), model_rotate_x, vec3(1, 0, 0));
   mat4 rotate_y = rotate(mat4(1.0), model_rotate_y, vec3(0, 1, 0));

   mat4 model = translation * scaler * rotate_x * rotate_y;

   for (unsigned i = 0; i < meshes.size(); i++)
   {
      meshes[i]->set_model(model);
      meshes[i]->set_ambient_lighting(ambient_light_r, ambient_light_g, ambient_light_b);
      meshes[i]->set_lighting(light_r, light_g, light_b);
   }
   //check_collision_cube();

   return player_size;
}

static void init_mesh(const std::string& path)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Loading Mesh ...\n");

   static const std::string vertex_shader =
      "uniform mat4 uModel;\n"
      "uniform mat4 uMVP;\n"
      "attribute vec4 aVertex;\n"
      "attribute vec3 aNormal;\n"
      "attribute vec2 aTex;\n"
      "varying vec4 vNormal;\n"
      "varying vec2 vTex;\n"
      "varying vec4 vPos;\n"
      "void main() {\n"
      "  gl_Position = uMVP * aVertex;\n"
      "  vTex = aTex;\n"
      "  vPos = uModel * aVertex;\n"
      "  vNormal = uModel * vec4(aNormal, 0.0);\n"
      "}";

   static const std::string fragment_shader_avoid_discard_hack =
      "#ifdef GL_ES\n"
      "precision mediump float;\n"
      "#endif\n"
      "varying vec2 vTex;\n"
      "varying vec4 vNormal;\n"
      "varying vec4 vPos;\n"

      "uniform sampler2D sDiffuse;\n"
      "uniform sampler2D sAmbient;\n"

      "uniform vec3 uLightPos;\n"
      "uniform vec3 uLightAmbient;\n"
      "uniform vec3 uMTLAmbient;\n"
      "uniform float uMTLAlphaMod;\n"
      "uniform vec3 uMTLDiffuse;\n"
      "uniform vec3 uMTLSpecular;\n"
      "uniform float uMTLSpecularPower;\n"

      "void main() {\n"
      "  vec4 colorDiffuseFull = texture2D(sDiffuse, vTex);\n"
      "  vec4 colorAmbientFull = texture2D(sAmbient, vTex);\n"

      "  if (colorDiffuseFull.a < 0.5)\n"
      "     discard;\n"

      "  vec3 colorDiffuse = mix(uMTLDiffuse, colorDiffuseFull.rgb, vec3(colorDiffuseFull.a));\n"
      "  vec3 colorAmbient = mix(uMTLAmbient, colorAmbientFull.rgb, vec3(colorAmbientFull.a));\n"

      "  vec3 normal = normalize(vNormal.xyz);\n"
      "  float directivity = dot(uLightPos, -normal);\n"

      "  vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);\n"
      "  vec3 ambient = colorAmbient * uLightAmbient;\n"

      "  vec3 modelToFace = normalize(-vPos.xyz);\n"
      "  float specularity = pow(clamp(dot(modelToFace, reflect(uLightPos, normal)), 0.0, 1.0), uMTLSpecularPower);\n"
      "  vec3 specular = uMTLSpecular * specularity;\n"

      "  gl_FragColor = vec4(diffuse + ambient + specular, uMTLAlphaMod);\n"
      "}";

   static const std::string fragment_shader =
      "#ifdef GL_ES\n"
      "precision mediump float;\n"
      "#endif\n"
      "varying vec2 vTex;\n"
      "varying vec4 vNormal;\n"
      "varying vec4 vPos;\n"

      "uniform sampler2D sDiffuse;\n"
      "uniform sampler2D sAmbient;\n"

      "uniform vec3 uLightPos;\n"
      "uniform vec3 uLightAmbient;\n"
      "uniform vec3 uMTLAmbient;\n"
      "uniform float uMTLAlphaMod;\n"
      "uniform vec3 uMTLDiffuse;\n"
      "uniform vec3 uMTLSpecular;\n"
      "uniform float uMTLSpecularPower;\n"

      "void main() {\n"
      "  vec4 colorDiffuseFull = texture2D(sDiffuse, vTex);\n"
      "  vec4 colorAmbientFull = texture2D(sAmbient, vTex);\n"

      "  vec3 colorDiffuse = mix(uMTLDiffuse, colorDiffuseFull.rgb, vec3(colorDiffuseFull.a));\n"
      "  vec3 colorAmbient = mix(uMTLAmbient, colorAmbientFull.rgb, vec3(colorAmbientFull.a));\n"

      "  vec3 normal = normalize(vNormal.xyz);\n"
      "  float directivity = dot(uLightPos, -normal);\n"

      "  vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);\n"
      "  vec3 ambient = colorAmbient * uLightAmbient;\n"

      "  vec3 modelToFace = normalize(-vPos.xyz);\n"
      "  float specularity = pow(clamp(dot(modelToFace, reflect(uLightPos, normal)), 0.0, 1.0), uMTLSpecularPower);\n"
      "  vec3 specular = uMTLSpecular * specularity;\n"

      "  gl_FragColor = vec4(diffuse + ambient + specular, uMTLAlphaMod * colorDiffuseFull.a);\n"
      "}";

   static const std::string fragment_shader_scene =
      "#ifdef GL_ES\n"
      "precision mediump float;\n"
      "#endif\n"
      "varying vec2 vTex;\n"
      "varying vec4 vNormal;\n"
      "varying vec4 vPos;\n"

      "uniform sampler2D sDiffuse;\n"
      "uniform sampler2D sAmbient;\n"

      "uniform vec3 uLightPos;\n"
      "uniform vec3 uLightAmbient;\n"
      "uniform vec3 uEyePos;\n"
      "uniform vec3 uMTLAmbient;\n"
      "uniform float uMTLAlphaMod;\n"
      "uniform vec3 uMTLDiffuse;\n"
      "uniform vec3 uMTLSpecular;\n"
      "uniform float uMTLSpecularPower;\n"

      "void main() {\n"
      "  vec4 colorDiffuseFull = texture2D(sDiffuse, vTex);\n"
      "  vec4 colorAmbientFull = texture2D(sAmbient, vTex);\n"

      "  vec3 lightDir = normalize(vPos.xyz - uLightPos);\n"

      "  vec3 colorDiffuse = mix(uMTLDiffuse, colorDiffuseFull.rgb, vec3(colorDiffuseFull.a));\n"
      "  vec3 colorAmbient = mix(uMTLAmbient, colorAmbientFull.rgb, vec3(colorAmbientFull.a));\n"

      "  vec3 normal = normalize(vNormal.xyz);\n"
      "  float directivity = dot(lightDir, -normal);\n"

      "  vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);\n"
      "  vec3 ambient = colorAmbient * uLightAmbient;\n"

      "  vec3 modelToFace = normalize(uEyePos - vPos.xyz);\n"
      "  float specularity = pow(clamp(dot(modelToFace, reflect(lightDir, normal)), 0.0, 1.0), uMTLSpecularPower);\n"
      "  vec3 specular = uMTLSpecular * specularity;\n"

      "  gl_FragColor = vec4(diffuse + ambient + specular, uMTLAlphaMod * colorDiffuseFull.a);\n"
      "}";
   
   std1::shared_ptr<GL::Shader> shader(new GL::Shader(vertex_shader, (mode_engine == MODE_SCENEWALKER) ? fragment_shader_scene : ((discard_hack_enable) ? fragment_shader_avoid_discard_hack : fragment_shader)));
   meshes = OBJ::load_from_file(path);

   mat4 projection;
   if (mode_engine == MODE_SCENEWALKER)
      projection = scale(mat4(1.0), vec3(1, -1, 1)) * perspective(45.0f, 640.0f / 480.0f, 1.0f, 100.0f);
   else
      projection = scale(mat4(1.0), vec3(1, -1, 1)) * perspective(45.0f, 4.0f / 3.0f, 0.2f, 100.0f);

   for (unsigned i = 0; i < meshes.size(); i++)
   {
      meshes[i]->set_projection(projection);
      meshes[i]->set_shader(shader);
      meshes[i]->set_blank(blank);

      if (mode_engine == MODE_SCENEWALKER)
      {
         meshes[i]->set_lighting(0, 10, 0);

         const std::vector<GL::Vertex>& vertices = *meshes[i]->get_vertex();
         for (unsigned v = 0; v < vertices.size(); v += 3)
            coll_triangles_push(v, vertices, player_size);
      }
   }

   if (mode_engine == MODE_SCENEWALKER)
   {
      light_r = normalize(0);
      light_g = normalize(10);
      light_b = normalize(0);
   }
   else
   {
      light_r = normalize(-1);
      light_g = normalize(-1);
      light_b = normalize(-1);
   }
   ambient_light_r = 0.25f;
   ambient_light_g = 0.25f;
   ambient_light_b = 0.25f;
}

extern char retro_path_info[1024];

static void modelviewer_context_reset(void)
{
   GL::dead_state = true;
   meshes.clear();
   blank.reset();
   GL::dead_state = false;

   if (strstr(retro_path_info, ".mtl") || mode_engine == MODE_SCENEWALKER)
   {
      coll_triangles_clear();
      scenewalker_reset_mesh_path();
      mode_engine = MODE_SCENEWALKER;
   }

   GL::set_function_cb(hw_render.get_proc_address);
   GL::init_symbol_map();

   blank = GL::Texture::blank();
   init_mesh(mesh_path);

   update = true;
}

static void modelviewer_update_variables(retro_environment_t environ_cb)
{
   struct retro_variable var;

   var.key = "3dengine-modelviewer-discard-hack";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0)
         discard_hack_enable = false;
      else if (strcmp(var.value, "enabled") == 0)
         discard_hack_enable = true;

      if (!first_init)
         modelviewer_context_reset();
   }
}


static void modelviewer_run(void)
{
   vec3 look_dir = modelviewer_check_input();
   (void)look_dir;

   SYM(glBindFramebuffer)(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
   SYM(glViewport)(0, 0, engine_width, engine_height);
   SYM(glClearColor)(0.2f, 0.2f, 0.2f, 1.0f);
   SYM(glClear)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   SYM(glEnable)(GL_DEPTH_TEST);
   SYM(glFrontFace)(GL_CW); // When we flip vertically, orientation changes.
   SYM(glEnable)(GL_CULL_FACE);
   SYM(glEnable)(GL_BLEND);

   for (unsigned i = 0; i < meshes.size(); i++)
      meshes[i]->render();

   SYM(glDisable)(GL_BLEND);
   SYM(glDisable)(GL_DEPTH_TEST);
   SYM(glDisable)(GL_CULL_FACE);
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, engine_width, engine_height, 0);
}

static void scenewalker_reset_mesh_path(void)
{
   //always assume matching .obj file is there
   size_t extPos = mesh_path.rfind('.');
   if (extPos != std::string::npos)
   {
      // Erase the current extension.
      mesh_path.erase(extPos);
      // Add the new extension.
      mesh_path.append(".obj");

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "New path: %s\n", mesh_path.c_str());
   }
}

static void modelviewer_load_game(const struct retro_game_info *info)
{
   mode_engine = MODE_MODELVIEWER;

   mesh_path = info->path;

   if (strstr(info->path, ".mtl"))
   {
      mode_engine = MODE_SCENEWALKER;
      coll_test_crash_detection();
      scenewalker_reset_mesh_path();
   }
   else
      player_size = vec3(0, 0, 0);

   first_init = false;
}

const engine_program_t engine_program_modelviewer = {
   modelviewer_load_game,
   modelviewer_run,
   modelviewer_context_reset,
   modelviewer_update_variables,
   modelviewer_check_input,
};
