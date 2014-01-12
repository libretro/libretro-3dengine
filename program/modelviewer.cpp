#include "libretro.h"
#include "program.h"
#include "../engine/mesh.hpp"
#include "../engine/texture.hpp"
#include "../engine/object.hpp"

#include <vector>
#include <string>

static bool first_init = true;
static std::string mesh_path;
static bool discard_hack_enable = false;

static std::vector<std1::shared_ptr<GL::Mesh> > meshes;
static std1::shared_ptr<GL::Texture> blank;

static bool update;

static glm::vec3 player_pos;

static glm::vec3 program_check_input(void)
{
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

   model_scale *= 1.0f - analog_ry * 0.000001f;
   model_scale = glm::clamp(model_scale, 0.0001f, 100.0f);
   model_rotate_x += analog_y * 0.0001f;
   model_rotate_y += analog_x * 0.00015f;

   glm::mat4 translation = glm::translate(glm::mat4(1.0), glm::vec3(0, 0, -40));
   glm::mat4 scaler = glm::scale(glm::mat4(1.0), glm::vec3(model_scale, model_scale, model_scale));
   glm::mat4 rotate_x = glm::rotate(glm::mat4(1.0), model_rotate_x, glm::vec3(1, 0, 0));
   glm::mat4 rotate_y = glm::rotate(glm::mat4(1.0), model_rotate_y, glm::vec3(0, 1, 0));

   glm::mat4 model = translation * scaler * rotate_x * rotate_y;

   for (unsigned i = 0; i < meshes.size(); i++)
      meshes[i]->set_model(model);
   //check_collision_cube();

   return player_pos;
}

void program_context_reset(void)
{
   meshes.clear();
   blank.reset();

   update = true;
}

void program_update_variables(retro_environment_t environ_cb)
{
   (void)environ_cb;
#if 0
   struct retro_variable var;

   var.key = "3dengine-modelviewer-discard-hack";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      if (strcmp(var.value, "disabled") == 0)
         discard_hack_enable = false;
      else if (strcmp(var.value, "enabled") == 0)
         discard_hack_enable = true;
   }
#endif
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

      "uniform vec3 uLightDir;\n"
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
      "  float directivity = dot(uLightDir, -normal);\n"

      "  vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);\n"
      "  vec3 ambient = colorAmbient * uLightAmbient;\n"

      "  vec3 modelToFace = normalize(-vPos.xyz);\n"
      "  float specularity = pow(clamp(dot(modelToFace, reflect(uLightDir, normal)), 0.0, 1.0), uMTLSpecularPower);\n"
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

      "uniform vec3 uLightDir;\n"
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
      "  float directivity = dot(uLightDir, -normal);\n"

      "  vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);\n"
      "  vec3 ambient = colorAmbient * uLightAmbient;\n"

      "  vec3 modelToFace = normalize(-vPos.xyz);\n"
      "  float specularity = pow(clamp(dot(modelToFace, reflect(uLightDir, normal)), 0.0, 1.0), uMTLSpecularPower);\n"
      "  vec3 specular = uMTLSpecular * specularity;\n"

      "  gl_FragColor = vec4(diffuse + ambient + specular, uMTLAlphaMod * colorDiffuseFull.a);\n"
      "}";

   std1::shared_ptr<GL::Shader> shader(new GL::Shader(vertex_shader, (discard_hack_enable) ? fragment_shader_avoid_discard_hack : fragment_shader));
   meshes = OBJ::load_from_file(path);

   glm::mat4 projection = glm::scale(glm::mat4(1.0), glm::vec3(1, -1, 1)) * glm::perspective(45.0f, 640.0f / 480.0f, 1.0f, 100.0f);

   for (unsigned i = 0; i < meshes.size(); i++)
   {
      meshes[i]->set_projection(projection);
      meshes[i]->set_shader(shader);
      meshes[i]->set_blank(blank);
   }
}

void program_compile_shaders(void)
{
   blank = GL::Texture::blank();
   init_mesh(mesh_path);
}

void program_run(void)
{
   glm::vec3 look_dir = program_check_input();
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

void program_load_game(const struct retro_game_info *info)
{
   player_pos = glm::vec3(0, 0, 0);
   mesh_path = info->path;
   first_init = false;
}
