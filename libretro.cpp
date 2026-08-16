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

#include "libretro.h"
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>
#include "rpng.h"
#include "program.h"
#include "gl.hpp"

#define FPS 60.0

retro_position_t previous_location;
retro_position_t current_location;

bool renderer_dead_state = true;
bool sensor_initialized = false;
bool sensor_enable = false;
bool location_camera_control_enable = false;
static bool location_enable = false;
bool camera_enable = false;
struct retro_hw_render_callback hw_render;
static struct retro_camera_callback camera_cb;
retro_log_printf_t log_cb;
struct retro_location_callback location_cb;
retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
retro_environment_t environ_cb;
retro_input_poll_t input_poll_cb;
retro_input_state_t input_state_cb;
static const engine_program_t *engine_program_cb;
struct retro_sensor_interface sensor_cb;

static bool display_position;

#define BASE_WIDTH 320
#define BASE_HEIGHT 240
#ifdef HAVE_OPENGLES
#define MAX_WIDTH 1024
#define MAX_HEIGHT 1024
#else
#define MAX_WIDTH 2048
#define MAX_HEIGHT 2048
#endif

unsigned engine_width = BASE_WIDTH;
unsigned engine_height = BASE_HEIGHT;

GLuint tex;
GLuint g_texture_target = GL_TEXTURE_2D;

void retro_init(void)
{
   struct retro_log_callback log;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   environ_cb(RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE, &sensor_cb);
}

void retro_deinit(void)
{
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "Libretro 3DEngine";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version  = "v1" GIT_VERSION;
   info->need_fullpath    = false;
   info->valid_extensions = "png|jpg|mtl|obj";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps = FPS;
   info->timing.sample_rate = 30000.0;

   info->geometry.base_width  = BASE_WIDTH;
   info->geometry.base_height = BASE_HEIGHT;
   info->geometry.max_width   = MAX_WIDTH;
   info->geometry.max_height  = MAX_HEIGHT;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {
      { "3dengine-resolution",
         "Internal resolution; 320x240|360x480|480x272|512x384|512x512|640x240|640x448|640x480|720x576|800x600|960x720|1024x768"
#ifndef HAVE_OPENGLES
      "|1024x1024|1280x720|1280x960|1600x1200|1920x1080|1920x1440|1920x1600|2048x1152|2048x1536|2048x2048|320x240"
#endif
      },
                        {
         "3dengine-cube-size",
         "Cube size; 0|1|2|4|8|16|32|64|128" },
                        {
         "3dengine-cube-stride",
         "Cube stride; 2.0|3.0|4.0|5.0|6.0|7.0|8.0" },
                        {
         "3dengine-camera-enable",
         "Camera enable; disabled|enabled" },
                        {
         "3dengine-camera-type",
         "Camera FB Type; texture|raw framebuffer" },
                        {
                           "3dengine-sensor-enable",
                           "Sensor enable; disabled|enabled"
                        },
                        {
                           "3dengine-location-enable",
                           "Location enable; disabled|enabled"
                        },
                        {
                           "3dengine-location-camera-control-enable",
                           "Location camera control; disabled|enabled"
                        },
                  { "3dengine-modelviewer-discard-hack", "Discard hack enable; disabled|enabled" },
                  { "3dengine-location-display-position", "Location position OSD; disabled|enabled" },
      { NULL, NULL },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

static void update_variables(void)
{
   struct retro_variable var;

   var.key = "3dengine-resolution";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      snprintf(str, sizeof(str), "%s", var.value);
      
      pch = strtok(str, "x");
      if (pch)
         engine_width = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         engine_height = strtoul(pch, NULL, 0);

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Got size: %u x %u.\n", engine_width, engine_height);
   }

   var.key = "3dengine-location-display-position";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         display_position = false;
      else if (!strcmp(var.value, "enabled"))
         display_position = true;
   }

   var.key = "3dengine-location-camera-control-enable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         location_camera_control_enable = false;
      else if (!strcmp(var.value, "enabled"))
         location_camera_control_enable = true;
   }

   if (engine_program_cb && engine_program_cb->update_variables)
      engine_program_cb->update_variables(environ_cb);
}

void retro_run(void)
{
   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   if (location_enable && location_cb.get_position)
   {
      double lat, lon, hacc, vacc;
      if (location_cb.get_position(&lat, &lon, &hacc, &vacc))
      {
         previous_location = current_location;

         current_location.latitude = lat;
         current_location.longitude = lon;
         current_location.horizontal_accuracy = hacc;
         current_location.vertical_accuracy = vacc;

         if (display_position)
         {
            struct retro_message msg; 
            char msg_local[512];
            snprintf(msg_local, sizeof(msg_local), "LAT %f LON %f HACC %f VACC %f", current_location.latitude, current_location.longitude,
                  current_location.horizontal_accuracy, current_location.vertical_accuracy);
            msg.msg = msg_local;
            msg.frames = 180;
            environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, (void*)&msg);
         }
      }
   }

   if (engine_program_cb && engine_program_cb->run)
      engine_program_cb->run();
}

static void camera_gl_callback(unsigned texture_id, unsigned texture_target, const float *affine)
{
   g_texture_target = texture_target;
   // TODO: support GL_TEXTURE_RECTANGLE and others?
   if (texture_target == GL_TEXTURE_2D)
      tex = texture_id;
#ifdef ANDROID
   else if (texture_target == GL_TEXTURE_EXTERNAL_OES)
      tex = texture_id;
#endif
}

static inline bool gl_query_extension(const char *ext)
{
#if !defined(ANDROID) && !defined(EMSCRIPTEN)
   // This code crashes right now on Android 4.4 (but not 4.0 to 4.3), so comment it out for now
   const char *str = (const char*)glGetString(GL_EXTENSIONS);
   bool ret = str && strstr(str, ext);

   return ret;
#else
   return false;
#endif
}

static bool support_unpack_row_length;
static uint8_t *convert_buffer;

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH  0x0CF2
#endif

#ifdef HAVE_OPENGLES
#define INTERNAL_FORMAT GL_BGRA_EXT
#define TEX_TYPE        GL_BGRA_EXT
#define TEX_FORMAT      GL_UNSIGNED_BYTE
#else
#define INTERNAL_FORMAT GL_RGBA
#define TEX_TYPE        GL_BGRA
#define TEX_FORMAT      GL_UNSIGNED_INT_8_8_8_8_REV
#endif

static void camera_raw_fb_callback(const uint32_t *buffer, unsigned width, unsigned height, size_t pitch)
{
   unsigned base_size = 4;
   unsigned h;

   if (!tex)
   {
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, INTERNAL_FORMAT, width, height, 0, TEX_TYPE, TEX_FORMAT, NULL);
      if (!support_unpack_row_length)
         convert_buffer = new uint8_t[width * height * 4];
   }
   else
      glBindTexture(GL_TEXTURE_2D, tex);

   if (support_unpack_row_length)
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / base_size);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, TEX_TYPE,
            TEX_FORMAT, buffer);

      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   }
   else
   {
      // No GL_UNPACK_ROW_LENGTH ;(
      unsigned pitch_width = pitch / base_size;
      if (width == pitch_width) // Happy path :D
      {
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, TEX_TYPE,
               TEX_FORMAT, buffer);
      }
      else // Slower path.
      {
         const unsigned line_bytes = width * base_size;

         uint8_t *dst = (uint8_t*)convert_buffer; // This buffer is preallocated for this purpose.
         const uint8_t *src = (const uint8_t*)buffer;

         for (h = 0; h < height; h++, src += pitch, dst += line_bytes)
            memcpy(dst, src, line_bytes);

         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, TEX_TYPE,
               TEX_FORMAT, convert_buffer);         
      }
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

static void camera_initialized(void)
{
   if (camera_enable && camera_cb.start)
      camera_cb.start();
}

static void context_reset(void)
{
   if (engine_program_cb && engine_program_cb->context_reset)
      engine_program_cb->context_reset();
}

char retro_path_info[1024];

static void location_initialized(void)
{
   if (location_enable && location_cb.start)
      location_cb.start();
}

static void location_deinitialized(void)
{
   if (location_enable && location_cb.stop)
      location_cb.stop();
}

bool retro_load_game(const struct retro_game_info *info)
{
   retro_variable var;

   if (!info)
      return false;

   strcpy(retro_path_info, info->path);
   if (strstr(info->path, ".obj") || strstr(info->path, ".mtl"))
      engine_program_cb = &engine_program_modelviewer;
   else
      engine_program_cb = &engine_program_instancingviewer;

   update_variables();

   var.key = "3dengine-sensor-enable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      if (!strcmp(var.value, "enabled"))
      {
         if (sensor_initialized)
            return true;

         log_cb(RETRO_LOG_INFO, "Sensor interface found, enabling...\n");
         if (sensor_cb.set_sensor_state)
         {
            sensor_cb.set_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_ENABLE, FPS);
            sensor_enable = true;
         }

         sensor_initialized = true;
      }
      if (!strcmp(var.value, "disabled"))
      {
         if (sensor_initialized)
            return true;

         log_cb(RETRO_LOG_INFO, "Sensor interface found, disabling...\n");
         if (sensor_cb.set_sensor_state)
         {
            sensor_cb.set_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_DISABLE, FPS);
            sensor_enable = false;
         }
      }
   }

   var.key = "3dengine-location-enable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
      {
         location_enable = true;
         location_cb.initialized = location_initialized;
         location_cb.deinitialized = location_deinitialized;
         environ_cb(RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE, &location_cb);
      }
   }

   memset(&camera_cb, 0, sizeof(camera_cb));

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "XRGB8888 is not supported.\n");
      return false;
   }

   var.key = "3dengine-camera-enable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
      {
         camera_enable = true;
         var.key = "3dengine-camera-type";
         var.value = NULL;

         if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
         {
            if (!strcmp(var.value, "texture"))
            {
               camera_cb.caps = (1 << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE);
               camera_cb.frame_opengl_texture = camera_gl_callback;
            }
            else
            {
               camera_cb.caps = (1 << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER);
               camera_cb.frame_raw_framebuffer = camera_raw_fb_callback;
            }
         }

         camera_cb.initialized = camera_initialized;

         if (camera_enable && !environ_cb(RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE, &camera_cb))
         {
            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "camera is not supported.\n");
            return false;
         }
      }
   }

#ifdef HAVE_OPENGLES
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2;
#else
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
   hw_render.context_reset = context_reset;
   hw_render.depth = true;
   if (!camera_enable)
      hw_render.bottom_left_origin = true;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;

#ifdef HAVE_OPENGLES
   if (camera_enable && camera_cb.caps & (1 << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER) && !gl_query_extension("BGRA8888"))
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "no BGRA8888 support for raw framebuffer, exiting...\n");
      return false;
   }
   support_unpack_row_length = gl_query_extension("GL_EXT_unpack_subimage");
#else
   support_unpack_row_length = true;
#endif

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Loaded game!\n");


   if (engine_program_cb && engine_program_cb->load_game)
      engine_program_cb->load_game(info);

   return true;
}

void retro_unload_game(void)
{
   renderer_dead_state = true;

   if (convert_buffer)
      delete[] convert_buffer;
   convert_buffer = NULL;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_reset(void)
{
   if (engine_program_cb && engine_program_cb->context_reset)
      engine_program_cb->context_reset();
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

