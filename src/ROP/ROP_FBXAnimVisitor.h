/*
 * Copyright (c) 2017
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

class OBJ_Node;
class SOP_Node;
class GU_Detail;

class PRM_Parm;
class CH_Channel;

class UT_Interrupt;
class UT_XformOrder;


/********************************************************************************************************/
class ROP_FBXAnimNodeVisitInfo : public ROP_FBXBaseNodeVisitInfo
{
public:
    ROP_FBXAnimNodeVisitInfo(OP_Node* hd_node);
    virtual ~ROP_FBXAnimNodeVisitInfo();

private:

};
/********************************************************************************************************/
class ROP_FBXAnimVisitor : public ROP_FBXBaseVisitor
{
public:
    ROP_FBXAnimVisitor(ROP_FBXExporter* parent_exporter);
    virtual ~ROP_FBXAnimVisitor();

    ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node, int input_idx_on_this_node);
    ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info);
    void onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info);

    void reset(FbxAnimLayer* curr_layer);

    void exportTRSAnimation(
            OBJ_Node* node,
            FbxAnimLayer* curr_fbx_anim_layer,
            FbxNode* fbx_node);

protected:

    void exportResampledAnimation(
            FbxAnimLayer* curr_fbx_anim_layer,
            OBJ_Node* source_node,
            FbxNode* fbx_node,
            ROP_FBXBaseNodeVisitInfo *node_info);
    void exportChannel(FbxAnimCurve* fbx_anim_curve, OP_Node* source_node, const char* parm_name, int parm_idx, double scale_factor = 1.0, const int& param_inst = -1);

    void outputResampled(FbxAnimCurve* fbx_curve, CH_Channel *ch, int start_array_idx, int end_array_idx, UT_FprealArray& time_array, bool do_insert, PRM_Parm* direct_eval_parm, int parm_idx, double scale_factor = 1.0);

    bool outputVertexCache(FbxNode* fbx_node, OP_Node* geo_node, const char* file_name, ROP_FBXBaseNodeVisitInfo* node_info_in, ROP_FBXNodeInfo* node_pair_info);
    FbxVertexCacheDeformer* addedVertexCacheDeformerToNode(FbxNode* fbx_node, const char* file_name);
    bool fillVertexArray(OP_Node* node, fpreal time, ROP_FBXBaseNodeVisitInfo* node_info_in, double* vert_array, int num_array_points, ROP_FBXNodeInfo* node_pair_info, fpreal frame_num);
    int lookupExactPointCount(OP_Node *node, fpreal time, int selected_prim_idx);

    bool exportBlendShapeAnimation(OP_Node* blend_shape_node, FbxNode* fbx_node);

    bool exportPackedPrimAnimation(OP_Node* node, const UT_StringRef& path_attrib_name,
                                   FbxAnimLayer* fbx_anim_layer);

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
