/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "Misc/Guid.h"

// Deprecated per-class versions used to load old files
//
// Serialization of parameter name map.
#define VER_HOUDINI_ENGINE_COMPONENT_PARAMETER_NAME_MAP 2
// Serialization of instancer material, if it is available.
#define VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_MATERIAL_NAME 1
// Serialization of attribute instancer material, if it is available.
#define VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_ATTRIBUTE_MATERIAL_NAME 2
// Landscape serialization in asset inputs.
#define VER_HOUDINI_ENGINE_PARAM_LANDSCAPE_INPUT 1
// Asset instance member.
#define VER_HOUDINI_ENGINE_PARAM_ASSET_INSTANCE_MEMBER 2
// World Outliner inputs.
#define VER_HOUDINI_ENGINE_PARAM_WORLD_OUTLINER_INPUT 3


enum EHoudiniNiagaraSerializationVersion
{
    // before any serialization version changes were made in the plugin
    BeforeCustomVersionAdded = 0,

    // Added support for files >2GB
    PointCache_LargeFileSupport = 1,

    // -----<new versions can be added before this line>-------------------------------------------------
    
    // - this needs to be the last line (see note below)
    VersionPlusOne,
    LatestVersion = VersionPlusOne - 1,
};

struct FHoudiniNiagaraCustomSerializationVersion
{
    // The GUID for this custom version number
    const static FGuid GUID;

private:
    FHoudiniNiagaraCustomSerializationVersion() {}
};
