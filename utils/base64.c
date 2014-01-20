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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "base64.h"

static uint8_t dec(char n)
{
   if (n >= 'A' && n <= 'Z')
      return n - 'A';
   if (n >= 'a' && n <= 'z')
      return n - 'a' + 26;
   if (n >= '0' && n <= '9')
      return n - '0' + 52;
   if (n == '-')
      return 62;
   if (n == '_')
      return 63;
   return 0;
}

static char enc(uint8_t n)
{
   //base64 for URL encodings
   static char lookup_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
   return lookup_table[n & 63];
}

bool base64_encode(char *output, const uint8_t* input, unsigned inlength)
{
   unsigned i, o;
   size_t sizeof_output = inlength * 8 / 6 + 6;
   output = (char*)malloc(sizeof_output * sizeof(char));

   i = 0;
   o = 0;
   while(i < inlength)
   {
      switch(i % 3)
      {
         case 0:
            output[o++] = enc(input[i] >> 2);
            output[o] = enc((input[i] & 3) << 4);
            break;
         case 1:
            {
               uint8_t prev = dec(output[o]);
               output[o++] = enc(prev + (input[i] >> 4));
               output[o] = enc((input[i] & 15) << 2);
            }
            break;
         case 2:
            {
               uint8_t prev = dec(output[o]);
               output[o++] = enc(prev + (input[i] >> 6));
               output[o++] = enc(input[i] & 63);
            }
            break;
      }

      i++;
   }

   return true;
}

bool base64_decode(uint8_t *output, unsigned *outlength, const char *input)
{
   unsigned i, o, inlength;
   inlength = strlen(input);
   output = (uint8_t*)malloc(inlength * sizeof(uint8_t));

   i = 0;
   o = 0;
   while(i < inlength)
   {
      uint8_t x = dec(input[i]);

      switch(i++ & 3)
      {
         case 0:
            output[o] = x << 2;
            break;
         case 1:
            output[o++] |= x >> 4;
            output[o] = (x & 15) << 4;
            break;
         case 2:
            output[o++] |= x >> 2;
            output[o] = (x & 3) << 6;
            break;
         case 3:
            output[o++] |= x;
            break;
      }
   }

   *outlength = o;
   return true;
}
