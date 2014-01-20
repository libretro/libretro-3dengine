/*
 *  Libretro 3DEngine
 *  Copyright (C) 2013-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2013-2014 - Daniel De Matteis
 *
 *  InstancingViewer is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  InstancingViewer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with InstancingViewer.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
