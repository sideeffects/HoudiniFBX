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
#include "ROP_FBXUtil.h"
#include <OP/OP_Node.h>
#include <OP/OP_Network.h>
#include <OP/OP_Input.h>
#include <OP/OP_InputIndirect.h>
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
	visitNetworkNodes(op_net, NULL, -1, ROP_FBXNetNodesToVisitAll);
    }
    else
    {
	// Visit just a single node
	visitNodeAndChildren(start_node, NULL, -1);
    }
    

}
/********************************************************************************************************/
OP_Node*
ROP_FBXBaseVisitor::whichNetworkNodeIs(OP_Node* input_node, int input_counter, OP_Network* network)
{
    OP_Node* child_node;
    OP_Input *temp_input;
    int curr_child, num_children = network->getNchildren();
    int child_input, child_num_inputs;
    int curr_counter = 0;
    for(curr_child = 0; curr_child < num_children; curr_child++)
    {
	child_node = network->getChild(curr_child);
	child_num_inputs = child_node->nInputs();
	for(child_input = 0; child_input < child_num_inputs; child_input++)
	{
	    temp_input = child_node->getInputReferenceConst(child_input);
	    if(!temp_input || !temp_input->getNode())
		continue;
	    if(temp_input->getNode() == input_node)
	    {
		if(curr_counter == input_counter)
		    return child_node;
		else
		    curr_counter++;
	    }
	}
    }

    return NULL;
}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::visitNetworkNodes(OP_Network* network_node, ROP_FBXBaseNodeVisitInfo* parent_info, int connected_input_idx, /* ROP_FBXBaseNodeVisitInfo** per_input_infos, */ ROP_FBXNetNodesToVisitType nodes_to_visit_flag)
{
    if(!network_node)
	return;

    int input_idx_on_target_node, temp_counter;
    THdNodeIntMap node_inp_counters;
    OP_Input *input_ptr;

    // Go through all of our inputs, see if anything is connected to them, visit that first.
    if(nodes_to_visit_flag == ROP_FBXNetNodesToVisitAll || nodes_to_visit_flag == ROP_FBXNetNodesToVisitConnected)
    {
	OP_Node* target_child;
	if(connected_input_idx >= 0)
	{
	    input_ptr = network_node->getInputReferenceConst(connected_input_idx);
	    if(input_ptr && input_ptr->getNode())
	    {
		if(node_inp_counters.find(target_child) == node_inp_counters.end())
		    temp_counter = 0;
		else
		    temp_counter = node_inp_counters[target_child] + 1;
		node_inp_counters[target_child] = temp_counter;

		target_child = whichNetworkNodeIs(input_ptr->getNode(), temp_counter, network_node);
		if(target_child && parent_info)
		{
		    if(visitNodeAndChildren(target_child, parent_info, connected_input_idx) == ROP_FBXInternalVisitorResultStop)
		    {
			myDidCancel = true;
			return;
		    }
		}
	    }
	}
    }


    if(nodes_to_visit_flag == ROP_FBXNetNodesToVisitAll || nodes_to_visit_flag == ROP_FBXNetNodesToVisitDisconnected)
    {
	int curr_child, num_children = network_node->getNchildren();
	for(curr_child = 0; curr_child < num_children; curr_child++)
	{
	    OP_Node* child_node = network_node->getChild(curr_child);

	    // If this node has any inputs, it will be visited by one of the parents. Ignore it.
	    if(child_node->nConnectedInputs() > 0)
	    {
		// Unless it's a subnet. If nodes are free in it, we must create another copy parented
		// to the root.
		OP_Network* test_net = dynamic_cast<OP_Network*>(child_node);
		if(!(test_net && isNetworkVisitable(test_net)))
		    continue;
	    }

	    if(visitNodeAndChildren(child_node, parent_info, -1) == ROP_FBXInternalVisitorResultStop)
	    {
		myDidCancel = true;
		break;
	    }
	}
    }
}
/********************************************************************************************************/
int 
ROP_FBXBaseVisitor::whichInputIs(OP_Node* source_node, int counter, OP_Node* target_node)
{
    int curr_matching_input = 0;
    int curr_input, num_inputs = target_node->nInputs();
    for(curr_input = 0; curr_input < num_inputs; curr_input++)
    {
	if(target_node->getInput(curr_input) == source_node)
	{
	    if(curr_matching_input == counter)
		return curr_input;
	    else
		curr_matching_input++;
	}
    }

    return -1;
}
/********************************************************************************************************/
ROP_FBXInternalVisitorResultType
ROP_FBXBaseVisitor::visitNodeAndChildren(OP_Node* node, ROP_FBXBaseNodeVisitInfo* parent_info, int input_idx_on_this_node)
{
    if(!node)
	return ROP_FBXInternalVisitorResultContinue;

    ROP_FBXBaseNodeVisitInfo* thisNodeInfo = NULL;
    ROP_FBXVisitorResultType visit_result = ROP_FBXVisitorResultOk;
    OP_Network* test_net = dynamic_cast<OP_Network*>(node);

    bool delete_this_info = true;
    bool is_network_visitable = false;
    if(test_net)
    {
	is_network_visitable = isNetworkVisitable(test_net);
    }

    bool allow_full_processing_this_node = !is_network_visitable || (is_network_visitable && input_idx_on_this_node < 0);

    // If this is a subnet we got to through another node, don't create it here.
    if(allow_full_processing_this_node)
    {
	thisNodeInfo = visitBegin(node, input_idx_on_this_node);
	thisNodeInfo->setTraveledInputIndex(input_idx_on_this_node);
	UT_ASSERT(thisNodeInfo);
	thisNodeInfo->setParentInfo(parent_info);
	visit_result = visit(node, thisNodeInfo);
    }

    if(visit_result == ROP_FBXVisitorResultAbort)
    {
	myDidCancel = true;
	return ROP_FBXInternalVisitorResultStop;
    }

    // See if this node is a network we have to dive into
    if(test_net && visit_result != ROP_FBXVisitorResultSkipSubnet && visit_result != ROP_FBXVisitorResultSkipSubtreeAndSubnet)
    {
	// If this is a network with any inputs connected, we must create
	// an additional node for each connected input.
	ROP_FBXVisitorResultType interm_visit_result;
	int connected_input_idx = -1;
	if(test_net->nConnectedInputs() > 0 && input_idx_on_this_node >= 0)
	{
	    OP_Input* temp_input = test_net->getInputReferenceConst(input_idx_on_this_node);
	    if(temp_input && temp_input->getNode())  
	    {
		// Connected input.
		if(thisNodeInfo)
		    delete thisNodeInfo;

		thisNodeInfo = visitBegin(node, input_idx_on_this_node);
		thisNodeInfo->setTraveledInputIndex(input_idx_on_this_node);
		thisNodeInfo->setParentInfo(parent_info);
		interm_visit_result = visit(node, thisNodeInfo);

		connected_input_idx = input_idx_on_this_node;

		if(interm_visit_result == ROP_FBXVisitorResultAbort)
		{
		    if(thisNodeInfo)
			delete thisNodeInfo;
		    myDidCancel = true;
		    return ROP_FBXInternalVisitorResultStop;
		}
	    }
	}

	if(is_network_visitable)
	{
	    ROP_FBXNetNodesToVisitType nodesType;
	    if(input_idx_on_this_node < 0)
		nodesType = ROP_FBXNetNodesToVisitDisconnected;
	    else
		nodesType = ROP_FBXNetNodesToVisitConnected;
	    visitNetworkNodes(test_net, thisNodeInfo, connected_input_idx, nodesType);
	}
    }

    // Now visit the hierarchy children, if any
    if(visit_result != ROP_FBXVisitorResultSkipSubtree && visit_result != ROP_FBXVisitorResultSkipSubtreeAndSubnet && allow_full_processing_this_node )
    {
	int curr_child, num_children = node->nOutputs();
	int input_idx_on_target_node, temp_counter;
	THdNodeIntMap node_inp_counters;
	OP_Node* target_child;

	for(curr_child = 0; curr_child < num_children; curr_child++)
	{
	    target_child = node->getOutput(curr_child);
	    if(node_inp_counters.find(target_child) == node_inp_counters.end())
		temp_counter = 0;
	    else
		temp_counter = node_inp_counters[target_child] + 1;
	    node_inp_counters[target_child] = temp_counter;

	    input_idx_on_target_node = whichInputIs(node, temp_counter, target_child);
	    if(visitNodeAndChildren(target_child, thisNodeInfo, input_idx_on_target_node) == ROP_FBXInternalVisitorResultStop)
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
    myTraveledInputIndex = -1;
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
void ROP_FBXBaseNodeVisitInfo::setTraveledInputIndex(int index)
{
    myTraveledInputIndex = index;
}
/********************************************************************************************************/
int ROP_FBXBaseNodeVisitInfo::getTraveledInputIndex(void)
{
    return myTraveledInputIndex;
}
/********************************************************************************************************/
