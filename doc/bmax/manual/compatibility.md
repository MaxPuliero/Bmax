# Saving and compatibility

[Artist guide](README.md)

## Keep an editable Bmax project

Save your working project normally in Bmax to retain its custom settings, painted Multires detail and Sculpt Layers.

Keep this editable original when preparing a version for another application or for someone using official Blender.

## Choose which modified images to save

With Preferences > Save & Load > Save Modified Images set to Ask, the project-save
confirmation has a **Modified Images** expander. Each entry shows its image name and
file destination, or **Packed in blend file** for images stored inside the project.
The top checkbox selects or deselects all saveable entries; individual checkboxes
let you choose a subset. The counter shows selected / saveable images, and a dash
in the top checkbox indicates a partial selection. The list shows four to six rows
initially and scrolls with the mouse wheel or scrollbar;
Save and Cancel stay outside the scrolling area. Selections persist while scrolling
and while collapsed. Each UDIM set is a single entry with its tile count; its checkbox
controls the whole image, using the native UDIM save/pack path.

**Save** saves the project and the selected images using Blender's native save/pack
rules. Unselected external files remain untouched and their images stay modified
in memory. **Cancel** (or Esc) cancels this project-save request and writes no files.
Unsavable entries are disabled, with the native warnings and a Save As hint.
Generated images are packed using the existing Blender behavior.

This selection applies to the project-save confirmation. The separate close-file
prompt, Image > Save All Modified, and the Always/Never preference retain their
existing behavior. Save As still opens its normal file browser after confirmation.

<img src="../media/modified_images_save_prompt.png" alt="Modified Images save prompt with three packed textures, individual selections and Save and Cancel buttons" width="659">

## Save a copy for official Blender

Choose **File > Save as Official…**. The command creates a separate file while leaving your open project, filename and editable data in place.

By default, it preserves Bmax normal weighting as standard custom normals and merges the visible Sculpt Layers into standard Multires sculpt detail while retaining subdivision levels.

<img src="../media/save_as_official_menu.png" alt="File menu with Save as Official and its compatible-copy tooltip" width="743">

<img src="../media/save_as_official_options.png" alt="Save as Official options for normals, Sculpt Layers, a separate Multires color copy and resource packing" width="349">

## Include a color backup

Enable **Separate Multires Color Copy** to create a separate copy with the painted color on finished meshes. Choose Highest Available, Render or Viewport level.

This backup contains ordinary color attributes, rather than editable Multires paint. Existing color backups receive a numbered filename instead of being overwritten. You can also pack supported external resources.

## Check before handing off

- **Instances Random** can produce different colors in official Blender. Save as Official warns about this and does not automatically rewrite materials.
- Turning off conversion options can leave Bmax-specific results unavailable in the copy.
- Add-ons and external caches may still be required.
- Modified tiled, animated or multiview images must be saved before conversion.
- Opening and resaving a Bmax project directly in official Blender can discard Bmax-only settings.

For a finished painted asset, another option is to apply Multires in Bmax to turn the result into normal mesh color data. Preserve a separate editable original first.
