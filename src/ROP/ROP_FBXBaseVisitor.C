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
 * COMMENTS:	Class for FBX output.
 *
 */

#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"
#include <OP/OP_Node.h>
#include <OP/OP_Network.h>
/********************************************************************************************************/
ROP_FBXBaseVisitor::ROP_FBXBaseVisitor()
{

}
/********************************************************************************************************/
ROP_FBXBaseVisitor::~ROP_FBXBaseVisitor()
{

}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::visitScene(OP_Network* start_net)
{
    visitNetworkNodes(start_net, NULL);
}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::visitNetworkNodes(OP_Network* network_node, ROP_FBXBaseNodeVisitInfo* parent_info)
{
    if(!network_node)
	return;

    int curr_child, num_children = network_node->getNchildren();
    for(curr_child = 0; curr_child < num_children; curr_child++)
    {
	// If this node has any inputs, it will be visited by one of the parents. Ignore it.
	if(network_node->getChild(curr_child)->nInputs() > 0)
	    continue;

	visitNodeAndChildren(network_node->getChild(curr_child), parent_info);
    }
}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::visitNodeAndChildren(OP_Node* node, ROP_FBXBaseNodeVisitInfo* parent_info)
{
    if(!node)
	return;

    ROP_FBXBaseNodeVisitInfo* thisNodeInfo = NULL;
    
    ROP_FBXVisitorResultType visit_result = ROP_FBXVisitorResultOk;

    thisNodeInfo = visitBegin(node);
    UT_ASSERT(thisNodeInfo);
    thisNodeInfo->setParentInfo(parent_info);
    visit_result = visit(node, thisNodeInfo);

    // See if this node is a network we have to dive into
    OP_Network* test_net = dynamic_cast<OP_Network*>(node);
    if(test_net && visit_result != ROP_FBXVisitorResultSkipSubnet && visit_result != ROP_FBXVisitorResultSkipSubtreeAndSubnet)
    {
	UT_String node_type = test_net->getOperator()->getName();
	if(isNetworkVisitable(node_type))
	{
	    visitNetworkNodes(test_net, thisNodeInfo);
	}
    }

    // Now visit the hierarchy children, if any
    if(visit_result != ROP_FBXVisitorResultSkipSubtree && visit_result != ROP_FBXVisitorResultSkipSubtreeAndSubnet)
    {
	int curr_child, num_children = node->nOutputs();
	for(curr_child = 0; curr_child < num_children; curr_child++)
	{
	    visitNodeAndChildren(node->getOutput(curr_child), thisNodeInfo);
	}

	if(num_children == 0)
	    onEndHierarchyBranchVisiting(node, thisNodeInfo);
    }

    if(thisNodeInfo)
	delete thisNodeInfo;

}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::addVisitableNetworkType(const char *net_type)
{
    myNetworkTypesToVisit.push_back(net_type);
}
/********************************************************************************************************/
bool 
ROP_FBXBaseVisitor::isNetworkVisitable(const char* type_name)
{
    string string_type(type_name);
    int curr_id, num_ids = myNetworkTypesToVisit.size();
    for(curr_id = 0; curr_id < num_ids; curr_id++)
    {
	if(string_type == myNetworkTypesToVisit[curr_id])
	    return true;
    }
    return false;
}
/********************************************************************************************************/
// ROP_FBXBaseNodeVisitInfo
/********************************************************************************************************/
ROP_FBXBaseNodeVisitInfo::ROP_FBXBaseNodeVisitInfo(OP_Node *hd_node)
{
    myHdNode = hd_node;
    myParentInfo = NULL;
    myFbxNode = NULL;
    myMaxObjectPoints = 0;
    myVertexCacheMethod = ROP_FBXVertexCacheMethodNone;
}
/********************************************************************************************************/
ROP_FBXBaseNodeVisitInfo::~ROP_FBXBaseNodeVisitInfo()
{

}
/********************************************************************************************************/
void 
ROP_FBXBaseNodeVisitInfo::setParentInfo(ROP_FBXBaseNodeVisitInfo* parent_info)
{
    myParentInfo = parent_info;
}
/********************************************************************************************************/
void 
ROP_FBXBaseNodeVisitInfo::setFbxNode(KFbxNode* node)
{
    myFbxNode = node;
}
/********************************************************************************************************/
KFbxNode* 
ROP_FBXBaseNodeVisitInfo::getFbxNode(void)
{
    return myFbxNode;
}
/********************************************************************************************************/
ROP_FBXBaseNodeVisitInfo* 
ROP_FBXBaseNodeVisitInfo::getParentInfo(void)
{
    return myParentInfo;
}
/********************************************************************************************************/
int 
ROP_FBXBaseNodeVisitInfo::getMaxObjectPoints(void)
{
    return myMaxObjectPoints;
}
/********************************************************************************************************/
void 
ROP_FBXBaseNodeVisitInfo::setMaxObjectPoints(int num_points)
{
    myMaxObjectPoints = num_points;
}
/********************************************************************************************************/
ROP_FBXVertexCacheMethodType 
ROP_FBXBaseNodeVisitInfo::getVertexCacheMethod(void)
{
    return myVertexCacheMethod;
}
/********************************************************************************************************/
void 
ROP_FBXBaseNodeVisitInfo::setVertexCacheMethod(ROP_FBXVertexCacheMethodType vc_method)
{
    myVertexCacheMethod = vc_method;
}
/********************************************************************************************************/