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

#ifndef __ROP_FBXBaseVisitor_h__
#define __ROP_FBXBaseVisitor_h__

#include "ROP_API.h"
#include "ROP_FBXCommon.h"
#include <fbx/fbxsdk.h>

class OP_Network;
class OP_Node;
class GEO_Primitive;
class FBX_FILMBOX_NAMESPACE::KFbxNode;
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
class ROP_API ROP_FBXBaseNodeVisitInfo
{
public:
    ROP_FBXBaseNodeVisitInfo(OP_Node* hd_node);
    virtual ~ROP_FBXBaseNodeVisitInfo();

    void setParentInfo(ROP_FBXBaseNodeVisitInfo* parent_info);
    ROP_FBXBaseNodeVisitInfo* getParentInfo(void);

    void setFbxNode(KFbxNode* node);
    KFbxNode* getFbxNode(void);

    OP_Node* getHdNode(void);
    void setHdNode(OP_Node* hd_node);

    int getMaxObjectPoints(void);
    void setMaxObjectPoints(int num_points);

    ROP_FBXVertexCacheMethodType getVertexCacheMethod(void);
    void setVertexCacheMethod(ROP_FBXVertexCacheMethodType vc_method);

    void setIsSurfacesOnly(bool value);
    bool getIsSurfacesOnly(void);

    void setSourcePrimitive(int prim_cnt);
    int getSourcePrimitive(void);

    void setTraveledInputIndex(int index);
    int getTraveledInputIndex(void);

private:

    OP_Node* myHdNode;
    KFbxNode* myFbxNode;
    ROP_FBXBaseNodeVisitInfo* myParentInfo;

    // Used for vertex caching
    int myMaxObjectPoints;
    ROP_FBXVertexCacheMethodType myVertexCacheMethod;

    bool myIsSurfacesOnly;
    int mySourcePrim;
    // Index on myHdNode through which we're visiting. -1 if none.
    int myTraveledInputIndex;
};

typedef vector  < string > TStringVector;
/********************************************************************************************************/
class ROP_API ROP_FBXBaseVisitor
{
public:
    ROP_FBXBaseVisitor(ROP_FBXInvisibleNodeExportType hidden_node_export_mode);
    virtual ~ROP_FBXBaseVisitor();

    /// Called before visiting a node. Must return a new instance of
    /// the node info visit structure or a class derived from it.
    virtual ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node, int input_idx_on_this_node) = 0;

    virtual ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info) = 0;

    virtual void onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info) = 0;

    /// Calls visitNodeAndChildren() on the root (given) node.
    void visitScene(OP_Node* start_node);

    void addNonVisitableNetworkType(const char *net_type);
    void addNonVisitableNetworkTypes(const char* const net_types[]);

    bool getDidCancel(void);

private:
    /// Calls visit() on the specified node and then calls itself
    /// on all children.
    ROP_FBXInternalVisitorResultType visitNodeAndChildren(OP_Node* node, ROP_FBXBaseNodeVisitInfo* parent_info, int input_idx_on_this_node);

    /// Visits all nodes in a network, together with their hierarchies
    void visitNetworkNodes(OP_Network* network_node, ROP_FBXBaseNodeVisitInfo* parent_info, int connected_input_idx,ROP_FBXNetNodesToVisitType nodes_to_visit_flag);

    bool isNetworkVisitable(OP_Node* node);

    int whichInputIs(OP_Node* source_node, int counter, OP_Node* target_node);
    OP_Node* whichNetworkNodeIs(OP_Node* input_node, int input_counter, OP_Network* network);

private:
    TStringVector myNetworkTypesNotToVisit;
    ROP_FBXInvisibleNodeExportType myHiddenNodeExportMode;

    bool myDidCancel;
};
/********************************************************************************************************/
#endif