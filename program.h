/*
 *  Libretro 3DEngine
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

#ifndef PROGRAM_H__
#define PROGRAM_H__

#include "libretro.h"
#include "gl.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;
extern struct retro_hw_render_callback hw_render;
extern retro_video_refresh_t video_cb;

extern unsigned engine_width;
extern unsigned engine_height;

extern GLuint g_texture_target;
extern GLuint tex;

extern bool location_camera_control_enable;
extern bool sensor_enable;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct
{
   float latitude;
   float longitude;
   float horizontal_accuracy;
   float vertical_accuracy;
} retro_position_t;

extern retro_position_t previous_location;
extern retro_position_t current_location;

typedef struct engine_program
{
   void (*load_game)(const struct retro_game_info *info);
   void (*run)(void);
   void (*context_reset)(void);
   void (*update_variables)(retro_environment_t environ_cb);
   glm::vec3 (*check_input)(void);
} engine_program_t;

extern const engine_program_t engine_program_instancingviewer;
extern const engine_program_t engine_program_modelviewer;

#endif
