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

#include <stdlib.h>
#include <string.h>
#include "libretro.h"
#include "program.h"
#include "rpng.h"
#include "rtga.h"
#include "picojpeg.h"

#include <glsym/glsym.h>
#include <retro_miscellaneous.h>
#include <gfx/math/vector_3.h>

#include "location_math.h"

#include <vector>

#define CUBE_VERTS 36

static bool first_init = true;
static std::string texpath;
extern bool camera_enable;

struct Vertex
{
   GLfloat vert[4];
   GLfloat normal[4];
   GLfloat tex[2];
};

struct Cube
{
   struct Vertex vertices[CUBE_VERTS];
};

using namespace glm;

static bool update;
static GLuint prog;
static GLuint background_prog;
static GLuint vbo;
static GLuint background_vbo;
static float cube_stride = 4.0f;
static unsigned cube_size = 1;

static float light_r;
static float light_g;
static float light_b;
static float ambient_light_r;
static float ambient_light_g;
static float ambient_light_b;
static float ambient_light_a;

static vec3 player_pos;

static float camera_rot_x;
static float camera_rot_y;

static const char *vertex_shader[] = {
   "uniform mat4 uVP;",
   "uniform mat4 uM;",
   "attribute vec4 aVertex;",
   "attribute vec4 aNormal;",
   "attribute vec2 aTexCoord;",
   "varying vec3 normal;",
   "varying vec4 model_pos;",
   "varying vec2 tex_coord;",
   "void main() {",
   "  model_pos = uM * aVertex;",
   "  gl_Position = uVP * model_pos;",
   "  vec4 trans_normal = uM * aNormal;",
   "  normal = trans_normal.xyz;",
   "  tex_coord = vec2(1.0 - aTexCoord.x, aTexCoord.y);",
   "}",
};

static const char *fragment_shader[] = {
#ifdef ANDROID
   "#extension GL_OES_EGL_image_external : require\n",
#endif
   "#ifdef GL_ES\n",
   "precision mediump float; \n",
   "#endif\n",
   "varying vec3 normal;",
   "varying vec4 model_pos;",
   "varying vec2 tex_coord;",
   "uniform vec3 light_pos;",
   "uniform vec4 ambient_light;",
#ifdef ANDROID
   "uniform samplerExternalOES uTexture;",
#else
   "uniform sampler2D uTexture;",
#endif

   "void main() {",
   "  vec3 diff = light_pos - model_pos.xyz;",
   "  float dist_mod = 100.0 * inversesqrt(dot(diff, diff));",
   "  gl_FragColor = texture2D(uTexture, tex_coord) * (ambient_light + dist_mod * smoothstep(0.0, 1.0, dot(normalize(diff), normal)));",
   "}",
};

static const char *background_vertex_shader[] = {
   "attribute vec2 TexCoord;\n",
   "attribute vec2 VertexCoord;\n",
   "varying vec2 tex_coord;\n",
   "void main() {\n",
   "   gl_Position = vec4(VertexCoord, 1.0, 1.0);\n",
   "   tex_coord = TexCoord;\n",
   "}",
};

static const char *background_fragment_shader[] = {
#ifdef ANDROID
   "#extension GL_OES_EGL_image_external : require\n",
#endif
   "#ifdef GL_ES\n",
   "precision mediump float;\n",
   "#endif\n",
#ifdef ANDROID
   "uniform samplerExternalOES Texture;",
#else
   "uniform sampler2D Texture;\n",
#endif
   "varying vec2 tex_coord;\n",
   "void main() {\n",
   "   gl_FragColor = texture2D(Texture, tex_coord);\n",
   "}",
};

static const GLfloat background_data[] = {
   -1, -1, // vertex
    0,  0, // tex
    1, -1, // vertex
    1,  0, // tex
   -1,  1, // vertex
    0,  1, // tex
    1,  1, // vertex
    1,  1, // tex
};

static const Vertex vertex_data[] = {
   { { -1, -1, -1, 1 }, { 0, 0, -1, 0 }, { 0, 1 } }, // Front
   { {  1, -1, -1, 1 }, { 0, 0, -1, 0 }, { 1, 1 } },
   { { -1,  1, -1, 1 }, { 0, 0, -1, 0 }, { 0, 0 } },
   { {  1,  1, -1, 1 }, { 0, 0, -1, 0 }, { 1, 0 } },

   { {  1, -1,  1, 1 }, { 0, 0,  1, 0 }, { 0, 1 } }, // Back
   { { -1, -1,  1, 1 }, { 0, 0,  1, 0 }, { 1, 1 } },
   { {  1,  1,  1, 1 }, { 0, 0,  1, 0 }, { 0, 0 } },
   { { -1,  1,  1, 1 }, { 0, 0,  1, 0 }, { 1, 0 } },
   
   { { -1, -1,  1, 1 }, { -1, 0, 0, 0 }, { 0, 1 } }, // Left
   { { -1, -1, -1, 1 }, { -1, 0, 0, 0 }, { 1, 1 } },
   { { -1,  1,  1, 1 }, { -1, 0, 0, 0 }, { 0, 0 } },
   { { -1,  1, -1, 1 }, { -1, 0, 0, 0 }, { 1, 0 } },

   { { 1, -1, -1, 1 }, { 1, 0, 0, 0 }, { 0, 1 } }, // Right
   { { 1, -1,  1, 1 }, { 1, 0, 0, 0 }, { 1, 1 } },
   { { 1,  1, -1, 1 }, { 1, 0, 0, 0 }, { 0, 0 } },
   { { 1,  1,  1, 1 }, { 1, 0, 0, 0 }, { 1, 0 } },

   { { -1,  1, -1, 1 }, { 0, 1, 0, 0 }, { 0, 1 } }, // Top
   { {  1,  1, -1, 1 }, { 0, 1, 0, 0 }, { 1, 1 } },
   { { -1,  1,  1, 1 }, { 0, 1, 0, 0 }, { 0, 0 } },
   { {  1,  1,  1, 1 }, { 0, 1, 0, 0 }, { 1, 0 } },

   { { -1, -1,  1, 1 }, { 0, -1, 0, 0 }, { 0, 1 } }, // Bottom
   { {  1, -1,  1, 1 }, { 0, -1, 0, 0 }, { 1, 1 } },
   { { -1, -1, -1, 1 }, { 0, -1, 0, 0 }, { 0, 0 } },
   { {  1, -1, -1, 1 }, { 0, -1, 0, 0 }, { 1, 0 } },
};

static const GLubyte indices[] = {
   0, 1, 2, // Front
   3, 2, 1,

   4, 5, 6, // Back
   7, 6, 5,

   8, 9, 10, // Left
   11, 10, 9,

   12, 13, 14, // Right
   15, 14, 13,

   16, 17, 18, // Top
   19, 18, 17,

   20, 21, 22, // Bottom
   23, 22, 21,
};

static void display_cubes_array(void)
{
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   int vloc = glGetAttribLocation(prog, "aVertex");
   glVertexAttribPointer(vloc, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, vert)));
   glEnableVertexAttribArray(vloc);
   int nloc = glGetAttribLocation(prog, "aNormal");
   glVertexAttribPointer(nloc, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal)));
   glEnableVertexAttribArray(nloc);
   int tcloc = glGetAttribLocation(prog, "aTexCoord");
   glVertexAttribPointer(tcloc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, tex)));
   glEnableVertexAttribArray(tcloc);

   if (update)
   {
      unsigned x, y, z;

      update = false;

      std::vector<Cube> cubes;
      cubes.resize(cube_size * cube_size * cube_size);

      for (x = 0; x < cube_size; x++)
      {
         for (y = 0; y < cube_size; y++)
         {
            for (z = 0; z < cube_size; z++)
            {
               Cube &cube = cubes[((cube_size * cube_size * z) + (cube_size * y) + x)];

               float off_x = cube_stride * ((float)x - cube_size / 2);
               float off_y = cube_stride * ((float)y - cube_size / 2);
               float off_z = -100.0f + cube_stride * ((float)z - cube_size / 2);

               for (unsigned v = 0; v < CUBE_VERTS; v++)
               {
                  cube.vertices[v] = vertex_data[indices[v]];
                  cube.vertices[v].vert[0] += off_x;
                  cube.vertices[v].vert[1] += off_y;
                  cube.vertices[v].vert[2] += off_z;
               }
            }
         }
      }
      glBufferData(GL_ARRAY_BUFFER, cube_size * cube_size * cube_size * sizeof(Cube),
            &cubes[0], GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
   }

   glDrawArrays(GL_TRIANGLES, 0, CUBE_VERTS * cube_size * cube_size * cube_size);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDisableVertexAttribArray(vloc);
   glDisableVertexAttribArray(nloc);
   glDisableVertexAttribArray(tcloc);
}

#if 0
static bool check_closest_cube(vec3 cube_max, vec3 closest_cube)
{
   return true;/*(((closest_cube.x > 0.0f) && (closest_cube.y > 0.0f) && (closest_cube.z > 0.0f)) &&
      ((closest_cube.x < cube_max.x) && (closest_cube.y < cube_max.y) && (closest_cube.z < cube_max.z)));*/
}

static bool check_cube_distance_per_dimension(vec3 cube)
{
   return cube.x * cube.x + cube.y * cube.y + cube.z * cube.z < 25.0f;
}

static void hit(vec3 cube)
{
   (void)cube;
}

static void check_collision_cube(void)
{
   //float cube_origin = cube_stride * ((float)cube_size / -2.0f);
   // emulate cube origin at {0, 0, 0}
   vec3 shifted_player_pos = player_pos;
   shifted_player_pos.z += 100;
   vec3 closest_cube = vec3(0, 0, 0);//round((shifted_player_pos - cube_origin) / cube_stride);
   vec3 closest_cube_pos = vec3(0, 0, 0);//cube_origin + closest_cube * cube_stride;
   vec3 cube_distance = abs(shifted_player_pos - closest_cube_pos);
   vec3 cube_size_max = (vec3)((float)cube_size - 1);
#if 0
   if (log_cb)
   {
      log_cb(RETRO_LOG_INFO, "cube_origin: %f\n", cube_origin);
      log_cb(RETRO_LOG_INFO, "shifted_player_pos: %f %f %f\n", shifted_player_pos.x, shifted_player_pos.y, shifted_player_pos.z);
      log_cb(RETRO_LOG_INFO, "cube: %f %f %f\n", closest_cube.x, closest_cube.y, closest_cube.z);
      log_cb(RETRO_LOG_INFO, "cube_pos: %f %f %f\n", closest_cube_pos.x, closest_cube_pos.y, closest_cube_pos.z);
      log_cb(RETRO_LOG_INFO, "cube_distance: %f %f %f\n", cube_distance.x, cube_distance.y, cube_distance.z);
   }
#endif
   if (check_closest_cube(cube_size_max, closest_cube) &&
         check_cube_distance_per_dimension(cube_distance))
      hit(closest_cube);
}
#endif

static vec3 instancingviewer_check_input(void)
{
   static unsigned select_timeout = 0;
   input_poll_cb();

   int x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   int y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

   int analog_x = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);

   int analog_y = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

   int analog_ry = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

   int analog_rx = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);

   if (sensor_enable)
   {
      if (sensor_cb.get_sensor_input)
      {
         analog_rx = sensor_cb.get_sensor_input(0, RETRO_SENSOR_ACCELEROMETER_X);
         analog_ry = sensor_cb.get_sensor_input(0, RETRO_SENSOR_ACCELEROMETER_Y);
         log_cb(RETRO_LOG_INFO, "Sensor accelerometer X: %d\n", analog_rx);
         log_cb(RETRO_LOG_INFO, "Sensor accelerometer Y: %d\n", analog_ry);
      }
   }

   if (abs(analog_rx) < 10000)
      analog_rx = 0;
   if (abs(analog_ry) < 10000)
      analog_ry = 0;

   if (abs(analog_x) < 10000)
      analog_x = 0;
   if (abs(analog_y) < 10000)
      analog_y = 0;
   if (analog_ry)
      y  = analog_ry;
   if (analog_rx)
      x = analog_rx;

   x = std::max(std::min(x, 20), -20);
   y = std::max(std::min(y, 20), -20);
   camera_rot_x -= 0.20 * x;
   camera_rot_y -= 0.10 * y;

   camera_rot_y = std::max(std::min(camera_rot_y, 80.0f), -80.0f);

   mat4 look_rot_x = rotate(mat4(1.0), camera_rot_x, vec3(0, 1, 0));
   mat4 look_rot_y = rotate(mat4(1.0), camera_rot_y, vec3(1, 0, 0));
   vec3 look_dir = vec3(look_rot_x * look_rot_y * vec4(0, 0, -1, 0));

   vec3 look_dir_side = vec3(look_rot_x * vec4(1, 0, 0, 0));

   mat3 s = mat3(scale(mat4(1.0), vec3(0.25, 0.25, 0.25)));

   if (location_camera_control_enable)
   {
      if (loc_float_greater_than(current_location.longitude, previous_location.longitude))
         player_pos += s * look_dir;
      else if (loc_float_lesser_than(current_location.longitude, previous_location.longitude))
         player_pos -= s * look_dir;
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
      player_pos += s * look_dir;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
      player_pos -= s * look_dir;

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
      player_pos -= s * look_dir_side;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
      player_pos += s * look_dir_side;
   else if (select_timeout != 0)
      select_timeout--;

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


   //check_collision_cube();

   return look_dir;
}

static void print_shader_log(GLuint shader)
{
   GLsizei len = 0;
   glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
   if (!len)
      return;

   char *buffer = new char[len];
   glGetShaderInfoLog(shader, len, &len, buffer);
   log_cb(RETRO_LOG_INFO, ":%s\n", buffer);
   delete[] buffer;
}

static GLuint load_texture(const char *path)
{
   uint8_t *data;
   unsigned width, height;
   if (strstr(path, ".png"))
   {
      if (!rpng_load_image_rgba(path, &data, &width, &height))
      {
         log_cb(RETRO_LOG_ERROR, "Couldn't load PNG texture: %s\n", path);
         return 0;
      }
   }
   else if (strstr(path, ".tga"))
   {
      if (!texture_image_load_tga(path, data, width, height))
      {
         log_cb(RETRO_LOG_ERROR, "Couldn't load TGA texture: %s\n", path);
         return 0;
      }
   }
   else if (strstr(path, ".jpeg") || strstr(path, ".jpg"))
   {
      pjpeg_scan_type_t scan;
      int comps, reduce;
      reduce = 0;
      data = (uint8_t*)pjpeg_load_from_file(path, &width, &height, &comps, &scan, reduce);
      if (!data)
      {
         log_cb(RETRO_LOG_ERROR, "Couldn't load JPEG texture: %s\n", path);
         return 0;
      }
   }

   GLuint tex;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);

   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
         0, GL_RGBA, GL_UNSIGNED_BYTE, data);
   free(data);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   return tex;
}

static void instancingviewer_compile_shaders(void)
{
   prog = glCreateProgram();
   GLuint vert = glCreateShader(GL_VERTEX_SHADER);
   GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

   glShaderSource(vert, ARRAY_SIZE(vertex_shader), vertex_shader, 0);
   glShaderSource(frag, ARRAY_SIZE(fragment_shader), fragment_shader, 0);
   glCompileShader(vert);
   glCompileShader(frag);

   int status = 0;
   glGetShaderiv(vert, GL_COMPILE_STATUS, &status);
   if (!status && log_cb)
   {
      log_cb(RETRO_LOG_ERROR, "Vertex shader failed to compile!\n");
      print_shader_log(vert);
   }
   glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
   if (!status && log_cb)
   {
      log_cb(RETRO_LOG_ERROR, "Fragment shader failed to compile!\n");
      print_shader_log(frag);
   }

   glAttachShader(prog, vert);
   glAttachShader(prog, frag);
   glLinkProgram(prog);

   glGetProgramiv(prog, GL_LINK_STATUS, &status);
   if (!status && log_cb)
      log_cb(RETRO_LOG_ERROR, "Program failed to link!\n");


   background_prog        = glCreateProgram();
   GLuint background_vert = glCreateShader(GL_VERTEX_SHADER);
   GLuint background_frag = glCreateShader(GL_FRAGMENT_SHADER);

   glShaderSource(background_vert, ARRAY_SIZE(background_vertex_shader), background_vertex_shader, 0);
   glShaderSource(background_frag, ARRAY_SIZE(background_fragment_shader), background_fragment_shader, 0);
   glCompileShader(background_vert);
   glCompileShader(background_frag);

   glGetShaderiv(background_vert, GL_COMPILE_STATUS, &status);
   if (!status && log_cb)
   {
      log_cb(RETRO_LOG_ERROR, "Background vertex shader failed to compile!\n");
      print_shader_log(background_vert);
   }
   glGetShaderiv(background_frag, GL_COMPILE_STATUS, &status);
   if (!status && log_cb)
   {
      log_cb(RETRO_LOG_ERROR, "Background fragment shader failed to compile!\n");
      print_shader_log(background_frag);
   }

   glAttachShader(background_prog, background_vert);
   glAttachShader(background_prog, background_frag);
   glLinkProgram(background_prog);

   glGetProgramiv(background_prog, GL_LINK_STATUS, &status);
   if (!status && log_cb)
      log_cb(RETRO_LOG_ERROR, "Background program failed to link!\n");

   glBindBuffer(GL_ARRAY_BUFFER, background_vbo);
   int vloc = glGetAttribLocation(background_prog, "VertexCoord");
   glVertexAttribPointer(vloc, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)(0));
   glEnableVertexAttribArray(vloc);
   int tcloc = glGetAttribLocation(background_prog, "TexCoord");
   glVertexAttribPointer(tcloc, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)(sizeof(GLfloat) * 2));
   glEnableVertexAttribArray(tcloc);

   glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat) * 4,
         &background_data[0], GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDisableVertexAttribArray(tcloc);
   glDisableVertexAttribArray(vloc);

   tex = 0;

   if (!camera_enable)
      tex = load_texture(texpath.c_str());
}

static void instancingviewer_context_reset(void)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Context reset!\n");

   rglgen_resolve_symbols(hw_render.get_proc_address);

   glGenBuffers(1, &vbo);
   glGenBuffers(1, &background_vbo);
   instancingviewer_compile_shaders();
   update = true;
}

static void instancingviewer_update_variables(retro_environment_t environ_cb)
{
   struct retro_variable var;

   var.key = "3dengine-cube-size";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      cube_size = atoi(var.value);
      update = true;

      if (!first_init)
         instancingviewer_context_reset();
   }

   var.key = "3dengine-cube-stride";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      cube_stride = atof(var.value);
      update = true;

      if (!first_init)
         instancingviewer_context_reset();
   }
}

static void instancingviewer_run(void)
{
   vec3 look_dir = instancingviewer_check_input();

   glBindFramebuffer(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
   glClearColor(0.1, 0.1, 0.1, 1.0);
   glViewport(0, 0, engine_width, engine_height);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glUseProgram(background_prog);

   glDisable(GL_DEPTH_TEST);
   glEnable(GL_CULL_FACE);

   int texloc = glGetUniformLocation(background_prog, "Texture");
   glUniform1i(texloc, 0);
   glActiveTexture(GL_TEXTURE0);
   glBindBuffer(GL_ARRAY_BUFFER, background_vbo);
   int vloc = glGetAttribLocation(background_prog, "VertexCoord");
   glVertexAttribPointer(vloc, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)(0));
   glEnableVertexAttribArray(vloc);
   int tcloc = glGetAttribLocation(background_prog, "TexCoord");
   glVertexAttribPointer(tcloc, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)(sizeof(GLfloat) * 2));
   glEnableVertexAttribArray(tcloc);

   glBindTexture(g_texture_target, tex);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisableVertexAttribArray(tcloc);
   glDisableVertexAttribArray(vloc);

   glUseProgram(prog);

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_CULL_FACE);

   int tloc = glGetUniformLocation(prog, "uTexture");
   glUniform1i(tloc, 0);
   glActiveTexture(GL_TEXTURE0);

   glBindTexture(g_texture_target, tex);

   int lloc = glGetUniformLocation(prog, "light_pos");

   vec3_t light_pos;

   light_pos[0] = light_r;
   light_pos[1] = light_g;
   light_pos[2] = light_b;

   glUniform3fv(lloc, 1, &light_pos[0]);

   vec4 ambient_light(ambient_light_r, ambient_light_g, ambient_light_b, ambient_light_a);
   lloc = glGetUniformLocation(prog, "ambient_light");
   glUniform4fv(lloc, 1, &ambient_light[0]);

   int vploc = glGetUniformLocation(prog, "uVP");
   mat4 view = lookAt(player_pos, player_pos + look_dir, vec3(0, 1, 0));
   mat4 proj = scale(mat4(1.0), vec3(1, -1, 1)) * perspective(45.0f, 640.0f / 480.0f, 5.0f, 500.0f);
   mat4 vp = proj * view;
   glUniformMatrix4fv(vploc, 1, GL_FALSE, &vp[0][0]);

   int modelloc = glGetUniformLocation(prog, "uM");
   mat4 model = mat4(1.0);
   glUniformMatrix4fv(modelloc, 1, GL_FALSE, &model[0][0]);

   display_cubes_array();

   glUseProgram(0);
   glBindTexture(g_texture_target, 0);

   video_cb(RETRO_HW_FRAME_BUFFER_VALID, engine_width, engine_height, 0);
}

static void instancingviewer_load_game(const struct retro_game_info *info)
{
   player_pos      = vec3(0, 0, 0);
   texpath         = info->path;
   first_init      = false;

   light_r         = 0;
   light_g         = 150;
   light_b         = 15;
   ambient_light_r = 0.2;
   ambient_light_g = 0.2;
   ambient_light_b = 0.2;
   ambient_light_a = 1.0;
}

const engine_program_t engine_program_instancingviewer = {
   instancingviewer_load_game,
   instancingviewer_run,
   instancingviewer_context_reset,
   instancingviewer_update_variables,
   instancingviewer_check_input,
};
