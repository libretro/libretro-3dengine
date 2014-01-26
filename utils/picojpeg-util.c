#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "boolean.h"
#include "picojpeg.h"

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

FILE *jpegfile;
int jpeg_filesize = 0;
int jpeg_filepos = 0;

unsigned char pjpeg_need_bytes_callback(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data)
{
   unsigned int n = min((unsigned int)(jpeg_filesize - jpeg_filepos), (unsigned int)buf_size);
   if (n && (fread(pBuf, 1, n, jpegfile) != n))
      return PJPG_STREAM_READ_ERROR;
   *pBytes_actually_read = (unsigned char)(n);
   jpeg_filepos += n;
   return 0;
}

bool texture_image_load_jpeg(const char *filename, uint8_t **data)
{
   int status, mcu_x, mcu_y;
   unsigned int row_pitch;
   pjpeg_image_info_t imageInfo;

   *data = NULL;

   jpegfile = fopen(filename,"rb");
   fseek(jpegfile, 0, SEEK_END);
   jpeg_filesize = ftell(jpegfile);
   jpeg_filepos = 0;
   fseek(jpegfile, 0, SEEK_SET);
   status = pjpeg_decode_init(&imageInfo, pjpeg_need_bytes_callback, NULL, 0);
   row_pitch = imageInfo.m_width * imageInfo.m_comps;
   mcu_x = 0;
   mcu_y = 0;
   *data = (uint8_t*)malloc(row_pitch *imageInfo.m_height * sizeof(uint32_t));

   for ( ; ; )
   {
      status = pjpeg_decode_mcu();

      if (status)
      {
         if (status != PJPG_NO_MORE_BLOCKS)
         {
            //pc.printf("pjpeg_decode_mcu() failed with status %u\n", status);
            fclose(jpegfile);
            goto fail;
         }

         break;
      }

      if (mcu_y >= imageInfo.m_MCUSPerCol)
      {
         fclose(jpegfile);
         goto fail;
      }

      // Copy MCU's pixel blocks into the destination bitmap.
      uint8_t *pDst_row = *data + (mcu_y * imageInfo.m_MCUHeight) * row_pitch + (mcu_x * imageInfo.m_MCUWidth * imageInfo.m_comps);

      for (int y = 0; y < imageInfo.m_MCUHeight; y += 8)
      {
         const int by_limit = min(8, imageInfo.m_height - (mcu_y * imageInfo.m_MCUHeight + y));

         for (int x = 0; x < imageInfo.m_MCUWidth; x += 8)
         {
            uint8_t *pDst_block = pDst_row + x * imageInfo.m_comps;

            uint32_t src_ofs = (x * 8U) + (y * 16U);
            const uint8_t *pSrcR = imageInfo.m_pMCUBufR + src_ofs;
            const uint8_t *pSrcG = imageInfo.m_pMCUBufG + src_ofs;
            const uint8_t *pSrcB = imageInfo.m_pMCUBufB + src_ofs;

            const int bx_limit = min(8, imageInfo.m_width - (mcu_x * imageInfo.m_MCUWidth + x));

            if (imageInfo.m_scanType == PJPG_GRAYSCALE)
            {
               for (int by = 0; by < by_limit; by++)
               {
                  uint8_t *pDst = pDst_block;

                  for (int bx = 0; bx < bx_limit; bx++)
                     *pDst++ = *pSrcR++;

                  pSrcR += (8 - bx_limit);

                  pDst_block += row_pitch;
               }
            }
            else
            {
               for (int by = 0; by < by_limit; by++)
               {
                  uint8_t *pDst = pDst_block;

                  for (int bx = 0; bx < bx_limit; bx++)
                  {
                     pDst[0] = *pSrcR++;
                     pDst[1] = *pSrcG++;
                     pDst[2] = *pSrcB++;
                     pDst += 3;
                  }

                  pSrcR += (8 - bx_limit);
                  pSrcG += (8 - bx_limit);
                  pSrcB += (8 - bx_limit);

                  pDst_block += row_pitch;
               }
            }
         }

         pDst_row += (row_pitch * 8);
      }

      mcu_x++;
      if (mcu_x == imageInfo.m_MCUSPerRow)
      {
         mcu_x = 0;
         mcu_y++;
      }
   }

   fclose(jpegfile);
   return true;

fail:
   free(*data);
   return false;
}
