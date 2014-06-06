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

#include <map>
#include <string>
#include <vector>

class ROP_FBXExporter;
class GU_Detail;
class GA_Attribute;
class GA_ROAttributeRef;
namespace GA_PrimCompat { class TypeMask; }
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
typedef std::vector < GA_Attribute* > THDAttributeVector;
typedef std::map < OP_Node* , FbxSurfaceMaterial* > THdFbxMaterialMap;
typedef std::map < OP_Node* , int > THdNodeIntMap;
//typedef set < OP_Node* > THdNodeSet;
typedef std::map < std::string , FbxTexture* > THdFbxTextureMap;
typedef std::vector < FbxLayerElementTexture* > TFbxLayerElemsVector;
//typedef std::vector < FbxNode* > TFbxNodesVector;
/********************************************************************************************************/
class ROP_API ROP_FBXAttributeLayerManager
{
public:
    ROP_FBXAttributeLayerManager(FbxLayerContainer* attr_node) 
    { 
	memset(myNextLayerIndex, 0, sizeof(int)*ROP_FBXAttributeLastPlaceholder); 
	myAttrNode = attr_node;
	UT_ASSERT(attr_node);
    }
    ~ROP_FBXAttributeLayerManager() { } 

    FbxLayer* getAttributeLayer(ROP_FBXAttributeType attr_type, int *index_out = NULL)
    {
	UT_ASSERT(attr_type >= 0 && attr_type < ROP_FBXAttributeLastPlaceholder);
	FbxLayer* attr_layer = myAttrNode->GetLayer(myNextLayerIndex[attr_type]);
	if (attr_layer == NULL)
	{
	    UT_VERIFY(myAttrNode->CreateLayer() == myNextLayerIndex[attr_type]);
	    attr_layer = myAttrNode->GetLayer(myNextLayerIndex[attr_type]);
	}
	if(index_out)
	    *index_out = myNextLayerIndex[attr_type];
        myNextLayerIndex[attr_type]++;
	return attr_layer;
    }

private:

    int myNextLayerIndex[ROP_FBXAttributeLastPlaceholder];
    FbxLayerContainer* myAttrNode;
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
    ROP_FBXConstructionInfo(FbxNode* fbx_node)
    {
	myNode = fbx_node;
	myHdPrimCnt = -1;
    }
    ~ROP_FBXConstructionInfo()  { }

    void setHdPrimitiveIndex(int prim_cnt) { myHdPrimCnt = prim_cnt; }
    int getHdPrimitiveIndex(void) { return myHdPrimCnt; }

    FbxNode* getFbxNode(void) { return myNode; }

private:
    FbxNode* myNode;
    int myHdPrimCnt;
};
typedef std::vector < ROP_FBXConstructionInfo > TFbxNodesVector;
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

    // Given a gdp pointer, this will return a pointer to a gdp which consists of
    // only supported primitives for export. This may or may not be a converted geo.
    // The conversion_spare parm is the object which may be used for conversion - 
    // the idea is to avoid dynamically allocating one and avoiding ambiguity 
    // regarding whether the returned pointer needs to be deleted or not. 
    // In this case, it doesn't ever need to be deleted.
    const GU_Detail* getExportableGeo(const GU_Detail* gdp_orig, GU_Detail& conversion_spare, GA_PrimCompat::TypeMask &prim_types_in_out);

    void setProperName(FbxLayerElement* fbx_layer_elem, const GU_Detail* gdp, GA_Attribute* attr);
    bool outputGeoNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, ROP_FBXGDPCache* &v_cache_out, bool& did_cancel_out, TFbxNodesVector& res_nodes);
    bool outputNullNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes);
    bool outputLightNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes);
    bool outputCameraNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes);
    bool outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, bool is_a_null, TFbxNodesVector& res_nodes);
    void outputNURBSSurfaces(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputNURBSCurves(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputPolylines(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes);
    void outputBezierCurves(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputBezierSurfaces(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);

    void outputSingleNURBSSurface(const GU_PrimNURBSurf* hd_nurb, const char* curr_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int prim_cnt);

    int createTexturesForMaterial(OP_Node* mat_node, FbxSurfaceMaterial* fbx_material, THdFbxTextureMap& tex_map);

    FbxNodeAttribute* outputPolygons(const GU_Detail* gdp, const char* node_name, int max_points, ROP_FBXVertexCacheMethodType vc_method);
    void outputNURBSSurface(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes);
    void addUserData(const GU_Detail* gdp, THDAttributeVector& hd_attribs, ROP_FBXAttributeLayerManager& attr_manager, FbxMesh* mesh_attr, FbxLayerElement::EMappingMode mapping_mode );

    void exportAttributes(const GU_Detail* gdp, FbxMesh* mesh_attr);
    void exportMaterials(OP_Node* source_node, FbxNode* fbx_node);

    FbxSurfaceMaterial* generateFbxMaterial(OP_Node* mat_node, THdFbxMaterialMap& mat_map);
    FbxSurfaceMaterial* getDefaultMaterial(THdFbxMaterialMap& mat_map);
    FbxTexture* getDefaultTexture(THdFbxTextureMap& tex_map);
    OP_Node* getSurfaceNodeFromMaterialNode(OP_Node* material_node);
    FbxTexture* generateFbxTexture(OP_Node* mat_node, int texture_idx, THdFbxTextureMap& tex_map);
    bool isTexturePresent(OP_Node* mat_node, int texture_idx, UT_String* texture_path_out);

    ROP_FBXAttributeType getAttrTypeByName(const GU_Detail* gdp, const char* attr_name);
    FbxLayerElement* getAndSetFBXLayerElement(FbxLayer* attr_layer, ROP_FBXAttributeType attr_type, 
	const GU_Detail* gdp, const GA_ROAttributeRef &attr_offset, const GA_ROAttributeRef &extra_attr_offset,  FbxLayerElement::EMappingMode mapping_mode, FbxLayerContainer* layer_container);

    void finalizeNewNode(ROP_FBXConstructionInfo& constr_info, OP_Node* hd_node, ROP_FBXMainNodeVisitInfo *node_info, FbxNode* fbx_parent_node, 
	UT_String& override_node_type, const char* lookat_parm_name, ROP_FBXVisitorResultType res_type, 
	ROP_FBXGDPCache *v_cache, bool is_visible);
    void finalizeGeoNode(FbxNodeAttribute *res_attr, OP_Node* skin_deform_node, int capture_frame, int opt_prim_cnt, TFbxNodesVector& res_nodes);

    void setNURBSSurfaceInfo(FbxNurbsSurface *nurbs_surf_attr, const GU_PrimNURBSurf* hd_nurb);
    void setNURBSCurveInfo(FbxNurbsCurve* nurbs_curve_attr, const GU_PrimNURBCurve* hd_nurb);
    void setTrimRegionInfo(GD_TrimRegion* region, FbxTrimNurbsSurface *trim_nurbs_surf_attr, bool& have_fbx_region);

private:

    fpreal myStartTime;
    ROP_FBXExporter* myParentExporter;
    FbxManager* mySDKManager;
    FbxScene* myScene;
    ROP_FBXErrorManager* myErrorManager;
    ROP_FBXNodeManager* myNodeManager;
    ROP_FBXActionManager* myActionManager; 

    THdFbxMaterialMap myMaterialsMap;
    THdFbxTextureMap myTexturesMap;
    FbxSurfaceMaterial* myDefaultMaterial;
    FbxTexture* myDefaultTexture;

    UT_Color myAmbientColor;
    UT_Interrupt* myBoss;

    ROP_FBXCreateInstancesAction* myInstancesActionPtr;
};
/********************************************************************************************************/
#endif
