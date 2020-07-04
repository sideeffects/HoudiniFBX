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

#ifndef __ROP_FBXExporter_h__
#define __ROP_FBXExporter_h__

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
class ROP_FBXExporter
{
public:
    ROP_FBXExporter();
    ~ROP_FBXExporter();

    // These are responsible for the actual conversion process.
    bool initializeExport(const char* output_name, fpreal tstart, fpreal tend, ROP_FBXExportOptions* options);
    void doExport();
    bool finishExport();

    FbxManager* getSDKManager();
    FbxScene* getFBXScene();
    ROP_FBXErrorManager* getErrorManager();
    ROP_FBXNodeManager* getNodeManager();
    ROP_FBXActionManager* getActionManager();

    ROP_FBXExportOptions* getExportOptions();
    const char* getOutputFileName();

    fpreal getStartTime();
    fpreal getEndTime();
    bool getExportingAnimation();

    void queueStringToDeallocate(char* string_ptr);
    FbxNode* getFBXRootNode(OP_Node* asking_node, bool create_subnet_root);
    UT_Interrupt* GetBoss();

    static void getVersions(TStringVector& versions_out);

private:
    void deallocateQueuedStrings();

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
