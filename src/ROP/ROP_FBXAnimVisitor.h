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

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"

#include <UT/UT_VectorTypes.h>

#include <string>


class ROP_FBXActionManager;
class ROP_FBXErrorManager;
class ROP_FBXExporter;
class ROP_FBXNodeInfo;
class ROP_FBXNodeManager;

class SOP_Node;
class GU_Detail;

class PRM_Parm;
class CH_Channel;

class UT_Interrupt;
class UT_XformOrder;


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

    ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node, int input_idx_on_this_node);
    ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info);
    void onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info);

    void reset(FbxAnimLayer* curr_layer);

    void exportTRSAnimation(OP_Node* node, FbxAnimLayer* curr_fbx_anim_layer, FbxNode* fbx_node);

protected:

    void exportResampledAnimation(FbxAnimLayer* curr_fbx_anim_layer, OP_Node* source_node, FbxNode* fbx_node, ROP_FBXBaseNodeVisitInfo *node_info, const UT_XformOrder& xform_order, bool force_obj_transfrom_from_world);
    void exportChannel(FbxAnimCurve* fbx_anim_curve, OP_Node* source_node, const char* parm_name, int parm_idx, double scale_factor = 1.0);
    void outputResampled(FbxAnimCurve* fbx_curve, CH_Channel *ch, int start_array_idx, int end_array_idx, UT_FprealArray& time_array, bool do_insert, PRM_Parm* direct_eval_parm, int parm_idx);

    bool outputVertexCache(FbxNode* fbx_node, OP_Node* geo_node, const char* file_name, ROP_FBXBaseNodeVisitInfo* node_info_in, ROP_FBXNodeInfo* node_pair_info);
    FbxVertexCacheDeformer* addedVertexCacheDeformerToNode(FbxNode* fbx_node, const char* file_name);
    bool fillVertexArray(OP_Node* node, fpreal time, ROP_FBXBaseNodeVisitInfo* node_info_in, double* vert_array, int num_array_points, ROP_FBXNodeInfo* node_pair_info, fpreal frame_num);
    int lookupExactPointCount(OP_Node *node, fpreal time, int selected_prim_idx);
    bool hasPivotInfo(OP_Node* node);
private:

    ROP_FBXExporter* myParentExporter;
    FbxManager* mySDKManager;
    FbxScene* myScene;
    ROP_FBXErrorManager* myErrorManager;
    ROP_FBXNodeManager* myNodeManager;
    ROP_FBXActionManager* myActionManager; 
    ROP_FBXExportOptions *myExportOptions;

    FbxAnimLayer* myAnimLayer;

    std::string myOutputFileName, myFBXFileSourceFolder, myFBXShortFileName;
    UT_Interrupt* myBoss;
};
/********************************************************************************************************/
#endif
