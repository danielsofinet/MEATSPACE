# Batch-imports the greatsword animation FBXs.
#
# Run from the editor: Tools -> Execute Python Script, or paste into the Python console
# (Window -> Developer Tools -> Output Log, then switch the command dropdown to "Python").
# Needs the "Python Editor Script Plugin" enabled.
#
# Importing 40 animations by hand is 40 dialogs and 40 chances to pick the wrong skeleton.
# This does it once, consistently.
#
# SET SKELETON_PATH BELOW before running. Which skeleton depends on where the animations
# came from:
#   - already authored for the UE5 mannequin -> point at SK_Mannequin and they are ready
#   - Mixamo or any other rig -> import against THEIR skeleton first, then retarget the whole
#     folder onto SK_Mannequin with an IK Retargeter. Forcing a foreign rig onto SK_Mannequin
#     at import does not convert it; it just produces a mess.

import os
import unreal

SOURCE_DIR = r"C:\Users\D\Documents\Unreal Projects\MEATSPACE\3D Assets\Characters\Animation source downloads\Sword animations"
DEST_PATH = "/Game/Characters/Player/Animations/Sword"

# Leave as "" on the FIRST run for a foreign rig: the first file imported will create its own
# skeleton, and every later file is matched to it so they all share one.
SKELETON_PATH = ""


def make_task(fbx_path, skeleton):
    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = DEST_PATH
    task.automated = True          # no dialog per file
    task.replace_existing = True
    task.save = True

    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION

    if skeleton:
        options.skeleton = skeleton

    task.options = options
    return task


def run():
    if not os.path.isdir(SOURCE_DIR):
        unreal.log_error("Source folder not found: %s" % SOURCE_DIR)
        return

    files = sorted(
        os.path.join(SOURCE_DIR, f)
        for f in os.listdir(SOURCE_DIR)
        if f.lower().endswith(".fbx")
    )

    if not files:
        unreal.log_error("No FBX files in %s" % SOURCE_DIR)
        return

    skeleton = None
    if SKELETON_PATH:
        skeleton = unreal.load_asset(SKELETON_PATH)
        if not skeleton:
            unreal.log_error("Could not load skeleton: %s" % SKELETON_PATH)
            return

    tools = unreal.AssetToolsHelpers.get_asset_tools()

    # Imported one at a time rather than as a batch: if a file fails, the rest still land and
    # the log names the one that broke.
    imported = 0
    for fbx in files:
        try:
            tools.import_asset_tasks([make_task(fbx, skeleton)])
            imported += 1
            unreal.log("imported: %s" % os.path.basename(fbx))
        except Exception as error:
            unreal.log_error("FAILED %s -- %s" % (os.path.basename(fbx), error))

    unreal.log("=== done: %d / %d imported into %s ===" % (imported, len(files), DEST_PATH))


run()
