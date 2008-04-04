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
#include <fbx/fbxsdk.h>
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseVisitor.h"

class ROP_FBXExporter;
class GU_Detail;
class GB_Attribute;
class ROP_FBXErrorManager;
class ROP_FBXGDPCache;
class ROP_FBXNodeManager;
class ROP_FBXActionManager;
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
typedef map < string , KFbxTexture* > THdFbxTextureMap;
typedef vector < KFbxLayerElementTexture* > TFbxLayerElemsVector;
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

private:

    double myBoneLength;
};
/********************************************************************************************************/
class ROP_API ROP_FBXMainVisitor : public ROP_FBXBaseVisitor
{
public:
    ROP_FBXMainVisitor(ROP_FBXExporter* parent_exporter);
    virtual ~ROP_FBXMainVisitor();

    ROP_FBXBaseNodeVisitInfo* visitBegin(OP_Node* node);
    ROP_FBXVisitorResultType visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info);
    void onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info);

    UT_Color getAccumAmbientColor(void);

protected:
    KFbxNode* outputGeoNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, ROP_FBXGDPCache* &v_cache_out);
    KFbxNode* outputNullNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node);
    KFbxNode* outputLightNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node);
    KFbxNode* outputCameraNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node);
    KFbxNode* outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, bool is_a_null);

    KFbxNodeAttribute* outputPolygons(const GU_Detail* gdp, const char* node_name, int max_points, ROP_FBXVertexCacheMethodType vc_method);
    void addUserData(const GU_Detail* gdp, THDAttributeVector& hd_attribs, ROP_FBXAttributeLayerManager& attr_manager, KFbxMesh* mesh_attr, KFbxLayerElement::EMappingMode mapping_mode );

    void exportAttributes(const GU_Detail* gdp, KFbxMesh* mesh_attr);
    void exportMaterials(OP_Node* source_node, KFbxNode* fbx_node);

    KFbxSurfaceMaterial* generateFbxMaterial(OP_Node* mat_node, THdFbxMaterialMap& mat_map);
    KFbxSurfaceMaterial* getDefaultMaterial(THdFbxMaterialMap& mat_map);
    KFbxTexture* getDefaultTexture(THdFbxTextureMap& tex_map);
    OP_Node* getSurfaceNodeFromMaterialNode(OP_Node* material_node);
    KFbxTexture* generateFbxTexture(OP_Node* mat_node, int texture_idx, THdFbxTextureMap& tex_map);

    ROP_FBXAttributeType getAttrTypeByName(const GU_Detail* gdp, const char* attr_name);
    KFbxLayerElement* getAndSetFBXLayerElement(KFbxLayer* attr_layer, ROP_FBXAttributeType attr_type, 
	const GU_Detail* gdp, int attr_offset, KFbxLayerElement::EMappingMode mapping_mode);

private:

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
};
/********************************************************************************************************/
#endif
