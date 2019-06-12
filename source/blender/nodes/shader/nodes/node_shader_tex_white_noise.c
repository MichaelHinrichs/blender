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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

#include "../node_shader_util.h"

/* **************** WHITE NOISE **************** */

static bNodeSocketTemplate sh_node_tex_white_noise_in[] = {
    {SOCK_VECTOR, 1, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, 1, N_("W"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, 0, ""}};

static bNodeSocketTemplate sh_node_tex_white_noise_out[] = {
    {SOCK_FLOAT, 0, N_("Fac"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, 0, ""},
};

static void node_shader_init_tex_white_noise(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 3;
}

static int gpu_shader_tex_white_noise(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData *UNUSED(execdata),
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  static const char *names[] = {
      "",
      "white_noise_1D",
      "white_noise_2D",
      "white_noise_3D",
      "white_noise_4D",
  };

  GPU_stack_link(mat, node, names[node->custom1], in, out);
  return true;
}

static void node_shader_update_tex_white_noise(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *inVecSock = BLI_findlink(&node->inputs, 0);
  bNodeSocket *inFacSock = BLI_findlink(&node->inputs, 1);

  switch (node->custom1) {
    case 1:
      inVecSock->flag |= SOCK_UNAVAIL;
      inFacSock->flag &= ~SOCK_UNAVAIL;
      break;
    case 4:
      inVecSock->flag &= ~SOCK_UNAVAIL;
      inFacSock->flag &= ~SOCK_UNAVAIL;
      break;
    default:
      inVecSock->flag &= ~SOCK_UNAVAIL;
      inFacSock->flag |= SOCK_UNAVAIL;
  }
}

void register_node_type_sh_tex_white_noise(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_WHITE_NOISE, "White Noise Texture", NODE_CLASS_TEXTURE, 0);
  node_type_socket_templates(&ntype, sh_node_tex_white_noise_in, sh_node_tex_white_noise_out);
  node_type_init(&ntype, node_shader_init_tex_white_noise);
  node_type_storage(&ntype, "", NULL, NULL);
  node_type_gpu(&ntype, gpu_shader_tex_white_noise);
  node_type_update(&ntype, node_shader_update_tex_white_noise);

  nodeRegisterType(&ntype);
}
