# Saving and compatibility

[Artist guide](README.md)

## Keep an editable Bmax project

Save your working project normally in Bmax to retain its custom settings, painted Multires detail and Sculpt Layers.

Keep this editable original when preparing a version for another application or for someone using official Blender.

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
