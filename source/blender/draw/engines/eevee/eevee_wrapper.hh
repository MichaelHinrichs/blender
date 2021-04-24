/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * Templated wrappers to make it easier to use GPU objects in C++.
 */

#pragma once

#include "BLI_utildefines.h"
#include "GPU_framebuffer.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

namespace blender::eevee {

template<
    /** Type of the values stored in this uniform buffer. */
    typename T,
    /** The number of values that can be stored in this uniform buffer. */
    int64_t len>
class StructArrayBuffer {
 private:
  T data_[len];
  GPUUniformBuf *ubo_;

 public:
  StructArrayBuffer()
  {
    ubo_ = GPU_uniformbuf_create_ex(sizeof(data_), nullptr, "StructArrayBuffer");
  }
  ~StructArrayBuffer()
  {
    GPU_uniformbuf_free(ubo_);
  }

  void push_update(void)
  {
    GPU_uniformbuf_update(ubo_, data_);
  }

  const GPUUniformBuf *ubo_get(void) const
  {
    return ubo_;
  }

  /**
   * Get the value at the given index. This invokes undefined behavior when the index is out of
   * bounds.
   */
  const T &operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < len);
    return data_[index];
  }

  T &operator[](int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < len);
    return data_[index];
  }

  /**
   * Get a pointer to the beginning of the array.
   */
  const T *data() const
  {
    return data_;
  }
  T *data()
  {
    return data_;
  }

  /**
   * Iterator
   */
  const T *begin() const
  {
    return data_;
  }
  const T *end() const
  {
    return data_ + len;
  }

  T *begin()
  {
    return data_;
  }
  T *end()
  {
    return data_ + len;
  }
};

/** Simpler version where data is not an array. */
template<typename T> class StructBuffer : public T {
 private:
  GPUUniformBuf *ubo_;

 public:
  StructBuffer()
  {
    ubo_ = GPU_uniformbuf_create_ex(sizeof(T), nullptr, "StructBuffer");
  }
  ~StructBuffer()
  {
    DRW_UBO_FREE_SAFE(ubo_);
  }

  void push_update(void)
  {
    T *data = static_cast<T *>(this);
    GPU_uniformbuf_update(ubo_, data);
  }

  const GPUUniformBuf *ubo_get(void) const
  {
    return ubo_;
  }

  StructBuffer<T> &operator=(const T &other)
  {
    *static_cast<T *>(this) = other;
    return *this;
  }
};

class Texture {
 private:
  GPUTexture *tx_ = nullptr;
  const char *name_;

 public:
  Texture() : name_("eevee::Texture"){};
  Texture(const char *name) : name_(name){};

  Texture(const char *name,
          int w,
          int h = 0,
          int d = 0,
          int mips = 1,
          eGPUTextureFormat format = GPU_RGBA8,
          float *data = nullptr,
          bool layered = false,
          bool cubemap = false)
      : Texture(name)
  {
    if (h == 0) {
      tx_ = GPU_texture_create_1d(name, w, mips, format, data);
    }
    else if (d == 0) {
      if (layered) {
        tx_ = GPU_texture_create_1d_array(name, w, h, mips, format, data);
      }
      else {
        tx_ = GPU_texture_create_2d(name, w, h, mips, format, data);
      }
    }
    else if (cubemap) {
      if (layered) {
        tx_ = GPU_texture_create_cube_array(name, w, d, mips, format, data);
      }
      else {
        tx_ = GPU_texture_create_cube(name, w, mips, format, data);
      }
    }
    else {
      if (layered) {
        tx_ = GPU_texture_create_2d_array(name, w, h, d, mips, format, data);
      }
      else {
        tx_ = GPU_texture_create_3d(name, w, h, d, mips, format, GPU_DATA_FLOAT, data);
      }
    }
  }

  ~Texture()
  {
    GPU_TEXTURE_FREE_SAFE(tx_);
  }

  void ensure(const char *name, int w, int h, int mips, eGPUTextureFormat format)
  {
    /* TODO(fclem) In the future, we need to check if mip_count did not change.
     * For now it's ok as we always define all mip level.*/
    if (tx_ && (GPU_texture_width(tx_) != w || GPU_texture_height(tx_) != h)) {
      GPU_TEXTURE_FREE_SAFE(tx_);
    }
    if (tx_ == nullptr) {
      tx_ = GPU_texture_create_2d(name, w, h, mips, format, nullptr);
      if (mips > 1) {
        /* TODO(fclem) Remove once we have immutable storage or when mips are
         * generated on creation. */
        GPU_texture_generate_mipmap(tx_);
      }
    }
  }

  void ensure(int w, int h, int mips, eGPUTextureFormat format)
  {
    ensure(name_, w, h, mips, format);
  }

  Texture &operator=(Texture &a)
  {
    if (*this != a) {
      this->tx_ = a.tx_;
      this->name_ = a.name_;
      a.tx_ = nullptr;
    }
    return *this;
  }
  /* To be able to use it with DRW_shgroup_uniform_texture(). */
  operator GPUTexture *() const
  {
    return tx_;
  }
  /* To be able to use it with DRW_shgroup_uniform_texture_ref(). */
  GPUTexture **operator&()
  {
    return &tx_;
  }

  int width(void) const
  {
    return GPU_texture_width(tx_);
  }
  int height(void) const
  {
    return GPU_texture_height(tx_);
  }
};

class Framebuffer {
 private:
  GPUFrameBuffer *fb_ = nullptr;
  const char *name_;

 public:
  Framebuffer() : name_(""){};
  Framebuffer(const char *name) : name_(name){};

  ~Framebuffer()
  {
    GPU_FRAMEBUFFER_FREE_SAFE(fb_);
  }

  void ensure(GPUAttachment depth = GPU_ATTACHMENT_NONE,
              GPUAttachment color1 = GPU_ATTACHMENT_NONE,
              GPUAttachment color2 = GPU_ATTACHMENT_NONE,
              GPUAttachment color3 = GPU_ATTACHMENT_NONE,
              GPUAttachment color4 = GPU_ATTACHMENT_NONE,
              GPUAttachment color5 = GPU_ATTACHMENT_NONE,
              GPUAttachment color6 = GPU_ATTACHMENT_NONE)
  {
    GPU_framebuffer_ensure_config(&fb_, {depth, color1, color2, color3, color4, color5, color6});
  }

  Framebuffer &operator=(Framebuffer &a)
  {
    if (*this != a) {
      this->fb_ = a.fb_;
      this->name_ = a.name_;
      a.fb_ = nullptr;
    }
    return *this;
  }

  operator GPUFrameBuffer *() const
  {
    return fb_;
  }
};

}  // namespace blender::eevee