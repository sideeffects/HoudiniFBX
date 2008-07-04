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

    bool getResampleAllAnimation(void);
    void setResampleAllAnimation(bool value);

    float getResampleIntervalInFrames(void);
    void setResampleIntervalInFrames(float frames);

    void setVertexCacheFormat(ROP_FBXVertexCacheExportFormatType format_type);
    ROP_FBXVertexCacheExportFormatType getVertexCacheFormat(void);

    bool getExportInAscii(void);
    void setExportInAscii(bool value);

    void setStartNodePath(const char* node_path, bool autohandle_bundles);
    const char* getStartNodePath(void);

    bool getDetectConstantPointCountObjects(void);
    void setDetectConstantPointCountObjects(bool value);

    void setPolyConvertLOD(float lod);
    float getPolyConvertLOD(void);

    bool getExportDeformsAsVC(void);
    void setExportDeformsAsVC(bool value);

    void setExportTakeName(const char* pcsName);
    const char* getExportTakeName(void);

    ROP_FBXInvisibleNodeExportType getInvisibleNodeExportMethod(void);
    void setInvisibleNodeExportMethod(ROP_FBXInvisibleNodeExportType exp_type);

    void setConvertSurfaces(bool value);
    bool getConvertSurfaces(void);

    void setVersion(const char* sdk_version);
    const char* getVersion(void);

    void setBundlesString(const char* bundles);
    const char* getBundlesString(void);
    bool isExportingBundles(void);

private:

    // Sample every N frames
    float myResampleIntervalInFrames;
    bool myResampleAllAnimation;

    ROP_FBXVertexCacheExportFormatType myVertexCacheFormat;

    bool myExportInAscii;
    // If true, the code will attempt to find those vertex cacheable objects which
    // have a constant point count throughout the exported animation, and export them
    // as "normal" vertex caches, without breaking them up and triangulating them.
    // NOTE: This can fail when an object will happen to have a constant vertex count
    // while changing point connectivity. This is why this is a UI option
    bool myDetectConstantPointCountObjects;

    string myStartNodePath;

    // Level of detail to use when converting things to polygons.
    float myPolyConvertLOD;

    // If true, geometry with Deform SOPs will be exported as vertex caches. False by default.
    bool myExportDeformsAsVC;

    // The name of the take to export. If empty, export the current take (default).
    string myExportTakeName;

    // Determines how invisible objects are to be exported.
    ROP_FBXInvisibleNodeExportType myInvisibleObjectsExportType;

    // If true, NURBS and Bezier surfaces will be converted to polygons on export.
    bool myConvertSurfaces;

    //  Which version of the SDK to use for export. Defaults to an empty string,
    //	meaning the most current version.
    string mySdkVersion;

    // Optionally, contains the names of the bundles we're exporting. Empty by default.
    string myBundleNames;
};
/********************************************************************************************************/
#endif
