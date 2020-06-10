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
 * COMMENTS:	FBX output
 *
 */

#ifndef __ROP_FBXMainVisitor_h__
#define __ROP_FBXMainVisitor_h__

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"

#include <GA/GA_OffsetList.h>
#include <UT/UT_Color.h>
#include <UT/UT_Array.h>
#include <UT/UT_Assert.h>
#include <UT/UT_String.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_Set.h>

#include <map>
#include <string>
#include <vector>

class ROP_FBXActionManager;
class ROP_FBXCreateInstancesAction;
class ROP_FBXErrorManager;
class ROP_FBXExporter;
class ROP_FBXGDPCache;
class ROP_FBXGDPCache;
class ROP_FBXNodeManager;

class OBJ_Camera;
class OBJ_Node;
class GU_Detail;
class GU_PrimNURBCurve;
class GU_PrimNURBSurf;
class GEO_Primitive;
class GD_TrimRegion;
class GA_Attribute;
class GA_ROAttributeRef;
namespace GA_PrimCompat { class TypeMask; }
class UT_Interrupt;
class UT_StringRef;
class SOP_Node;
class OP_Node;

/********************************************************************************************************/
enum ROP_FBXAttributeType
{
    ROP_FBXAttributeNormal = 0,
    ROP_FBXAttributeTangent,
    ROP_FBXAttributeBinormal,
    ROP_FBXAttributeUV,
    ROP_FBXAttributeVertexColor,
    ROP_FBXAttributeUser,

    ROP_FBXAttributeLastPlaceholder
};
/********************************************************************************************************/
typedef UT_Array < const GA_Attribute* > THDAttributeVector;
typedef std::map < OP_Node* , FbxSurfaceMaterial* > THdFbxMaterialMap;
typedef std::map < std::string, FbxSurfaceMaterial* > THdFbxStringMaterialMap;
typedef std::map < OP_Node* , int > THdNodeIntMap;
typedef std::map < std::string, int > THdFbxStringIntMap;
//typedef set < OP_Node* > THdNodeSet;
typedef std::map < std::string , FbxTexture* > THdFbxTextureMap;
typedef std::vector < FbxLayerElementTexture* > TFbxLayerElemsVector;
//typedef std::vector < FbxNode* > TFbxNodesVector;
/********************************************************************************************************/
class ROP_FBXAttributeLayerManager
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
class ROP_FBXMainNodeVisitInfo : public ROP_FBXBaseNodeVisitInfo
{
public:
    ROP_FBXMainNodeVisitInfo(OP_Node* hd_node);
    ~ROP_FBXMainNodeVisitInfo() override;

    double getBoneLength(void);
    void setBoneLength(double b_length);

    bool getIsVisitingFromInstance(void);
    void setIsVisitingFromInstance(bool value);

private:

    bool myIsVisitingFromInstance;
    double myBoneLength;
};
/********************************************************************************************************/
class ROP_FBXConstructionInfo
{
public:
    ROP_FBXConstructionInfo(FbxNode* fbx_node) : myNode(fbx_node) { }

    void setHdPrimitiveIndex(int prim_cnt) { myHdPrimCnt = prim_cnt; }
    int getHdPrimitiveIndex(void) { return myHdPrimCnt; }

    FbxNode* getFbxNode(void) { return myNode; }

    const UT_StringHolder& getPathValue() const { return myPathValue; }
    void setPathValue(const UT_StringHolder &path) { myPathValue = path; }

    bool getNeedMaterialExport() const { return myNeedMaterialExport; }
    void setNeedMaterialExport(bool need) { myNeedMaterialExport = need; }

    bool getExportObjTransform() const { return myExportObjTransform; }
    void setExportObjTransform(bool flag) { myExportObjTransform = flag; }

    bool getHasPrimTransform() const { return myHasPrimTransform; }
    void setHasPrimTransform(bool flag) { myHasPrimTransform = flag; }

private:
    FbxNode* myNode = nullptr;
    UT_StringHolder myPathValue;
    int myHdPrimCnt = -1;
    bool myNeedMaterialExport = true;
    bool myExportObjTransform = true;
    bool myHasPrimTransform = false;
};
typedef std::vector < ROP_FBXConstructionInfo > TFbxNodesVector;
/********************************************************************************************************/
class ROP_FBXMainVisitor : public ROP_FBXBaseVisitor
{
public:
    ROP_FBXMainVisitor(ROP_FBXExporter* parent_exporter);
    ~ROP_FBXMainVisitor() override;

    ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node, int input_idx_on_this_node) override;
    ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info) override;
    void onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info) override;

    UT_Color getAccumAmbientColor(void);
    ROP_FBXCreateInstancesAction* getCreateInstancesAction(void);

private:

    // Given a gdp pointer, this will return a pointer to a gdp which consists of
    // only supported primitives for export. This may or may not be a converted geo.
    // The conversion_spare parm is the object which may be used for conversion - 
    // the idea is to avoid dynamically allocating one and avoiding ambiguity 
    // regarding whether the returned pointer needs to be deleted or not. 
    // In this case, it doesn't ever need to be deleted.
    const GU_Detail* getExportableGeo(const GU_Detail* gdp_orig, GU_Detail& conversion_spare,
                                      GA_PrimCompat::TypeMask &prim_types_in_out);

    void setProperName(FbxLayerElement* fbx_layer_elem, const GU_Detail* gdp, const GA_Attribute* attr);
    bool outputGeoNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, ROP_FBXGDPCache* &v_cache_out, bool& did_cancel_out, TFbxNodesVector& res_nodes);
    bool outputShapePrimitives(
            const UT_Matrix4D& parent_xform,
            SOP_Node* sop_node,
            const char* node_name,
            const UT_StringHolder& path_value,
            const GU_Detail* gdp,
            const GA_OffsetList& prims,
            TFbxNodesVector& res_nodes);
    bool outputSOPNodeByPath(
            FbxNode* fbx_root,
            const UT_StringRef& path_attrib_name,
            OBJ_Node* obj_node,
            SOP_Node* sop_node,
            ROP_FBXMainNodeVisitInfo* node_info,
            ROP_FBXGDPCache *&v_cache_out,
            bool& did_cancel_out,
            TFbxNodesVector& res_nodes);
    bool outputSOPNodeWithVC(SOP_Node* node, const UT_String& node_name, ROP_FBXMainNodeVisitInfo* node_info, ROP_FBXGDPCache *&v_cache_out, bool& did_cancel_out, TFbxNodesVector& res_nodes);
    bool outputSOPNodeWithoutVC(SOP_Node* node, const UT_String& node_name, OP_Node* skin_deform_node, bool& did_cancel_out, TFbxNodesVector& res_nodes);
    bool outputNullNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes);
    bool outputLightNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes);
    bool outputCameraNode(OP_Node* node,
                          OBJ_Camera* cam,
                          ROP_FBXMainNodeVisitInfo* node_info,
                          FbxNode* parent_node,
                          TFbxNodesVector& res_nodes);
    bool outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, bool is_a_null, TFbxNodesVector& res_nodes);
    void outputNURBSSurfaces(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputNURBSCurves(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputPolylines(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes);
    void outputBezierCurves(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    void outputBezierSurfaces(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr = NULL);
    bool outputLODGroupNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes);

    void outputSingleNURBSSurface(const GU_PrimNURBSurf* hd_nurb, const char* curr_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes, int prim_cnt);

    int createTexturesForMaterial(OP_Node* mat_node, FbxSurfaceMaterial* fbx_material, THdFbxTextureMap& tex_map);

    void outputPolygons(const GU_Detail* gdp, const char* node_name, int max_points, ROP_FBXVertexCacheMethodType vc_method, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes);
    void outputNURBSSurface(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes);
    void addUserData(const GU_Detail* gdp, THDAttributeVector& hd_attribs, ROP_FBXAttributeLayerManager& attr_manager, FbxMesh* mesh_attr, FbxLayerElement::EMappingMode mapping_mode );

    void exportAttributes(const GU_Detail* gdp, FbxMesh* mesh_attr);
    void exportMaterials(OP_Node* source_node, FbxNode* fbx_node, const GU_Detail *mat_gdp = nullptr);

    FbxSurfaceMaterial* generateFbxMaterial(OP_Node* mat_node, THdFbxMaterialMap& mat_map);
    FbxSurfaceMaterial* generateFbxMaterial(const char * mat_string, THdFbxStringMaterialMap& mat_map);
    FbxSurfaceMaterial* getDefaultMaterial(THdFbxMaterialMap& mat_map);
    FbxTexture* getDefaultTexture(THdFbxTextureMap& tex_map);
    OP_Node* getSurfaceNodeFromMaterialNode(OP_Node* material_node);
    FbxTexture* generateFbxTexture(OP_Node* mat_node, int texture_idx, UT_StringRef text_parm_name, THdFbxTextureMap& tex_map);
    bool isTexturePresent(OP_Node* mat_node, UT_StringRef text_parm_name, UT_String* texture_path_out);

    ROP_FBXAttributeType getAttrTypeByName(const GU_Detail* gdp, const char* attr_name);
    FbxLayerElement* getAndSetFBXLayerElement(FbxLayer* attr_layer, ROP_FBXAttributeType attr_type, 
	const GU_Detail* gdp, const GA_ROAttributeRef &attr_offset, const GA_ROAttributeRef &extra_attr_offset,  FbxLayerElement::EMappingMode mapping_mode, FbxLayerContainer* layer_container);

    void setFbxNodeVisibility(FbxNode &node, OP_Node *hd_node, bool visible);

    void finalizeNewNode(ROP_FBXConstructionInfo& constr_info, OP_Node* hd_node, ROP_FBXMainNodeVisitInfo *node_info, FbxNode* fbx_parent_node, 
	const UT_StringRef& override_node_type, const char* lookat_parm_name, ROP_FBXVisitorResultType res_type, 
	ROP_FBXGDPCache *v_cache, bool is_visible);
    void finalizeGeoNode(FbxNodeAttribute *res_attr, OP_Node* skin_deform_node, int capture_frame, int opt_prim_cnt, TFbxNodesVector& res_nodes);

    void exportFBXTransform(fpreal t, const OBJ_Node *hd_node, FbxNode* fbx_node);

    void setNURBSSurfaceInfo(FbxNurbsSurface *nurbs_surf_attr, const GU_PrimNURBSurf* hd_nurb);
    void setNURBSCurveInfo(FbxNurbsCurve* nurbs_curve_attr, const GU_PrimNURBCurve* hd_nurb);
    void setTrimRegionInfo(GD_TrimRegion* region, FbxTrimNurbsSurface *trim_nurbs_surf_attr, bool& have_fbx_region);

    bool outputBlendShapesNodesIn(OP_Node* node, const UT_String& node_name, OP_Node* skin_deform_node, bool& did_cancel_out, TFbxNodesVector& res_nodes, UT_Set<OP_Node*> *already_visited, ROP_FBXMainNodeVisitInfo* node_info);
    bool outputBlendShapeNode(OP_Node* node, const UT_String& node_name, OP_Node* skin_deform_node, bool& did_cancel_out, TFbxNodesVector& res_nodes, ROP_FBXMainNodeVisitInfo* node_info);
    bool outputSOPNodeToShape(SOP_Node* node, const char* node_name, FbxBlendShapeChannel* fbx_blend_shape_channel, const float& blend_percent, TFbxNodesVector& res_nodes);
    bool outputSequenceBlendNode(SOP_Node* seq_blend_node, const char* node_name, FbxBlendShapeChannel* fbx_blend_shape_channel, TFbxNodesVector& res_nodes);

private:

    fpreal myStartTime;
    ROP_FBXExporter* myParentExporter;
    FbxManager* mySDKManager;
    FbxScene* myScene;
    ROP_FBXErrorManager* myErrorManager;
    ROP_FBXNodeManager* myNodeManager;
    ROP_FBXActionManager* myActionManager; 

    THdFbxMaterialMap myMaterialsMap;
    THdFbxStringMaterialMap myStringMaterialMap;
    THdFbxTextureMap myTexturesMap;
    FbxSurfaceMaterial* myDefaultMaterial;
    FbxTexture* myDefaultTexture;

    UT_Color myAmbientColor;
    UT_Interrupt* myBoss;

    ROP_FBXCreateInstancesAction* myInstancesActionPtr;
};
/********************************************************************************************************/
#endif
