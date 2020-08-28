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

#ifndef __ROP_FBXBaseVisitor_h__
#define __ROP_FBXBaseVisitor_h__

#include "ROP_FBXCommon.h"
#include "ROP_FBXHeaderWrapper.h"
#include <UT/UT_StringArray.h>

#include <map>
#include <string>
#include <vector>

class OP_Network;
class OP_Node;
class GEO_Primitive;
/********************************************************************************************************/
enum ROP_FBXVisitorResultType
{
    ROP_FBXVisitorResultSkipSubnet = 0,
    ROP_FBXVisitorResultSkipSubtree = 1,
    ROP_FBXVisitorResultSkipSubtreeAndSubnet,
    ROP_FBXVisitorResultOk,
    ROP_FBXVisitorResultAbort
};
enum ROP_FBXInternalVisitorResultType
{
    ROP_FBXInternalVisitorResultStop = 0,
    ROP_FBXInternalVisitorResultContinue
};
enum ROP_FBXNetNodesToVisitType
{
    ROP_FBXNetNodesToVisitAll = 0,
    ROP_FBXNetNodesToVisitConnected,
    ROP_FBXNetNodesToVisitDisconnected
};
/********************************************************************************************************/
/// This is an object which gets pushed onto the stack when a node is entered,
/// and gets automatically popped (and destroyed) when it is left.
class ROP_FBXBaseNodeVisitInfo
{
public:
    ROP_FBXBaseNodeVisitInfo(OP_Node* hd_node);
    virtual ~ROP_FBXBaseNodeVisitInfo();

    void setParentInfo(ROP_FBXBaseNodeVisitInfo* parent_info);
    ROP_FBXBaseNodeVisitInfo* getParentInfo();

    void setFbxNode(FbxNode* node);
    FbxNode* getFbxNode();

    OP_Node* getHdNode();
    void setHdNode(OP_Node* hd_node);

    int getMaxObjectPoints();
    void setMaxObjectPoints(int num_points);

    ROP_FBXVertexCacheMethodType getVertexCacheMethod();
    void setVertexCacheMethod(ROP_FBXVertexCacheMethodType vc_method);

    void setIsSurfacesOnly(bool value);
    bool getIsSurfacesOnly();

    void setSourcePrimitive(int prim_cnt);
    int getSourcePrimitive();

    void setTraveledInputIndex(int index);
    int getTraveledInputIndex();

    void addBlendShapeNode(OP_Node* node);
    int getBlendShapeNodeCount() const;
    OP_Node* getBlendShapeNodeAt(const int& index);

private:

    OP_Node* myHdNode;
    FbxNode* myFbxNode;
    ROP_FBXBaseNodeVisitInfo* myParentInfo;

    // Used for vertex caching
    int myMaxObjectPoints;
    ROP_FBXVertexCacheMethodType myVertexCacheMethod;

    bool myIsSurfacesOnly;
    int mySourcePrim;
    // Index on myHdNode through which we're visiting. -1 if none.
    int myTraveledInputIndex;

    std::vector<OP_Node*> myBlendShapeNodes;
};

typedef std::multimap < OP_Node*, ROP_FBXBaseNodeVisitInfo* > TBaseNodeVisitInfos;
typedef std::vector < ROP_FBXBaseNodeVisitInfo* > TBaseNodeVisitInfoVector;
typedef std::vector < OP_Node* > THDNodeVector;
/********************************************************************************************************/
class ROP_FBXBaseVisitor
{
public:
    ROP_FBXBaseVisitor(ROP_FBXInvisibleNodeExportType hidden_node_export_mode, fpreal start_time);
    virtual ~ROP_FBXBaseVisitor();

    /// Called before visiting a node. Must return a new instance of
    /// the node info visit structure or a class derived from it.
    virtual ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node, int input_idx_on_this_node) = 0;

    virtual ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info) = 0;

    virtual void onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info) = 0;

    /// Calls visitNodeAndChildren() on the root (given) node.
    void visitScene(OP_Node* start_node);

    bool getDidCancel();

private:
    /// Calls visit() on the specified node and then calls itself
    /// on all children.
    ROP_FBXInternalVisitorResultType visitNodeAndChildren(OP_Node* node, ROP_FBXBaseNodeVisitInfo* parent_info, int input_idx_on_this_node, int connection_count);

    /// Visits all nodes in a network, together with their hierarchies
    void visitNetworkNodes(OP_Network* network_node, ROP_FBXBaseNodeVisitInfo* parent_info, int connected_input_idx,ROP_FBXNetNodesToVisitType nodes_to_visit_flag, int connection_count);

    int findParentInfoForChildren(OP_Node* op_parent, TBaseNodeVisitInfoVector* res_out);

    bool isNetworkVisitable(OP_Node* node);

    int whichInputIs(OP_Node* source_node, int counter, OP_Node* target_node);

    void addNodeVisitInfo(ROP_FBXBaseNodeVisitInfo* visit_info);
    void clearVisitInfos();
    void findVisitInfos(OP_Node* hd_node, TBaseNodeVisitInfoVector &res_infos);

private:
    ROP_FBXInvisibleNodeExportType myHiddenNodeExportMode;

    bool myDidCancel;

    TBaseNodeVisitInfos myAllVisitInfos;
    fpreal myStartTime;
};
/********************************************************************************************************/
#endif
