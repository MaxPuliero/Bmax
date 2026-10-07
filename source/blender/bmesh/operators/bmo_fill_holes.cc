/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Fill boundary edge loop(s) with faces.
 */

#include <climits>
#include <cstdlib>

#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

namespace blender {

enum {
  EDGE_BOUNDARY = (1 << 0),
  EDGE_VISITED = (1 << 1),
  FACE_NEW = (1 << 0),
  FACE_VISITED = (1 << 1),
};

/** Fill simple boundary cycles directly, avoiding the general edge-net path search. */
static bool bm_holes_fill_boundary_cycles(BMesh *bm, BMOperator *op, const uint sides)
{
  BMOIter iter;
  BMEdge *edge;
  bool use_edgenet = false;

  /* Classify before creating faces: filling a cycle changes its boundary status. */
  BMO_ITER (edge, &iter, op->slots_in, "edges", BM_EDGE) {
    if (BM_edge_is_boundary(edge)) {
      BMO_edge_flag_enable(bm, edge, EDGE_BOUNDARY);
    }
    else if (BM_edge_is_wire(edge)) {
      use_edgenet = true;
    }
  }

  Vector<BMEdge *> component;
  BMO_ITER (edge, &iter, op->slots_in, "edges", BM_EDGE) {
    if (!BMO_edge_flag_test(bm, edge, EDGE_BOUNDARY) || BMO_edge_flag_test(bm, edge, EDGE_VISITED))
    {
      continue;
    }

    component.clear();
    component.append(edge);
    BMO_edge_flag_enable(bm, edge, EDGE_VISITED);
    bool is_cycle = true;
    for (int i = 0; i < component.size(); i++) {
      BMEdge *current = component[i];
      for (BMVert *vert : {current->v1, current->v2}) {
        BMIter edge_iter;
        BMEdge *neighbor;
        int degree = 0;
        BM_ITER_ELEM (neighbor, &edge_iter, vert, BM_EDGES_OF_VERT) {
          if (!BMO_edge_flag_test(bm, neighbor, EDGE_BOUNDARY)) {
            continue;
          }
          degree++;
          if (!BMO_edge_flag_test(bm, neighbor, EDGE_VISITED)) {
            BMO_edge_flag_enable(bm, neighbor, EDGE_VISITED);
            component.append(neighbor);
          }
        }
        is_cycle &= degree == 2;
      }
    }

    if (!is_cycle || component.size() < 3) {
      /* Keep the existing behavior for partial selections and branched edge networks. */
      use_edgenet = true;
      continue;
    }

    /* Consume even cycles above the size limit, without creating and deleting a face. */
    for (BMEdge *boundary : component) {
      BM_elem_flag_disable(boundary, BM_ELEM_TAG);
    }
    if (sides != 0 && component.size() > sides) {
      continue;
    }

    const int faces_before = bm->totface;
    BMLoop *adjacent = edge->l;
    BMFace *face = BM_face_create_ngon(bm,
                                       adjacent->next->v,
                                       adjacent->v,
                                       component.data(),
                                       component.size(),
                                       nullptr,
                                       BM_CREATE_NO_DOUBLE);
    /* NO_DOUBLE may return an existing face; never include it in faces.out. */
    if (face && bm->totface != faces_before) {
      BM_elem_flag_enable(face, BM_ELEM_TAG);
    }
  }
  return use_edgenet;
}

/** Give each connected patch its own Sculpt Face Set without changing existing face IDs. */
static void bm_holes_fill_face_sets(BMesh *bm, BMOperator *op)
{
  if (BMO_slot_buffer_len(op->slots_out, "faces.out") == 0) {
    return;
  }
  int offset = CustomData_get_offset_named(&bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
  if (offset == -1) {
    BM_data_layer_add_named(bm, &bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
    offset = CustomData_get_offset_named(&bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
    BMIter iter;
    BMFace *face;
    BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
      BM_ELEM_CD_SET_INT(face, offset, 1);
    }
  }

  Set<int> used_ids;
  int maximum = 1;
  BMIter iter;
  BMFace *face;
  BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
    const int value = BM_ELEM_CD_GET_INT(face, offset);
    if (value != INT_MIN) {
      const int id = std::abs(value);
      used_ids.add(id);
      maximum = std::max(maximum, id);
    }
  }
  int next_id = maximum < INT_MAX ? maximum + 1 : 1;
  BMO_slot_buffer_flag_enable(bm, op->slots_out, "faces.out", BM_FACE, FACE_NEW);

  BMOIter output_iter;
  Vector<BMFace *> patch;
  BMO_ITER (face, &output_iter, op->slots_out, "faces.out", BM_FACE) {
    if (BMO_face_flag_test(bm, face, FACE_VISITED)) {
      continue;
    }
    while (used_ids.contains(next_id)) {
      next_id = next_id < INT_MAX ? next_id + 1 : 1;
    }
    used_ids.add(next_id);
    patch.clear();
    patch.append(face);
    BMO_face_flag_enable(bm, face, FACE_VISITED);
    for (int i = 0; i < patch.size(); i++) {
      BMFace *current = patch[i];
      BM_ELEM_CD_SET_INT(current, offset, next_id);
      BMLoop *loop = BM_FACE_FIRST_LOOP(current);
      BMLoop *first = loop;
      do {
        BMLoop *radial = loop->radial_next;
        while (radial != loop) {
          BMFace *neighbor = radial->f;
          if (BMO_face_flag_test(bm, neighbor, FACE_NEW) &&
              !BMO_face_flag_test(bm, neighbor, FACE_VISITED))
          {
            BMO_face_flag_enable(bm, neighbor, FACE_VISITED);
            patch.append(neighbor);
          }
          radial = radial->radial_next;
        }
      } while ((loop = loop->next) != first);
    }
  }
}

void bmo_holes_fill_exec(BMesh *bm, BMOperator *op)
{
  BMOperator op_attr;
  const uint sides = BMO_slot_int_get(op->slots_in, "sides");

  BM_mesh_elem_hflag_disable_all(bm, BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "edges", BM_EDGE, BM_ELEM_TAG, false);

  if (bm_holes_fill_boundary_cycles(bm, op, sides)) {
    BM_mesh_edgenet(bm, true, true);
  }

  /* bad - remove faces after as a workaround */
  if (sides != 0) {
    BMOIter siter;
    BMFace *f;

    BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "faces.out", BM_FACE, BM_ELEM_TAG);
    BMO_ITER (f, &siter, op->slots_out, "faces.out", BM_FACE) {
      if (f->len > sides) {
        BM_face_kill(bm, f);
      }
    }
  }

  BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "faces.out", BM_FACE, BM_ELEM_TAG);

  /* --- Attribute Fill --- */
  /* may as well since we have the faces already in a buffer */
  BMO_op_initf(bm,
               &op_attr,
               op->flag,
               "face_attribute_fill faces=%S use_normals=%b use_data=%b",
               op,
               "faces.out",
               true,
               true);

  BMO_op_exec(bm, &op_attr);

  /* check if some faces couldn't be touched */
  if (BMO_slot_buffer_len(op_attr.slots_out, "faces_fail.out")) {
    BMOIter siter;
    BMFace *f;

    BMO_ITER (f, &siter, op_attr.slots_out, "faces_fail.out", BM_FACE) {
      BM_face_normal_update(f); /* Normals are zeroed. */
    }

    BMO_op_callf(bm, op->flag, "recalc_face_normals faces=%S", &op_attr, "faces_fail.out");
  }
  BMO_op_finish(bm, &op_attr);

  if (BMO_slot_bool_get(op->slots_in, "use_new_face_sets")) {
    bm_holes_fill_face_sets(bm, op);
  }
}

}  // namespace blender
