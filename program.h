#ifndef PROGRAM_H__
#define PROGRAM_H__

#include "libretro.h"
#include "gl.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

extern void program_run(void);
extern void program_context_reset(void);
extern void program_update_variables(retro_environment_t environ_cb);
extern void program_load_game(const struct retro_game_info *info);
extern void program_compile_shaders(void);

extern void context_reset(void);

extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;

extern GLuint g_texture_target;
extern GLuint tex;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#endif
