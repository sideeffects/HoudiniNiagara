# HoudiniNiagara

Houdini Niagara plug-in for UE4 Niagara

This plug-in will add a new Houdini CSV File Data Interface to Niagara.
The data interface is a simple one allowing import of CSV files exported from Houdini in Niagara.

Updated for UE4.20.

To build it:
- Copy the plug-in files to your UE4 source directory.
- Build UE4

You will now have access to the Houdini Niagara plug-in (in the FX Category).
Once enabled, the plug-in will give you access to a CSV Data Interface (Houdini CSV File) in the Niagara Script Editor.

This will give you access to 5 nodes:

- GetCSVFloat
Returns a float value at a given row/col index in a CSV File.

- GetNumberOfPointsInCSV
Returns the number of points found in the CSV file (which should be the number of lines minus one)

- GetCSVPosition
Returns the Position vector found in the CSV file at a given index N.
The position vector is properly converted to Unreal coordinate system.
Title in the CSV file for the position column should be "P", "Px,Py,Pz" or "X,Y,Z" 

- GetCSVNormal
Returns the Normal vector found in the CSV file at a given index N.
The normal vector is properly converted to Unreal coordinate system.
Title in the CSV file for the normal column should be "N" or "Nx,Ny,Nz".

- GetCSVTime
Return the float value for the time attribute found in the CSV file at a given index N.
Title in the CSV file for the time column should be "T" or containing "time".
