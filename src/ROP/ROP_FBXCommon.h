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
/********************************************************************************************************/
class ROP_FBXExportOptions
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

    void setStartNodePath(const char* node_path);
    const char* getStartNodePath(void);

    bool getDetectConstantPointCountObjects(void);
    void setDetectConstantPointCountObjects(bool value);

    void setPolyConvertLOD(float lod);
    float getPolyConvertLOD(void);

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
};
/********************************************************************************************************/
#endif
