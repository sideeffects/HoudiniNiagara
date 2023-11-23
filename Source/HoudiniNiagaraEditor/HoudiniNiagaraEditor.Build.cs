/*
* Copyright (c) <2018> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

using UnrealBuildTool;

public class HoudiniNiagaraEditor : ModuleRules
{
	public HoudiniNiagaraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		
		PrivateIncludePaths.AddRange(
			new string[] 
            {
				"HoudiniNiagaraEditor/Private",
            }
        );
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "Niagara",
                "NiagaraShader",
				"CoreUObject",
                "VectorVM",
                "RHI",
                "NiagaraVertexFactories",
                "RenderCore",
                "HoudiniNiagara",
                "UnrealEd",
				"EditorStyle",
			}
        );
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "Niagara",
                "NiagaraShader",
                "HoudiniNiagara",
                "Projects",
                "ToolMenus",
			}
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] 
            {
                "Engine",
                "LevelEditor",
                "AssetTools",
                "ContentBrowser",
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
		    new string[]
		    {
			    // ... add any modules that your module loads dynamically here ...
		    }
        );
	}
}
