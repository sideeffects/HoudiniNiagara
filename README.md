# Houdini Niagara plug-in for Unreal

This plug-in adds a new "Houdini Data Interface" to Niagara.
This version of the plugin is currently updated for UE4.25.

The data interface allows importing and processing Houdini Point Cache assets in Niagara.
The point cache files can be exported using the Niagara ROP, available via the SideFXLabs tools.
https://github.com/sideeffects/SideFXLabs

Supported file types for the point caches are:
- *.hjson: Houdini JSON point cache (ascii)
- *.hbjson: Houdini JSON point cache (binary)
- *.hcsv: Houdini CSV point cache (legacy CSV, used by previous version of this plugin)

To build it:
- Copy the plug-in files to your UE4 source directory. (in Engine/Plugins/FX)
- Build UE4

Alternatively, you can also download the prebuilt binaries in the "releases" section of this repo.

You will now have access to the Houdini Niagara plug-in (under the FX Category).

Once enabled, the plug-in will give you access to a new Houdini Point Cache Data Interface in the Niagara Script Editor. The Data Interface will allow you to use multiples nodes and functions to access and parse data from the exported point cache.

The Niagara plug-in must be enabled as well, as the Houdini Niagara plug-in depends on it too.


For more information:

A simple documentation for the plugin can be found here:
https://www.sidefx.com/docs/unreal/_niagara.html

Additional infos and link to video tutorials here:
https://www.sidefx.com/forum/topic/56573/

Programmable VFX with Unreal Engine's Niagara | GDC 2018
https://youtu.be/mNPYdfRVPtM?t=2269


