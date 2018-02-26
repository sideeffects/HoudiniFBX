/*
 * Copyright (c) 2017
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
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
 * NAME:	ROP_FBXDerivedActions.h (FBX Library, C++)
 *
 * COMMENTS:	
 *
 */
#ifndef __ROP_FBXDerivedActions_h__
#define __ROP_FBXDerivedActions_h__

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseAction.h"

#include <vector>

class ROP_FBXMainVisitor;
class OP_Network;
class GU_Detail;
class GEO_CaptureData;
class ROP_FBXNodeManager;
class ROP_FBXIntTranslator;
class OP_Context;
/********************************************************************************************************/
class ROP_FBXLookAtAction : public ROP_FBXBaseFbxNodeAction
{
public:

    ROP_FBXLookAtAction(FbxNode *acted_on_node, OP_Node* look_at_node, ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXLookAtAction();

    ROP_FBXActionType getType(void);
    void performAction(void);

private:
    OP_Node* myLookAtHdNode;    
};
/********************************************************************************************************/
class SOP_CaptureRegion;
class ROP_FBXSkinningAction : public ROP_FBXBaseFbxNodeAction
{
public:

    ROP_FBXSkinningAction(FbxNode *acted_on_node, OP_Node* deform_node, fpreal capture_frame, ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXSkinningAction();

    ROP_FBXActionType getType(void);
    void performAction(void);

private:
    void createSkinningInfo(
	    FbxNode* fbx_joint_node, FbxNode* fbx_deformed_node, FbxSkin* fbx_skin,
	    GEO_CaptureData& cap_data, int region_idx, SOP_CaptureRegion *cregion, OP_Context& capt_context);
    void addNodeRecursive(FbxArray<FbxNode*>& node_array, FbxNode* curr_node);
    void storeBindPose(FbxNode* fbx_node, fpreal capture_frame);

private:
    OP_Node* myDeformNode;    
    fpreal myCaptureFrame;
};
/********************************************************************************************************/
class ROP_FBXInstanceActionBundle
{
public:
    ROP_FBXInstanceActionBundle() { myHdNode = NULL; myFbxNode = NULL; }
    ROP_FBXInstanceActionBundle(OP_Node* instance_hd_node, FbxNode* instance_fbx_node) { myHdNode = instance_hd_node; myFbxNode = instance_fbx_node; }
    ~ROP_FBXInstanceActionBundle() { }

    OP_Node* myHdNode;
    FbxNode* myFbxNode;
};
typedef std::vector< ROP_FBXInstanceActionBundle > TInstanceBundleVector;

class ROP_FBXCreateInstancesAction : public ROP_FBXBaseAction
{
public:
    ROP_FBXCreateInstancesAction(ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXCreateInstancesAction();

    void addInstance(OP_Node* instance_hd_node, FbxNode* instance_fbx_node);

    ROP_FBXActionType getType(void);
    void performAction(void);

private:
    TInstanceBundleVector myItems;
};
/********************************************************************************************************/
#endif // __ROP_FBXDerivedActions_h__
