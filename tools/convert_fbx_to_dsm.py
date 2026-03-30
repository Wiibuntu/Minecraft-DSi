#!/usr/bin/env python3
from __future__ import annotations
import argparse, shutil, subprocess, sys, tempfile
from pathlib import Path

BLENDER_SCRIPT = r'''
import bpy, struct, sys, math
from pathlib import Path
from mathutils import Matrix

argv = sys.argv
sep = argv.index('--')
fbx_path = Path(argv[sep + 1])
out_path = Path(argv[sep + 2])
tex_w = int(argv[sep + 3])
tex_h = int(argv[sep + 4])
target_height = float(argv[sep + 5])


def pick_best_mesh(objs):
    best = None
    best_score = -1
    for obj in objs:
        if obj.type != 'MESH':
            continue
        mesh = obj.data
        score = len(mesh.polygons)
        if score > best_score:
            best = obj
            best_score = score
    return best


def quant_norm(v):
    return (
        int(max(-127, min(127, round(v[0] * 127.0)))),
        int(max(-127, min(127, round(v[1] * 127.0)))),
        int(max(-127, min(127, round(v[2] * 127.0)))),
    )


def normalize_weights(vgroups, vert, bone_index):
    groups = []
    for g in vert.groups:
        name = vgroups[g.group].name
        groups.append((g.weight, bone_index.get(name, 0)))
    groups.sort(reverse=True)
    groups = groups[:4]
    if not groups:
        return [0, 0, 0, 0], [255, 0, 0, 0]
    total = sum(w for w, _ in groups)
    if total <= 1e-8:
        return [0, 0, 0, 0], [255, 0, 0, 0]
    bi = [0, 0, 0, 0]
    bw = [0, 0, 0, 0]
    used = 0
    for i, (w, b) in enumerate(groups):
        bi[i] = b
        q = int(round((w / total) * 255.0))
        bw[i] = max(0, min(255, q))
        used += bw[i]
    if used == 0:
        bw[0] = 255
    else:
        bw[0] = max(0, min(255, bw[0] + (255 - used)))
    return bi, bw


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(fbx_path), automatic_bone_orientation=True)

scene_objs = list(bpy.context.scene.objects)
mesh_obj = pick_best_mesh(scene_objs)
arm_obj = next((o for o in scene_objs if o.type == 'ARMATURE'), None)
if mesh_obj is None:
    raise RuntimeError('No mesh found in FBX')

# Evaluate with modifiers applied so the exported mesh matches what Blender shows.
deps = bpy.context.evaluated_depsgraph_get()
mesh_eval_obj = mesh_obj.evaluated_get(deps)
mesh = mesh_eval_obj.to_mesh(preserve_all_data_layers=True, depsgraph=deps)
mesh.calc_loop_triangles()
mesh.calc_normals_split()

# Work in the evaluated mesh's local space, then normalize into a DSi-friendly size.
coords = [mesh.vertices[i].co.copy() for i in range(len(mesh.vertices))]
if not coords:
    raise RuntimeError('Mesh has no vertices')
min_x = min(v.x for v in coords)
max_x = max(v.x for v in coords)
min_y = min(v.y for v in coords)
max_y = max(v.y for v in coords)
min_z = min(v.z for v in coords)
max_z = max(v.z for v in coords)
size_x = max_x - min_x
size_y = max_y - min_y
size_z = max_z - min_z
height = max(size_y, 1e-6)
scale = target_height / height
center_x = (min_x + max_x) * 0.5
center_z = (min_z + max_z) * 0.5

bones = arm_obj.data.bones if arm_obj else []
bone_index = {b.name: i for i, b in enumerate(bones)}
uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None

verts = []
indices = []
vert_map = {}

for tri in mesh.loop_triangles:
    for loop_index in tri.loops:
        loop = mesh.loops[loop_index]
        vert = mesh.vertices[loop.vertex_index]
        co = vert.co.copy()
        co.x = (co.x - center_x) * scale
        co.y = (co.y - min_y) * scale
        co.z = (co.z - center_z) * scale

        no = loop.normal.normalized()
        qnx, qny, qnz = quant_norm((no.x, no.y, no.z))

        u = 0.0
        vv = 0.0
        if uv_layer:
            uv = uv_layer[loop_index].uv
            u = max(0.0, min(1.0, uv.x))
            vv = max(0.0, min(1.0, 1.0 - uv.y))

        bi, bw = normalize_weights(mesh_obj.vertex_groups, vert, bone_index)

        key = (
            int(round(co.x * 256.0)),
            int(round(co.y * 256.0)),
            int(round(co.z * 256.0)),
            int(round(u * 65535.0)),
            int(round(vv * 65535.0)),
            qnx, qny, qnz,
            tuple(bi),
            tuple(bw),
        )
        idx = vert_map.get(key)
        if idx is None:
            idx = len(verts)
            vert_map[key] = idx
            verts.append(key)
        indices.append(idx)

clips = []
if arm_obj and bpy.data.actions:
    for action in bpy.data.actions:
        clips.append((action.name, int(action.frame_range[0]), int(action.frame_range[1]), bpy.context.scene.render.fps))

with open(out_path, 'wb') as f:
    f.write(b'DSM1')
    f.write(struct.pack('<IIIIIIHH', 1, len(verts), len(indices), len(bones), len(clips), 0, tex_w, tex_h))
    for x, y, z, u, v, nx, ny, nz, bi, bw in verts:
        f.write(struct.pack('<hhhHHbbbB4B4B', x, y, z, u, v, nx, ny, nz, 0, *bi, *bw))
    for idx in indices:
        f.write(struct.pack('<H', idx))
    for b in bones:
        parent = bone_index[b.parent.name] if b.parent else -1
        f.write(struct.pack('<h', parent))
        ident = [1, 0, 0, 0,
                 0, 1, 0, 0,
                 0, 0, 1, 0]
        for v in ident:
            f.write(struct.pack('<i', int(v * 4096)))
    for name, start, end, fps in clips:
        enc = name.encode('utf-8')[:31]
        f.write(enc + b'\0' * (32 - len(enc)))
        frame_count = max(0, end - start + 1)
        f.write(struct.pack('<HH', frame_count, fps))
        for frame in range(start, end + 1):
            bpy.context.scene.frame_set(frame)
            for b in bones:
                pose_b = arm_obj.pose.bones.get(b.name)
                if pose_b:
                    mat = pose_b.matrix
                    loc = mat.to_translation()
                    rot = mat.to_quaternion()
                else:
                    loc = (0.0, 0.0, 0.0)
                    rot = (1.0, 0.0, 0.0, 0.0)
                f.write(struct.pack('<hhh', int(round(loc.x * 256)), int(round(loc.y * 256)), int(round(loc.z * 256))))
                f.write(struct.pack('<hhhh', int(round(rot.x * 32767)), int(round(rot.y * 32767)), int(round(rot.z * 32767)), int(round(rot.w * 32767))))
'''


def main() -> int:
    ap = argparse.ArgumentParser(description='Convert FBX to DSi-friendly DSM format using Blender')
    ap.add_argument('fbx')
    ap.add_argument('output')
    ap.add_argument('--texture-width', type=int, default=64)
    ap.add_argument('--texture-height', type=int, default=32)
    ap.add_argument('--target-height', type=float, default=1.0,
                    help='Normalize exported model height to this many blocks/world units (default: 1.0)')
    ap.add_argument('--blender', default=shutil.which('blender') or 'blender')
    args = ap.parse_args()

    if shutil.which(args.blender) is None and args.blender == 'blender':
        print('Blender is required for FBX import. Install Blender or pass --blender /path/to/blender.', file=sys.stderr)
        return 2

    with tempfile.NamedTemporaryFile('w', suffix='.py', delete=False) as tf:
        tf.write(BLENDER_SCRIPT)
        script_path = tf.name
    cmd = [
        args.blender, '--background', '--python', script_path, '--',
        args.fbx, args.output, str(args.texture_width), str(args.texture_height), str(args.target_height)
    ]
    print('Running:', ' '.join(cmd))
    return subprocess.call(cmd)


if __name__ == '__main__':
    raise SystemExit(main())
