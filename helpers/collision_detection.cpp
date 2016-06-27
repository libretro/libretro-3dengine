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

#include "libretro.h"
#include "program.h"
#include "../engine/mesh.hpp"

#include <vector>
#include <string>

using namespace glm;

struct Triangle
{
   vec3 a, b, c;
   vec3 normal;
   float n0;
};

static std::vector<Triangle> triangles;

/* Probably not the most efficient way to do collision handling ... :) */
static inline bool inside_triangle(const Triangle& tri, const vec3& pos)
{
   vec3 real_normal = -tri.normal;
   vec3 ab          = tri.b - tri.a;
   vec3 ac          = tri.c - tri.a;
   vec3 ap          = pos - tri.a;
   vec3 bp          = pos - tri.b;
   vec3 bc          = tri.c - tri.b;

   /* Checks if point exists inside triangle. */
   if (dot(cross(ab, ap), real_normal) < 0.0f)
      return false;

   if (dot(cross(ap, ac), real_normal) < 0.0f)
      return false;

   if (dot(cross(bc, bp), real_normal) < 0.0f)
      return false;

   return true;
}

#define TWIDDLE_FACTOR (-0.5f)

/* Here be dragons. 2-3 pages of mathematical derivations. */
static inline float point_crash_time(const vec3& pos, const vec3& v, const vec3& edge)
{
   vec3 l  = pos - edge;
   float A = dot(v, v);
   float B = 2 * dot(l, v);
   float C = dot(l, l) - 1;
   float d = B * B - 4.0f * A * C;

   if (d < 0.0f) /* No solution, can't hit the sphere ever. */
      return 10.0f; /* Return number > 1.0f to signal no collision. Makes taking min() easier. */

   float d_sqrt = std::sqrt(d);
   float sol0   = (-B + d_sqrt) / (2.0f * A);
   float sol1   = (-B - d_sqrt) / (2.0f * A);

   if (sol0 >= TWIDDLE_FACTOR && sol1 >= TWIDDLE_FACTOR)
      return std::min(sol0, sol1);
   else if (sol0 >= TWIDDLE_FACTOR && sol1 < TWIDDLE_FACTOR)
      return sol0;
   else if (sol0 < TWIDDLE_FACTOR && sol1 >= TWIDDLE_FACTOR)
      return sol1;

   return 10.0f;
}

static inline float line_crash_time(const vec3& pos, const vec3& v, const vec3& a, const vec3& b, vec3& crash_pos)
{
   crash_pos    = vec3(0.0f);
   vec3 ab      = b - a;
   vec3 d       = pos - a;
   float ab_sqr = dot(ab, ab);
   float T      = dot(ab, v) / ab_sqr;
   float S      = dot(ab, d) / ab_sqr;
   vec3 V       = v - vec3(T) * ab;
   vec3 W       = d - vec3(S) * ab;
   float A      = dot(V, V);
   float B      = 2.0f * dot(V, W);
   float C      = dot(W, W) - 1.0f;
   float D      = B * B - 4.0f * A * C; 

   if (D < 0.0f) /* No solutions exist :( */
      return 10.0f;

   float D_sqrt = std::sqrt(D);
   float sol0 = (-B + D_sqrt) / (2.0f * A);
   float sol1 = (-B - D_sqrt) / (2.0f * A);

   float solution;
   if (sol0 >= TWIDDLE_FACTOR && sol1 >= TWIDDLE_FACTOR)
      solution = std::min(sol0, sol1);
   else if (sol0 >= TWIDDLE_FACTOR && sol1 < TWIDDLE_FACTOR)
      solution = sol0;
   else if (sol0 < TWIDDLE_FACTOR && sol1 >= TWIDDLE_FACTOR)
      solution = sol1;
   else
      return 10.0f;

   /* Check if solution hits the actual line ... */
   float k = dot(ab, d + vec3(solution) * v) / ab_sqr;
   if (k >= 0.0f && k <= 1.0f)
   {
      crash_pos = a + vec3(k) * ab;
      return solution;
   }

   return 10.0f;
}
/////////// End dragons

void coll_wall_hug_detection(vec3& player_pos)
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

void coll_detection(vec3& player_pos, vec3& velocity)
{
   if (velocity == vec3(0.0))
      return;

   unsigned i;
   float min_time                   = 1.0f;
   bool crash                       = false;
   vec3 crash_point                 = vec3(0.0f);
   const Triangle *closest_triangle = 0;

   for (i = 0; i < triangles.size(); i++)
   {
      const Triangle& tri   = triangles[i];
      float plane_dist      = tri.n0 - dot(player_pos, tri.normal); 
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
            vec3 crash_pos_ab, crash_pos_ac, crash_pos_bc;

            /* Check how we can hit the triangle. Can hit edges or lines ... */
            float min_time_crash = point_crash_time(player_pos, velocity, tri.a);
            vec3 crash_pos_tmp   = tri.a;

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

void coll_test_crash_detection(void)
{
   vec3 out_pos;
   vec3 pos = vec3(0.0f);
   float  a = point_crash_time(pos, vec3(1, 0, 0), vec3(3, 0, 0));
   float  b = point_crash_time(pos, vec3(1, 0, 0), vec3(2, 2, 0));
   float  c = point_crash_time(pos, vec3(1, 0, 0), vec3(1.0, 0.5, 0.0));
   float  d = point_crash_time(pos, vec3(0, 1, 0), vec3(0.5, 1.0, 0.0));
   float  e = line_crash_time(pos, vec3(1, 0, 0), vec3(4, -1, 0), vec3(4, 1, 0), out_pos);

   assert(fequal(a, 2.0f));
   assert(fequal(b, 10.0f));
   assert(fequal(c, 1.0f - std::cos(30.0f / 180.0f * M_PI)));
   assert(fequal(d, 1.0f - std::cos(30.0f / 180.0f * M_PI)));
   assert(fequal(e, 3.0f) && vequal(out_pos, vec3(4, 0, 0)));

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Collision tests passed!\n");
}

void coll_triangles_push(unsigned v, const std::vector<GL::Vertex>& vertices, vec3 player_size)
{
   Triangle tri;

   tri.a      = vertices[v + 0].vert / player_size;
   tri.b      = vertices[v + 1].vert / player_size;
   tri.c      = vertices[v + 2].vert / player_size;
   /* Make normals point inward. Makes for simpler computation. */
   tri.normal = -normalize(cross(tri.b - tri.a, tri.c - tri.a));
   /* Plane constant */
   tri.n0     = dot(tri.normal, tri.a);

   triangles.push_back(tri);
}

void coll_triangles_clear(void)
{
   triangles.clear();
}
