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
    ROP_FBXVertexCacheMethodGeometry,
    ROP_FBXVertexCacheMethodParticles
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

private:

    // Sample every N frames
    float myResampleIntervalInFrames;
    bool myResampleAllAnimation;

    ROP_FBXVertexCacheExportFormatType myVertexCacheFormat;
};
/********************************************************************************************************/
#endif
