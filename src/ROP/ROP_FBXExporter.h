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

#include "ROP_FBXErrorManager.h"

#ifdef UT_DEBUG
#include <UT/UT_Debug.h>
#endif

#include <vector>
#include <string>

class ROP_FBXNodeManager;
class ROP_FBXActionManager;
class UT_Interrupt;

typedef std::vector < char* > TCharPtrVector;
/********************************************************************************************************/
// Note: When adding public members, make sure to add an equivalent to dummy exporter for cases when FBX is
// disabled.
class ROP_API ROP_FBXExporter
{
public:
    ROP_FBXExporter();
    ~ROP_FBXExporter();

    // These are responsible for the actual conversion process.
    bool initializeExport(const char* output_name, fpreal tstart, fpreal tend, ROP_FBXExportOptions* options);
    void doExport(void);
    bool finishExport(void);

    FbxManager* getSDKManager(void);
    FbxScene* getFBXScene(void);
    ROP_FBXErrorManager* getErrorManager(void);
    ROP_FBXNodeManager* getNodeManager(void);
    ROP_FBXActionManager* getActionManager(void);

    ROP_FBXExportOptions* getExportOptions(void);
    const char* getOutputFileName(void);

    fpreal getStartTime(void);
    fpreal getEndTime(void);
    bool getExportingAnimation(void);

    void queueStringToDeallocate(char* string_ptr);
    FbxNode* getFBXRootNode(OP_Node* asking_node, bool create_subnet_root);
    UT_Interrupt* GetBoss(void);

    static void getVersions(TStringVector& versions_out);

private:
    void deallocateQueuedStrings(void);

private:

    ROP_FBXExportOptions myExportOptions;

    FbxManager* mySDKManager;
    FbxScene* myScene;
    ROP_FBXErrorManager* myErrorManager;
    ROP_FBXNodeManager* myNodeManager;
    ROP_FBXActionManager* myActionManager;

    std::string myOutputFile;

    fpreal myStartTime, myEndTime;

    TCharPtrVector myStringsToDeallocate;
    FbxNode* myDummyRootNullNode;

    UT_Interrupt	*myBoss;
    bool myDidCancel;

#ifdef UT_DEBUG
    // Timing variables
    double myDBStartTime, myDBEndTime;
#endif
};
/********************************************************************************************************/
#endif
