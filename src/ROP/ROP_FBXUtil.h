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

#ifndef __ROP_FBXUtil_h__
#define __ROP_FBXUtil_h__

#include <fbx/fbxsdk.h>
#include <GU/GU_Detail.h>
#include "ROP_API.h"
#include "ROP_FBXCommon.h"

class GU_DetailHandle;
class GU_Detail;
class ROP_FBXGDPCache;
class SOP_Node;
class OP_Node;
class OP_Network;
class UT_String;
/********************************************************************************************************/
class ROP_API ROP_FBXUtil
{
public:

    static bool getGeometryHandle(SOP_Node* sop_node, float time, GU_DetailHandle &gdh);
    static void getStringOPParm(OP_Node *node, const char* parmName, UT_String &strref, bool do_expand = false);
    static int getIntOPParm(OP_Node *node, const char* parmName, int index = 0);
    static float getFloatOPParm(OP_Node *node, const char* parmName, int index = 0, float ftime = 0.0);

    static int getMaxPointsOverAnimation(SOP_Node* sop_node, float start_time, float end_time, ROP_FBXGDPCache* v_cache_out);
    static bool isVertexCacheable(OP_Network *op_net, bool& found_particles);

    static void convertParticleGDPtoPolyGDP(const GU_Detail* src_gdp, GU_Detail& out_gdp);
    static void convertGeoGDPtoVertexCacheableGDP(const GU_Detail* src_gdp, GU_Detail& out_gdp);

    static bool getFinalTransforms(OP_Node* hd_node, bool has_lookat_node, float bone_length, float time_in,
	UT_Vector3& t_out, UT_Vector3& r_out, UT_Vector3& s_out, KFbxVector4* post_rotation);
};
/********************************************************************************************************/
class ROP_FBXNodeInfo
{
public:
    ROP_FBXNodeInfo();
    ROP_FBXNodeInfo(KFbxNode* main_node);
    ~ROP_FBXNodeInfo();

    KFbxNode* getFbxNode(void) const;
    void setFbxNode(KFbxNode* node);

    int getMaxObjectPoints(void);
    void setMaxObjectPoints(int num_points);

    ROP_FBXVertexCacheMethodType getVertexCacheMethod(void);
    void setVertexCacheMethod(ROP_FBXVertexCacheMethodType vc_method);

    ROP_FBXGDPCache* getVertexCache(void);
    void setVertexCache(ROP_FBXGDPCache* v_cache);

private:
    KFbxNode* myFbxNode;

    // Used for vertex caching
    int myMaxObjectPoints;
    ROP_FBXVertexCacheMethodType myVertexCacheMethod;
    ROP_FBXGDPCache* myVertexCache;
};
typedef map < OP_Node* , ROP_FBXNodeInfo > THDToFbxNodeMap;
/********************************************************************************************************/
class ROP_FBXNodeManager
{
public:
    ROP_FBXNodeManager();
    virtual ~ROP_FBXNodeManager();

    KFbxNode* findFbxNode(OP_Node* hd_node);
    ROP_FBXNodeInfo* findNodeInfo(OP_Node* hd_node);

    ROP_FBXNodeInfo& addNodePair(OP_Node* hd_node, KFbxNode* fbx_node);

private:
    THDToFbxNodeMap myHdToFbxNodeMap;
};
/********************************************************************************************************/
class ROP_FBXGDPCacheItem
{
public:
    ROP_FBXGDPCacheItem(float frame_num) { myFrame = frame_num; }
    ~ROP_FBXGDPCacheItem() { }

    GU_Detail* getDetail(void) { return &myDetail; }
    float getFrame(void) { return myFrame; }

private:
    float myFrame;
    GU_Detail myDetail;
};
typedef vector < ROP_FBXGDPCacheItem* > TGeomCacheItems;
/********************************************************************************************************/
// NOTE: This class assumes frames are added in increasing order, and no frames are skipped.
class ROP_FBXGDPCache
{
public:
    ROP_FBXGDPCache();
    ~ROP_FBXGDPCache();

    GU_Detail* addFrame(float frame_num);
    GU_Detail* getFrameGeometry(float frame_num);

    float getFirstFrame(void);

private:
    TGeomCacheItems myFrameItems;
    float myMinFrame;
};
/********************************************************************************************************/
#endif
