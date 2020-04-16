# Houdini Niagara plug-in for Unreal

This plug-in will add a new Houdini Data Interface to Niagara.

The data interface allows importing Houdini Point Cache asset exported from Houdini in Niagara.
Support file types for the point cache are:
- *.hjson: Houdini JSON point cache (ascii)
- *.hbjson: Houdini JSON point cache (binary)
- *.hcsv: Houdini CSV point cache (legacy CSV, used by previous version of this plugin)

This version of the plugin is currently updated for UE4.25.

To build it:
- Copy the plug-in files to your UE4 source directory.
- Build UE4

You will now have access to the Houdini Niagara plug-in (in the FX Category).
Once enabled, the plug-in will give you access to a new Houdini Point Cache Data Interface in the Niagara Script Editor.
The Niagara plug-in must be enabled as well, as the Houdini Niagara plug-in depends on it too.

This will give you access to multiples nodes and functions that will let you access and parse data from the exported data.

For more information:

A simple documentation for the plugin can be found here:
https://www.sidefx.com/docs/unreal/_niagara.html

Additional infos and link to video tutorials here:
https://www.sidefx.com/forum/topic/56573/

Programmable VFX with Unreal Engine's Niagara | GDC 2018
https://youtu.be/mNPYdfRVPtM?t=2269


