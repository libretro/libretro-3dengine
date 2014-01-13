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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct
{
   double latitude;
   double longitude;
   double horizontal_accuracy;
   double vertical_accuracy;
} retro_position_t;

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
