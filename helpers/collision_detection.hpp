#ifndef _COLLISION_DETECTION_H
#define _COLLISION_DETECTION_H

#include "program.h"

using namespace glm;

extern void coll_wall_hug_detection(vec3& player_pos);
extern void coll_detection(vec3& player_pos, vec3& velocity);
extern void coll_test_crash_detection(void);
extern void coll_triangles_push(unsigned v, const std::vector<GL::Vertex>& vertices,
      vec3 player_size);
extern void coll_triangles_clear(void);

#endif
