/*
 * Copyright (c) 2017
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NAME:	ROP library (C++)
 *
 * COMMENTS:	FBX output
 *
 */

#ifndef __ROP_FBXCommon_h__
#define __ROP_FBXCommon_h__

#include <string>
#include <vector>

#include <SYS/SYS_Types.h>
#include <UT/UT_StringHolder.h>


typedef std::vector  < std::string > TStringVector;
/********************************************************************************************************/
static const int ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT = 4;

static inline bool
ROPfbxIsLightNodeType(const UT_StringRef &node_type)
{
    return (node_type == "hlight" || node_type == "hlight::2.0");
}

// Determines which network types the visitor dives into
static const char* const ROP_FBXnetworkTypesToIgnore[] = { "geo", "bone", "null", "cam", "instance", "hlight", 
	"hlight::2.0", "ambient", "dopnet", "ropnet", "chopnet",  "popnet",  "vopnet",  "shopnet", 0 };

// These declare any node that does not modify the mesh, its vertices or points.
static const char* const ROP_FBXallowed_inbetween_node_types[] = {"null", "switch", "subnet", "attribcomposite",
"attribcopy", "attribcreate", "attribmirror", "attribpromote", "attribreorient", 
"attribpromote", "attribstringedit", "attribute", "cache", "stash", nullptr};

/********************************************************************************************************/
enum ROP_FBXVertexCacheExportFormatType
{
    ROP_FBXVertexCacheExportFormatMaya = 0,
    ROP_FBXVertexCacheExportFormat3DStudio
};
enum ROP_FBXVertexCacheMethodType
{
    ROP_FBXVertexCacheMethodNone = 0,
    ROP_FBXVertexCacheMethodGeometry,		// Any kind of geometry, including variable num points objects.
    ROP_FBXVertexCacheMethodGeometryConstant,	// Geometry with constant number of points (i.g. skinning, RBD, cloth).
    ROP_FBXVertexCacheMethodParticles		// Pure particle systems with no instance geometry
};

enum ROP_FBXInvisibleNodeExportType
{
    ROP_FBXInvisibleNodeExportAsNulls = 0,
    ROP_FBXInvisibleNodeExportFull,
    ROP_FBXInvisibleNodeExportAsVisible,
    ROP_FBXInvisibleNodeDontExport
};
/********************************************************************************************************/
class ROP_FBXExportOptions
{
public:
    ROP_FBXExportOptions();
    ~ROP_FBXExportOptions();
    void reset(void);

    /// If true, all animation curves will be resampled on export. If false,
    /// only the unsupported types will be.
    bool getResampleAllAnimation(void);
    /// If true, all animation curves will be resampled on export. If false,
    /// only the unsupported types will be.
    void setResampleAllAnimation(bool value);

    /// Resampling frequency, in frames. A linear key frame will be exported
    /// every N frames.
    fpreal getResampleIntervalInFrames(void);
    /// Resampling frequency, in frames. A linear key frame will be exported
    /// every N frames.
    void setResampleIntervalInFrames(fpreal frames);

    /// Specified the format to use for exporting vertex caches, whether compatbile 
    /// with Maya's or 3DS MAX.
    void setVertexCacheFormat(ROP_FBXVertexCacheExportFormatType format_type);
    /// Specified the format to use for exporting vertex caches, whether compatbile 
    /// with Maya's or 3DS MAX.
    ROP_FBXVertexCacheExportFormatType getVertexCacheFormat(void);

    /// If true, the exported file will be in the human-readable ASCII FBX format.
    /// Otherwise, it will be in binary.
    bool getExportInAscii(void);
    /// If true, the exported file will be in the human-readable ASCII FBX format.
    /// Otherwise, it will be in binary.
    void setExportInAscii(bool value);

    /// The first network the to start exporting from. Everything (recursively) in this
    /// network will be exported to the FBX file. Defaults to "/obj".
    void setStartNodePath(const char* node_path, bool autohandle_bundles = true);
    /// The first network the to start exporting from. Everything (recursively) in this
    /// network will be exported to the FBX file. Defaults to "/obj".
    const char* getStartNodePath(void);

    /// Create an additional root when exporting subnets. Note that this "/"
    /// and "/obj" start nodes will never be exported.
    /// @{
    void setCreateSubnetRoot(bool f) { myCreateSubnetRoot = f; }
    bool getCreateSubnetRoot() const { return myCreateSubnetRoot; }
    /// @}

    /// If true, the code will attempt to find those vertex cacheable objects which
    /// have a constant point count throughout the exported animation, and export them
    /// as "normal" vertex caches, without breaking them up and triangulating them.
    /// NOTE: This can fail when an object will happen to have a constant vertex count
    /// while changing point connectivity. This is why this is a UI option
    bool getDetectConstantPointCountObjects(void);
    /// If true, the code will attempt to find those vertex cacheable objects which
    /// have a constant point count throughout the exported animation, and export them
    /// as "normal" vertex caches, without breaking them up and triangulating them.
    /// NOTE: This can fail when an object will happen to have a constant vertex count
    /// while changing point connectivity. This is why this is a UI option
    void setDetectConstantPointCountObjects(bool value);

    /// Level of detail to use when converting various primitives to polygons.
    void setPolyConvertLOD(float lod);
    /// Level of detail to use when converting various primitives to polygons.
    float getPolyConvertLOD(void);

    /// If true, geometry with Deform SOPs will be exported as vertex caches. False by default.
    bool getExportDeformsAsVC(void);
    /// If true, geometry with Deform SOPs will be exported as vertex caches. False by default.
    void setExportDeformsAsVC(bool value);

    /// The name of the take to export. If empty, export the current take (default).
    void setExportTakeName(const char* pcsName);
    /// The name of the take to export. If empty, export the current take (default).
    const char* getExportTakeName(void);

    /// Determines how invisible objects are to be exported.
    ROP_FBXInvisibleNodeExportType getInvisibleNodeExportMethod(void);
    /// Determines how invisible objects are to be exported.
    void setInvisibleNodeExportMethod(ROP_FBXInvisibleNodeExportType exp_type);

    /// If true, NURBS and Bezier surfaces will be converted to polygons on export.
    void setConvertSurfaces(bool value);
    /// If true, NURBS and Bezier surfaces will be converted to polygons on export.
    bool getConvertSurfaces(void);

    /// Specifies which version of the SDK to use for export. Defaults to an empty string,
    ///	meaning the most current version.
    void setVersion(const char* sdk_version);
    /// Specifies which version of the SDK to use for export. Defaults to an empty string,
    ///	meaning the most current version.
    const char* getVersion(void);

    /// Optionally contains the names of the bundles we're exporting. Empty by default.
    /// If not empty, only the bundles specified will be exported.
    void setBundlesString(const char* bundles);
    /// Optionally contains the names of the bundles we're exporting. Empty by default.
    /// If not empty, only the bundles specified will be exported.
    const char* getBundlesString(void);
    /// Returns true if we are restricted to exporting certain bundles, false if
    /// everything is to be exported.
    bool isExportingBundles(void);

    /// If true, vertex cache frame snapshots will not be stored in memory, resulting in
    /// less memory usage, but slower performance.
    void setSaveMemory(bool value);
    /// If true, vertex cache frame snapshots will not be stored in memory, resulting in
    /// less memory usage, but slower performance.
    bool getSaveMemory(void);

    /// If true, blendshape nodes found in geometry nodes will always be exported, potentially loosing
    /// informations doing so, as nodes modifying geometry after the blend shapes will be ignored.
    void setForceBlendShapeExport(bool value);
    /// If true, blendshape nodes found in geometry nodes will always be exported, potentially loosing
    /// informations doing so, as nodes modifying geometry after the blend shapes will be ignored.
    bool getForceBlendShapeExport(void);

    /// If true, deform nodes found in geometry nodes will always be exported, potentially loosing
    /// informations doing so, as nodes modifying geometry after the deform will be ignored.
    void setForceSkinDeformExport(bool value);
    /// If true, deform nodes found in geometry nodes will always be exported, potentially loosing
    /// informations doing so, as nodes modifying geometry after the deform will be ignored.
    bool getForceSkinDeformExport(void);

    /// If true, Indicates that the export is being called from a SOP network
    bool isSopExport(void);

    /// If true, Indicates that the export is being called from a SOP network
    void setSopExport(const bool& sopexport);

    /// Indicates weither or not we want to export bones end effectors
    bool getExportBonesEndEffectors(void);

    /// Indicates weither or not we want to export bones end effectors
    void setExportBonesEndEffectors(const bool& export_end_effectors);

private:

    /// Resampling frequency, in frames. A linear key frame will be exported
    /// every N frames.
    fpreal myResampleIntervalInFrames;

    /// If true, all animation curves will be resampled on export. If false,
    /// only the unsupported types will be.
    bool myResampleAllAnimation;

    /// Specified the format to use for exporting vertex caches, whether compatbile 
    /// with Maya's or 3DS MAX.
    ROP_FBXVertexCacheExportFormatType myVertexCacheFormat;

    /// If true, the exported file will be in the human-readable ASCII FBX format.
    /// Otherwise, it will be in binary.
    bool myExportInAscii;

    /// If true, the code will attempt to find those vertex cacheable objects which
    /// have a constant point count throughout the exported animation, and export them
    /// as "normal" vertex caches, without breaking them up and triangulating them.
    /// NOTE: This can fail when an object will happen to have a constant vertex count
    /// while changing point connectivity. This is why this is a UI option
    bool myDetectConstantPointCountObjects;

    /// The first network the to start exporting from. Everything (recursively) in this
    /// network will be exported to the FBX file. Defaults to "/obj".
    std::string myStartNodePath;

    /// Create an additional root when exporting subnets. Note that this "/"
    /// and "/obj" start nodes will never be exported.
    bool myCreateSubnetRoot;

    /// Level of detail to use when converting various primitives to polygons.
    float myPolyConvertLOD;

    /// If true, geometry with Deform SOPs will be exported as vertex caches. False by default.
    bool myExportDeformsAsVC;

    /// The name of the take to export. If empty, export the current take (default).
    std::string myExportTakeName;

    /// Determines how invisible objects are to be exported.
    ROP_FBXInvisibleNodeExportType myInvisibleObjectsExportType;

    /// If true, NURBS and Bezier surfaces will be converted to polygons on export.
    bool myConvertSurfaces;

    /// Specifies which version of the SDK to use for export. Defaults to an empty string,
    ///	meaning the most current version.
    std::string mySdkVersion;

    /// Optionally contains the names of the bundles we're exporting. Empty by default.
    /// If not empty, only the bundles specified will be exported.
    std::string myBundleNames;

    /// If true, vertex cache frame snapshots will not be stored in memory, resulting in
    /// less memory usage, but slower performance.
    bool mySaveMemory;

    /// If true, blendshape nodes found in geometry nodes will always be exported, potentially loosing
    /// informations doing so, as nodes modifying geometry after the blend shapes will be ignored.
    bool myForceBlendShapeExport;

    /// If true, deform nodes found in geometry nodes will always be exported, potentially loosing
    /// informations doing so, as nodes modifying geometry after the deform will be ignored.
    bool myForceSkinDeformExport;

    /// Indicates that the export is being called from a SOP network
    bool mySopExport;

    /// Indicates weither or not we want to export bones end effectors
    bool myExportBonesEndEffectors;
};
/********************************************************************************************************/
#endif
