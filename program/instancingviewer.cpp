#include "libretro.h"
#include "program.h"

#include <vector>

#define CUBE_VERTS 36

static bool first_init = true;
static std::string texpath;

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

extern bool update;
extern GLuint prog;
GLuint vbo;
static float cube_stride = 4.0f;
static unsigned cube_size = 1;

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
   SYM(glBindBuffer)(GL_ARRAY_BUFFER, vbo);
   int vloc = SYM(glGetAttribLocation)(prog, "aVertex");
   SYM(glVertexAttribPointer)(vloc, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, vert)));
   SYM(glEnableVertexAttribArray)(vloc);
   int nloc = SYM(glGetAttribLocation)(prog, "aNormal");
   SYM(glVertexAttribPointer)(nloc, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal)));
   SYM(glEnableVertexAttribArray)(nloc);
   int tcloc = SYM(glGetAttribLocation)(prog, "aTexCoord");
   SYM(glVertexAttribPointer)(tcloc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, tex)));
   SYM(glEnableVertexAttribArray)(tcloc);

   if (update)
   {
      update = false;

      std::vector<Cube> cubes;
      cubes.resize(cube_size * cube_size * cube_size);

      for (unsigned x = 0; x < cube_size; x++)
      {
         for (unsigned y = 0; y < cube_size; y++)
         {
            for (unsigned z = 0; z < cube_size; z++)
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
      SYM(glBufferData)(GL_ARRAY_BUFFER, cube_size * cube_size * cube_size * sizeof(Cube),
            &cubes[0], GL_STATIC_DRAW);
      SYM(glBindBuffer)(GL_ARRAY_BUFFER, 0);
   }

   SYM(glDrawArrays)(GL_TRIANGLES, 0, CUBE_VERTS * cube_size * cube_size * cube_size);
   SYM(glBindBuffer)(GL_ARRAY_BUFFER, 0);
   SYM(glDisableVertexAttribArray)(vloc);
   SYM(glDisableVertexAttribArray)(nloc);
   SYM(glDisableVertexAttribArray)(tcloc);
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

void program_check_input(glm::mat4 look_rot_x, glm::mat4 look_rot_y,
      glm::vec3 look_dir, glm::vec3 look_dir_side, glm::vec3 player_pos)
{
   (void)look_rot_x;
   (void)look_rot_y;
   (void)look_dir;
   (void)look_dir_side;
   (void)player_pos;
   //check_collision_cube();
}

void program_context_reset(void)
{
   SYM(glGenBuffers)(1, &vbo);
   update = true;
}

void program_update_variables(retro_environment_t environ_cb)
{
   struct retro_variable var;

   var.key = "cube_size";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      cube_size = atoi(var.value);
      update = true;

      if (!first_init)
         context_reset();
   }

   var.key = "cube_stride";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      cube_stride = atof(var.value);
      update = true;

      if (!first_init)
         context_reset();
   }
}

void program_run(void)
{
   display_cubes_array();
}

void program_load_game(const struct retro_game_info *info)
{
   texpath = info->path;
   first_init = false;
}
