/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include <fmt/format.h>

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_query.hh"

#include "GEO_randomize.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

// #define USE_TIMEIT

#ifdef USE_TIMEIT
#  include "BLI_time.h"
#  include "BLI_time_utildefines.h"
#endif

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  DecimateModifierData *dmd = reinterpret_cast<DecimateModifierData *>(md);
  INIT_DEFAULT_STRUCT_AFTER(dmd, modifier);
}

/** Owned by the evaluated modifier, preserved by depsgraph runtime backup. Mesh copies use
 * implicit sharing, so downstream modifiers cannot overwrite the cached input or result. */
struct DecimateRuntimeData {
  Mesh *input = nullptr;
  Mesh *result = nullptr;
  DecimateModifierData settings;

  ~DecimateRuntimeData()
  {
    if (input) {
      BKE_id_free(nullptr, input);
    }
    if (result) {
      BKE_id_free(nullptr, result);
    }
  }
};

static void free_runtime_data(void *runtime)
{
  MEM_delete(static_cast<DecimateRuntimeData *>(runtime));
}

static void free_data(ModifierData *md)
{
  free_runtime_data(md->runtime);
  md->runtime = nullptr;
}

static bool bytes_equal(const void *a, const void *b, const size_t size)
{
  return size == 0 || a == b || (a && b && memcmp(a, b, size) == 0);
}

static bool attribute_values_equal(const CPPType &type,
                                   const void *a,
                                   const void *b,
                                   const int64_t size)
{
  if (a == b || size == 0) {
    return true;
  }
  if (type.is_trivial) {
    return bytes_equal(a, b, size * type.size);
  }
  for (const int64_t i : IndexRange(size)) {
    if (!type.is_equal_or_false(static_cast<const char *>(a) + i * type.size,
                                static_cast<const char *>(b) + i * type.size))
    {
      return false;
    }
  }
  return true;
}

static bool attributes_equal(const Mesh &a, const Mesh &b)
{
  const bke::AttributeStorage &attrs_a = a.attribute_storage.wrap();
  const bke::AttributeStorage &attrs_b = b.attribute_storage.wrap();
  if (attrs_a.count() != attrs_b.count()) {
    return false;
  }
  for (const int i : IndexRange(attrs_a.count())) {
    const bke::Attribute &attr_a = attrs_a.at_index(i);
    const bke::Attribute *attr_b = &attrs_b.at_index(i);
    if (attr_a.name() != attr_b->name() || attr_a.domain() != attr_b->domain() ||
        attr_a.data_type() != attr_b->data_type() ||
        attr_a.storage_type() != attr_b->storage_type())
    {
      return false;
    }
    const CPPType &type = bke::attribute_type_to_cpp_type(attr_a.data_type());
    if (const auto *data_a = std::get_if<bke::Attribute::ArrayData>(&attr_a.data())) {
      const auto &data_b = std::get<bke::Attribute::ArrayData>(attr_b->data());
      if (data_a->size != data_b.size ||
          !attribute_values_equal(type, data_a->data, data_b.data, data_a->size))
      {
        return false;
      }
    }
    else {
      const auto &single_a = std::get<bke::Attribute::SingleData>(attr_a.data());
      const auto &single_b = std::get<bke::Attribute::SingleData>(attr_b->data());
      if (!attribute_values_equal(type, single_a.value, single_b.value, 1)) {
        return false;
      }
    }
  }
  return true;
}

static bool custom_data_equal(const CustomData &a, const CustomData &b, const int size)
{
  int index_b = 0;
  for (const int i : IndexRange(a.totlayer)) {
    const CustomDataLayer &la = a.layers[i];
    /* The BMesh conversion ignores these layers, as does the evaluated mesh snapshot copy.
     * Requiring them in the key would prevent reuse whenever an unused vertex group exists. */
    if (la.flag & CD_FLAG_NOCOPY) {
      continue;
    }
    while (index_b < b.totlayer && (b.layers[index_b].flag & CD_FLAG_NOCOPY)) {
      index_b++;
    }
    if (index_b == b.totlayer) {
      return false;
    }
    const CustomDataLayer &lb = b.layers[index_b++];
    if (la.type != lb.type || la.flag != lb.flag || la.uid != lb.uid || la.active != lb.active ||
        la.active_rnd != lb.active_rnd || !STREQ(la.name, lb.name))
    {
      return false;
    }
    switch (la.type) {
      case CD_MDEFORMVERT: {
        const auto *va = static_cast<const MDeformVert *>(la.data);
        const auto *vb = static_cast<const MDeformVert *>(lb.data);
        if (va == vb) {
          break;
        }
        for (const int j : IndexRange(size)) {
          if (va[j].totweight != vb[j].totweight || va[j].flag != vb[j].flag ||
              !bytes_equal(va[j].dw, vb[j].dw, sizeof(MDeformWeight) * va[j].totweight))
          {
            return false;
          }
        }
        break;
      }
      case CD_ORIGINDEX:
      case CD_NORMAL:
      case CD_ORCO:
      case CD_CLOTH_ORCO:
      case CD_SHAPE_KEYINDEX:
      case CD_SHAPEKEY:
      case CD_ORIGSPACE_MLOOP:
      case CD_MVERT_SKIN:
        if (!bytes_equal(la.data, lb.data, size_t(size) * CustomData_sizeof(la.type))) {
          return false;
        }
        break;
      default:
        /* In particular, never compare pointer-containing Multires layers as raw bytes.
         * Unhandled layers conservatively use the ordinary evaluation path. */
        return false;
    }
  }
  while (index_b < b.totlayer && (b.layers[index_b].flag & CD_FLAG_NOCOPY)) {
    index_b++;
  }
  return index_b == b.totlayer;
}

static bool mesh_input_equal(const Mesh &a, const Mesh &b)
{
  if (a.verts_num != b.verts_num || a.edges_num != b.edges_num || a.faces_num != b.faces_num ||
      a.corners_num != b.corners_num || a.normal_weighting_mode != b.normal_weighting_mode ||
      a.normal_weight != b.normal_weight ||
      a.normal_weight_threshold != b.normal_weight_threshold || a.act_face != b.act_face ||
      a.totselect != b.totselect ||
      !bytes_equal(a.mselect, b.mselect, sizeof(MSelect) * size_t(a.totselect)) ||
      !bytes_equal(a.face_offset_indices, b.face_offset_indices, sizeof(int) * (a.faces_num + 1)))
  {
    return false;
  }
  /* Group ordering controls the interpretation of deform weights. */
  const auto *ga = static_cast<const bDeformGroup *>(a.vertex_group_names.first);
  const auto *gb = static_cast<const bDeformGroup *>(b.vertex_group_names.first);
  for (; ga && gb; ga = ga->next, gb = gb->next) {
    if (!STREQ(ga->name, gb->name)) {
      return false;
    }
  }
  return ga == nullptr && gb == nullptr && attributes_equal(a, b) &&
         custom_data_equal(a.vert_data, b.vert_data, a.verts_num) &&
         custom_data_equal(a.edge_data, b.edge_data, a.edges_num) &&
         custom_data_equal(a.face_data, b.face_data, a.faces_num) &&
         custom_data_equal(a.corner_data, b.corner_data, a.corners_num);
}

static bool settings_equal(const DecimateModifierData &a, const DecimateModifierData &b)
{
  return a.mode == b.mode && a.percent == b.percent && a.iter == b.iter &&
         a.delimit == b.delimit && a.symmetry_axis == b.symmetry_axis && a.angle == b.angle &&
         STREQ(a.defgrp_name, b.defgrp_name) && a.defgrp_factor == b.defgrp_factor &&
         a.flag == b.flag;
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  DecimateModifierData *dmd = reinterpret_cast<DecimateModifierData *>(md);

  /* Ask for vertex-groups if we need them. */
  if (dmd->defgrp_name[0] != '\0' && (dmd->defgrp_factor > 0.0f)) {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static DecimateModifierData *getOriginalModifierData(const DecimateModifierData *dmd,
                                                     const ModifierEvalContext *ctx)
{
  Object *ob_orig = DEG_get_original(ctx->object);
  return reinterpret_cast<DecimateModifierData *>(
      BKE_modifiers_findby_name(ob_orig, dmd->modifier.name));
}

static void updateFaceCount(const ModifierEvalContext *ctx,
                            DecimateModifierData *dmd,
                            int face_count)
{
  dmd->face_count = face_count;

  if (DEG_is_active(ctx->depsgraph)) {
    /* update for display only */
    DecimateModifierData *dmd_orig = getOriginalModifierData(dmd, ctx);
    dmd_orig->face_count = face_count;
  }
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *meshData)
{
  DecimateModifierData *dmd = reinterpret_cast<DecimateModifierData *>(md);
  Mesh *mesh = meshData, *result = nullptr;
  BMesh *bm;
  bool calc_vert_normal;
  bool calc_face_normal;
  float *vweights = nullptr;

#ifdef USE_TIMEIT
  TIMEIT_START(decim);
#endif

  const bool use_cache = (dmd->flag & MOD_DECIM_FLAG_DISABLE_CACHE) == 0;
  if (!use_cache) {
    /* Release even for no-op settings or an input with too few faces. */
    free_data(md);
  }

  /* Set up front so we don't show invalid info in the UI. */
  updateFaceCount(ctx, dmd, mesh->faces_num);

  switch (dmd->mode) {
    case MOD_DECIM_MODE_COLLAPSE:
      if (dmd->percent == 1.0f) {
        return mesh;
      }
      calc_face_normal = true;
      calc_vert_normal = true;
      break;
    case MOD_DECIM_MODE_UNSUBDIV:
      if (dmd->iter == 0) {
        return mesh;
      }
      calc_face_normal = false;
      calc_vert_normal = false;
      break;
    case MOD_DECIM_MODE_DISSOLVE:
      if (dmd->angle == 0.0f) {
        return mesh;
      }
      calc_face_normal = true;
      calc_vert_normal = false;
      break;
    default:
      return mesh;
  }

  if (dmd->face_count <= 3) {
    BKE_modifier_set_error(ctx->object, md, "Modifier requires more than 3 input faces");
    return mesh;
  }

  auto *runtime = static_cast<DecimateRuntimeData *>(md->runtime);
  if (use_cache && runtime && runtime->result && settings_equal(*dmd, runtime->settings) &&
      mesh_input_equal(*mesh, *runtime->input))
  {
    result = BKE_mesh_copy_for_eval(*runtime->result);
    /* Refresh non-geometric settings and ID pointers from this evaluation, not the snapshot. */
    BLI_freelistN(&result->vertex_group_names);
    MEM_SAFE_DELETE(result->active_color_attribute);
    MEM_SAFE_DELETE(result->default_color_attribute);
    MEM_SAFE_DELETE(result->active_uv_map_attribute);
    MEM_SAFE_DELETE(result->default_uv_map_attribute);
    MEM_SAFE_DELETE(result->stencil_uv_map_attribute);
    MEM_SAFE_DELETE(result->clone_uv_map_attribute);
    BKE_mesh_copy_parameters_for_eval(result, mesh);
    updateFaceCount(ctx, dmd, result->faces_num);
    return result;
  }
  /* Release an obsolete entry before doing the expensive evaluation. */
  free_data(md);

  if (dmd->mode == MOD_DECIM_MODE_COLLAPSE) {
    if (dmd->defgrp_name[0] && (dmd->defgrp_factor > 0.0f)) {
      const MDeformVert *dvert;
      int defgrp_index;

      MOD_get_vgroup(ctx->object, mesh, dmd->defgrp_name, &dvert, &defgrp_index);

      if (dvert) {
        const uint vert_tot = mesh->verts_num;
        uint i;

        vweights = MEM_new_array_uninitialized<float>(vert_tot, __func__);

        if (dmd->flag & MOD_DECIM_FLAG_INVERT_VGROUP) {
          for (i = 0; i < vert_tot; i++) {
            vweights[i] = 1.0f - BKE_defvert_find_weight(&dvert[i], defgrp_index);
          }
        }
        else {
          for (i = 0; i < vert_tot; i++) {
            vweights[i] = BKE_defvert_find_weight(&dvert[i], defgrp_index);
          }
        }
      }
    }
  }

  BMeshCreateParams create_params{};
  BMeshFromMeshParams convert_params{};
  convert_params.calc_face_normal = calc_face_normal;
  convert_params.calc_vert_normal = calc_vert_normal;
  convert_params.cd_mask_extra.vmask = CD_MASK_ORIGINDEX;
  convert_params.cd_mask_extra.emask = CD_MASK_ORIGINDEX;
  convert_params.cd_mask_extra.pmask = CD_MASK_ORIGINDEX;

  bm = BKE_mesh_to_bmesh_ex(mesh, &create_params, &convert_params);

  switch (dmd->mode) {
    case MOD_DECIM_MODE_COLLAPSE: {
      const bool do_triangulate = (dmd->flag & MOD_DECIM_FLAG_TRIANGULATE) != 0;
      const int symmetry_axis = (dmd->flag & MOD_DECIM_FLAG_SYMMETRY) ? dmd->symmetry_axis : -1;
      const float symmetry_eps = 0.00002f;
      BM_mesh_decimate_collapse(bm,
                                dmd->percent,
                                vweights,
                                dmd->defgrp_factor,
                                do_triangulate,
                                symmetry_axis,
                                symmetry_eps);
      break;
    }
    case MOD_DECIM_MODE_UNSUBDIV: {
      BM_mesh_decimate_unsubdivide(bm, dmd->iter);
      break;
    }
    case MOD_DECIM_MODE_DISSOLVE: {
      const bool do_dissolve_boundaries = (dmd->flag & MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS) != 0;
      BM_mesh_decimate_dissolve(bm, dmd->angle, do_dissolve_boundaries, BMO_Delimit(dmd->delimit));
      break;
    }
  }

  if (vweights) {
    MEM_delete(vweights);
  }

  updateFaceCount(ctx, dmd, bm->totface);

  /* Make sure we never allocated these. */
  BLI_assert(bm->vtoolflagpool == nullptr && bm->etoolflagpool == nullptr &&
             bm->ftoolflagpool == nullptr);

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);

  BM_mesh_free(bm);

  geometry::debug_randomize_mesh_order(result);

  /* A single entry per evaluated modifier. No file data or original-ID cache is retained. */
  if (use_cache && mesh_input_equal(*mesh, *mesh)) {
    runtime = MEM_new<DecimateRuntimeData>(__func__);
    runtime->settings = *dmd;
    runtime->input = BKE_mesh_copy_for_eval(*mesh);
    runtime->result = BKE_mesh_copy_for_eval(*result);
    md->runtime = runtime;
  }

#ifdef USE_TIMEIT
  TIMEIT_END(decim);
#endif

  return result;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  ui::Layout &layout = *panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int decimate_type = RNA_enum_get(ptr, "decimate_type");
  char face_count_str[BLI_STR_FORMAT_INT32_GROUPED_SIZE];
  BLI_str_format_int_grouped(face_count_str, RNA_int_get(ptr, "face_count"));
  std::string count_info = fmt::format(fmt::runtime(RPT_("Face Count: {}")), face_count_str);

  layout.prop(ptr, "decimate_type", ui::ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout.use_property_split_set(true);

  if (decimate_type == MOD_DECIM_MODE_COLLAPSE) {
    layout.prop(ptr, "ratio", ui::ITEM_R_SLIDER, std::nullopt, ICON_NONE);

    ui::Layout &row = layout.row(true, IFACE_("Symmetry"));
    row.use_property_decorate_set(false);
    ui::Layout *sub = &row.row(true);
    sub->prop(ptr, "use_symmetry", UI_ITEM_NONE, "", ICON_NONE);
    sub = &sub->row(true);
    sub->active_set(RNA_boolean_get(ptr, "use_symmetry"));
    sub->prop(ptr, "symmetry_axis", ui::ITEM_R_EXPAND, std::nullopt, ICON_NONE);
    row.decorator(ptr, "symmetry_axis", 0);

    layout.prop(ptr, "use_collapse_triangulate", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);
    sub = &layout.row(true);
    bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;
    sub->active_set(has_vertex_group);
    sub->prop(ptr, "vertex_group_factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (decimate_type == MOD_DECIM_MODE_UNSUBDIV) {
    layout.prop(ptr, "iterations", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else { /* decimate_type == MOD_DECIM_MODE_DISSOLVE. */
    layout.prop(ptr, "angle_limit", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    ui::Layout &col = layout.column(false);
    col.prop(ptr, "delimit", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout.prop(ptr, "use_dissolve_boundaries", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  layout.prop(ptr, "use_cache", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout.label(count_info, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Decimate, panel_draw);
}

ModifierTypeInfo modifierType_Decimate = {
    /*idname*/ "Decimate",
    /*name*/ N_("Decimate"),
    /*struct_name*/ "DecimateModifierData",
    /*struct_size*/ sizeof(DecimateModifierData),
    /*srna*/ &RNA_DecimateModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs,
    /*icon*/ ICON_MOD_DECIM,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ free_runtime_data,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};

}  // namespace blender
