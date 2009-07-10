/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Oleg Samus
 *	Side Effects
 *	123 Front Street West
 *	Toronto, Ontario
 *	Canada   M5V 3E7
 *	416-504-9876
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
#include <map>
#include <set>

#include "ROP_API.h"

#ifdef FBXFILESDK_NAMESPACE_USE
    #define FBX_FILMBOX_NAMESPACE FBXFILESDK_NAMESPACE
    using namespace FBX_FILMBOX_NAMESPACE;
#else
    #define FBX_FILMBOX_NAMESPACE  
#endif

using namespace std;
/********************************************************************************************************/
const int ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT = 4;

// Determines which network types the visitor dives into
const char* const ROP_FBXnetworkTypesToIgnore[] = { "geo", "bone", "null", "cam", "instance", "hlight", 
	"ambient", "dopnet", "ropnet", "chopnet",  "popnet",  "vopnet",  "shopnet", 0 };

// These declare any node that does not modify the mesh, its vertices or points.
const char* const ROP_FBXallowed_inbetween_node_types[] = {"null", "switch", "subnet", "attribcomposite",
"attribcopy", "attribcreate", "attribmirror", "attribpromote", "attribreorient", 
"attribpromote", "attribstringedit", "attribute", 0};
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
    ROP_FBXInvisibleNodeExportFull
};
/********************************************************************************************************/
class ROP_API ROP_FBXExportOptions
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
    float getResampleIntervalInFrames(void);
    /// Resampling frequency, in frames. A linear key frame will be exported
    /// every N frames.
    void setResampleIntervalInFrames(float frames);

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

private:

    /// Resampling frequency, in frames. A linear key frame will be exported
    /// every N frames.
    float myResampleIntervalInFrames;

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
    string myStartNodePath;

    /// Level of detail to use when converting various primitives to polygons.
    float myPolyConvertLOD;

    /// If true, geometry with Deform SOPs will be exported as vertex caches. False by default.
    bool myExportDeformsAsVC;

    /// The name of the take to export. If empty, export the current take (default).
    string myExportTakeName;

    /// Determines how invisible objects are to be exported.
    ROP_FBXInvisibleNodeExportType myInvisibleObjectsExportType;

    /// If true, NURBS and Bezier surfaces will be converted to polygons on export.
    bool myConvertSurfaces;

    /// Specifies which version of the SDK to use for export. Defaults to an empty string,
    ///	meaning the most current version.
    string mySdkVersion;

    /// Optionally contains the names of the bundles we're exporting. Empty by default.
    /// If not empty, only the bundles specified will be exported.
    string myBundleNames;

    /// If true, vertex cache frame snapshots will not be stored in memory, resulting in
    /// less memory usage, but slower performance.
    bool mySaveMemory;
};
/********************************************************************************************************/
#endif
