# Load some animation data inside of load_model_from_gltf_into_memory

- STATUS: CLOSED
- PRIORITY: 99
- TAGS: animation

Note, not handling a case where the mesh is animated, not given any bone assignements, and is the child of a bone
node. In that case, the verts should probably be attached to the parent bone so it will animate with the bone.
