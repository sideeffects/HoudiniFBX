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
#include "ROP_FBXBaseVisitor.h"
#include "ROP_FBXMainVisitor.h"
#include <UT/UT_DMatrix4.h>

class GU_DetailHandle;
class GU_Detail;
class ROP_FBXGDPCache;
class SOP_Node;
class OP_Node;
class OP_Network;
class GEO_Primitive;
class UT_String;
class ROP_FBXMainNodeVisitInfo;
/********************************************************************************************************/
class ROP_API ROP_FBXUtil
{
public:

    static bool getGeometryHandle(SOP_Node* sop_node, float time, GU_DetailHandle &gdh);
    static void getStringOPParm(OP_Node *node, const char* parmName, UT_String &strref, bool do_expand = false, float ftime = 0.0);
    static int getIntOPParm(OP_Node *node, const char* parmName, int index = 0, float ftime = 0.0);
    static float getFloatOPParm(OP_Node *node, const char* parmName, int index = 0, float ftime = 0.0, bool *did_find = NULL);

    static int getMaxPointsOverAnimation(OP_Node* op_node, float start_time, float end_time, float lod, bool allow_constant_point_detection, 
	bool convert_surfaces, UT_Interrupt* boss_op, ROP_FBXGDPCache* v_cache_out, bool &is_pure_surfaces);
    static bool isVertexCacheable(OP_Network *op_net, bool include_deform_nodes, float ftime, bool& found_particles);

    static void convertParticleGDPtoPolyGDP(const GU_Detail* src_gdp, GU_Detail& out_gdp);
    static void convertGeoGDPtoVertexCacheableGDP(const GU_Detail* src_gdp, float lod, bool do_triangulate_and_rearrange, GU_Detail& out_gdp, int& num_pre_proc_points);

    static bool getFinalTransforms(OP_Node* hd_node, bool has_lookat_node, float bone_length, float time_in, UT_String* override_node_type,
	UT_Vector3& t_out, UT_Vector3& r_out, UT_Vector3& s_out, KFbxVector4* post_rotation);

    static OP_Node* findOpInput(OP_Node *op, const char * const find_op_types[], bool include_me, const char* const  allowed_node_types[], bool *did_find_allowed_only, int rec_level = 0);
    static bool findTimeDependentNode(OP_Node *op, const char * const ignored_node_types[], const char * const opt_more_types[], float ftime, bool include_me);
    static void setStandardTransforms(OP_Node* hd_node, KFbxNode* fbx_node, bool has_lookat_node, float bone_length, float ftime, UT_String* override_node_type, bool use_world_transform = false);
    static OP_Node* findNonInstanceTargetFromInstance(OP_Node* instance_ptr);

    static unsigned getGdpPrimId(const GU_Detail* gdp);

    static bool isDummyBone(OP_Node* bone_node);
    static bool isJointNullNode(OP_Node* null_node);

    template < class FBX_MATRIX >
    static void convertHdMatrixToFbxMatrix(const UT_DMatrix4& hd_matrix, FBX_MATRIX& fbx_matrix)
    {
	double *data_ptr = (double*)fbx_matrix.mData;

	data_ptr[0] = hd_matrix(0, 0);
	data_ptr[1] = hd_matrix(0, 1);
	data_ptr[2] = hd_matrix(0, 2);
	data_ptr[3] = hd_matrix(0, 3);

	data_ptr[4] = hd_matrix(1, 0);
	data_ptr[5] = hd_matrix(1, 1);
	data_ptr[6] = hd_matrix(1, 2);
	data_ptr[7] = hd_matrix(1, 3);

	data_ptr[8] = hd_matrix(2, 0);
	data_ptr[9] = hd_matrix(2, 1);
	data_ptr[10] = hd_matrix(2, 2);
	data_ptr[11] = hd_matrix(2, 3);

	data_ptr[12] = hd_matrix(3, 0);
	data_ptr[13] = hd_matrix(3, 1);
	data_ptr[14] = hd_matrix(3, 2);
	data_ptr[15] = hd_matrix(3, 3);
    }

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

    OP_Node* getHdNode(void) const;
    void setHdNode(OP_Node* node);

    int getMaxObjectPoints(void);
    void setMaxObjectPoints(int num_points);

    ROP_FBXVertexCacheMethodType getVertexCacheMethod(void);
    void setVertexCacheMethod(ROP_FBXVertexCacheMethodType vc_method);

    ROP_FBXGDPCache* getVertexCache(void);
    void setVertexCache(ROP_FBXGDPCache* v_cache);

    ROP_FBXVisitorResultType getVisitResultType(void);
    void  setVisitResultType(ROP_FBXVisitorResultType res_type);

    ROP_FBXMainNodeVisitInfo& getVisitInfo(void);
    void setVisitInfoCopy(ROP_FBXMainNodeVisitInfo& info);

    void setIsSurfacesOnly(bool value);
    bool getIsSurfacesOnly(void);

    void setSourcePrimitive(int prim_cnt);
    int getSourcePrimitive(void);

private:
    KFbxNode* myFbxNode;

    // Used for vertex caching
    int myMaxObjectPoints;
    bool myIsSurfacesOnly;
    ROP_FBXVertexCacheMethodType myVertexCacheMethod;
    ROP_FBXGDPCache* myVertexCache;
    OP_Node* myHdNode;

    ROP_FBXVisitorResultType myVisitResultType;

    // Needed for the ugly way in which we have to handle instancing...
    ROP_FBXMainNodeVisitInfo myVisitInfoCopy;

    int mySourcePrim;
};
typedef multimap < OP_Node* , ROP_FBXNodeInfo* > THDToNodeInfoMap;
typedef map < KFbxNode* , ROP_FBXNodeInfo* > TFbxToNodeInfoMap;
typedef vector < ROP_FBXNodeInfo* > TFbxNodeInfoVector;
typedef set < string > TStringSet;
//typedef set < OP_Node* > THDNodeSet;
/********************************************************************************************************/
class ROP_FBXNodeManager
{
public:
    ROP_FBXNodeManager();
    virtual ~ROP_FBXNodeManager();

    void findNodeInfos(OP_Node* hd_node, TFbxNodeInfoVector &res_infos);
    ROP_FBXNodeInfo* findNodeInfo(KFbxNode* fbx_node);

    ROP_FBXNodeInfo& addNodePair(OP_Node* hd_node, KFbxNode* fbx_node, ROP_FBXMainNodeVisitInfo& visit_info);

    void makeNameUnique(UT_String& strName);

    void addBundledNode(OP_Node* hd_node);
    bool isNodeBundled(OP_Node* hd_node);

private:
    THDToNodeInfoMap myHdToNodeInfoMap;
    TFbxToNodeInfoMap myFbxToNodeInfoMap;

    TStringSet myNamesSet;

    // Includes all nodes that are in the bundles we're exporting.
//    THDNodeSet myNodesInBundles;
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

    void setNumConstantPoints(int num_points);
    int getNumConstantPoints(void);
    bool getIsNumPointsConstant(void);

    void clearFrames(void);

private:
    TGeomCacheItems myFrameItems;
    float myMinFrame;
    int myNumConstantPoints;
};
typedef set < ROP_FBXGDPCache* > TGDPCacheSet;
/********************************************************************************************************/
#endif
