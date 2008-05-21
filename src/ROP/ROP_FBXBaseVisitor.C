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
ROP_FBXBaseVisitor::ROP_FBXBaseVisitor(ROP_FBXInvisibleNodeExportType hidden_node_export_mode)
{
    myHiddenNodeExportMode = hidden_node_export_mode;
    myDidCancel = false;
}
/********************************************************************************************************/
ROP_FBXBaseVisitor::~ROP_FBXBaseVisitor()
{

}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::visitScene(OP_Node* start_node)
{
    myDidCancel = false;

    if(start_node->isNetwork() && isNetworkVisitable(start_node))
    {
	OP_Network* op_net = dynamic_cast<OP_Network*>(start_node);
	UT_ASSERT(op_net);
	visitNetworkNodes(op_net, NULL);
    }
    else
    {
	// Visit just a single node
	visitNodeAndChildren(start_node, NULL);
    }
    

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

	if(visitNodeAndChildren(network_node->getChild(curr_child), parent_info) == ROP_FBXInternalVisitorResultStop)
	{
	    myDidCancel = true;
	    break;
	}
    }
}
/********************************************************************************************************/
ROP_FBXInternalVisitorResultType
ROP_FBXBaseVisitor::visitNodeAndChildren(OP_Node* node, ROP_FBXBaseNodeVisitInfo* parent_info)
{
    if(!node)
	return ROP_FBXInternalVisitorResultContinue;

    ROP_FBXBaseNodeVisitInfo* thisNodeInfo = NULL;
    
    ROP_FBXVisitorResultType visit_result = ROP_FBXVisitorResultOk;

    thisNodeInfo = visitBegin(node);
    UT_ASSERT(thisNodeInfo);
    thisNodeInfo->setParentInfo(parent_info);
    visit_result = visit(node, thisNodeInfo);

    if(visit_result == ROP_FBXVisitorResultAbort)
    {
	myDidCancel = true;
	return ROP_FBXInternalVisitorResultStop;
    }

    // See if this node is a network we have to dive into
    OP_Network* test_net = dynamic_cast<OP_Network*>(node);
    if(test_net && visit_result != ROP_FBXVisitorResultSkipSubnet && visit_result != ROP_FBXVisitorResultSkipSubtreeAndSubnet)
    {
	if(isNetworkVisitable(test_net))
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
	    if(visitNodeAndChildren(node->getOutput(curr_child), thisNodeInfo) == ROP_FBXInternalVisitorResultStop)
	    {
		myDidCancel = true;
		break;
	    }
	}

	if(num_children == 0)
	    onEndHierarchyBranchVisiting(node, thisNodeInfo);
    }

    if(thisNodeInfo)
	delete thisNodeInfo;

    return ROP_FBXInternalVisitorResultContinue;

}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::addNonVisitableNetworkType(const char *net_type)
{
    myNetworkTypesNotToVisit.push_back(net_type);
}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::addNonVisitableNetworkTypes(const char* const net_types[])
{
    int arr_pos = 0;
    while(net_types[arr_pos])
    {
	addNonVisitableNetworkType(net_types[arr_pos]);
	arr_pos++;
    }
}
/********************************************************************************************************/
bool 
ROP_FBXBaseVisitor::isNetworkVisitable(OP_Node* node)
{
    // TODO: Now check for excluded, not included, types; not visitable
    // if hidden and export as nulls is set.

    // Excluded types: dopnet, ropnet, chopnet, popnet, shopnet, vopnet
    if(!node || !node->isNetwork())
	return false;

    // Check if the network type is black-listed.
    UT_String type_name = node->getOperator()->getName();
    string string_type(type_name);
    int curr_id, num_ids = myNetworkTypesNotToVisit.size();
    for(curr_id = 0; curr_id < num_ids; curr_id++)
    {
	if(string_type == myNetworkTypesNotToVisit[curr_id])
	    return false;
    }

    // If not, check if it is hidden and if we're set to export hidden nodes
    // as nulls.
    if(!node->getDisplay() && myHiddenNodeExportMode == ROP_FBXInvisibleNodeExportAsNulls)
	return false;

    return true;
}
/********************************************************************************************************/
bool 
ROP_FBXBaseVisitor::getDidCancel(void)
{
    return myDidCancel;
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
    myIsSurfacesOnly = false;
    mySourcePrim = -1;
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
OP_Node* 
ROP_FBXBaseNodeVisitInfo::getHdNode(void)
{
    return myHdNode;
}
/********************************************************************************************************/
void 
ROP_FBXBaseNodeVisitInfo::setHdNode(OP_Node* hd_node)
{
    myHdNode = hd_node;
}
/********************************************************************************************************/
void 
ROP_FBXBaseNodeVisitInfo::setIsSurfacesOnly(bool value)
{
    myIsSurfacesOnly = value;
}
/********************************************************************************************************/
bool 
ROP_FBXBaseNodeVisitInfo::getIsSurfacesOnly(void)
{
    return myIsSurfacesOnly; 
}
/********************************************************************************************************/
void 
ROP_FBXBaseNodeVisitInfo::setSourcePrimitive(int prim_cnt)
{
    mySourcePrim = prim_cnt;
}
/********************************************************************************************************/
int
ROP_FBXBaseNodeVisitInfo::getSourcePrimitive(void)
{
    return mySourcePrim;
}
/********************************************************************************************************/