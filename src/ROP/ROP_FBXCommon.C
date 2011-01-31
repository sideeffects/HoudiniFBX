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
#include <UT/UT_String.h>

using namespace std;

/********************************************************************************************************/
ROP_FBXExportOptions::ROP_FBXExportOptions()
{
    reset();
}
/********************************************************************************************************/
ROP_FBXExportOptions::~ROP_FBXExportOptions()
{

}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::reset(void)
{
    myResampleAllAnimation = false;
    myResampleIntervalInFrames = 1.0;
    myExportInAscii = false;
    myVertexCacheFormat = ROP_FBXVertexCacheExportFormatMaya;
    myStartNodePath = "/obj";
    myDetectConstantPointCountObjects = true;
    myPolyConvertLOD = 1.0;
    myExportDeformsAsVC = false;
    myExportTakeName = "";
    myInvisibleObjectsExportType = ROP_FBXInvisibleNodeExportAsNulls;
    myConvertSurfaces = false;
    mySdkVersion = "";
    myBundleNames = "";

    mySaveMemory = false;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setSaveMemory(bool value)
{
    mySaveMemory = value;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getSaveMemory(void)
{
    return mySaveMemory;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getResampleAllAnimation(void)
{
    return myResampleAllAnimation;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setResampleAllAnimation(bool value)
{
    myResampleAllAnimation = value;
}
/********************************************************************************************************/
fpreal 
ROP_FBXExportOptions::getResampleIntervalInFrames(void)
{
    return myResampleIntervalInFrames;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setResampleIntervalInFrames(fpreal frames)
{
    myResampleIntervalInFrames = frames;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setVertexCacheFormat(ROP_FBXVertexCacheExportFormatType format_type)
{
    myVertexCacheFormat = format_type;
}
/********************************************************************************************************/
ROP_FBXVertexCacheExportFormatType 
ROP_FBXExportOptions::getVertexCacheFormat(void)
{
    return myVertexCacheFormat;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getExportInAscii(void)
{
    return myExportInAscii;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setExportInAscii(bool value)
{
    myExportInAscii = value;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setStartNodePath(const char* node_path, bool autohandle_bundles)
{
    if(!node_path)
	return;

    myStartNodePath = node_path;

    if(autohandle_bundles)
    {
	UT_String str_temp(UT_String::ALWAYS_DEEP, node_path);
	str_temp.trimSpace();
	
	if(str_temp[0] == '@')
	{
	    // Bundles. 
	    myStartNodePath = "";
	    setBundlesString(node_path);
	}
    }
}
/********************************************************************************************************/
const char* 
ROP_FBXExportOptions::getStartNodePath(void)
{
    return myStartNodePath.c_str();
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getDetectConstantPointCountObjects(void)
{
    return myDetectConstantPointCountObjects;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setDetectConstantPointCountObjects(bool value)
{
    myDetectConstantPointCountObjects = value;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setPolyConvertLOD(float lod)
{
    myPolyConvertLOD = lod;
    if(myPolyConvertLOD <= 0.0)
	myPolyConvertLOD = 1.0;
}
/********************************************************************************************************/
float 
ROP_FBXExportOptions::getPolyConvertLOD(void)
{
    return myPolyConvertLOD;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getExportDeformsAsVC(void)
{
    return myExportDeformsAsVC;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setExportDeformsAsVC(bool value)
{
    myExportDeformsAsVC = value;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setExportTakeName(const char* pcsName)
{
    if(pcsName)
	myExportTakeName = pcsName;
    else
	myExportTakeName = "";
}
/********************************************************************************************************/
const char* 
ROP_FBXExportOptions::getExportTakeName(void)
{
    return myExportTakeName.c_str();
}
/********************************************************************************************************/
ROP_FBXInvisibleNodeExportType 
ROP_FBXExportOptions::getInvisibleNodeExportMethod(void)
{
    return myInvisibleObjectsExportType;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setInvisibleNodeExportMethod(ROP_FBXInvisibleNodeExportType exp_type)
{
    myInvisibleObjectsExportType = exp_type;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setConvertSurfaces(bool value)
{
    myConvertSurfaces = value;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getConvertSurfaces(void)
{
    return myConvertSurfaces;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setVersion(const char* sdk_version)
{
    if(sdk_version)
    {
	if(strcmp(sdk_version, "(Current)") == 0)
	    mySdkVersion = "";
	else
	    mySdkVersion = sdk_version;
    }
    else
	mySdkVersion = "";
}
/********************************************************************************************************/
const char* 
ROP_FBXExportOptions::getVersion(void)
{
    return mySdkVersion.c_str();
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setBundlesString(const char* bundles)
{
    myBundleNames = bundles;
}
/********************************************************************************************************/
const char* 
ROP_FBXExportOptions::getBundlesString(void)
{
    return myBundleNames.c_str();
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::isExportingBundles(void)
{
    if(myBundleNames.length() > 0)
	return true;
    else
	return false;
}
/********************************************************************************************************/
