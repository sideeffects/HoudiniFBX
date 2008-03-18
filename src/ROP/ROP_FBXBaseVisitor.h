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
class FBX_FILMBOX_NAMESPACE::KFbxNode;
/********************************************************************************************************/
enum ROP_FBXVisitorResultType
{
    ROP_FBXVisitorResultSkipSubnet = 0,
    ROP_FBXVisitorResultSkipSubtree = 1,
    ROP_FBXVisitorResultSkipSubtreeAndSubnet,
    ROP_FBXVisitorResultOk
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

    int getMaxObjectPoints(void);
    void setMaxObjectPoints(int num_points);

    ROP_FBXVertexCacheMethodType getVertexCacheMethod(void);
    void setVertexCacheMethod(ROP_FBXVertexCacheMethodType vc_method);

private:

    OP_Node* myHdNode;
    KFbxNode* myFbxNode;
    ROP_FBXBaseNodeVisitInfo* myParentInfo;

    // Used for vertex caching
    int myMaxObjectPoints;
    ROP_FBXVertexCacheMethodType myVertexCacheMethod;
};

typedef vector  < string > TStringVector;
/********************************************************************************************************/
class ROP_API ROP_FBXBaseVisitor
{
public:
    ROP_FBXBaseVisitor();
    virtual ~ROP_FBXBaseVisitor();

    /// Called before visiting a node. Must return a new instance of
    /// the node info visit structure or a class derived from it.
    virtual ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node) = 0;

    virtual ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info) = 0;

    /// Calls visitNodeAndChildren() on the root (given) node.
    void visitScene(OP_Network* start_net);

    void addVisitableNetworkType(const char *net_type);

private:
    /// Calls visit() on the specified node and then calls itself
    /// on all children.
    void visitNodeAndChildren(OP_Node* node, ROP_FBXBaseNodeVisitInfo* parent_info);

    /// Visits all nodes in a network, together with their hierarchies
    void visitNetworkNodes(OP_Network* network_node, ROP_FBXBaseNodeVisitInfo* parent_info);

    bool isNetworkVisitable(const char* type_name);

private:
    TStringVector myNetworkTypesToVisit;
};
/********************************************************************************************************/
#endif