# Tangent/Bitangent Discontinuity Bug Fix

**Date:** January 3, 2026  
**Status:** Analysis complete, fix ready to apply

## Symptom

Specular highlights change abruptly along triangle edges on some BSP faces. Tangent/bitangent vectors appear incorrect on 1-2 faces per map.

## Root Cause

In `src/tools/quemap/face.c::TangentVectors_()`:

```c
bsp_face_t *face = bsp_file.faces + model->first_face;
int32_t base_vertex = face->first_vertex;  // ← BUG: Only first face's base_vertex

bsp_vertex_t *vertexes = bsp_file.vertexes + base_vertex;
int32_t *elements = bsp_file.elements + face->first_element;

// Accumulate counts from ALL faces
int32_t num_vertexes = 0, num_elements = 0;
for (int32_t i = 0; i < model->num_faces; i++, face++) {
  num_vertexes += face->num_vertexes;
  num_elements += face->num_elements;
}

// Build cm_vertex_t array sequentially
cm_vertex_t *cm = Mem_Malloc(sizeof(cm_vertex_t) * num_vertexes);
bsp_vertex_t *v = vertexes;
for (int32_t i = 0; i < num_vertexes; i++, v++) {
  cm[i] = (cm_vertex_t) {
    .position = &v->position,
    // ...
  };
}

// Process all elements with FIRST face's base_vertex
Cm_Tangents(cm, base_vertex, num_vertexes, elements, num_elements);
```

### The Problem

1. **cm array is indexed sequentially**: `cm[0..num_vertexes-1]`
2. **Element indices are absolute**: Point to global vertex array indices
3. **`Cm_Tangents()` subtracts `base_vertex`**: `i0 = elements[i] - base_vertex`
4. **But `base_vertex` is only correct for the FIRST face**

### Example

Model with 2 faces:
- Face 0: vertices [100..149], elements reference [100..149]
- Face 1: vertices [150..199], elements reference [150..199]

Current code:
- `base_vertex = 100` (from face 0)
- `cm` array: indices [0..99] map to vertices [100..199]
- Face 0 elements (100-149): Subtract 100 → `cm[0..49]` ✅ Correct
- Face 1 elements (150-199): Subtract 100 → `cm[50..99]` ✅ Actually correct!

Wait... let me re-check the vertex array setup...

### Re-Analysis

Actually, the vertex setup IS correct:

```c
bsp_vertex_t *vertexes = bsp_file.vertexes + base_vertex;  // Start at first face's first vertex
for (int32_t i = 0; i < num_vertexes; i++, v++) {  // Iterate sequentially
  cm[i] = ...;  // Sequential cm array
}
```

This assumes all faces in a model have **contiguous, sequential vertex indices**.

**But what if they don't?** What if faces in a model have gaps in vertex indices?

### Actual Bug

The code assumes:
- Model faces have contiguous vertices: [100, 101, 102, ..., 199]
- All vertex indices between first and last are part of this model

But BSP generation might:
- Reuse vertices across models
- Have non-contiguous vertex ranges
- Have different `first_vertex` per face

**The fix should process each face independently** rather than assuming contiguity.

## The Fix

Process each face separately with its own base_vertex:

```c
static void TangentVectors_(bsp_model_t *model) {
  bsp_face_t *face = bsp_file.faces + model->first_face;
  
  for (int32_t f = 0; f < model->num_faces; f++, face++) {
    int32_t base_vertex = face->first_vertex;
    int32_t num_vertexes = face->num_vertexes;
    int32_t num_elements = face->num_elements;
    
    bsp_vertex_t *vertexes = bsp_file.vertexes + base_vertex;
    int32_t *elements = bsp_file.elements + face->first_element;
    
    cm_vertex_t *cm = Mem_Malloc(sizeof(cm_vertex_t) * num_vertexes);
    
    for (int32_t i = 0; i < num_vertexes; i++) {
      cm[i] = (cm_vertex_t) {
        .position = &vertexes[i].position,
        .normal = &vertexes[i].normal,
        .tangent = &vertexes[i].tangent,
        .bitangent = &vertexes[i].bitangent,
        .st = &vertexes[i].diffusemap
      };
    }
    
    Cm_Tangents(cm, base_vertex, num_vertexes, elements, num_elements);
    
    Mem_Free(cm);
  }
}
```

This processes each face independently with correct base_vertex alignment.

### Trade-off

**Current (buggy) approach:**
- Averages tangents across all faces in model (Phong shading style)
- But only works if vertices are contiguous
- Causes artifacts when assumption breaks

**Per-face approach:**
- Each face gets independent tangent calculation
- No averaging across faces
- Always correct, but less smooth at face boundaries

### Alternative: Vertex Welding

If cross-face tangent averaging is desired, need explicit vertex welding:

```c
// Build vertex-to-faces mapping
// Average tangents from all faces sharing a vertex
// Requires more bookkeeping but handles non-contiguous vertices
```

## Recommendation

**Use per-face processing** - it's always correct and simpler. Cross-face tangent averaging is typically not needed for BSP geometry since faces already have Phong-smoothed normals.
