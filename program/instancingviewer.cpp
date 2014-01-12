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

static bool update;
static GLuint prog;
static GLuint vbo;
static float cube_stride = 4.0f;
static unsigned cube_size = 1;

static glm::vec3 player_pos;

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
   "#extension GL_OES_EGL_image_external : require\n"
#endif
#ifdef GLES
   "precision mediump float; \n",
#endif
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

static glm::vec3 program_check_input(void)
{
   static unsigned select_timeout = 0;
   input_poll_cb();

   int x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   int y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
   x = std::max(std::min(x, 20), -20);
   y = std::max(std::min(y, 20), -20);
   camera_rot_x -= 0.20 * x;
   camera_rot_y -= 0.10 * y;

   camera_rot_y = std::max(std::min(camera_rot_y, 80.0f), -80.0f);

   glm::mat4 look_rot_x = glm::rotate(glm::mat4(1.0), camera_rot_x, glm::vec3(0, 1, 0));
   glm::mat4 look_rot_y = glm::rotate(glm::mat4(1.0), camera_rot_y, glm::vec3(1, 0, 0));
   glm::vec3 look_dir = glm::vec3(look_rot_x * look_rot_y * glm::vec4(0, 0, -1, 0));

   glm::vec3 look_dir_side = glm::vec3(look_rot_x * glm::vec4(1, 0, 0, 0));

   glm::mat3 s = glm::mat3(glm::scale(glm::mat4(1.0), glm::vec3(0.25, 0.25, 0.25)));
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

   //check_collision_cube();

   return look_dir;
}

void program_context_reset(void)
{
   SYM(glGenBuffers)(1, &vbo);
   update = true;
}

void program_update_variables(retro_environment_t environ_cb)
{
   struct retro_variable var;

   var.key = "3dengine-cube-size";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      cube_size = atoi(var.value);
      update = true;

      if (!first_init)
         context_reset();
   }

   var.key = "3dengine-cube-stride";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      cube_stride = atof(var.value);
      update = true;

      if (!first_init)
         context_reset();
   }
}

static void print_shader_log(GLuint shader)
{
   GLsizei len = 0;
   SYM(glGetShaderiv)(shader, GL_INFO_LOG_LENGTH, &len);
   if (!len)
      return;

   char *buffer = new char[len];
   SYM(glGetShaderInfoLog)(shader, len, &len, buffer);
   log_cb(RETRO_LOG_INFO, ":%s\n", buffer);
   delete[] buffer;
}

void program_compile_shaders(void)
{
   prog = SYM(glCreateProgram)();
   GLuint vert = SYM(glCreateShader)(GL_VERTEX_SHADER);
   GLuint frag = SYM(glCreateShader)(GL_FRAGMENT_SHADER);

   SYM(glShaderSource)(vert, ARRAY_SIZE(vertex_shader), vertex_shader, 0);
   SYM(glShaderSource)(frag, ARRAY_SIZE(fragment_shader), fragment_shader, 0);
   SYM(glCompileShader)(vert);
   SYM(glCompileShader)(frag);

   int status = 0;
   SYM(glGetShaderiv)(vert, GL_COMPILE_STATUS, &status);
   if (!status && log_cb)
   {
      log_cb(RETRO_LOG_ERROR, "Vertex shader failed to compile!\n");
      print_shader_log(vert);
   }
   SYM(glGetShaderiv)(frag, GL_COMPILE_STATUS, &status);
   if (!status && log_cb)
   {
      log_cb(RETRO_LOG_ERROR, "Fragment shader failed to compile!\n");
      print_shader_log(frag);
   }

   SYM(glAttachShader)(prog, vert);
   SYM(glAttachShader)(prog, frag);
   SYM(glLinkProgram)(prog);

   SYM(glGetProgramiv)(prog, GL_LINK_STATUS, &status);
   if (!status && log_cb)
      log_cb(RETRO_LOG_ERROR, "Program failed to link!\n");

   program_context_reset();
   tex = 0;
}

void program_run(void)
{
   glm::vec3 look_dir = program_check_input();

   SYM(glUseProgram)(prog);

   SYM(glEnable)(GL_DEPTH_TEST);
   SYM(glEnable)(GL_CULL_FACE);

   int tloc = SYM(glGetUniformLocation)(prog, "uTexture");
   SYM(glUniform1i)(tloc, 0);
   SYM(glActiveTexture)(GL_TEXTURE0);

   SYM(glBindTexture)(g_texture_target, tex);

   int lloc = SYM(glGetUniformLocation)(prog, "light_pos");
   glm::vec3 light_pos(0, 150, 15);
   SYM(glUniform3fv)(lloc, 1, &light_pos[0]);

   glm::vec4 ambient_light(0.2, 0.2, 0.2, 1.0);
   lloc = SYM(glGetUniformLocation)(prog, "ambient_light");
   SYM(glUniform4fv)(lloc, 1, &ambient_light[0]);

   int vploc = SYM(glGetUniformLocation)(prog, "uVP");
   glm::mat4 view = glm::lookAt(player_pos, player_pos + look_dir, glm::vec3(0, 1, 0));
   glm::mat4 proj = glm::scale(glm::mat4(1.0), glm::vec3(1, -1, 1)) * glm::perspective(45.0f, 640.0f / 480.0f, 5.0f, 500.0f);
   glm::mat4 vp = proj * view;
   SYM(glUniformMatrix4fv)(vploc, 1, GL_FALSE, &vp[0][0]);

   int modelloc = SYM(glGetUniformLocation)(prog, "uM");
   glm::mat4 model = glm::mat4(1.0);
   SYM(glUniformMatrix4fv)(modelloc, 1, GL_FALSE, &model[0][0]);

   display_cubes_array();

   SYM(glUseProgram)(0);
   SYM(glBindTexture)(g_texture_target, 0);
}

void program_load_game(const struct retro_game_info *info)
{
   player_pos = glm::vec3(0, 0, 0);
   texpath = info->path;
   first_init = false;
}
