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
float 
ROP_FBXExportOptions::getResampleIntervalInFrames(void)
{
    return myResampleIntervalInFrames;
}
/********************************************************************************************************/
void 
ROP_FBXExportOptions::setResampleIntervalInFrames(float frames)
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
ROP_FBXExportOptions::setStartNodePath(const char* node_path)
{
    if(node_path)
	myStartNodePath = node_path;
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