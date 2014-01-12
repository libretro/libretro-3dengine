#include "libretro.h"
#include "program.h"
#include "../engine/mesh.hpp"
#include "../engine/texture.hpp"
#include "../engine/object.hpp"

#include <vector>
#include <string>

using namespace glm;

static bool first_init = true;
static std::string mesh_path;
static bool discard_hack_enable = false;

struct Triangle
{
   vec3 a, b, c;
   vec3 normal;
   float n0;
};

static std::vector<std1::shared_ptr<GL::Mesh> > meshes;
static std::vector<Triangle> triangles;
static std1::shared_ptr<GL::Texture> blank;

//forward decls
static void collision_detection(vec3& player_pos, vec3& velocity);
static void wall_hug_detection(vec3& player_pos);
static void scenewalker_reset_mesh_path(void);

static float ambient_light_r;
static float ambient_light_g;
static float ambient_light_b;

static bool update;

static vec3 player_size(0.4f, 0.8f, 0.4f);

enum {
   MODE_MODELVIEWER = 0,
   MODE_SCENEWALKER = 1,
};

unsigned mode_engine = MODE_MODELVIEWER;

static vec3 scenewalker_check_input(void)
{
   static float player_view_deg_x;
   static float player_view_deg_y;
   static vec3 player_pos(0, 2, 0);

   input_poll_cb();

   int analog_x = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);

   int analog_y = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

   int analog_ry = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

   int analog_rx = input_state_cb(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);

   static bool old_jump;
   bool new_jump = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_B);

   bool jump = new_jump && !old_jump;
   old_jump = new_jump;

   bool run_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_Y);
   bool mouselook_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_X);

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_LEFT))
      analog_rx = run_pressed ? -32767 : -16384;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_RIGHT))
      analog_rx = run_pressed ? 32767 : 16384;

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_UP))
   {
      if (mouselook_pressed)
         analog_ry = run_pressed ? -32767 : -16384;
      else
         analog_y = run_pressed ? -32767 : -16384;
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_DOWN))
   {
      if (mouselook_pressed)
         analog_ry = run_pressed ? 32767 : 16384;
      else
         analog_y = run_pressed ? 32767 : 16384;
   }


   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_L))
      analog_x = run_pressed ? -32767 : -16384;

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_R))
      analog_x = run_pressed ? 32767 : 16384;

   if (abs(analog_x) < 10000)
      analog_x = 0;
   if (abs(analog_y) < 10000)
      analog_y = 0;
   if (abs(analog_rx) < 10000)
      analog_rx = 0;
   if (abs(analog_ry) < 10000)
      analog_ry = 0;

#if 0
   if (log_cb)
   {
      log_cb(RETRO_LOG_INFO, "analog_x: %d\n", analog_x);
      log_cb(RETRO_LOG_INFO, "analog_y: %d\n", analog_y);
      log_cb(RETRO_LOG_INFO, "analog_rx: %d\n", analog_rx);
      log_cb(RETRO_LOG_INFO, "analog_ry: %d\n", analog_ry);
   }
#endif

   player_view_deg_y += analog_rx * -0.00008f;
   player_view_deg_x += analog_ry * -0.00005f;

   player_view_deg_x = clamp(player_view_deg_x, -80.0f, 80.0f);

   mat4 rotate_x = rotate(mat4(1.0), player_view_deg_x, vec3(1, 0, 0));
   mat4 rotate_y = rotate(mat4(1.0), player_view_deg_y, vec3(0, 1, 0));
   mat4 rotate_y_right = rotate(mat4(1.0), player_view_deg_y - 90.0f, vec3(0, 1, 0));

   vec3 look_dir = vec3(rotate_y * rotate_x * vec4(0, 0, -1, 1));

   vec3 right_walk_dir = vec3(rotate_y_right * vec4(0, 0, -1, 1));
   vec3 front_walk_dir = vec3(rotate_y * vec4(0, 0, -1, 1));

   vec3 velocity = front_walk_dir * vec3(analog_y * -0.000002f) +
      right_walk_dir * vec3(analog_x * 0.000002f);

   vec3 player_pos_espace = player_pos / player_size;
   vec3 velocity_espace = velocity / player_size;

   collision_detection(player_pos_espace, velocity_espace);
   player_pos_espace += velocity_espace;
   wall_hug_detection(player_pos_espace);

   static vec3 gravity;
   static bool can_jump;
   gravity += vec3(0.0f, -0.01f, 0.0f);
   if (can_jump && jump)
   {
      gravity[1] += 0.3f;
      can_jump = false;
   }
   gravity[1] -= gravity[1] * 0.01f;

   vec3 old_gravity = gravity;
   collision_detection(player_pos_espace, gravity);
   if (old_gravity[1] != gravity[1])
   {
      gravity = vec3(0.0f);
      can_jump = true;
   }

   player_pos_espace += gravity;
   wall_hug_detection(player_pos_espace);

   player_pos = player_pos_espace * player_size;

   mat4 view = lookAt(player_pos, player_pos + look_dir, vec3(0, 1, 0));

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
         ambient_light_r -= 0.01;
      else
         ambient_light_r += 0.01;
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
         ambient_light_g -= 0.01;
      else
         ambient_light_g += 0.01;
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
         ambient_light_b -= 0.01;
      else
         ambient_light_b += 0.01;
   }

   for (unsigned i = 0; i < meshes.size(); i++)
   {
      meshes[i]->set_view(view);
      meshes[i]->set_eye(player_pos);
      meshes[i]->set_ambient_lighting(ambient_light_r, ambient_light_g, ambient_light_b);
   }

   return player_size;
}

// Probably not the most efficient way to do collision handling ... :)
static inline bool inside_triangle(const Triangle& tri, const vec3& pos)
{
   vec3 real_normal = -tri.normal;

   vec3 ab = tri.b - tri.a;
   vec3 ac = tri.c - tri.a;
   vec3 ap = pos - tri.a;
   vec3 bp = pos - tri.b;
   vec3 bc = tri.c - tri.b;

   // Checks if point exists inside triangle.
   if (dot(cross(ab, ap), real_normal) < 0.0f)
      return false;

   if (dot(cross(ap, ac), real_normal) < 0.0f)
      return false;

   if (dot(cross(bc, bp), real_normal) < 0.0f)
      return false;

   return true;
}

static const float twiddle_factor = -0.5f;

// Here be dragons. 2-3 pages of mathematical derivations.
static inline float point_crash_time(const vec3& pos, const vec3& v, const vec3& edge)
{
   vec3 l = pos - edge;

   float A = dot(v, v);
   float B = 2 * dot(l, v);
   float C = dot(l, l) - 1;

   float d = B * B - 4.0f * A * C;
   if (d < 0.0f) // No solution, can't hit the sphere ever.
      return 10.0f; // Return number > 1.0f to signal no collision. Makes taking min() easier.

   float d_sqrt = std::sqrt(d);
   float sol0 = (-B + d_sqrt) / (2.0f * A);
   float sol1 = (-B - d_sqrt) / (2.0f * A);
   if (sol0 >= twiddle_factor && sol1 >= twiddle_factor)
      return std::min(sol0, sol1);
   else if (sol0 >= twiddle_factor && sol1 < twiddle_factor)
      return sol0;
   else if (sol0 < twiddle_factor && sol1 >= twiddle_factor)
      return sol1;

   return 10.0f;
}

static inline float line_crash_time(const vec3& pos, const vec3& v, const vec3& a, const vec3& b, vec3& crash_pos)
{
   crash_pos = vec3(0.0f);

   vec3 ab = b - a;
   vec3 d = pos - a;

   float ab_sqr = dot(ab, ab);
   float T = dot(ab, v) / ab_sqr;
   float S = dot(ab, d) / ab_sqr;

   vec3 V = v - vec3(T) * ab;
   vec3 W = d - vec3(S) * ab;

   float A = dot(V, V);
   float B = 2.0f * dot(V, W);
   float C = dot(W, W) - 1.0f;

   float D = B * B - 4.0f * A * C; 
   if (D < 0.0f) // No solutions exist :(
      return 10.0f;

   float D_sqrt = std::sqrt(D);
   float sol0 = (-B + D_sqrt) / (2.0f * A);
   float sol1 = (-B - D_sqrt) / (2.0f * A);

   float solution;
   if (sol0 >= twiddle_factor && sol1 >= twiddle_factor)
      solution = std::min(sol0, sol1);
   else if (sol0 >= twiddle_factor && sol1 < twiddle_factor)
      solution = sol0;
   else if (sol0 < twiddle_factor && sol1 >= twiddle_factor)
      solution = sol1;
   else
      return 10.0f;

   // Check if solution hits the actual line ...
   float k = dot(ab, d + vec3(solution) * v) / ab_sqr;
   if (k >= 0.0f && k <= 1.0f)
   {
      crash_pos = a + vec3(k) * ab;
      return solution;
   }
   else
      return 10.0f;
}
/////////// End dragons

static void wall_hug_detection(vec3& player_pos)
{
   float min_dist = 1.0f;
   const Triangle *closest_triangle_hug = 0;

   for (unsigned i = 0; i < triangles.size(); i++)
   {
      const Triangle& tri = triangles[i];
      float plane_dist = tri.n0 - dot(player_pos, tri.normal); 

      // Might be hugging too close.
      if (plane_dist >= -0.01f && plane_dist < min_dist)
      {
         vec3 projected_pos = player_pos + tri.normal * vec3(plane_dist); 
         if (inside_triangle(tri, projected_pos))
         {
            min_dist = plane_dist;
            closest_triangle_hug = &tri;
         }
      }
   }

   if (closest_triangle_hug)
   {
#if 0
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Fixup hugging: Dist: %.6f.\n", min_dist);
#endif
      // Push player out.
      player_pos += vec3(min_dist - 1.0f) * closest_triangle_hug->normal;
   }
}

static void collision_detection(vec3& player_pos, vec3& velocity)
{
   if (velocity == vec3(0.0))
      return;

   float min_time = 1.0f;
   bool crash = false;
   vec3 crash_point = vec3(0.0f);

   const Triangle *closest_triangle = 0;

   for (unsigned i = 0; i < triangles.size(); i++)
   {
      const Triangle& tri = triangles[i];

      float plane_dist = tri.n0 - dot(player_pos, tri.normal); 
      float towards_plane_v = dot(velocity, tri.normal);

      if (towards_plane_v > 0.00001f) // We're moving towards the plane.
      {
         float ticks_to_hit = (plane_dist - 1.0f) / towards_plane_v;

         // We'll hit the plane in this frame.
         if (ticks_to_hit >= 0.0f && ticks_to_hit < min_time)
         {
            vec3 projected_pos = (player_pos + tri.normal) + vec3(ticks_to_hit) * velocity; 

            if (inside_triangle(tri, projected_pos))
            {
               min_time = ticks_to_hit;
               closest_triangle = &tri;
               crash = false;
            }

         }
         else if (plane_dist >= 0.0f && plane_dist < 1.0f + towards_plane_v) // Can potentially hit vertex ...
         {
            vec3 crash_pos_tmp;
            vec3 crash_pos_ab, crash_pos_ac, crash_pos_bc;

            // Check how we can hit the triangle. Can hit edges or lines ...
            float min_time_crash = point_crash_time(player_pos, velocity, tri.a);
            crash_pos_tmp = tri.a;

            float time_point_b = point_crash_time(player_pos, velocity, tri.b);
            if (time_point_b < min_time_crash)
            {
               crash_pos_tmp  = tri.b;
               min_time_crash = time_point_b;
            }

            float time_point_c = point_crash_time(player_pos, velocity, tri.c);
            if (time_point_c < min_time_crash)
            {
               crash_pos_tmp  = tri.c;
               min_time_crash = time_point_c; 
            }

            float time_line_ab = line_crash_time(player_pos, velocity, tri.a, tri.b, crash_pos_ab);
            if (time_line_ab < min_time_crash)
            {
               crash_pos_tmp = crash_pos_ab;
               min_time_crash = time_line_ab;
            }

            float time_line_ac = line_crash_time(player_pos, velocity, tri.a, tri.c, crash_pos_ac);
            if (time_line_ac < min_time_crash)
            {
               crash_pos_tmp = crash_pos_ac;
               min_time_crash = time_line_ac;
            }

            float time_line_bc = line_crash_time(player_pos, velocity, tri.b, tri.c, crash_pos_bc);
            if (time_line_bc < min_time_crash)
            {
               crash_pos_tmp = crash_pos_bc;
               min_time_crash = time_line_bc;
            }

            if (min_time_crash < min_time)
            {
               min_time = min_time_crash;
               closest_triangle = &tri;
               crash = true;
               crash_point = crash_pos_tmp;
            }
         }
      }
   }

  
   if (closest_triangle)
   {
      if (!crash)
      {
         vec3 normal = closest_triangle->normal;

         // Move player to wall.
         player_pos += vec3(1.0f * min_time) * velocity;

         // Make velocity vector parallel with plane.
         velocity -= vec3(dot(velocity, normal)) * normal;

         // Used up some time moving to wall.
         velocity *= vec3(1.0f - min_time);
      }
      else
      {
         // Avoid possible numerical inaccuracies by going fully to crash point.
         player_pos += vec3(1.0f * min_time) * velocity;
         vec3 normal = crash_point - player_pos;
         velocity -= vec3(dot(velocity, normal) / dot(normal, normal)) * normal;
         velocity *= vec3(1.0f - min_time);
      }
   }
}

static vec3 modelviewer_check_input(void)
{
   if (mode_engine == MODE_SCENEWALKER)
      return scenewalker_check_input();

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

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
         ambient_light_r -= 0.01;
      else
         ambient_light_r += 0.01;
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
         ambient_light_g -= 0.01;
      else
         ambient_light_g += 0.01;
   }

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3))
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
         ambient_light_b -= 0.01;
      else
         ambient_light_b += 0.01;
   }

   model_scale *= 1.0f - analog_ry * 0.000001f;
   model_scale = clamp(model_scale, 0.0001f, 100.0f);
   model_rotate_x += analog_y * 0.0001f;
   model_rotate_y += analog_x * 0.00015f;

   mat4 translation = translate(mat4(1.0), vec3(0, 0, -40));
   mat4 scaler = scale(mat4(1.0), vec3(model_scale, model_scale, model_scale));
   mat4 rotate_x = rotate(mat4(1.0), model_rotate_x, vec3(1, 0, 0));
   mat4 rotate_y = rotate(mat4(1.0), model_rotate_y, vec3(0, 1, 0));

   mat4 model = translation * scaler * rotate_x * rotate_y;

   for (unsigned i = 0; i < meshes.size(); i++)
   {
      meshes[i]->set_model(model);
      meshes[i]->set_ambient_lighting(ambient_light_r, ambient_light_g, ambient_light_b);
   }
   //check_collision_cube();

   return player_size;
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

      "uniform vec3 uLightPos;\n"
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
      "  float directivity = dot(uLightPos, -normal);\n"

      "  vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);\n"
      "  vec3 ambient = colorAmbient * uLightAmbient;\n"

      "  vec3 modelToFace = normalize(-vPos.xyz);\n"
      "  float specularity = pow(clamp(dot(modelToFace, reflect(uLightPos, normal)), 0.0, 1.0), uMTLSpecularPower);\n"
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

      "uniform vec3 uLightPos;\n"
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
      "  float directivity = dot(uLightPos, -normal);\n"

      "  vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);\n"
      "  vec3 ambient = colorAmbient * uLightAmbient;\n"

      "  vec3 modelToFace = normalize(-vPos.xyz);\n"
      "  float specularity = pow(clamp(dot(modelToFace, reflect(uLightPos, normal)), 0.0, 1.0), uMTLSpecularPower);\n"
      "  vec3 specular = uMTLSpecular * specularity;\n"

      "  gl_FragColor = vec4(diffuse + ambient + specular, uMTLAlphaMod * colorDiffuseFull.a);\n"
      "}";

   static const std::string fragment_shader_scene =
      "#ifdef GL_ES\n"
      "precision mediump float;\n"
      "#endif\n"
      "varying vec2 vTex;\n"
      "varying vec4 vNormal;\n"
      "varying vec4 vPos;\n"

      "uniform sampler2D sDiffuse;\n"
      "uniform sampler2D sAmbient;\n"

      "uniform vec3 uLightPos;\n"
      "uniform vec3 uLightAmbient;\n"
      "uniform vec3 uEyePos;\n"
      "uniform vec3 uMTLAmbient;\n"
      "uniform float uMTLAlphaMod;\n"
      "uniform vec3 uMTLDiffuse;\n"
      "uniform vec3 uMTLSpecular;\n"
      "uniform float uMTLSpecularPower;\n"

      "void main() {\n"
      "  vec4 colorDiffuseFull = texture2D(sDiffuse, vTex);\n"
      "  vec4 colorAmbientFull = texture2D(sAmbient, vTex);\n"

      "  vec3 lightDir = normalize(vPos.xyz - uLightPos);\n"

      "  vec3 colorDiffuse = mix(uMTLDiffuse, colorDiffuseFull.rgb, vec3(colorDiffuseFull.a));\n"
      "  vec3 colorAmbient = mix(uMTLAmbient, colorAmbientFull.rgb, vec3(colorAmbientFull.a));\n"

      "  vec3 normal = normalize(vNormal.xyz);\n"
      "  float directivity = dot(lightDir, -normal);\n"

      "  vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);\n"
      "  vec3 ambient = colorAmbient * uLightAmbient;\n"

      "  vec3 modelToFace = normalize(uEyePos - vPos.xyz);\n"
      "  float specularity = pow(clamp(dot(modelToFace, reflect(lightDir, normal)), 0.0, 1.0), uMTLSpecularPower);\n"
      "  vec3 specular = uMTLSpecular * specularity;\n"

      "  gl_FragColor = vec4(diffuse + ambient + specular, uMTLAlphaMod * colorDiffuseFull.a);\n"
      "}";
   
   std1::shared_ptr<GL::Shader> shader(new GL::Shader(vertex_shader, (mode_engine == MODE_SCENEWALKER) ? fragment_shader_scene : ((discard_hack_enable) ? fragment_shader_avoid_discard_hack : fragment_shader)));
   meshes = OBJ::load_from_file(path);

   mat4 projection;
   if (mode_engine == MODE_SCENEWALKER)
      projection = scale(mat4(1.0), vec3(1, -1, 1)) * perspective(45.0f, 640.0f / 480.0f, 1.0f, 100.0f);
   else
      projection = scale(mat4(1.0), vec3(1, -1, 1)) * perspective(45.0f, 4.0f / 3.0f, 0.2f, 100.0f);

   for (unsigned i = 0; i < meshes.size(); i++)
   {
      meshes[i]->set_projection(projection);
      meshes[i]->set_shader(shader);
      meshes[i]->set_blank(blank);

      if (mode_engine == MODE_SCENEWALKER)
      {
         meshes[i]->set_scenewalker_default_lighting();

         const std::vector<GL::Vertex>& vertices = *meshes[i]->get_vertex();
         for (unsigned v = 0; v < vertices.size(); v += 3)
         {
            Triangle tri;
            tri.a = vertices[v + 0].vert / player_size;
            tri.b = vertices[v + 1].vert / player_size;
            tri.c = vertices[v + 2].vert / player_size;
            tri.normal = -normalize(cross(tri.b - tri.a, tri.c - tri.a)); // Make normals point inward. Makes for simpler computation.
            tri.n0 = dot(tri.normal, tri.a); // Plane constant
            triangles.push_back(tri);
         }
      }
   }

   ambient_light_r = 0.25f;
   ambient_light_g = 0.25f;
   ambient_light_b = 0.25f;
}

extern char retro_path_info[1024];

static void modelviewer_context_reset(void)
{
   GL::dead_state = true;
   meshes.clear();
   blank.reset();
   GL::dead_state = false;

   if (strstr(retro_path_info, ".mtl") || mode_engine == MODE_SCENEWALKER)
   {
      triangles.clear();
      scenewalker_reset_mesh_path();
      mode_engine = MODE_SCENEWALKER;
   }

   GL::set_function_cb(hw_render.get_proc_address);
   GL::init_symbol_map();

   blank = GL::Texture::blank();
   init_mesh(mesh_path);

   update = true;
}

static void modelviewer_update_variables(retro_environment_t environ_cb)
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


static void modelviewer_run(void)
{
   vec3 look_dir = modelviewer_check_input();
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

static inline bool fequal(float a, float b)
{
   return std::fabs(a - b) < 0.0001f;
}

static inline bool vequal(const vec3& a, const vec3& b)
{
   return fequal(a[0], b[0]) &&
      fequal(a[1], b[1]) &&
      fequal(a[2], b[2]);
}

static void test_crash_detection(void)
{
   vec3 pos = vec3(0.0f);
   
   float a = point_crash_time(pos, vec3(1, 0, 0), vec3(3, 0, 0));
   assert(fequal(a, 2.0f));

   float b = point_crash_time(pos, vec3(1, 0, 0), vec3(2, 2, 0));
   assert(fequal(b, 10.0f));

   float c = point_crash_time(pos, vec3(1, 0, 0), vec3(1.0, 0.5, 0.0));
   assert(fequal(c, 1.0f - std::cos(30.0f / 180.0f * M_PI)));

   float d = point_crash_time(pos, vec3(0, 1, 0), vec3(0.5, 1.0, 0.0));
   assert(fequal(d, 1.0f - std::cos(30.0f / 180.0f * M_PI)));

   vec3 out_pos;
   float e = line_crash_time(pos, vec3(1, 0, 0), vec3(4, -1, 0), vec3(4, 1, 0), out_pos);
   assert(fequal(e, 3.0f) && vequal(out_pos, vec3(4, 0, 0)));

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Collision tests passed!\n");
}

static void scenewalker_reset_mesh_path(void)
{
   //always assume matching .obj file is there
   size_t extPos = mesh_path.rfind('.');
   if (extPos != std::string::npos)
   {
      // Erase the current extension.
      mesh_path.erase(extPos);
      // Add the new extension.
      mesh_path.append(".obj");

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "New path: %s\n", mesh_path.c_str());
   }
}

static void modelviewer_load_game(const struct retro_game_info *info)
{
   mode_engine = MODE_MODELVIEWER;

   mesh_path = info->path;

   if (strstr(info->path, ".mtl"))
   {
      mode_engine = MODE_SCENEWALKER;
      test_crash_detection();
      scenewalker_reset_mesh_path();
   }
   else
      player_size = vec3(0, 0, 0);

   first_init = false;
}

const engine_program_t engine_program_modelviewer = {
   modelviewer_load_game,
   modelviewer_run,
   modelviewer_context_reset,
   modelviewer_update_variables,
   modelviewer_check_input,
};
