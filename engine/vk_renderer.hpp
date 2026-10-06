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
#ifndef VK_RENDERER_HPP__
#define VK_RENDERER_HPP__

#ifdef HAVE_VULKAN

#include <libretro.h>
#include <vector>
#include "glm/glm.hpp"
#include "object.hpp"

/* Minimal Vulkan mesh renderer used by the model viewer / scene walker
 * when the frontend runs the core on a Vulkan context.
 *
 * Matrices passed in are the same OpenGL-convention matrices the GL path
 * uses (perspective() / vr_projection() / lookAt()); this module applies
 * the GL->Vulkan clip space correction (Y down, depth 0..1) itself, so the
 * caller must NOT pre-flip Y. */
namespace VKR
{
   enum ShaderMode
   {
      SHADER_MODEL         = 0,
      SHADER_MODEL_DISCARD = 1,
      SHADER_SCENE         = 2
   };

   struct Pass
   {
      glm::mat4 proj;      /* GL-convention projection */
      glm::mat4 view;
      glm::vec3 eye_pos;
      unsigned x;          /* viewport/scissor origin (pixels) */
      unsigned width;
      unsigned height;
   };

   struct Frame
   {
      glm::mat4 model;
      glm::vec3 light;     /* direction (model viewer) or position (scene) */
      glm::vec3 ambient;
      unsigned width;      /* size of the image handed to the frontend */
      unsigned height;
      unsigned num_passes; /* 1 = flat, 2 = side-by-side stereo */
      Pass passes[2];
   };

   /* Call right after RETRO_ENVIRONMENT_SET_HW_RENDER(VULKAN) succeeded. */
   bool set_negotiation_interface(retro_environment_t env);

   /* context_reset: fetch the frontend's Vulkan interface, build pipelines. */
   bool init(retro_environment_t env);

   /* context_destroy: device is still alive, free everything. */
   void destroy(void);

   /* True between a successful init() and destroy(). */
   bool initialized(void);

   bool load_scene(const std::vector<OBJ::Part>& parts, ShaderMode mode);
   void unload_scene(void);

   /* Renders, hands the image to the frontend with set_image() and calls
    * video_cb(RETRO_HW_FRAME_BUFFER_VALID, ...). Returns false (and does
    * not call video_cb) if nothing could be rendered. */
   bool render(const Frame& frame, retro_video_refresh_t video_cb);
}

#endif /* HAVE_VULKAN */
#endif
