# Houdini Niagara plug-in for Unreal

This plug-in will add a new Houdini CSV File Data Interface to Niagara.
The data interface is a simple one allowing import of HCSV files exported from Houdini in Niagara.

HCSV are simply renamed CSV file, with an append H ot avoid clashing with Data Tables import in UE4...

This version of the plugin is currently updated for UE4.24

To build it:
- Copy the plug-in files to your UE4 source directory.
- Build UE4

You will now have access to the Houdini Niagara plug-in (in the FX Category).
Once enabled, the plug-in will give you access to a CSV Data Interface (Houdini CSV File) in the Niagara Script Editor.
The Niagara plug-in must be enabled as well, as the Houdini Niagara plug-in depends on it too.

This will give you access to multiples nodes and functions that will let you access and parse data from the exported data.

For more information:

A simple documentation for the plugin can be found here:
https://www.sidefx.com/docs/unreal/_niagara.html

Additional infos and link to video tutorials here:
https://www.sidefx.com/forum/topic/56573/

Programmable VFX with Unreal Engine's Niagara | GDC 2018
https://youtu.be/mNPYdfRVPtM?t=2269


