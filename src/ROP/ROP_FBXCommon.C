/*
 * Copyright (c) 2022
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
#include <UT/UT_String.h>


/********************************************************************************************************/
void 
ROP_FBXExportOptions::setSaveMemory(bool value)
{
    mySaveMemory = value;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getSaveMemory()
{
    return mySaveMemory;
}
/********************************************************************************************************/
void
ROP_FBXExportOptions::setForceBlendShapeExport(bool value)
{
    myForceBlendShapeExport = value;
}
/********************************************************************************************************/
bool
ROP_FBXExportOptions::getForceBlendShapeExport()
{
    return myForceBlendShapeExport;
}
/********************************************************************************************************/
void
ROP_FBXExportOptions::setForceSkinDeformExport(bool value)
{
    myForceSkinDeformExport = value;
}
/********************************************************************************************************/
bool
ROP_FBXExportOptions::getForceSkinDeformExport()
{
    return myForceSkinDeformExport;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getResampleAllAnimation()
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
ROP_FBXExportOptions::getResampleIntervalInFrames()
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
ROP_FBXExportOptions::getVertexCacheFormat()
{
    return myVertexCacheFormat;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getExportInAscii()
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
ROP_FBXExportOptions::getStartNodePath()
{
    return myStartNodePath.c_str();
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getDetectConstantPointCountObjects()
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
ROP_FBXExportOptions::getPolyConvertLOD()
{
    return myPolyConvertLOD;
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::getExportDeformsAsVC()
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
ROP_FBXExportOptions::getExportTakeName()
{
    return myExportTakeName.c_str();
}
/********************************************************************************************************/
ROP_FBXInvisibleNodeExportType 
ROP_FBXExportOptions::getInvisibleNodeExportMethod()
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
ROP_FBXExportOptions::getConvertSurfaces()
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
ROP_FBXExportOptions::getVersion()
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
ROP_FBXExportOptions::getBundlesString()
{
    return myBundleNames.c_str();
}
/********************************************************************************************************/
bool 
ROP_FBXExportOptions::isExportingBundles()
{
    if(myBundleNames.length() > 0)
	return true;
    else
	return false;
}
/********************************************************************************************************/
bool
ROP_FBXExportOptions::isSopExport()
{
    return mySopExport;
}
/********************************************************************************************************/
void
ROP_FBXExportOptions::setSopExport(const bool& sopexport)
{
    mySopExport = sopexport;
}
/********************************************************************************************************/
bool
ROP_FBXExportOptions::getExportBonesEndEffectors()
{
    return myExportBonesEndEffectors;
}
/********************************************************************************************************/
void
ROP_FBXExportOptions::setExportBonesEndEffectors(const bool& export_end_effectors)
{
    myExportBonesEndEffectors = export_end_effectors;
}
/********************************************************************************************************/
bool
ROP_FBXExportOptions::getEmbedMedia()
{
    return myEmbedMedia;
}
/********************************************************************************************************/
void
ROP_FBXExportOptions::setEmbedMedia(const bool& embed_media)
{
    myEmbedMedia = embed_media;
}
/********************************************************************************************************/
bool
ROP_FBXExportOptions::getComputeSmoothingGroups()
{
    return myComputeSmoothingGroups;
}
/********************************************************************************************************/
void
ROP_FBXExportOptions::setComputeSmoothingGroups(const bool& compute_smoothing_groups)
{
    myComputeSmoothingGroups = compute_smoothing_groups;
}
/********************************************************************************************************/
void
ROP_FBXExportOptions::appendExportClip(ROP_FBXExportClip clip)
{
    myExportClips.append(clip);
}
/********************************************************************************************************/
ROP_FBXExportClip
ROP_FBXExportOptions::getExportClip(int index)
{
    return myExportClips[index];
}
/********************************************************************************************************/
int 
ROP_FBXExportOptions::getNumExportClips()
{
    return myExportClips.size();
}
/********************************************************************************************************/
