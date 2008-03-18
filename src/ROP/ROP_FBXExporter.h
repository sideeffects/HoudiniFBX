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

#ifndef __ROP_FBXExporter_h__
#define __ROP_FBXExporter_h__

#include "ROP_API.h"
#include "ROP_FBXCommon.h"

#ifdef FBX_SUPPORTED

#include "ROP_FBXErrorManager.h"
#include "ROP_FBXActionManager.h"

#include <fbx/fbxsdk.h>

#ifdef UT_DEBUG
#include <UT/UT_Debug.h>
#endif

class FBX_FILMBOX_NAMESPACE::KFbxSdkManager;
class FBX_FILMBOX_NAMESPACE::KFbxScene;
class ROP_FBXNodeManager;
class ROP_FBXActionManager;
/********************************************************************************************************/
// Note: When adding public members, make sure to add an equivalent to dummy exporter for cases when FBX is
// disabled.
class ROP_API ROP_FBXExporter
{
public:
    ROP_FBXExporter();
    ~ROP_FBXExporter();

    // These are responsible for the actual conversion process.
    bool initializeExport(const char* output_name, float tstart, float tend, ROP_FBXExportOptions* options);
    void doExport(void);
    bool finishExport(void);

    KFbxSdkManager* getSDKManager(void);
    KFbxScene* getFBXScene(void);
    ROP_FBXErrorManager* getErrorManager(void);
    ROP_FBXNodeManager* getNodeManager(void);
    ROP_FBXActionManager* getActionManager(void);

    ROP_FBXExportOptions* getExportOptions(void);
    const char* getOutputFileName(void);

    float getStartTime(void);
    float getEndTime(void);
    bool getExportingAnimation(void);

private:

    ROP_FBXExportOptions myExportOptions;

    KFbxSdkManager* mySDKManager;
    KFbxScene* myScene;
    ROP_FBXErrorManager* myErrorManager;
    ROP_FBXNodeManager* myNodeManager;
    ROP_FBXActionManager* myActionManager;

    string myOutputFile;

    float myStartTime, myEndTime;

#ifdef UT_DEBUG
    // Timing variables
    double myDBStartTime, myDBEndTime;
#endif
};
/********************************************************************************************************/
#else // FBX_SUPPORTED

class KFbxSdkManager;
class KFbxScene;
class ROP_FBXNodeManager;
class ROP_FBXActionManager;
class ROP_FBXExportOptions;
class ROP_FBXErrorManager;

#define NULL 0

class ROP_API ROP_FBXExporter
{
public:
    ROP_FBXExporter() { }
    ~ROP_FBXExporter() { }

    // These are responsible for the actual conversion process.
    bool initializeExport(const char* output_name, float tstart, float tend, ROP_FBXExportOptions* options) { return false; }
    void doExport(void) { }
    bool finishExport(void) { return false; }

    KFbxSdkManager* getSDKManager(void) { return NULL; }
    KFbxScene* getFBXScene(void) { return NULL; }
    ROP_FBXErrorManager* getErrorManager(void) { return NULL; }
    ROP_FBXNodeManager* getNodeManager(void) { return NULL; }
    ROP_FBXActionManager* getActionManager(void) { return NULL; }

    ROP_FBXExportOptions* getExportOptions(void) { return NULL; }
    const char* getOutputFileName(void) { return NULL; }

    float getStartTime(void) { return 0; }
    float getEndTime(void) { return 0; }
    bool getExportingAnimation(void) { return false; }
};

#endif // FBX_SUPPORTED
/********************************************************************************************************/
#endif
