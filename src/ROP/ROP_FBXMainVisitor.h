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

#ifndef __ROP_FBXMainVisitor_h__
#define __ROP_FBXMainVisitor_h__

#include <fbx/fbxsdk.h>
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"

class ROP_FBXExporter;
class GU_Detail;
class ROP_FBXErrorManager;
class ROP_FBXGDPCache;
class ROP_FBXNodeManager;
class ROP_FBXActionManager;
/********************************************************************************************************/
class ROP_API ROP_FBXMainNodeVisitInfo : public ROP_FBXBaseNodeVisitInfo
{
public:
    ROP_FBXMainNodeVisitInfo(OP_Node* hd_node);
    virtual ~ROP_FBXMainNodeVisitInfo();

    KFbxNode* GetSkeletonRootNode(void);
    void SetSkeletonRootNode(KFbxNode* node);

private:

    KFbxNode* mySkeletonRootNode;

};
/********************************************************************************************************/
class ROP_API ROP_FBXMainVisitor : public ROP_FBXBaseVisitor
{
public:
    ROP_FBXMainVisitor(ROP_FBXExporter* parent_exporter);
    virtual ~ROP_FBXMainVisitor();

    ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node);
    ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info);

protected:
    KFbxNode* outputGeoNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, ROP_FBXGDPCache* &v_cache_out);
    KFbxNode* outputNullNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node);
    KFbxNode* outputLightNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node);
    KFbxNode* outputCameraNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node);
    KFbxNode* outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, float& bone_length_out);
    void setStandardTransforms(OP_Node* hd_node, KFbxNode* fbx_node, bool has_lookat_node, float bone_length);

    KFbxNodeAttribute* outputPolygons(const GU_Detail* gdp, const char* node_name, int max_points, ROP_FBXVertexCacheMethodType vc_method);

    void exportPointNormals(KFbxMesh* mesh_attr, const GU_Detail *gdp, int attr_offset);
    void exportVertexNormals(KFbxMesh* mesh_attr, const GU_Detail *gdp, int attr_offset);

private:

    ROP_FBXExporter* myParentExporter;
    KFbxSdkManager* mySDKManager;
    KFbxScene* myScene;
    ROP_FBXErrorManager* myErrorManager;
    ROP_FBXNodeManager* myNodeManager;
    ROP_FBXActionManager* myActionManager; 
};
/********************************************************************************************************/
#endif
