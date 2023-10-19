/*
    misc.cpp -- Bindings for miscellaneous implementation details

    Dr.Jit: A Just-In-Time-Compiler for Differentiable Rendering
    Copyright 2023, Realistic Graphics Lab, EPFL.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "misc.h"
#include "apply.h"

/**
 * \brief Return Dr.Jit variable indices associated with the provided data structure.
 *
 * This function traverses Dr.Jit arrays, tensors, :ref:`Pytree <pytrees>` (lists,
 * tuples, dicts, custom data structures) and returns the indices of all detected
 * variables (in the order of traversal, may contain duplicates). The index
 * information is returned as a list of encoded 64 bit integers, where each
 * contains the AD variable index in the upper 32 bits and the JIT variable
 * index in the lower 32 bit.
 *
 * Intended purely for internal Dr.Jit use, you probably should not call this in
 * your own application.
 *
 * (Note: this explanation is also part of src/python/docstr.h -- please keep
 * them in sync in case you make a change here)
*/
void collect_indices(nb::handle h, dr::dr_vector<uint64_t> &indices) {
    struct CollectIndices : TraverseCallback {
        dr::dr_vector<uint64_t> &result;
        CollectIndices(dr::dr_vector<uint64_t> &result) : result(result) { }

        void operator()(nb::handle h) const override {
            auto index = supp(h.type()).index;
            if (index)
                result.push_back(index(inst_ptr(h)));
        }
    };

    if (!h.is_valid())
        return;

    traverse("drjit.detail.collect_indices", CollectIndices { indices }, h);
}

dr::dr_vector<uint64_t> collect_indices(nb::handle h) {
    dr::dr_vector<uint64_t> result;
    collect_indices(h, result);
    return result;
}

/**
 * \brief Create a copy of the provided input while replacing Dr.Jit variables with
 * new ones based on a provided set of indices.
 *
 * This function works analogously to ``collect_indices``, except that it
 * consumes an index array and produces an updated output.
 *
 * It recursively traverses and copies an input object that may be a Dr.Jit array,
 * tensor, or :ref:`Pytree <pytrees>` (list, tuple, dict, custom data structure)
 * while replacing any detected Dr.Jit variables with new ones based on the
 * provided index vector. The function returns the resulting object, while leaving
 * the input unchanged. The output array borrows the referenced array indices
 * as opposed to stealing them.
 *
 * Intended purely for internal Dr.Jit use, you probably should not call this in
 * your own application.
 *
 * (Note: this explanation is also part of src/python/docstr.h -- please keep
 * them in sync in case you make a change here)
 */

nb::object update_indices(nb::handle h, const dr::dr_vector<uint64_t> &indices_) {
    struct UpdateIndices : TransformCallback {
        const dr::dr_vector<uint64_t> &indices;
        mutable size_t counter = 0;
        UpdateIndices(const dr::dr_vector<uint64_t> &indices) : indices(indices) { }

        void operator()(nb::handle h1, nb::handle h2) const override {
            const ArraySupplement &s = supp(h1.type());
            if (s.index) {
                if (counter >= indices.size())
                    nb::raise("too few (%zu) indices provided", indices.size());

                s.init_index(indices[counter++], inst_ptr(h2));
            }
        }
    };

    if (!h.is_valid())
        return nb::object();

    UpdateIndices ui { indices_ };
    nb::object result = transform("drjit.detail.update_indices", ui, h);

    if (ui.counter != indices_.size())
        nb::raise("drjit.detail.update_indices(): too many indices "
                  "provided (needed %zu, got %zu)!",
                  ui.counter, indices_.size());

    return result;
}

/**
 * \brief Traverse two pytrees in parallel and ensure that they have an
 * identical structure.
 *
 * Raises an exception is a mismatch is found (e.g., different types, arrays with
 * incompatible numbers of elements, dictionaries with different keys, etc.)
 *
 * (Note: this explanation is also part of src/python/docstr.h -- please keep
 * them in sync in case you make a change here)
 */
void check_compatibility(nb::handle h1, nb::handle h2) {
    struct CheckCompatibility : TraversePairCallback {
        void operator()(nb::handle, nb::handle) const override { }
    };

    traverse_pair("drjit.detail.check_compatibility",
                  CheckCompatibility{ }, h1, h2);
}

void export_misc(nb::module_ &) {
    nb::module_ detail = nb::module_::import_("drjit.detail");
    detail.def("collect_indices",
               nb::overload_cast<nb::handle>(&collect_indices),
               doc_collect_indices);
    detail.def("update_indices", &update_indices, doc_update_indices);
    detail.def("check_compatibility", &check_compatibility,
               doc_check_compatibility);
}
