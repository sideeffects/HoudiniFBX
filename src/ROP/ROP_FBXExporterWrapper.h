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

#ifndef __ROP_FBXExporterWrapper_h__
#define __ROP_FBXExporterWrapper_h__

#include "ROP_API.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXErrorManager.h"

#ifdef FBX_SUPPORTED

class ROP_FBXExporter;
/********************************************************************************************************/
class ROP_API ROP_FBXExporterWrapper
{
public:
    ROP_FBXExporterWrapper();
    ~ROP_FBXExporterWrapper();

    // These are responsible for the actual conversion process.
    bool initializeExport(const char* output_name, float tstart, float tend, ROP_FBXExportOptions* options);
    void doExport(void);
    bool finishExport(void);

    ROP_FBXErrorManager* getErrorManager(void);
    static bool isSupported(void);

private:

    ROP_FBXExporter* myFBXExporter;
};
/********************************************************************************************************/
#else // FBX_SUPPORTED

#define NULL 0

class ROP_API ROP_FBXExporterWrapper
{
public:
    ROP_FBXExporterWrapper() { }
    ~ROP_FBXExporterWrapper() { }

    // These are responsible for the actual conversion process.
    bool initializeExport(const char* output_name, float tstart, float tend, ROP_FBXExportOptions* options) { return false; }
    void doExport(void) {  }
    bool finishExport(void) { return false; }

    ROP_FBXErrorManager* getErrorManager(void) { return NULL; }
    static bool isSupported(void) { return false; }
};

#endif // FBX_SUPPORTED
/********************************************************************************************************/
#endif
