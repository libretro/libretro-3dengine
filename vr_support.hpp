/*
 *  Libretro 3DEngine - VR helpers
 *
 */

#ifndef VR_SUPPORT_HPP__
#define VR_SUPPORT_HPP__

#include <math.h>
#include <string.h>
#include "libretro.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

/* Identity pose + 90 degree symmetric FOV. Used until the frontend gives us
 * a valid sample (first frame), so we never normalize a zero quaternion. */
static inline void vr_frame_defaults(struct retro_vr_frame_state *fs)
{
   unsigned i, j;
   memset(fs, 0, sizeof(*fs));
   for (i = 0; i < 2; i++)
   {
      fs->eyes[i].orientation[3] = 1.0f;
      for (j = 0; j < 4; j++)
         fs->eyes[i].fov_tan[j] = 1.0f;
   }
}

static inline glm::quat vr_quat(const float q[4])
{
   /* libretro: (x, y, z, w).  glm::quat constructor: (w, x, y, z). */
   return glm::normalize(glm::quat(q[3], q[0], q[1], q[2]));
}

/* Eye -> tracking space transform. View matrix is the inverse of this
 * (after applying the rig transform). */
static inline glm::mat4 vr_eye_pose_matrix(const struct retro_vr_eye_state &e)
{
   glm::mat4 m = glm::mat4_cast(vr_quat(e.orientation));
   m[3] = glm::vec4(e.position[0], e.position[1], e.position[2], 1.0f);
   return m;
}

/* Asymmetric frustum from the four tangents. fabsf() so it works whether the
 * frontend reports magnitudes or OpenXR-style signed angles
 * (left/down negative) - see notes on fov_tan in the reply. */
static inline glm::mat4 vr_projection(const float t[4], float znear, float zfar)
{
   float l = fabsf(t[0]) * znear;
   float r = fabsf(t[1]) * znear;
   float u = fabsf(t[2]) * znear;
   float d = fabsf(t[3]) * znear;
   return glm::frustum(-l, r, -d, u, znear, zfar);
}

/* Unit vector, in tracking space, of where the head is facing, flattened onto
 * the ground plane. Averages both eyes (some HMDs cant their displays). */
static inline glm::vec3 vr_head_forward_xz(const struct retro_vr_frame_state &fs)
{
   unsigned i;
   glm::vec3 f(0.0f);

   for (i = 0; i < 2; i++)
      f += vr_quat(fs.eyes[i].orientation) * glm::vec3(0.0f, 0.0f, -1.0f);

   f.y = 0.0f;
   if (glm::length(f) < 0.001f) /* looking straight up/down */
      return glm::vec3(0.0f, 0.0f, -1.0f);
   return glm::normalize(f);
}

#endif
