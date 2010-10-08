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

#ifndef __ROP_FBXMainVisitor_h__
#define __ROP_FBXMainVisitor_h__

#include <UT/UT_Color.h>
#include <UT/UT_Assert.h>
#include <UT/UT_String.h>
#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"

class ROP_FBXExporter;
class GU_Detail;
class GB_Attribute;
class GB_AttributeRef;
class GD_TrimRegion;
class ROP_FBXErrorManager;
class ROP_FBXGDPCache;
class ROP_FBXNodeManager;
class GEO_Primitive;
class GU_PrimNURBSurf;
class GU_PrimNURBCurve;
class ROP_FBXActionManager;
class UT_Interrupt;
class ROP_FBXCreateInstancesAction;
class ROP_FBXGDPCache;
/********************************************************************************************************/
enum ROP_FBXAttributeType
{
    ROP_FBXAttributeNormal = 0,
    ROP_FBXAttributeUV,
    ROP_FBXAttributeVertexColor,
    ROP_FBXAttributeUser,

    ROP_FBXAttributeLastPlaceholder
};
/********************************************************************************************************/
typedef vector < GB_Attribute* > THDAttributeVector;
typedef map < OP_Node* , KFbxSurfaceMaterial* > THdFbxMaterialMap;
typedef map < OP_Node* , int > THdNodeIntMap;
//typedef set < OP_Node* > THdNodeSet;
typedef map < string , KFbxTexture* > THdFbxTextureMap;
typedef vector < KFbxLayerElementTexture* > TFbxLayerElemsVector;
//typedef vector < KFbxNode* > TFbxNodesVector;
/********************************************************************************************************/
class ROP_API ROP_FBXAttributeLayerManager
{
public:
    ROP_FBXAttributeLayerManager(KFbxLayerContainer* attr_node) 
    { 
	memset(myNextLayerIndex, 0, sizeof(int)*ROP_FBXAttributeLastPlaceholder); 
	myAttrNode = attr_node;
	UT_ASSERT(attr_node);
    }
    ~ROP_FBXAttributeLayerManager() { } 

    KFbxLayer* getAttributeLayer(ROP_FBXAttributeType attr_type, int *index_out = NULL)
    {
	UT_ASSERT(attr_type >= 0 && attr_type < ROP_FBXAttributeLastPlaceholder);
	KFbxLayer* attr_layer = myAttrNode->GetLayer(myNextLayerIndex[attr_type]);
	if (attr_layer == NULL)
	{
	    int new_idx = myAttrNode->CreateLayer();
	    UT_ASSERT(new_idx == myNextLayerIndex[attr_type]);
	    attr_layer = myAttrNode->GetLayer(myNextLayerIndex[attr_type]);
	}
	if(index_out)
	    *index_out = myNextLayerIndex[attr_type];
        myNextLayerIndex[attr_type]++;
	return attr_layer;
    }

private:

    int myNextLayerIndex[ROP_FBXAttributeLastPlaceholder];
    KFbxLayerContainer* myAttrNode;
};
/********************************************************************************************************/
class ROP_API ROP_FBXMainNodeVisitInfo : public ROP_FBXBaseNodeVisitInfo
{
public:
    ROP_FBXMainNodeVisitInfo(OP_Node* hd_node);
    virtual ~ROP_FBXMainNodeVisitInfo();

    double getBoneLength(void);
    void setBoneLength(double b_length);

    bool getIsVisitingFromInstance(void);
    void setIsVisitingFromInstance(bool value);

    bool getIgnoreBoneLengthForTransforms(void);
    void setIgnoreBoneLengthForTransforms(bool bValue);

private:

    bool myIgnoreLengthForTransforms;
    bool myIsVisitingFromInstance;
    double myBoneLength;
};
/********************************************************************************************************/
class ROP_FBXConstructionInfo
{
public:
    ROP_FBXConstructionInfo() 
    {
	myNode = NULL;
	myHdPrimCnt = -1;
    }
    ROP_FBXConstructionInfo(KFbxNode* fbx_node)
    {
	myNode = fbx_node;
	myHdPrimCnt = -1;
    }
    ~ROP_FBXConstructionInfo()  { }

    void setHdPrimitiveIndex(int prim_cnt) { myHdPrimCnt = prim_cnt; }
    int getHdPrimitiveIndex(void) { return myHdPrimCnt; }

    KFbxNode* getFbxNode(void) { return myNode; }

private:
    KFbxNode* myNode;
    int myHdPrimCnt;
};
typedef vector < ROP_FBXConstructionInfo > TFbxNodesVector;
/********************************************************************************************************/
class ROP_API ROP_FBXMainVisitor : public ROP_FBXBaseVisitor
{
public:
    ROP_FBXMainVisitor(ROP_FBXExporter* parent_exporter);
    virtual ~ROP_FBXMainVisitor();

    ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node, int input_idx_on_this_node);
    ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info);
    void onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info);

    UT_Color getAccumAmbientColor(void);
    ROP_FBXCreateInstancesAction* getCreateInstancesAction(void);

protected:

    void setProperName(KFbxLayerElement* fbx_layer_elem, const GU_Detail* gdp, GB_Attribute* attr);
    bool outputGeoNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, ROP_FBXGDPCache* &v_cache_out, bool& did_cancel_out, TFbxNodesVector& res_nodes);
    bool outputNullNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, TFbxNodesVector& res_nodes);
    bool outputLightNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, TFbxNodesVector& res_nodes);
    bool outputCameraNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, TFbxNodesVector& res_nodes);
    bool outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, bool is_a_null, TFbxNodesVector& res_nodes);
    void outputNURBSSurfaces(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputNURBSCurves(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputPolylines(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes);
    void outputBezierCurves(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputBezierSurfaces(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);

    void outputSingleNURBSSurface(const GU_PrimNURBSurf* hd_nurb, const char* curr_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int prim_cnt);

    int createTexturesForMaterial(OP_Node* mat_node, KFbxSurfaceMaterial* fbx_material, THdFbxTextureMap& tex_map);

    KFbxNodeAttribute* outputPolygons(const GU_Detail* gdp, const char* node_name, int max_points, ROP_FBXVertexCacheMethodType vc_method);
    void outputNURBSSurface(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes);
    void addUserData(const GU_Detail* gdp, THDAttributeVector& hd_attribs, ROP_FBXAttributeLayerManager& attr_manager, KFbxMesh* mesh_attr, KFbxLayerElement::EMappingMode mapping_mode );

    void exportAttributes(const GU_Detail* gdp, KFbxMesh* mesh_attr);
    void exportMaterials(OP_Node* source_node, KFbxNode* fbx_node);

    KFbxSurfaceMaterial* generateFbxMaterial(OP_Node* mat_node, THdFbxMaterialMap& mat_map);
    KFbxSurfaceMaterial* getDefaultMaterial(THdFbxMaterialMap& mat_map);
    KFbxTexture* getDefaultTexture(THdFbxTextureMap& tex_map);
    OP_Node* getSurfaceNodeFromMaterialNode(OP_Node* material_node);
    KFbxTexture* generateFbxTexture(OP_Node* mat_node, int texture_idx, THdFbxTextureMap& tex_map);
    bool isTexturePresent(OP_Node* mat_node, int texture_idx, UT_String* texture_path_out);

    ROP_FBXAttributeType getAttrTypeByName(const GU_Detail* gdp, const char* attr_name);
    KFbxLayerElement* getAndSetFBXLayerElement(KFbxLayer* attr_layer, ROP_FBXAttributeType attr_type, 
	const GU_Detail* gdp, const GB_AttributeRef &attr_offset, KFbxLayerElement::EMappingMode mapping_mode, KFbxLayerContainer* layer_container);

    void finalizeNewNode(ROP_FBXConstructionInfo& constr_info, OP_Node* hd_node, ROP_FBXMainNodeVisitInfo *node_info, KFbxNode* fbx_parent_node, 
	UT_String& override_node_type, const char* lookat_parm_name, ROP_FBXVisitorResultType res_type, 
	ROP_FBXGDPCache *v_cache, bool is_visible);
    void finalizeGeoNode(KFbxNodeAttribute *res_attr, OP_Node* skin_deform_node, int capture_frame, int opt_prim_cnt, TFbxNodesVector& res_nodes);

    void setNURBSSurfaceInfo(KFbxNurbsSurface *nurbs_surf_attr, const GU_PrimNURBSurf* hd_nurb);
    void setNURBSCurveInfo(KFbxNurbsCurve* nurbs_curve_attr, const GU_PrimNURBCurve* hd_nurb);
    void setTrimRegionInfo(GD_TrimRegion* region, KFbxTrimNurbsSurface *trim_nurbs_surf_attr, bool& have_fbx_region);

private:

    fpreal myStartTime;
    ROP_FBXExporter* myParentExporter;
    KFbxSdkManager* mySDKManager;
    KFbxScene* myScene;
    ROP_FBXErrorManager* myErrorManager;
    ROP_FBXNodeManager* myNodeManager;
    ROP_FBXActionManager* myActionManager; 

    THdFbxMaterialMap myMaterialsMap;
    THdFbxTextureMap myTexturesMap;
    KFbxSurfaceMaterial* myDefaultMaterial;
    KFbxTexture* myDefaultTexture;

    UT_Color myAmbientColor;
    UT_Interrupt* myBoss;

    ROP_FBXCreateInstancesAction* myInstancesActionPtr;
};
/********************************************************************************************************/
#endif
