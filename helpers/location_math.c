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

// Very rudimentary stuff for now
#include <math.h>
#include <float.h>
#include <boolean.h>

bool loc_float_lesser_than(float current, float limit)
{
   return ((current - limit < DBL_EPSILON) && (fabs(current - limit) > DBL_EPSILON));
}

bool loc_float_greater_than(float current, float limit)
{
   return ((current - limit > DBL_EPSILON) && (fabs(current - limit) > DBL_EPSILON));
}
