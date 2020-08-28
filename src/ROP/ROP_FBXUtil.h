/*
 * Copyright (c) 2020
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

#ifndef __ROP_FBXUtil_h__
#define __ROP_FBXUtil_h__

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"
#include "ROP_FBXMainVisitor.h"

#include <GU/GU_Detail.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_Set.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_VectorTypes.h>
#include <SYS/SYS_Types.h>

#include <set>
#include <map>
#include <vector>
#include <string>


class ROP_FBXGDPCache;
class ROP_FBXMainNodeVisitInfo;

class OBJ_Node;
class SOP_Node;
class GEO_Primitive;
class GU_Detail;
class GU_DetailHandle;

class OP_Context;
class OP_Network;
class OP_Node;

class UT_String;
class UT_StringRef;
class UT_XformOrder;

/********************************************************************************************************/
class ROP_FBXUtil
{
public:

    static bool getGeometryHandle(SOP_Node* sop_node, OP_Context &context, GU_DetailHandle &gdh);
    static void getStringOPParm(OP_Node *node, const char* parmName, UT_String &strref, fpreal ftime);
    static int getIntOPParm(OP_Node *node, const char* parmName, fpreal ftime, int index = 0);
    static fpreal getFloatOPParm(OP_Node *node, const char* parmName, fpreal ftime, int index = 0, bool *did_find = NULL);

    static int getMaxPointsOverAnimation(OP_Node* op_node, fpreal start_time, fpreal end_time, float lod,
            bool allow_constant_point_detection, bool convert_surfaces, UT_Interrupt* boss_op,
            ROP_FBXGDPCache* v_cache_out, bool &is_pure_surfaces);
    static bool isVertexCacheable(OP_Network *op_net, bool include_deform_nodes, fpreal ftime, bool& found_particles, bool is_sop_export);

    static void convertParticleGDPtoPolyGDP(const GU_Detail* src_gdp, GU_Detail& out_gdp);
    static void convertGeoGDPtoVertexCacheableGDP(const GU_Detail* src_gdp, float lod,
            bool do_triangulate_and_rearrange, GU_Detail& out_gdp, int& num_pre_proc_points);

    static EFbxRotationOrder fbxRotationOrder(UT_XformOrder::xyzOrder rot_order);
    static bool mapsToFBXTransform(fpreal t, OBJ_Node* node);
    static void getFinalTransforms(OP_Node* hd_node, ROP_FBXBaseNodeVisitInfo *node_info, fpreal bone_length, fpreal time_in,
	const UT_XformOrder& xform_order, UT_Vector3D& t_out, UT_Vector3D& r_out, UT_Vector3D& s_out,
	UT_Vector3D* prev_frame_rotations);

    static bool getPostRotateAdjust(const UT_StringRef &node_type, FbxVector4 &post_rotate);
    static void doPostRotateAdjust(FbxVector4 &post_rotate, const FbxVector4 &adjustment);

    static OP_Node* findOpInput(OP_Node *op, const char * const find_op_types[], bool include_me, const char* const  allowed_node_types[], bool *did_find_allowed_only, int rec_level = 0, UT_Set<OP_Node*> *already_visited=NULL);
    static bool findTimeDependentNode(OP_Node *op, const char * const ignored_node_types[], const char * const opt_more_types[], fpreal ftime, bool include_me, UT_Set<OP_Node*> *already_visited=NULL);
    static void setStandardTransforms(OP_Node* hd_node, FbxNode* fbx_node, ROP_FBXBaseNodeVisitInfo *node_info, fpreal bone_length, fpreal ftime, bool use_world_transform = false);
    static OP_Node* findNonInstanceTargetFromInstance(OP_Node* instance_ptr, fpreal ftime);

    static GA_PrimCompat::TypeMask getGdpPrimId(const GU_Detail* gdp);

    static bool isDummyBone(OP_Node* bone_node, fpreal ftime);
    static bool isJointNullNode(OP_Node* null_node, fpreal ftime);

    static bool isLODGroupNullNode(OP_Node* null_node);

    static bool outputCustomProperties(OP_Node* node, FbxObject* fbx_node);
    

    static void getNodeName(OP_Node* node, UT_String& node_name, ROP_FBXNodeManager* node_manager, fpreal ftime);

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
    ROP_FBXNodeInfo(FbxNode* main_node);
    ~ROP_FBXNodeInfo();

    FbxNode* getFbxNode() const;
    void setFbxNode(FbxNode* node);

    OP_Node* getHdNode() const;
    void setHdNode(OP_Node* node);

    int getMaxObjectPoints();
    void setMaxObjectPoints(int num_points);

    ROP_FBXVertexCacheMethodType getVertexCacheMethod();
    void setVertexCacheMethod(ROP_FBXVertexCacheMethodType vc_method);

    ROP_FBXGDPCache* getVertexCache();
    void setVertexCache(ROP_FBXGDPCache* v_cache);

    ROP_FBXVisitorResultType getVisitResultType();
    void  setVisitResultType(ROP_FBXVisitorResultType res_type);

    ROP_FBXMainNodeVisitInfo& getVisitInfo();
    void setVisitInfoCopy(ROP_FBXMainNodeVisitInfo& info);

    void setIsSurfacesOnly(bool value);
    bool getIsSurfacesOnly();

    void setSourcePrimitive(int prim_cnt);
    int getSourcePrimitive();

    void setTraveledInputIndex(int idx);
    int getTraveledInputIndex();

    void addBlendShapeNode(OP_Node* node);
    int getBlendShapeNodeCount() const;
    OP_Node* getBlendShapeNodeAt(const int& index);

    void setPathValue(const UT_StringHolder& path) { myPathValue = path; }
    const UT_StringHolder& getPathValue() const { return myPathValue; }

    void setHasPrimTransform(bool flag) { myHasPrimTransform = flag; }
    bool getHasPrimTransform() const { return myHasPrimTransform; }

private:
    FbxNode* myFbxNode;

    // Used for vertex caching
    int myMaxObjectPoints;
    bool myIsSurfacesOnly;
    bool myHasPrimTransform;
    ROP_FBXVertexCacheMethodType myVertexCacheMethod;
    ROP_FBXGDPCache* myVertexCache;
    OP_Node* myHdNode;

    ROP_FBXVisitorResultType myVisitResultType;

    // Needed for the ugly way in which we have to handle instancing...
    ROP_FBXMainNodeVisitInfo myVisitInfoCopy;

    int myTravelledIndex;

    int mySourcePrim;

    UT_StringHolder myPathValue;

    std::vector<OP_Node*> myBlendShapeNodes;
};
typedef std::multimap < OP_Node* , ROP_FBXNodeInfo* > THDToNodeInfoMap;
typedef std::map < FbxNode* , ROP_FBXNodeInfo* > TFbxToNodeInfoMap;
typedef std::map < OP_Node*, FbxNode* > THdNodeToFbxNodeMap;
typedef std::vector < ROP_FBXNodeInfo* > TFbxNodeInfoVector;
typedef std::set < OP_Node* > THDNodeSet;
/********************************************************************************************************/
class ROP_FBXNodeManager
{
public:
    ROP_FBXNodeManager();
    virtual ~ROP_FBXNodeManager();

    void findNodeInfos(OP_Node* hd_node, TFbxNodeInfoVector &res_infos);
    ROP_FBXNodeInfo* findNodeInfo(FbxNode* fbx_node);

    ROP_FBXNodeInfo& addNodePair(OP_Node* hd_node, FbxNode* fbx_node, ROP_FBXMainNodeVisitInfo& visit_info);

    void makeNameUnique(UT_String& strName);

    void addBundledNode(OP_Node* hd_node);
    bool isNodeBundled(OP_Node* hd_node);

private:
    THDToNodeInfoMap myHdToNodeInfoMap;
    TFbxToNodeInfoMap myFbxToNodeInfoMap;

    // TStringSet myNamesSet; Removed as a fix for RFE #67311, more detailed 
    //                        comment in the .c file makeNameUnique()

    // Includes all nodes that are in the bundles we're exporting.
    THDNodeSet myNodesInBundles;
};
/********************************************************************************************************/
class ROP_FBXGDPCacheItem
{
public:
    ROP_FBXGDPCacheItem(fpreal frame_num) { myFrame = frame_num; }
    ~ROP_FBXGDPCacheItem() { }

    GU_Detail* getDetail() { return &myDetail; }
    fpreal getFrame() { return myFrame; }

private:
    fpreal myFrame;
    GU_Detail myDetail;
};
typedef std::vector < ROP_FBXGDPCacheItem* > TGeomCacheItems;
/********************************************************************************************************/
// NOTE: This class assumes frames are added in increasing order, and no frames are skipped.
class ROP_FBXGDPCache
{
public:
    ROP_FBXGDPCache();
    ~ROP_FBXGDPCache();

    GU_Detail* addFrame(fpreal frame_num);
    GU_Detail* getFrameGeometry(fpreal frame_num);

    fpreal getFirstFrame();

    void setNumConstantPoints(int num_points);
    int getNumConstantPoints();
    bool getIsNumPointsConstant();

    void clearFrames();

    bool getSaveMemory();
    void setSaveMemory(bool value);

    int getNumFrames() { return myFrameItems.size(); }

private:
    TGeomCacheItems myFrameItems;
    fpreal myMinFrame;
    int myNumConstantPoints;

    // Use less memory by not actually caching anything
    bool mySaveMemory;
};
typedef std::set < ROP_FBXGDPCache* > TGDPCacheSet;
/********************************************************************************************************/
#endif
