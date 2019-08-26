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
 * COMMENTS:	Class for FBX output.
 *
 */

#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"
#include "ROP_FBXUtil.h"
#include <OBJ/OBJ_Node.h>
#include <OP/OP_Input.h>
#include <OP/OP_Network.h>
#include <OP/OP_Node.h>
#include <OP/OP_Operator.h>
#include <OP/OP_SubnetIndirectInput.h>

#define NEEDED_INDEX_IS_INTERNAL_NODE	    -2
#define NEEDED_INDEX_UNDEFINED		    -1

using namespace std;

/********************************************************************************************************/
ROP_FBXBaseVisitor::ROP_FBXBaseVisitor(ROP_FBXInvisibleNodeExportType hidden_node_export_mode, fpreal start_time)
{
    myHiddenNodeExportMode = hidden_node_export_mode;
    myDidCancel = false;
    myStartTime = start_time;
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
	visitNetworkNodes(op_net, NULL, -1, ROP_FBXNetNodesToVisitAll, 0);
    }
    else
    {
	// Visit just a single node
	visitNodeAndChildren(start_node, NULL, -1, 0);
    }
    
    clearVisitInfos();
}
/********************************************************************************************************/
#if 0
OP_Node*
ROP_FBXBaseVisitor::whichNetworkNodeIs(OP_Node* input_node, int subnet_input, int input_counter, OP_Network* network)
{
    UT_ASSERT(subnet_input >= 0);

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
#endif
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::visitNetworkNodes(OP_Network* network_node, ROP_FBXBaseNodeVisitInfo* parent_info, int connected_input_idx, /* ROP_FBXBaseNodeVisitInfo** per_input_infos, */ ROP_FBXNetNodesToVisitType nodes_to_visit_flag, int connection_count)
{
    if(!network_node)
	return;

    int input_idx_on_target_node, temp_counter;
    THdNodeIntMap node_inp_counters;
    OP_Input *input_ptr;

    // First, visit anything connected to our input
    if(nodes_to_visit_flag == ROP_FBXNetNodesToVisitAll || nodes_to_visit_flag == ROP_FBXNetNodesToVisitConnected)
    {
	OP_Node* target_child;
	if(connected_input_idx >= 0)
	{
	    input_ptr = network_node->getInputReferenceConst(connected_input_idx);
	    if(input_ptr && input_ptr->getNode())
	    {
		OP_SubnetIndirectInput* indir_input = network_node->getParentInput(connected_input_idx);
		if(indir_input)
		{
		    OP_NodeList		 outputs;

		    indir_input->getOutputNodes(outputs);
		    int curr_output, num_outputs = outputs.entries();
		    for(curr_output = 0; curr_output < num_outputs; curr_output++)
		    {
			target_child = outputs(curr_output);

			UT_ASSERT(target_child);
			if(!target_child)
			    continue;

			// Must find out which input we're going into. Use the map and  whichInputIs
			if(node_inp_counters.find(target_child) == node_inp_counters.end())
			    temp_counter = 0;
			else
			    temp_counter = node_inp_counters[target_child] + 1;
			node_inp_counters[target_child] = temp_counter;

			input_idx_on_target_node = this->whichInputIs(input_ptr->getNode(), temp_counter,target_child);
			if(visitNodeAndChildren(target_child, parent_info, input_idx_on_target_node, temp_counter) == ROP_FBXInternalVisitorResultStop)
			{
			    myDidCancel = true;
			    return;
			}
		    }
		} 
	    }
	}
    }


    if(nodes_to_visit_flag == ROP_FBXNetNodesToVisitAll || nodes_to_visit_flag == ROP_FBXNetNodesToVisitDisconnected)
    {
	THDNodeVector postponed_subnets;

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

		// We must visit the subnets from the "free" inputs at the end, to make sure we have all
		// the nodes within subnet exported at that point, since we may be dependent on that fact.
		postponed_subnets.push_back(child_node);
		continue;
	    }

	    if(visitNodeAndChildren(child_node, parent_info, -1, 0) == ROP_FBXInternalVisitorResultStop)
	    {
		myDidCancel = true;
		break;
	    }
	}

	// Now go and export all postponed subnets
	num_children = postponed_subnets.size();
	for(curr_child = 0; curr_child < num_children; curr_child++)
	{
	    if(visitNodeAndChildren(postponed_subnets[curr_child], parent_info, -1, 0) == ROP_FBXInternalVisitorResultStop)
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

static int
compareNodeName( const OP_Node * const *p1, const OP_Node* const * p2)
{
    return UT_String::compareNumberedString((*p1)->getName(), (*p2)->getName());
};

/********************************************************************************************************/
ROP_FBXInternalVisitorResultType
ROP_FBXBaseVisitor::visitNodeAndChildren(OP_Node* node, ROP_FBXBaseNodeVisitInfo* parent_info, int input_idx_on_this_node, int connection_count)
{
    if(!node)
	return ROP_FBXInternalVisitorResultContinue;

    ROP_FBXBaseNodeVisitInfo* thisNodeInfo = NULL;
    ROP_FBXVisitorResultType visit_result = ROP_FBXVisitorResultOk;
    OP_Network* test_net = dynamic_cast<OP_Network*>(node);

    bool is_network_visitable = false;
    if(test_net)
    {
	is_network_visitable = isNetworkVisitable(test_net);
    }

    bool allow_full_processing_this_node = !is_network_visitable || (is_network_visitable && input_idx_on_this_node < 0);

    int needed_subnet_idx = NEEDED_INDEX_UNDEFINED;
    if(is_network_visitable)
    {
	// We just care for the subnet index
	needed_subnet_idx = this->findParentInfoForChildren(node, NULL);
    }

    bool needed_input_empty = false;
    if(needed_subnet_idx >= 0)
    {
	OP_Input* temp_input = test_net->getInputReferenceConst(needed_subnet_idx);
	needed_input_empty = ( (!temp_input) || (temp_input->getNode() == NULL));
    }

    bool allow_visiting_children = (!is_network_visitable) || (needed_subnet_idx == NEEDED_INDEX_UNDEFINED) || (needed_subnet_idx == input_idx_on_this_node) || (needed_input_empty) || (input_idx_on_this_node < 0 && needed_subnet_idx == NEEDED_INDEX_IS_INTERNAL_NODE);
    
    if (CAST_SOPNODE(node))
        allow_visiting_children = false;


    // If this is a subnet we got to through another node, don't create it here.
    if(allow_full_processing_this_node)
    {
	thisNodeInfo = visitBegin(node, input_idx_on_this_node);
	thisNodeInfo->setTraveledInputIndex(input_idx_on_this_node);
	UT_ASSERT(thisNodeInfo);
	thisNodeInfo->setParentInfo(parent_info);

	bool skip = false;
	// Skip if it does not have object children and either not an object or
	// an object that's been visited before, unless its as SOP node..
	skip |= !CAST_SOPNODE(node) &&
	    ( test_net && test_net->getChildTypeID() != OBJ_OPTYPE_ID 
		&& ( !test_net->castToOBJNode() || myAllVisitInfos.count(node) > 0 ) );

	// Skip if it's an hidden node that's not visible and has no connections
	skip |= (!node->getExpose() && !node->getVisible() && node->nConnectedInputs() == 0 && !node->hasAnyOutputNodes());

	addNodeVisitInfo(thisNodeInfo);

	if (skip)
	    visit_result = ROP_FBXVisitorResultSkipSubtreeAndSubnet;
	else
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
		thisNodeInfo = visitBegin(node, input_idx_on_this_node);
		thisNodeInfo->setTraveledInputIndex(input_idx_on_this_node);
		thisNodeInfo->setParentInfo(parent_info);
    
		addNodeVisitInfo(thisNodeInfo);

		interm_visit_result = visit(node, thisNodeInfo);
		connected_input_idx = input_idx_on_this_node;

		if(interm_visit_result == ROP_FBXVisitorResultAbort)
		{
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
	    visitNetworkNodes(test_net, thisNodeInfo, connected_input_idx, nodesType, connection_count);
	}
    }

    // Now visit the hierarchy children, if any
    if(visit_result != ROP_FBXVisitorResultSkipSubtree && visit_result != ROP_FBXVisitorResultSkipSubtreeAndSubnet && allow_visiting_children )
    {
	int input_idx_on_target_node, temp_counter;
	OP_OutputIterator node_outputs(*node);
	THdNodeIntMap node_inp_counters;

	// If we're exporting LODs, we might want to order our children by name if they follow a naming convention,
	// to ensure that the LODs are exported in the proper order
	bool need_to_sort_children_node_for_lods = ROP_FBXUtil::isLODGroupNullNode(node);
	if ( need_to_sort_children_node_for_lods )
	{
	    // If our children are called LODXXXX we  want to sort them by name
	    // If one name doesnt follow the convention, do not sort
	    for (auto &&child : OP_OutputIterator(*node))
	    {
		if (!child->getName().startsWith("LOD", false))
		    need_to_sort_children_node_for_lods = false;
	    }
	}

	if ( need_to_sort_children_node_for_lods )
	    node_outputs.sort((OP_NodeList::Comparator)&compareNodeName);

	ROP_FBXBaseNodeVisitInfo* parent_info_ptr = NULL;

	TBaseNodeVisitInfoVector all_parents;

	if(is_network_visitable)
	    needed_subnet_idx = this->findParentInfoForChildren(node, &all_parents);
	if(all_parents.size() == 0 && thisNodeInfo)
	    all_parents.push_back(thisNodeInfo);

	UT_ASSERT(all_parents.size() > 0);

	int curr_parent_info, num_parent_infos = all_parents.size();
	for(curr_parent_info = 0; curr_parent_info < num_parent_infos; curr_parent_info++)
	{
	    node_inp_counters.clear();
	    parent_info_ptr = all_parents[curr_parent_info];
	    for(auto &&target_child : node_outputs)
	    {
		if(node_inp_counters.find(target_child) == node_inp_counters.end())
		    temp_counter = 0;
		else
		    temp_counter = node_inp_counters[target_child] + 1;
		node_inp_counters[target_child] = temp_counter;

		input_idx_on_target_node = whichInputIs(node, temp_counter, target_child);

		// Only visit the input if it is the first connected input.
		// This helps getting a consistent traversal order with
		// nodes that have multiple inputs such as the OBJ_Blend.
		if( target_child->getNthConnectedInput(0) == input_idx_on_target_node )
		{
		    if(visitNodeAndChildren(target_child,
			parent_info_ptr,
			input_idx_on_target_node,
			temp_counter) == ROP_FBXInternalVisitorResultStop)
		    {
			myDidCancel = true;
			break;
		    }
		}
	    }

	    if(myDidCancel)
		break;
	}

	if(node_outputs.entries() == 0)
	    onEndHierarchyBranchVisiting(node, thisNodeInfo);
    }

    return ROP_FBXInternalVisitorResultContinue;

}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::addNonVisitableNetworkType(const char *net_type)
{
    myNetworkTypesNotToVisit.append(net_type);
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

    if (!node)
	return false;

    // Exclude if it has no children. If it does, then it must be an OP_Network
    if (!node->isNetwork())
	return false;

    // Exclude if it's not an object *and* cannot have object children
    if (static_cast<OP_Network*>(node)->getChildTypeID() != OBJ_OPTYPE_ID)
	return false;

    // Check if the network type is black-listed.
    // Excluded types: dopnet, ropnet, chopnet, popnet, shopnet, vopnet
    const UT_StringHolder &type_name = node->getOperator()->getName();
    int curr_id, num_ids = myNetworkTypesNotToVisit.size();
    for(curr_id = 0; curr_id < num_ids; curr_id++)
    {
	if(type_name == myNetworkTypesNotToVisit[curr_id])
	    return false;
    }

    // If not, check if it is hidden and if we're set to export hidden nodes
    // as nulls.
    bool is_visible = node->getDisplay();
    int use_display_parm = ROP_FBXUtil::getIntOPParm(node, "tdisplay", myStartTime);
    if(use_display_parm)
	is_visible &= (bool)(ROP_FBXUtil::getIntOPParm(node, "display", myStartTime));

    // Exception for the obj manager node
    if (!is_visible && node->isManager())
	is_visible = true;

    // Consider all node as visible
    if (myHiddenNodeExportMode == ROP_FBXInvisibleNodeExportAsVisible)
	is_visible = true;

    if(!is_visible && myHiddenNodeExportMode == ROP_FBXInvisibleNodeExportAsNulls)
    {
	// Note: we also have to check if, when the network is hidden, the transforms of
	// its children depend on any nodes inside the network. For this, it must:
	// 1) Have children.
	// 2) Have the output transform set to any node inside of it.
	UT_String output_transf_name(UT_String::ALWAYS_DEEP);
	ROP_FBXUtil::getStringOPParm(node, "outputobj", output_transf_name, myStartTime);
	if(output_transf_name == OBJ_Node::input1ObjectToken || 
	    output_transf_name == OBJ_Node::input2ObjectToken ||
	    output_transf_name == OBJ_Node::input3ObjectToken ||
	    output_transf_name == OBJ_Node::input4ObjectToken ||
	    output_transf_name.isstring() == false)
	return false;
    }

    return true;
}
/********************************************************************************************************/
bool 
ROP_FBXBaseVisitor::getDidCancel(void)
{
    return myDidCancel;
}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::addNodeVisitInfo(ROP_FBXBaseNodeVisitInfo* visit_info)
{
    myAllVisitInfos.insert(TBaseNodeVisitInfos::value_type(visit_info->getHdNode(), visit_info));
}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::clearVisitInfos(void)
{
    TBaseNodeVisitInfos::iterator mi;
    for(mi = myAllVisitInfos.begin(); mi != myAllVisitInfos.end(); mi++)
    {
	delete mi->second;
    }
    myAllVisitInfos.clear();
}
/********************************************************************************************************/
void 
ROP_FBXBaseVisitor::findVisitInfos(OP_Node* hd_node, TBaseNodeVisitInfoVector &res_infos)
{
    TBaseNodeVisitInfos::iterator mi, li, ui;
    res_infos.clear();
    li = myAllVisitInfos.lower_bound(hd_node);
    ui = myAllVisitInfos.upper_bound(hd_node);
    for(mi = li; mi != ui; mi++)
    {
	res_infos.push_back(mi->second);
    }
}
/********************************************************************************************************/
int 
ROP_FBXBaseVisitor::findParentInfoForChildren(OP_Node* op_parent, TBaseNodeVisitInfoVector* res_out)
{
    if(res_out)
	res_out->clear();

    int needed_idx_out = NEEDED_INDEX_UNDEFINED;

    // If this is subnet, look at its
    OP_Network* op_net = dynamic_cast<OP_Network*>(op_parent);

    if(!op_net)
	return needed_idx_out;

    // Look at the desired output for this network.
    UT_String output_transf_name;
    ROP_FBXUtil::getStringOPParm(op_net, "outputobj", output_transf_name, myStartTime);

    TBaseNodeVisitInfoVector res_infos;
    this->findVisitInfos(op_parent, res_infos);

    int needed_travel_index = NEEDED_INDEX_UNDEFINED;
    if(output_transf_name == OBJ_Node::input1ObjectToken)
	needed_travel_index = 0;
    else if(output_transf_name == OBJ_Node::input2ObjectToken)
	needed_travel_index = 1;
    else if(output_transf_name == OBJ_Node::input3ObjectToken)
	needed_travel_index = 2;
    else if(output_transf_name == OBJ_Node::input4ObjectToken)
	needed_travel_index = 3;

    needed_idx_out = needed_travel_index;

    if(output_transf_name.isstring() == false)
    {
	// This means we're set to "No object" setting. For us,
	// it is functionally equivalent to:
	needed_idx_out = NEEDED_INDEX_IS_INTERNAL_NODE;
	return needed_idx_out;
    }

    // If we have an index, go through the res array to find the nodes.
    if(needed_travel_index >= 0)
    {
	int curr_info, num_infos = res_infos.size();
	for(curr_info = 0; curr_info < num_infos; curr_info++)
	{
	    if(res_infos[curr_info]->getTraveledInputIndex() == needed_travel_index)
	    {
		if(res_out)
		    res_out->push_back(res_infos[curr_info]);
		return needed_idx_out; 
	    }
	}

	return needed_idx_out;
    }

    // If not, see if the subnet is set to output at a specific node inside of it.
    OP_Node* out_transform_node = op_parent->findNode(output_transf_name);
    if(output_transf_name)
    {
	needed_idx_out = NEEDED_INDEX_IS_INTERNAL_NODE;
	// Find it
	res_infos.clear();
	this->findVisitInfos(out_transform_node, res_infos);
	if(res_infos.size() > 0)
	{
	    if(res_out)
		res_out->push_back(res_infos[0]);
	    return needed_idx_out;
	}
    }

    // If we're still here, the subnet has no output set. Use "natural" parent instead.
    return needed_idx_out;
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
ROP_FBXBaseNodeVisitInfo::setFbxNode(FbxNode* node)
{
    myFbxNode = node;
}
/********************************************************************************************************/
FbxNode* 
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
void ROP_FBXBaseNodeVisitInfo::addBlendShapeNode(OP_Node* node)
{
    myBlendShapeNodes.push_back(node);
}
/********************************************************************************************************/
int ROP_FBXBaseNodeVisitInfo::getBlendShapeNodeCount() const
{
    return myBlendShapeNodes.size();
}
/********************************************************************************************************/
OP_Node* ROP_FBXBaseNodeVisitInfo::getBlendShapeNodeAt(const int& index)
{
    if( ( index >= 0 ) && ( index < myBlendShapeNodes.size() ) )
	return myBlendShapeNodes[index];
    
    return NULL;
}
