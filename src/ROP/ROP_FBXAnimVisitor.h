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

#ifndef __ROP_FBXAnimVisitor_h__
#define __ROP_FBXAnimVisitor_h__

#include <fbx/fbxsdk.h>
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"

class ROP_FBXExporter;
class GU_Detail;
class UT_Interrupt;
class ROP_FBXErrorManager;
class ROP_FBXNodeInfo;
class SOP_Node;
class CH_Channel;
class UT_IntArray;
class ROP_FBXNodeManager;
class ROP_FBXActionManager;
/********************************************************************************************************/
class ROP_API ROP_FBXAnimNodeVisitInfo : public ROP_FBXBaseNodeVisitInfo
{
public:
    ROP_FBXAnimNodeVisitInfo(OP_Node* hd_node);
    virtual ~ROP_FBXAnimNodeVisitInfo();

private:

};
/********************************************************************************************************/
class ROP_API ROP_FBXAnimVisitor : public ROP_FBXBaseVisitor
{
public:
    ROP_FBXAnimVisitor(ROP_FBXExporter* parent_exporter);
    virtual ~ROP_FBXAnimVisitor();

    ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node);
    ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info);
    void onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info);

    void reset(void);

    void exportTRSAnimation(OP_Node* node, KFbxTakeNode* curr_fbx_take, KFbxNode* fbx_node);
    static KFbxTakeNode* addFBXTakeNode(KFbxTakeNodeContainer *fbx_node);

protected:

    void exportBonesAnimation(KFbxTakeNode* curr_fbx_take, OP_Node* source_node, KFbxNode* fbx_node);
    void exportChannel(KFCurve* fbx_curve, OP_Node* source_node, const char* parm_name, int parm_idx, double scale_factor = 1.0);
    void outputResampled(KFCurve* fbx_curve, CH_Channel *ch, int start_array_idx, int end_array_idx, UT_IntArray& frame_array, bool do_insert);

    bool outputVertexCache(KFbxNode* fbx_node, OP_Node* geo_node, const char* file_name, ROP_FBXBaseNodeVisitInfo* node_info_in, ROP_FBXNodeInfo* node_pair_info);
    KFbxVertexCacheDeformer* addedVertexCacheDeformerToNode(KFbxNode* fbx_node, const char* file_name);
    bool fillVertexArray(OP_Node* node, float time, ROP_FBXBaseNodeVisitInfo* node_info_in, double* vert_array, int num_array_points, ROP_FBXNodeInfo* node_pair_info, float frame_num);

private:

    ROP_FBXExporter* myParentExporter;
    KFbxSdkManager* mySDKManager;
    KFbxScene* myScene;
    ROP_FBXErrorManager* myErrorManager;
    ROP_FBXNodeManager* myNodeManager;
    ROP_FBXActionManager* myActionManager; 
    ROP_FBXExportOptions *myExportOptions;

    string myOutputFileName, myFBXFileSourceFolder, myFBXShortFileName;
    UT_Interrupt* myBoss;
};
/********************************************************************************************************/
#endif
