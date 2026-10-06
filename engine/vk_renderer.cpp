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
#ifdef HAVE_VULKAN

#include <vulkan/vulkan_symbol_wrapper.h>
#include <libretro_vulkan.h>

#include "vk_renderer.hpp"
#include "vk_shaders.inc"
#include "rpng.h"
#include "rtga.h"
#include "util.hpp"
#include "shared.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <map>
#include <string>
#include <algorithm>

#define VKR_LOG(level, ...) do { if (log_cb) log_cb(level, __VA_ARGS__); } while (0)
#define VKR_MAX_SLOTS 16u
#define VKR_UNDEF32 ((uint32_t)-1)

namespace VKR
{
   namespace
   {
      struct Buffer
      {
         Buffer() : buf(VK_NULL_HANDLE), mem(VK_NULL_HANDLE), map(NULL), size(0) {}
         VkBuffer buf;
         VkDeviceMemory mem;
         uint8_t *map;
         VkDeviceSize size;
      };

      struct Tex
      {
         Tex() : image(VK_NULL_HANDLE), mem(VK_NULL_HANDLE), view(VK_NULL_HANDLE) {}
         VkImage image;
         VkDeviceMemory mem;
         VkImageView view;
      };

      struct MeshGPU
      {
         MeshGPU() : count(0), tex_set(VK_NULL_HANDLE), spec_power(60.0f), alpha(1.0f) {}
         Buffer vbo;
         uint32_t count;
         VkDescriptorSet tex_set;
         glm::vec3 ambient, diffuse, specular;
         float spec_power;
         float alpha;
      };

      /* std140: all vec4 so no padding surprises. Matches mesh.frag. */
      struct DrawUBO
      {
         float light[4];
         float light_ambient[4];
         float eye[4];
         float mtl_ambient[4];
         float mtl_diffuse[4];
         float mtl_specular[4];
         float mtl_params[4]; /* x = specular power, y = alpha mod */
      };

      struct PushConstants
      {
         float model[16];
         float mvp[16];
      };

      /* Color image that is handed to the frontend + its depth buffer. */
      struct Target
      {
         Target() : color(VK_NULL_HANDLE), depth(VK_NULL_HANDLE),
            color_mem(VK_NULL_HANDLE), depth_mem(VK_NULL_HANDLE),
            color_view(VK_NULL_HANDLE), depth_view(VK_NULL_HANDLE),
            fb(VK_NULL_HANDLE), w(0), h(0)
         { memset(&desc, 0, sizeof(desc)); }
         VkImage color, depth;
         VkDeviceMemory color_mem, depth_mem;
         VkImageView color_view, depth_view;
         VkFramebuffer fb;
         unsigned w, h;
         struct retro_vulkan_image desc; /* must outlive video_cb() */
      };

      /* One per frontend sync index. Everything the frontend may still be
       * reading (the Target) is only touched again after wait_sync_index()
       * for this index has returned. */
      struct Slot
      {
         Slot() : pool(VK_NULL_HANDLE), cmd(VK_NULL_HANDLE), fence(VK_NULL_HANDLE),
            ubo_set(VK_NULL_HANDLE) {}
         Target target;
         VkCommandPool pool;
         VkCommandBuffer cmd;
         VkFence fence;
         Buffer ubo;
         VkDescriptorSet ubo_set;
      };

      struct Globals
      {
         Globals() : iface(NULL), dev(VK_NULL_HANDLE), gpu(VK_NULL_HANDLE),
            queue(VK_NULL_HANDLE), qfamily(0), inited(false),
            depth_format(VK_FORMAT_UNDEFINED), render_pass(VK_NULL_HANDLE),
            ubo_layout(VK_NULL_HANDLE), tex_layout(VK_NULL_HANDLE),
            pipe_layout(VK_NULL_HANDLE), pipeline(VK_NULL_HANDLE),
            sampler(VK_NULL_HANDLE), ubo_pool(VK_NULL_HANDLE),
            tex_pool(VK_NULL_HANDLE), upload_pool(VK_NULL_HANDLE),
            ubo_stride(0), last_mask(0), scene_ready(false) {}

         const struct retro_hw_render_interface_vulkan *iface;
         VkDevice dev;
         VkPhysicalDevice gpu;
         VkQueue queue;
         uint32_t qfamily;
         bool inited;

         VkFormat depth_format;
         VkRenderPass render_pass;
         VkDescriptorSetLayout ubo_layout;
         VkDescriptorSetLayout tex_layout;
         VkPipelineLayout pipe_layout;
         VkPipeline pipeline;
         VkSampler sampler;
         VkDescriptorPool ubo_pool;
         VkDescriptorPool tex_pool;
         VkCommandPool upload_pool;
         VkDeviceSize ubo_stride;

         uint32_t last_mask;
         std::vector<Slot> slots;

         Tex blank;
         std::map<std::string, Tex> textures;
         std::vector<MeshGPU> meshes;
         bool scene_ready;
      };

      Globals g;

      const VkFormat COLOR_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

      /* ---- negotiation ---------------------------------------------- */

      const VkApplicationInfo *get_application_info(void)
      {
         static VkApplicationInfo info;
         memset(&info, 0, sizeof(info));
         info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
         info.pApplicationName   = "Libretro 3DEngine";
         info.applicationVersion = 1;
         info.pEngineName        = "Libretro 3DEngine";
         info.engineVersion      = 1;
         /* 1.1 as recommended in libretro_vulkan.h (old Android loaders). We
          * only use 1.0 functionality. */
         info.apiVersion         = VK_API_VERSION_1_1;
         return &info;
      }

      /* ---- small helpers -------------------------------------------- */

      uint32_t find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags props)
      {
         uint32_t i;
         VkPhysicalDeviceMemoryProperties mp;
         vkGetPhysicalDeviceMemoryProperties(g.gpu, &mp);
         for (i = 0; i < mp.memoryTypeCount; i++)
            if ((type_bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
               return i;
         return VKR_UNDEF32;
      }

      bool alloc_memory(const VkMemoryRequirements& req, VkMemoryPropertyFlags props,
            VkDeviceMemory *mem)
      {
         VkMemoryAllocateInfo ai;
         memset(&ai, 0, sizeof(ai));
         ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
         ai.allocationSize  = req.size;
         ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, props);
         if (ai.memoryTypeIndex == VKR_UNDEF32)
            return false;
         return vkAllocateMemory(g.dev, &ai, NULL, mem) == VK_SUCCESS;
      }

      void destroy_buffer(Buffer& b)
      {
         if (b.map)
            vkUnmapMemory(g.dev, b.mem);
         if (b.buf)
            vkDestroyBuffer(g.dev, b.buf, NULL);
         if (b.mem)
            vkFreeMemory(g.dev, b.mem, NULL);
         b = Buffer();
      }

      /* Host visible + coherent. */
      bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer& out)
      {
         VkMemoryRequirements req;
         VkBufferCreateInfo bi;
         memset(&bi, 0, sizeof(bi));
         bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
         bi.size        = size;
         bi.usage       = usage;
         bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

         out = Buffer();
         out.size = size;
         if (vkCreateBuffer(g.dev, &bi, NULL, &out.buf) != VK_SUCCESS)
            return false;

         vkGetBufferMemoryRequirements(g.dev, out.buf, &req);
         if (!alloc_memory(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &out.mem))
            goto fail;
         if (vkBindBufferMemory(g.dev, out.buf, out.mem, 0) != VK_SUCCESS)
            goto fail;

         {
            void *p = NULL;
            if (vkMapMemory(g.dev, out.mem, 0, VK_WHOLE_SIZE, 0, &p) != VK_SUCCESS)
               goto fail;
            out.map = (uint8_t*)p;
         }
         return true;

fail:
         destroy_buffer(out);
         return false;
      }

      bool create_image(unsigned w, unsigned h, unsigned mips, VkFormat format,
            VkImageUsageFlags usage, VkImageCreateFlags flags,
            VkImage *image, VkDeviceMemory *mem)
      {
         VkMemoryRequirements req;
         VkImageCreateInfo ii;
         memset(&ii, 0, sizeof(ii));
         ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
         ii.flags         = flags;
         ii.imageType     = VK_IMAGE_TYPE_2D;
         ii.format        = format;
         ii.extent.width  = w;
         ii.extent.height = h;
         ii.extent.depth  = 1;
         ii.mipLevels     = mips;
         ii.arrayLayers   = 1;
         ii.samples       = VK_SAMPLE_COUNT_1_BIT;
         ii.tiling        = VK_IMAGE_TILING_OPTIMAL;
         ii.usage         = usage;
         ii.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
         ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

         *image = VK_NULL_HANDLE;
         *mem   = VK_NULL_HANDLE;
         if (vkCreateImage(g.dev, &ii, NULL, image) != VK_SUCCESS)
            return false;
         vkGetImageMemoryRequirements(g.dev, *image, &req);
         if (!alloc_memory(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem) ||
               vkBindImageMemory(g.dev, *image, *mem, 0) != VK_SUCCESS)
         {
            if (*mem)
               vkFreeMemory(g.dev, *mem, NULL);
            vkDestroyImage(g.dev, *image, NULL);
            *image = VK_NULL_HANDLE;
            *mem   = VK_NULL_HANDLE;
            return false;
         }
         return true;
      }

      void fill_view_info(VkImageViewCreateInfo& vi, VkImage image, VkFormat format,
            VkImageAspectFlags aspect, unsigned mips)
      {
         memset(&vi, 0, sizeof(vi));
         vi.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
         vi.image                           = image;
         vi.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
         vi.format                          = format;
         vi.components.r                    = VK_COMPONENT_SWIZZLE_R;
         vi.components.g                    = VK_COMPONENT_SWIZZLE_G;
         vi.components.b                    = VK_COMPONENT_SWIZZLE_B;
         vi.components.a                    = VK_COMPONENT_SWIZZLE_A;
         vi.subresourceRange.aspectMask     = aspect;
         vi.subresourceRange.baseMipLevel   = 0;
         vi.subresourceRange.levelCount     = mips;
         vi.subresourceRange.baseArrayLayer = 0;
         vi.subresourceRange.layerCount     = 1;
      }

      void image_barrier(VkCommandBuffer cmd, VkImage image, unsigned base_mip,
            unsigned levels, VkImageLayout from, VkImageLayout to,
            VkAccessFlags src_access, VkAccessFlags dst_access,
            VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage)
      {
         VkImageMemoryBarrier b;
         memset(&b, 0, sizeof(b));
         b.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
         b.srcAccessMask                   = src_access;
         b.dstAccessMask                   = dst_access;
         b.oldLayout                       = from;
         b.newLayout                       = to;
         b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
         b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
         b.image                           = image;
         b.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
         b.subresourceRange.baseMipLevel   = base_mip;
         b.subresourceRange.levelCount     = levels;
         b.subresourceRange.baseArrayLayer = 0;
         b.subresourceRange.layerCount     = 1;
         vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
      }

      /* ---- one-shot uploads (init / scene load only) ---------------- */

      VkCommandBuffer begin_oneshot(void)
      {
         VkCommandBuffer cmd = VK_NULL_HANDLE;
         VkCommandBufferAllocateInfo ai;
         VkCommandBufferBeginInfo bi;
         memset(&ai, 0, sizeof(ai));
         ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         ai.commandPool        = g.upload_pool;
         ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         ai.commandBufferCount = 1;
         if (vkAllocateCommandBuffers(g.dev, &ai, &cmd) != VK_SUCCESS)
            return VK_NULL_HANDLE;

         memset(&bi, 0, sizeof(bi));
         bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
         bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
         vkBeginCommandBuffer(cmd, &bi);
         return cmd;
      }

      bool end_oneshot(VkCommandBuffer cmd)
      {
         VkSubmitInfo si;
         VkResult res;
         vkEndCommandBuffer(cmd);

         memset(&si, 0, sizeof(si));
         si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
         si.commandBufferCount = 1;
         si.pCommandBuffers    = &cmd;

         /* The frontend shares this queue; every submit must be locked. */
         g.iface->lock_queue(g.iface->handle);
         res = vkQueueSubmit(g.queue, 1, &si, VK_NULL_HANDLE);
         if (res == VK_SUCCESS)
            res = vkQueueWaitIdle(g.queue);
         g.iface->unlock_queue(g.iface->handle);

         vkFreeCommandBuffers(g.dev, g.upload_pool, 1, &cmd);
         return res == VK_SUCCESS;
      }

      void destroy_texture(Tex& t)
      {
         if (t.view)
            vkDestroyImageView(g.dev, t.view, NULL);
         if (t.image)
            vkDestroyImage(g.dev, t.image, NULL);
         if (t.mem)
            vkFreeMemory(g.dev, t.mem, NULL);
         t = Tex();
      }

      /* RGBA8 -> sampled, mip-mapped texture. Mirrors GL::Texture::upload_data
       * (linear + mipmaps, repeat). The loaders hand us bottom-left-origin
       * rows. */
      bool create_texture(const uint8_t *rgba, unsigned w, unsigned h, Tex& t)
      {
         unsigned i, mips = 1;
         Buffer staging;
         VkCommandBuffer cmd;
         VkBufferImageCopy copy;
         VkFormatProperties fp;
         VkImageViewCreateInfo vi;
         int32_t mw = (int32_t)w, mh = (int32_t)h;
         const VkDeviceSize size = (VkDeviceSize)w * h * 4;

         vkGetPhysicalDeviceFormatProperties(g.gpu, VK_FORMAT_R8G8B8A8_UNORM, &fp);
         if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT &&
               fp.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT &&
               fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
         {
            unsigned m = std::max(w, h);
            mips = 1;
            while (m > 1) { m >>= 1; mips++; }
         }

         t = Tex();
         if (!create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging))
            return false;
         memcpy(staging.map, rgba, (size_t)size);

         if (!create_image(w, h, mips, VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT, 0, &t.image, &t.mem))
         {
            destroy_buffer(staging);
            return false;
         }

         cmd = begin_oneshot();
         if (!cmd)
         {
            destroy_buffer(staging);
            destroy_texture(t);
            return false;
         }

         image_barrier(cmd, t.image, 0, mips, VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

         memset(&copy, 0, sizeof(copy));
         copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         copy.imageSubresource.layerCount = 1;
         copy.imageExtent.width           = w;
         copy.imageExtent.height          = h;
         copy.imageExtent.depth           = 1;
         vkCmdCopyBufferToImage(cmd, staging.buf, t.image,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

         for (i = 1; i < mips; i++)
         {
            VkImageBlit blit;
            int32_t nw = mw > 1 ? mw / 2 : 1;
            int32_t nh = mh > 1 ? mh / 2 : 1;

            image_barrier(cmd, t.image, i - 1, 1,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            memset(&blit, 0, sizeof(blit));
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel   = i - 1;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[1].x           = mw;
            blit.srcOffsets[1].y           = mh;
            blit.srcOffsets[1].z           = 1;
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel   = i;
            blit.dstSubresource.layerCount = 1;
            blit.dstOffsets[1].x           = nw;
            blit.dstOffsets[1].y           = nh;
            blit.dstOffsets[1].z           = 1;
            vkCmdBlitImage(cmd, t.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            image_barrier(cmd, t.image, i - 1, 1,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            mw = nw;
            mh = nh;
         }

         image_barrier(cmd, t.image, mips - 1, 1,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

         if (!end_oneshot(cmd))
         {
            destroy_buffer(staging);
            destroy_texture(t);
            return false;
         }
         destroy_buffer(staging);

         fill_view_info(vi, t.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, mips);
         if (vkCreateImageView(g.dev, &vi, NULL, &t.view) != VK_SUCCESS)
         {
            destroy_texture(t);
            return false;
         }
         return true;
      }

      /* Returns NULL on failure; the caller then falls back to the blank texture. */
      const Tex *get_texture(const std::string& path)
      {
         unsigned w = 0, h = 0;
         uint8_t *data = NULL;
         bool ok = false;
         Tex t;
         std::string ext;
         std::map<std::string, Tex>::iterator it;

         if (!path.size())
            return NULL;

         it = g.textures.find(path);
         if (it != g.textures.end())
            return it->second.view ? &it->second : NULL;

         ext = Path::ext(path);
         if (ext == "png")
            ok = rpng_load_image_rgba(path.c_str(), &data, &w, &h);
         else if (ext == "tga")
            ok = texture_image_load_tga(path.c_str(), data, w, h);
         else
            VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Unsupported texture extension: \"%s\"\n", ext.c_str());

         if (ok && data && create_texture(data, w, h, t))
         {
            free(data);
            return &(g.textures[path] = t);
         }

         if (ok)
            free(data);
         VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Failed to load texture: %s\n", path.c_str());
         g.textures[path] = Tex();
         return NULL;
      }

      /* ---- render targets / per-sync-index slots -------------------- */

      void destroy_target(Target& t)
      {
         if (t.fb)
            vkDestroyFramebuffer(g.dev, t.fb, NULL);
         if (t.color_view)
            vkDestroyImageView(g.dev, t.color_view, NULL);
         if (t.depth_view)
            vkDestroyImageView(g.dev, t.depth_view, NULL);
         if (t.color)
            vkDestroyImage(g.dev, t.color, NULL);
         if (t.depth)
            vkDestroyImage(g.dev, t.depth, NULL);
         if (t.color_mem)
            vkFreeMemory(g.dev, t.color_mem, NULL);
         if (t.depth_mem)
            vkFreeMemory(g.dev, t.depth_mem, NULL);
         t = Target();
      }

      bool create_target(Target& t, unsigned w, unsigned h)
      {
         VkImageViewCreateInfo vi;
         VkFramebufferCreateInfo fi;
         VkImageView atts[2];

         t = Target();

         /* MUTABLE_FORMAT: the frontend may reinterpret the 8-bit image as
          * sRGB. */
         if (!create_image(w, h, 1, COLOR_FORMAT,
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                  VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, &t.color, &t.color_mem))
            goto fail;
         if (!create_image(w, h, 1, g.depth_format,
                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &t.depth, &t.depth_mem))
            goto fail;

         fill_view_info(vi, t.color, COLOR_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);
         if (vkCreateImageView(g.dev, &vi, NULL, &t.color_view) != VK_SUCCESS)
            goto fail;

         {
            VkImageAspectFlags da = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (g.depth_format == VK_FORMAT_D24_UNORM_S8_UINT)
               da |= VK_IMAGE_ASPECT_STENCIL_BIT;
            fill_view_info(vi, t.depth, g.depth_format, da, 1);
         }
         if (vkCreateImageView(g.dev, &vi, NULL, &t.depth_view) != VK_SUCCESS)
            goto fail;

         atts[0] = t.color_view;
         atts[1] = t.depth_view;
         memset(&fi, 0, sizeof(fi));
         fi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
         fi.renderPass      = g.render_pass;
         fi.attachmentCount = 2;
         fi.pAttachments    = atts;
         fi.width           = w;
         fi.height          = h;
         fi.layers          = 1;
         if (vkCreateFramebuffer(g.dev, &fi, NULL, &t.fb) != VK_SUCCESS)
            goto fail;

         t.w = w;
         t.h = h;

         /* What we give the frontend. The render pass leaves the image in
          * SHADER_READ_ONLY_OPTIMAL, and the view carries the create info so
          * the frontend can re-view it (e.g. as sRGB). */
         t.desc.image_view   = t.color_view;
         t.desc.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         t.desc.create_info  = vi;
         fill_view_info(t.desc.create_info, t.color, COLOR_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);
         return true;

fail:
         destroy_target(t);
         return false;
      }

      void destroy_slot(Slot& s)
      {
         destroy_target(s.target);
         destroy_buffer(s.ubo);
         if (s.fence)
            vkDestroyFence(g.dev, s.fence, NULL);
         if (s.pool)
            vkDestroyCommandPool(g.dev, s.pool, NULL); /* frees s.cmd */
         s = Slot();
      }

      /* Waits only for *our* submissions. */
      void wait_own_work(void)
      {
         size_t i;
         for (i = 0; i < g.slots.size(); i++)
            if (g.slots[i].fence)
               vkWaitForFences(g.dev, 1, &g.slots[i].fence, VK_TRUE, (uint64_t)-1);
      }

      void destroy_slots(void)
      {
         size_t i;
         wait_own_work();
         for (i = 0; i < g.slots.size(); i++)
            destroy_slot(g.slots[i]);
         g.slots.clear();
         if (g.ubo_pool)
            vkResetDescriptorPool(g.dev, g.ubo_pool, 0);
      }

      bool create_slots(uint32_t mask)
      {
         unsigned i, n = 0;
         while ((mask >> n) != 0 && n < 32)
            n++;
         if (!n || n > VKR_MAX_SLOTS)
         {
            VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Unsupported sync index mask 0x%x\n", mask);
            return false;
         }

         g.slots.resize(n);
         for (i = 0; i < n; i++)
         {
            Slot& s = g.slots[i];
            VkCommandPoolCreateInfo pi;
            VkCommandBufferAllocateInfo ai;
            VkFenceCreateInfo fi;
            VkDescriptorSetAllocateInfo di;

            memset(&pi, 0, sizeof(pi));
            pi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pi.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pi.queueFamilyIndex = g.qfamily;
            if (vkCreateCommandPool(g.dev, &pi, NULL, &s.pool) != VK_SUCCESS)
               return false;

            memset(&ai, 0, sizeof(ai));
            ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.commandPool        = s.pool;
            ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandBufferCount = 1;
            if (vkAllocateCommandBuffers(g.dev, &ai, &s.cmd) != VK_SUCCESS)
               return false;

            memset(&fi, 0, sizeof(fi));
            fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            if (vkCreateFence(g.dev, &fi, NULL, &s.fence) != VK_SUCCESS)
               return false;

            memset(&di, 0, sizeof(di));
            di.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            di.descriptorPool     = g.ubo_pool;
            di.descriptorSetCount = 1;
            di.pSetLayouts        = &g.ubo_layout;
            if (vkAllocateDescriptorSets(g.dev, &di, &s.ubo_set) != VK_SUCCESS)
               return false;
         }
         g.last_mask = mask;
         return true;
      }

      /* Needs: GPU done with s.ubo (we waited s.fence). */
      bool ensure_ubo(Slot& s, VkDeviceSize needed)
      {
         VkDescriptorBufferInfo bi;
         VkWriteDescriptorSet w;

         if (s.ubo.buf && s.ubo.size >= needed)
            return true;

         destroy_buffer(s.ubo);
         if (!create_buffer(needed, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, s.ubo))
            return false;

         memset(&bi, 0, sizeof(bi));
         bi.buffer = s.ubo.buf;
         bi.offset = 0;
         bi.range  = sizeof(DrawUBO);

         memset(&w, 0, sizeof(w));
         w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         w.dstSet          = s.ubo_set;
         w.dstBinding      = 0;
         w.descriptorCount = 1;
         w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
         w.pBufferInfo     = &bi;
         vkUpdateDescriptorSets(g.dev, 1, &w, 0, NULL);
         return true;
      }

      /* ---- pipeline ------------------------------------------------- */

      VkShaderModule make_module(const uint32_t *code, size_t bytes)
      {
         VkShaderModule m = VK_NULL_HANDLE;
         VkShaderModuleCreateInfo mi;
         memset(&mi, 0, sizeof(mi));
         mi.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
         mi.codeSize = bytes;
         mi.pCode    = code;
         if (vkCreateShaderModule(g.dev, &mi, NULL, &m) != VK_SUCCESS)
            return VK_NULL_HANDLE;
         return m;
      }

      bool create_pipeline(ShaderMode mode)
      {
         VkShaderModule vs, fs;
         VkPipelineShaderStageCreateInfo stages[2];
         VkSpecializationMapEntry spec_entry;
         VkSpecializationInfo spec;
         int32_t spec_value = (int32_t)mode;
         VkVertexInputBindingDescription bind;
         VkVertexInputAttributeDescription attrs[3];
         VkPipelineVertexInputStateCreateInfo vin;
         VkPipelineInputAssemblyStateCreateInfo ia;
         VkPipelineViewportStateCreateInfo vp;
         VkPipelineRasterizationStateCreateInfo rs;
         VkPipelineMultisampleStateCreateInfo ms;
         VkPipelineDepthStencilStateCreateInfo ds;
         VkPipelineColorBlendAttachmentState cba;
         VkPipelineColorBlendStateCreateInfo cb;
         VkDynamicState dyn_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
         VkPipelineDynamicStateCreateInfo dyn;
         VkGraphicsPipelineCreateInfo pi;
         VkResult res;

         vs = make_module(vk_mesh_vert_spv, sizeof(vk_mesh_vert_spv));
         fs = make_module(vk_mesh_frag_spv, sizeof(vk_mesh_frag_spv));
         if (!vs || !fs)
         {
            if (vs) vkDestroyShaderModule(g.dev, vs, NULL);
            if (fs) vkDestroyShaderModule(g.dev, fs, NULL);
            return false;
         }

         spec_entry.constantID = 0;
         spec_entry.offset     = 0;
         spec_entry.size       = sizeof(spec_value);
         spec.mapEntryCount    = 1;
         spec.pMapEntries      = &spec_entry;
         spec.dataSize         = sizeof(spec_value);
         spec.pData            = &spec_value;

         memset(stages, 0, sizeof(stages));
         stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
         stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
         stages[0].module = vs;
         stages[0].pName  = "main";
         stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
         stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
         stages[1].module = fs;
         stages[1].pName  = "main";
         stages[1].pSpecializationInfo = &spec;

         bind.binding   = 0;
         bind.stride    = sizeof(GL::Vertex);
         bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
         attrs[0].location = 0; attrs[0].binding = 0;
         attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[0].offset = offsetof(GL::Vertex, vert);
         attrs[1].location = 1; attrs[1].binding = 0;
         attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[1].offset = offsetof(GL::Vertex, normal);
         attrs[2].location = 2; attrs[2].binding = 0;
         attrs[2].format = VK_FORMAT_R32G32_SFLOAT;    attrs[2].offset = offsetof(GL::Vertex, tex);

         memset(&vin, 0, sizeof(vin));
         vin.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
         vin.vertexBindingDescriptionCount   = 1;
         vin.pVertexBindingDescriptions      = &bind;
         vin.vertexAttributeDescriptionCount = 3;
         vin.pVertexAttributeDescriptions    = attrs;

         memset(&ia, 0, sizeof(ia));
         ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
         ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

         memset(&vp, 0, sizeof(vp));
         vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
         vp.viewportCount = 1;
         vp.scissorCount  = 1;

         /* The clip-space fix (Y flip) makes front faces counter-clockwise in
          * Vulkan's framebuffer space, */
         memset(&rs, 0, sizeof(rs));
         rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
         rs.polygonMode = VK_POLYGON_MODE_FILL;
         rs.cullMode    = VK_CULL_MODE_BACK_BIT;
         rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
         rs.lineWidth   = 1.0f;

         memset(&ms, 0, sizeof(ms));
         ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
         ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

         memset(&ds, 0, sizeof(ds));
         ds.sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
         ds.depthTestEnable   = VK_TRUE;
         ds.depthWriteEnable  = VK_TRUE;
         ds.depthCompareOp    = VK_COMPARE_OP_LESS;

         memset(&cba, 0, sizeof(cba));
         cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
         memset(&cb, 0, sizeof(cb));
         cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
         cb.attachmentCount = 1;
         cb.pAttachments    = &cba;

         memset(&dyn, 0, sizeof(dyn));
         dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
         dyn.dynamicStateCount = 2;
         dyn.pDynamicStates    = dyn_states;

         memset(&pi, 0, sizeof(pi));
         pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
         pi.stageCount          = 2;
         pi.pStages             = stages;
         pi.pVertexInputState   = &vin;
         pi.pInputAssemblyState = &ia;
         pi.pViewportState      = &vp;
         pi.pRasterizationState = &rs;
         pi.pMultisampleState   = &ms;
         pi.pDepthStencilState  = &ds;
         pi.pColorBlendState    = &cb;
         pi.pDynamicState       = &dyn;
         pi.layout              = g.pipe_layout;
         pi.renderPass          = g.render_pass;
         pi.subpass             = 0;

         res = vkCreateGraphicsPipelines(g.dev, VK_NULL_HANDLE, 1, &pi, NULL, &g.pipeline);
         vkDestroyShaderModule(g.dev, vs, NULL);
         vkDestroyShaderModule(g.dev, fs, NULL);
         if (res != VK_SUCCESS)
         {
            g.pipeline = VK_NULL_HANDLE;
            return false;
         }
         return true;
      }

      bool pick_depth_format(void)
      {
         static const VkFormat candidates[] = {
            VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM
         };
         unsigned i;
         for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
         {
            VkFormatProperties fp;
            vkGetPhysicalDeviceFormatProperties(g.gpu, candidates[i], &fp);
            if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
               g.depth_format = candidates[i];
               return true;
            }
         }
         return false;
      }

      bool create_render_pass(void)
      {
         VkAttachmentDescription att[2];
         VkAttachmentReference cref, dref;
         VkSubpassDescription sub;
         VkSubpassDependency deps[2];
         VkRenderPassCreateInfo ri;

         memset(att, 0, sizeof(att));
         att[0].format         = COLOR_FORMAT;
         att[0].samples        = VK_SAMPLE_COUNT_1_BIT;
         att[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
         att[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
         att[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
         att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
         att[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED; /* we clear; frontend may have moved it */
         att[0].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         att[1].format         = g.depth_format;
         att[1].samples        = VK_SAMPLE_COUNT_1_BIT;
         att[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
         att[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
         att[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
         att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
         att[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
         att[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

         cref.attachment = 0; cref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
         dref.attachment = 1; dref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

         memset(&sub, 0, sizeof(sub));
         sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
         sub.colorAttachmentCount    = 1;
         sub.pColorAttachments       = &cref;
         sub.pDepthStencilAttachment = &dref;

         /* In: do not write the image until earlier users (the frontend's
          * reads of the previous frame of this sync index) are done. */
         memset(deps, 0, sizeof(deps));
         deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
         deps[0].dstSubpass    = 0;
         deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
         deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
         deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
         deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
         /* Out: this is the barrier the frontend relies on when we pass no
          * semaphores to set_image(). */
         deps[1].srcSubpass    = 0;
         deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
         deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
         deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_TRANSFER_BIT;
         deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
         deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;

         memset(&ri, 0, sizeof(ri));
         ri.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
         ri.attachmentCount = 2;
         ri.pAttachments    = att;
         ri.subpassCount    = 1;
         ri.pSubpasses      = &sub;
         ri.dependencyCount = 2;
         ri.pDependencies   = deps;
         return vkCreateRenderPass(g.dev, &ri, NULL, &g.render_pass) == VK_SUCCESS;
      }

      bool create_static_objects(void)
      {
         VkPhysicalDeviceProperties props;
         VkDescriptorSetLayoutBinding b[2];
         VkDescriptorSetLayoutCreateInfo li;
         VkPushConstantRange pcr;
         VkPipelineLayoutCreateInfo pli;
         VkSamplerCreateInfo si;
         VkDescriptorPoolSize ps;
         VkDescriptorPoolCreateInfo dpi;
         VkCommandPoolCreateInfo cpi;
         VkDescriptorSetLayout layouts[2];
         static const uint8_t white[4] = { 255, 255, 255, 255 };

         vkGetPhysicalDeviceProperties(g.gpu, &props);
         g.ubo_stride = props.limits.minUniformBufferOffsetAlignment;
         if (g.ubo_stride < sizeof(DrawUBO))
         {
            VkDeviceSize a = g.ubo_stride ? g.ubo_stride : 1;
            g.ubo_stride = ((sizeof(DrawUBO) + a - 1) / a) * a;
         }

         if (!pick_depth_format() || !create_render_pass())
            return false;

         /* set 0: per-draw dynamic UBO (fragment). */
         memset(b, 0, sizeof(b));
         b[0].binding         = 0;
         b[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
         b[0].descriptorCount = 1;
         b[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
         memset(&li, 0, sizeof(li));
         li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
         li.bindingCount = 1;
         li.pBindings    = b;
         if (vkCreateDescriptorSetLayout(g.dev, &li, NULL, &g.ubo_layout) != VK_SUCCESS)
            return false;

         /* set 1: diffuse + ambient maps. */
         memset(b, 0, sizeof(b));
         b[0].binding         = 0;
         b[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         b[0].descriptorCount = 1;
         b[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
         b[1]                 = b[0];
         b[1].binding         = 1;
         li.bindingCount      = 2;
         if (vkCreateDescriptorSetLayout(g.dev, &li, NULL, &g.tex_layout) != VK_SUCCESS)
            return false;

         pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
         pcr.offset     = 0;
         pcr.size       = sizeof(PushConstants); /* 128 B = guaranteed minimum */
         layouts[0] = g.ubo_layout;
         layouts[1] = g.tex_layout;
         memset(&pli, 0, sizeof(pli));
         pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
         pli.setLayoutCount         = 2;
         pli.pSetLayouts            = layouts;
         pli.pushConstantRangeCount = 1;
         pli.pPushConstantRanges    = &pcr;
         if (vkCreatePipelineLayout(g.dev, &pli, NULL, &g.pipe_layout) != VK_SUCCESS)
            return false;

         memset(&si, 0, sizeof(si));
         si.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
         si.magFilter    = VK_FILTER_LINEAR;
         si.minFilter    = VK_FILTER_LINEAR;
         si.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
         si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
         si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
         si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
         si.maxLod       = VK_LOD_CLAMP_NONE;
         if (vkCreateSampler(g.dev, &si, NULL, &g.sampler) != VK_SUCCESS)
            return false;

         ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
         ps.descriptorCount = VKR_MAX_SLOTS;
         memset(&dpi, 0, sizeof(dpi));
         dpi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
         dpi.maxSets       = VKR_MAX_SLOTS;
         dpi.poolSizeCount = 1;
         dpi.pPoolSizes    = &ps;
         if (vkCreateDescriptorPool(g.dev, &dpi, NULL, &g.ubo_pool) != VK_SUCCESS)
            return false;

         memset(&cpi, 0, sizeof(cpi));
         cpi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
         cpi.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
         cpi.queueFamilyIndex = g.qfamily;
         if (vkCreateCommandPool(g.dev, &cpi, NULL, &g.upload_pool) != VK_SUCCESS)
            return false;

         return create_texture(white, 1, 1, g.blank);
      }

      void write_mesh_descriptors(MeshGPU& m, const Tex *diffuse, const Tex *ambient)
      {
         VkDescriptorImageInfo ii[2];
         VkWriteDescriptorSet w[2];
         unsigned i;

         if (!diffuse) diffuse = &g.blank;
         if (!ambient) ambient = diffuse; /* GL path: ambient falls back to diffuse, then blank */

         memset(ii, 0, sizeof(ii));
         ii[0].sampler     = g.sampler;
         ii[0].imageView   = diffuse->view;
         ii[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         ii[1]             = ii[0];
         ii[1].imageView   = ambient->view;

         memset(w, 0, sizeof(w));
         for (i = 0; i < 2; i++)
         {
            w[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w[i].dstSet          = m.tex_set;
            w[i].dstBinding      = i;
            w[i].descriptorCount = 1;
            w[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w[i].pImageInfo      = &ii[i];
         }
         vkUpdateDescriptorSets(g.dev, 2, w, 0, NULL);
      }

      void copy_vec4(float *dst, const glm::vec3& v, float w)
      {
         dst[0] = v.x; dst[1] = v.y; dst[2] = v.z; dst[3] = w;
      }

      void destroy_everything_but_state(void)
      {
         if (g.dev == VK_NULL_HANDLE)
            return;

         unload_scene();
         destroy_slots();
         destroy_texture(g.blank);
         if (g.upload_pool)  vkDestroyCommandPool(g.dev, g.upload_pool, NULL);
         if (g.ubo_pool)     vkDestroyDescriptorPool(g.dev, g.ubo_pool, NULL);
         if (g.sampler)      vkDestroySampler(g.dev, g.sampler, NULL);
         if (g.pipe_layout)  vkDestroyPipelineLayout(g.dev, g.pipe_layout, NULL);
         if (g.tex_layout)   vkDestroyDescriptorSetLayout(g.dev, g.tex_layout, NULL);
         if (g.ubo_layout)   vkDestroyDescriptorSetLayout(g.dev, g.ubo_layout, NULL);
         if (g.render_pass)  vkDestroyRenderPass(g.dev, g.render_pass, NULL);
      }
   } /* anonymous namespace */

   /* ====================== public API ================================ */

   bool set_negotiation_interface(retro_environment_t env)
   {
      static struct retro_hw_render_context_negotiation_interface_vulkan neg;
      struct retro_hw_render_context_negotiation_interface query;
      unsigned version = 1; /* frontends without the SUPPORT query only know v1 */

      memset(&query, 0, sizeof(query));
      query.interface_type = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN;
      if (env(RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT, &query))
      {
         if (query.interface_version == 0)
            return false; /* frontend has no Vulkan negotiation at all */
         version = query.interface_version < 2 ? query.interface_version : 2;
      }

      /* We only provide application info. Device/instance creation is left
       * to the frontend on purpose: an OpenXR frontend has to create them
       * through xrCreateVulkan{Instance,Device}KHR so the runtime can add
       * its required extensions and pick the physical device. */
      memset(&neg, 0, sizeof(neg));
      neg.interface_type        = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN;
      neg.interface_version     = version;
      neg.get_application_info  = get_application_info;
      neg.create_device         = NULL;
      neg.destroy_device        = NULL;
      neg.create_instance       = NULL;
      neg.create_device2        = NULL;
      return env(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE, &neg);
   }

   bool init(retro_environment_t env)
   {
      const struct retro_hw_render_interface_vulkan *iface = NULL;

      if (g.inited)
      {
         /* context_reset without context_destroy: the device was lost/replaced. */
         VKR_LOG(RETRO_LOG_WARN, "[Vulkan] Context reset without destroy, dropping old state.\n");
         g = Globals();
      }

      if (!env(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void*)&iface) || !iface)
      {
         VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Frontend gave no HW render interface.\n");
         return false;
      }
      if (iface->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN ||
            iface->interface_version < RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION)
      {
         VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Need HW render interface version %d, frontend has %u.\n",
               RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION, iface->interface_version);
         return false;
      }

      vulkan_symbol_wrapper_init(iface->get_instance_proc_addr);
      if (!vulkan_symbol_wrapper_load_core_instance_symbols(iface->instance) ||
            !vulkan_symbol_wrapper_load_core_device_symbols(iface->device))
      {
         VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Failed to load Vulkan entry points.\n");
         return false;
      }

      g.iface   = iface;
      g.dev     = iface->device;
      g.gpu     = iface->gpu;
      g.queue   = iface->queue;
      g.qfamily = iface->queue_index; /* despite the name this is the queue *family* */

      if (!create_static_objects())
      {
         VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Failed to create renderer objects.\n");
         destroy_everything_but_state();
         g = Globals();
         return false;
      }

      g.inited = true;
      return true;
   }

   bool initialized(void)
   {
      return g.inited;
   }

   void destroy(void)
   {
      if (!g.inited)
         return;
      destroy_everything_but_state();
      g = Globals();
   }

   void unload_scene(void)
   {
      size_t i;
      std::map<std::string, Tex>::iterator it;

      if (g.dev == VK_NULL_HANDLE)
         return;

      wait_own_work(); /* in-flight frames may still read these */

      for (i = 0; i < g.meshes.size(); i++)
         destroy_buffer(g.meshes[i].vbo);
      g.meshes.clear();

      for (it = g.textures.begin(); it != g.textures.end(); ++it)
         destroy_texture(it->second);
      g.textures.clear();

      if (g.tex_pool)
         vkDestroyDescriptorPool(g.dev, g.tex_pool, NULL);
      g.tex_pool = VK_NULL_HANDLE;
      if (g.pipeline)
         vkDestroyPipeline(g.dev, g.pipeline, NULL);
      g.pipeline    = VK_NULL_HANDLE;
      g.scene_ready = false;
   }

   bool load_scene(const std::vector<OBJ::Part>& parts, ShaderMode mode)
   {
      size_t i;
      VkDescriptorPoolSize ps;
      VkDescriptorPoolCreateInfo dpi;

      if (!g.inited)
         return false;
      unload_scene();

      if (!create_pipeline(mode))
      {
         VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Pipeline creation failed.\n");
         return false;
      }

      ps.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      ps.descriptorCount = (uint32_t)(parts.size() * 2 + 2);
      memset(&dpi, 0, sizeof(dpi));
      dpi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      dpi.maxSets       = (uint32_t)parts.size() + 1;
      dpi.poolSizeCount = 1;
      dpi.pPoolSizes    = &ps;
      if (vkCreateDescriptorPool(g.dev, &dpi, NULL, &g.tex_pool) != VK_SUCCESS)
         return false;

      g.meshes.resize(parts.size());
      for (i = 0; i < parts.size(); i++)
      {
         const OBJ::Part& p = parts[i];
         MeshGPU& m = g.meshes[i];
         VkDescriptorSetAllocateInfo ai;
         const Tex *diffuse, *ambient;

         m.count      = (uint32_t)p.vertices.size();
         m.ambient    = p.material.ambient;
         m.diffuse    = p.material.diffuse;
         m.specular   = p.material.specular;
         m.spec_power = p.material.specular_power;
         m.alpha      = p.material.alpha_mod;

         if (m.count)
         {
            if (!create_buffer(m.count * sizeof(GL::Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m.vbo))
               return false;
            memcpy(m.vbo.map, &p.vertices[0], m.count * sizeof(GL::Vertex));
         }

         memset(&ai, 0, sizeof(ai));
         ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
         ai.descriptorPool     = g.tex_pool;
         ai.descriptorSetCount = 1;
         ai.pSetLayouts        = &g.tex_layout;
         if (vkAllocateDescriptorSets(g.dev, &ai, &m.tex_set) != VK_SUCCESS)
            return false;

         diffuse = get_texture(p.material.diffuse_map);
         ambient = p.material.ambient_map == p.material.diffuse_map ?
            diffuse : get_texture(p.material.ambient_map);
         write_mesh_descriptors(m, diffuse, ambient);
      }

      g.scene_ready = true;
      return true;
   }

   bool render(const Frame& fr, retro_video_refresh_t video_cb)
   {
      const struct retro_hw_render_interface_vulkan *I = g.iface;
      uint32_t mask, idx;
      unsigned p;
      size_t mi;
      VkDeviceSize draws, needed, draw_index = 0;
      VkCommandBufferBeginInfo bi;
      VkRenderPassBeginInfo rp;
      VkClearValue clears[2];
      VkSubmitInfo si;
      VkResult res;
      glm::mat4 clip(1.0f);

      if (!g.inited || !g.scene_ready || !fr.num_passes || fr.num_passes > 2 ||
            !fr.width || !fr.height)
         return false;

      mask = I->get_sync_index_mask(I->handle);
      if (mask != g.last_mask || g.slots.empty())
      {
         /* Documented as "device is idle when the mask changes". */
         destroy_slots();
         if (!create_slots(mask))
            return false;
      }

      idx = I->get_sync_index(I->handle);
      if (idx >= g.slots.size())
         return false;

      /* Frontend is done with everything it was given for this index... */
      I->wait_sync_index(I->handle);
      Slot& s = g.slots[idx];
      /* ...and so are our own earlier submissions for it. */
      vkWaitForFences(g.dev, 1, &s.fence, VK_TRUE, (uint64_t)-1);

      if (s.target.w != fr.width || s.target.h != fr.height)
      {
         destroy_target(s.target);
         if (!create_target(s.target, fr.width, fr.height))
         {
            VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] Failed to create %ux%u target.\n", fr.width, fr.height);
            return false;
         }
      }

      draws  = (VkDeviceSize)g.meshes.size() * fr.num_passes;
      needed = (draws ? draws : 1) * g.ubo_stride;
      if (!ensure_ubo(s, needed))
         return false;

      /* GL clip space (Y up, z -1..1) -> Vulkan (Y down, z 0..1). */
      clip[1][1] = -1.0f;
      clip[2][2] = 0.5f;
      clip[3][2] = 0.5f;

      vkResetCommandPool(g.dev, s.pool, 0);
      memset(&bi, 0, sizeof(bi));
      bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(s.cmd, &bi);

      memset(clears, 0, sizeof(clears));
      clears[0].color.float32[0] = 0.2f;
      clears[0].color.float32[1] = 0.2f;
      clears[0].color.float32[2] = 0.2f;
      clears[0].color.float32[3] = 1.0f;
      clears[1].depthStencil.depth = 1.0f;

      memset(&rp, 0, sizeof(rp));
      rp.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      rp.renderPass               = g.render_pass;
      rp.framebuffer              = s.target.fb;
      rp.renderArea.extent.width  = fr.width;
      rp.renderArea.extent.height = fr.height;
      rp.clearValueCount          = 2;
      rp.pClearValues             = clears;
      vkCmdBeginRenderPass(s.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(s.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipeline);

      for (p = 0; p < fr.num_passes; p++)
      {
         const Pass& ps = fr.passes[p];
         VkViewport vp;
         VkRect2D sc;
         glm::mat4 vp_mat = clip * ps.proj * ps.view;
         unsigned pw = ps.width;

         if (ps.x >= fr.width)
            continue;
         if (ps.x + pw > fr.width)
            pw = fr.width - ps.x;

         vp.x = (float)ps.x;      vp.y = 0.0f;
         vp.width = (float)pw;    vp.height = (float)ps.height;
         vp.minDepth = 0.0f;      vp.maxDepth = 1.0f;
         sc.offset.x = (int32_t)ps.x;   sc.offset.y = 0;
         sc.extent.width = pw;          sc.extent.height = ps.height;
         vkCmdSetViewport(s.cmd, 0, 1, &vp);
         vkCmdSetScissor(s.cmd, 0, 1, &sc);

         for (mi = 0; mi < g.meshes.size(); mi++)
         {
            const MeshGPU& m = g.meshes[mi];
            DrawUBO *u;
            PushConstants pc;
            glm::mat4 mvp = vp_mat * fr.model;
            uint32_t dyn_offset = (uint32_t)(draw_index * g.ubo_stride);
            VkDeviceSize zero = 0;

            if (!m.count)
               continue;

            u = (DrawUBO*)(s.ubo.map + draw_index * g.ubo_stride);
            draw_index++;
            copy_vec4(u->light,         fr.light,    0.0f);
            copy_vec4(u->light_ambient, fr.ambient,  0.0f);
            copy_vec4(u->eye,           ps.eye_pos,  0.0f);
            copy_vec4(u->mtl_ambient,   m.ambient,   0.0f);
            copy_vec4(u->mtl_diffuse,   m.diffuse,   0.0f);
            copy_vec4(u->mtl_specular,  m.specular,  0.0f);
            u->mtl_params[0] = m.spec_power;
            u->mtl_params[1] = m.alpha;
            u->mtl_params[2] = 0.0f;
            u->mtl_params[3] = 0.0f;

            memcpy(pc.model, &fr.model[0][0], sizeof(pc.model));
            memcpy(pc.mvp,   &mvp[0][0],      sizeof(pc.mvp));

            vkCmdBindDescriptorSets(s.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipe_layout,
                  0, 1, &s.ubo_set, 1, &dyn_offset);
            vkCmdBindDescriptorSets(s.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipe_layout,
                  1, 1, &m.tex_set, 0, NULL);
            vkCmdPushConstants(s.cmd, g.pipe_layout, VK_SHADER_STAGE_VERTEX_BIT,
                  0, sizeof(pc), &pc);
            vkCmdBindVertexBuffers(s.cmd, 0, 1, &m.vbo.buf, &zero);
            vkCmdDraw(s.cmd, m.count, 1, 0, 0);
         }
      }

      vkCmdEndRenderPass(s.cmd);
      vkEndCommandBuffer(s.cmd);

      memset(&si, 0, sizeof(si));
      si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      si.commandBufferCount = 1;
      si.pCommandBuffers    = &s.cmd;

      vkResetFences(g.dev, 1, &s.fence);
      I->lock_queue(I->handle);
      res = vkQueueSubmit(g.queue, 1, &si, s.fence);
      I->unlock_queue(I->handle);
      if (res != VK_SUCCESS)
      {
         /* Keep the fence signalled so the next wait cannot deadlock. */
         VkSubmitInfo empty;
         memset(&empty, 0, sizeof(empty));
         empty.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
         I->lock_queue(I->handle);
         vkQueueSubmit(g.queue, 0, &empty, s.fence);
         I->unlock_queue(I->handle);
         VKR_LOG(RETRO_LOG_ERROR, "[Vulkan] vkQueueSubmit failed (%d).\n", (int)res);
         return false;
      }

      /* No semaphores: the render pass' external dependency is the barrier. */
      I->set_image(I->handle, &s.target.desc, 0, NULL, VK_QUEUE_FAMILY_IGNORED);
      video_cb(RETRO_HW_FRAME_BUFFER_VALID, fr.width, fr.height, 0);
      return true;
   }
}

#endif /* HAVE_VULKAN */
