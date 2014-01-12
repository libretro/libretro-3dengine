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
extern void program_check_input(glm::mat4 look_rot_x, glm::mat4 look_rot_y,
      glm::vec3 look_dir, glm::vec3 look_dir_side, glm::vec3 player_pos);

extern void context_reset(void);

#endif
