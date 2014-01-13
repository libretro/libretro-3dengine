// Very rudimentary stuff for now
#include <math.h>
#include <cfloat>
#include "boolean.h"

bool loc_float_lesser_than(float current, float limit)
{
   return ((current - limit < DBL_EPSILON) && (fabs(current - limit) > DBL_EPSILON));
}

bool loc_float_greater_than(float current, float limit)
{
   return ((current - limit > DBL_EPSILON) && (fabs(current - limit) > DBL_EPSILON));
}
