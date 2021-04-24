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
 * A culling object is a data structure that contains fine grained culling
 * of entities against in the whole view frustum. The Culling structure contains the
 * final entity list since it has to have a special order.
 *
 * Follows the principles of Tiled Culling + Z binning from:
 * "Improved Culling for Tiled and Clustered Rendering"
 * by Michal Drobot
 * http://advances.realtimerendering.com/s2017/2017_Sig_Improved_Culling_final.pdf
 */

#pragma once

#include "DRW_render.h"

#include "BLI_vector.hh"

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name CullingBatch
 * \{ */

/**
 * Do not use directly. Use Culling object instead.
 */
template<
    /* The type of items to cull */
    typename T,
    /* The type of data contained in T we need to send to GPU. */
    typename Tdata,
    /* Culling pass type for 2D culling. Should have a render() method. */
    typename Tculling_pass>
class CullingBatch {
 private:
  /* Items to order in Z. */
  struct ItemHandle {
    /** Index inside item_source_. */
    uint32_t source_index;
    /** Signed Z distance along camera Z axis. */
    float z_dist;
    /** Item radius. */
    float radius;
  };

  /** Compact handle list to order without moving source. */
  Vector<ItemHandle, CULLING_ITEM_BATCH> item_handles_;
  /** Z ordered items. */
  StructArrayBuffer<Tdata, CULLING_ITEM_BATCH> item_data_;
  /** Z bins. */
  CullingDataBuf culling_data_;
  /** Tile texture and framebuffer handling the 2D culling. */
  eevee::Texture tiles_tx_ = Texture("culling_tx_");
  eevee::Framebuffer tiles_fb_;

 public:
  CullingBatch(){};
  ~CullingBatch(){};

  void init(const int extent[2])
  {
    item_handles_.clear();

    uint tile_size = 8;

    uint res[2] = {divide_ceil_u(extent[0], tile_size), divide_ceil_u(extent[1], tile_size)};

    tiles_tx_.ensure(UNPACK2(res), 1, GPU_RGBA32UI);

    culling_data_.tile_size = tile_size;
    for (int i = 0; i < 2; i++) {
      culling_data_.tile_to_uv_fac[i] = tile_size / (float)extent[i];
    }

    // tiles_tx_.ensure(1, 1, 1, GPU_RGBA32UI);
    // uvec4 no_2D_culling = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    // GPU_texture_update(tiles_tx_, GPU_DATA_UINT, no_2D_culling);

    tiles_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(tiles_tx_));
  }

  void insert(int32_t index, float z_dist, float radius)
  {
    ItemHandle handle = {(uint32_t)index, z_dist, radius};
    item_handles_.append(handle);
  }

  void finalize(Tculling_pass &culling_pass,
                const Vector<T> &item_source,
                float near_z,
                float far_z)
  {
    culling_data_.zbin_scale = -CULLING_ZBIN_COUNT / fabsf(far_z - near_z);
    culling_data_.zbin_bias = -near_z * culling_data_.zbin_scale;

    /* Order items by Z distance to the camera. */
    std::sort(item_handles_.begin(),
              item_handles_.end(),
              [](const ItemHandle &a, const ItemHandle &b) { return a.z_dist > b.z_dist; });
    /* Init min-max for each bin. */
    for (auto i : IndexRange(CULLING_ZBIN_COUNT)) {
      uint16_t *zbin_minmax = (uint16_t *)culling_data_.zbins;
      zbin_minmax[i * 2 + 0] = CULLING_ITEM_BATCH - 1;
      zbin_minmax[i * 2 + 1] = 0;
    }
    /* Fill the GPU data buffer. */
    for (auto item_idx : item_handles_.index_range()) {
      ItemHandle &handle = item_handles_[item_idx];
      item_data_[item_idx] = item_source[handle.source_index];
      /* Register to Z bins. */
      int z_min = max_ii(culling_z_to_zbin(culling_data_, handle.z_dist + handle.radius), 0);
      int z_max = min_ii(culling_z_to_zbin(culling_data_, handle.z_dist - handle.radius),
                         CULLING_ZBIN_COUNT - 1);
      for (auto z : IndexRange(z_min, z_max - z_min + 1)) {
        BLI_assert(z >= 0 && z < CULLING_ZBIN_COUNT);
        uint16_t(&zbin_minmax)[2] = ((uint16_t(*)[2])culling_data_.zbins)[z];
        if (item_idx < zbin_minmax[0]) {
          zbin_minmax[0] = (uint16_t)item_idx;
        }
        if (item_idx > zbin_minmax[1]) {
          zbin_minmax[1] = (uint16_t)item_idx;
        }
      }
    }
    /* Set item count for no-cull iterator. */
    culling_data_.items_count = item_handles_.size();
    /* Upload data to GPU. */
    item_data_.push_update();
    culling_data_.push_update();

    GPU_framebuffer_bind(tiles_fb_);
    culling_pass.render(item_data_.ubo_get(), culling_data_.ubo_get());
  }

  /**
   * Getters
   **/
  bool is_full(void)
  {
    return item_handles_.size() == CULLING_ITEM_BATCH;
  }
  const GPUUniformBuf *data_ubo_get(void)
  {
    return item_data_.ubo_get();
  }
  const GPUUniformBuf *culling_ubo_get(void)
  {
    return culling_data_.ubo_get();
  }
  GPUTexture *culling_texture_get(void)
  {
    return tiles_tx_;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Culling
 * \{ */

template<
    /* The type of items to cull */
    typename T,
    /* The type of data contained in T we need to send to GPU. */
    typename Tdata,
    /* Culling pass type for 2D culling. Should have a render() method. */
    typename Tculling_pass,
    /* True if items can be added in multiple batches. */
    bool is_extendable = false>
class Culling {
 private:
  using CullingBatchType = CullingBatch<T, Tdata, Tculling_pass>;
  /** Multiple culling batches containing at most CULLING_ITEM_BATCH items worth of data. */
  Vector<CullingBatchType *> batches_;
  /** Number of active batches. Allocated count may be higher. */
  int used_batch_count_;
  /** Pointer to the active batch being filled. */
  CullingBatchType *active_batch_;
  /** Vector to source the item from. */
  const Vector<T> &item_source_;
  /** Used to get Z distance. */
  float camera_z_axis_[3];
  float camera_z_offset_;
  /** View for which the culling is computed. */
  const DRWView *view_;
  /** View resolution. */
  int extent_[2];

 public:
  Culling(const Vector<T> &items_source) : item_source_(items_source){};
  ~Culling()
  {
    for (CullingBatchType *batch : batches_) {
      delete batch;
    }
  }

  void set_view(const DRWView *view, const int extent[2])
  {
    view_ = view;
    copy_v2_v2_int(extent_, extent);

    float viewinv[4][4];
    DRW_view_viewmat_get(view, viewinv, true);

    copy_v3_v3(camera_z_axis_, viewinv[2]);
    camera_z_offset_ = -dot_v3v3(camera_z_axis_, viewinv[3]);

    if (batches_.size() == 0) {
      batches_.append(new CullingBatchType());
    }

    used_batch_count_ = 1;
    active_batch_ = batches_[0];
    active_batch_->init(extent_);
  }

  /* Returns true if we cannot add any more items.
   * In this case, the caller is expected to not try to insert another item.  */
  bool insert(int32_t index, BoundSphere &bsphere)
  {
    if (!DRW_culling_sphere_test(view_, &bsphere)) {
      return false;
    }

    if (active_batch_->is_full()) {
      BLI_assert(is_extendable);
      /* TODO(fclem) degrow vector of batches. */
      if (batches_.size() < (used_batch_count_ + 1)) {
        batches_.append(new CullingBatchType());
      }
      active_batch_ = batches_[used_batch_count_];
      active_batch_->init(extent_);
      used_batch_count_++;
    }

    float z_dist = dot_v3v3(bsphere.center, camera_z_axis_) + camera_z_offset_;
    active_batch_->insert(index, z_dist, bsphere.radius);

    return active_batch_->is_full();
  }

  void finalize(Tculling_pass &culling_pass)
  {
    float near_z = DRW_view_near_distance_get(view_);
    float far_z = DRW_view_far_distance_get(view_);

    for (auto i : IndexRange(used_batch_count_)) {
      batches_[i]->finalize(culling_pass, item_source_, near_z, far_z);
    }
  }

  /**
   * Getters
   **/
  CullingBatchType *operator[](int64_t index)
  {
    return batches_[index];
  }
  IndexRange index_range(void) const
  {
    return IndexRange(used_batch_count_);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name CullingDebugPass
 * \{ */

class CullingDebugPass {
 private:
  Instance &inst_;

  GPUTexture *input_depth_tx_ = nullptr;

  DRWPass *debug_ps_ = nullptr;

 public:
  CullingDebugPass(Instance &inst) : inst_(inst){};

  void sync(void);
  void render(GPUTexture *input_depth_tx);
};

/** \} */

}  // namespace blender::eevee