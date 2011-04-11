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
    /// This function must be called before doExport() is to initialize the exporter.
    /// @param	output_name	The output name of the FBX file. Please note that network (UNC) paths are 
    ///				currently not supported.
    /// @param	tstart		Export start time, in seconds.
    /// @param	tend		Export end time, in seconds.
    /// @param	options		An optional set of export options. If not provided (NULL), defaults are used.
    /// @return	True if successful, false on failure.
    bool initializeExport(const char* output_name, fpreal tstart, fpreal tend, ROP_FBXExportOptions* options);

    /// Performs the actual export process. ROP_FBXExporterWrapper::initializeExport() must be called first.
    void doExport(void);

    /// This function cleans up after the export is done. It must be called after the 
    /// ROP_FBXExporterWrapper::doExport() function.
    bool finishExport(void);

    /// Retrieves the error manager for this wrapper.
    ROP_FBXErrorManager* getErrorManager(void);

    /// Returns true if FBX is supported in the current Houdini build, false otherwise.
    static bool isSupported(void);
    static void getVersions(TStringVector& versions_out);

private:

    ROP_FBXExporter* myFBXExporter;
};
/********************************************************************************************************/
#else // FBX_SUPPORTED

#include <stddef.h>

class ROP_API ROP_FBXExporterWrapper
{
public:
    ROP_FBXExporterWrapper() { }
    ~ROP_FBXExporterWrapper() { }

    // These are responsible for the actual conversion process.
    /// This function must be called before doExport() is to initialize the exporter.
    /// @param	output_name	The output name of the FBX file. Please note that network (UNC) paths are 
    ///				currently not supported.
    /// @param	tstart		Export start time, in seconds.
    /// @param	tend		Export end time, in seconds.
    /// @param	options		An optional set of export options. If not provided (NULL), defaults are used.
    /// @return	True if successful, false on failure.
    bool initializeExport(const char* output_name, fpreal tstart, fpreal tend, ROP_FBXExportOptions* options) { return false; }

    /// Performs the actual export process. ROP_FBXExporterWrapper::initializeExport() must be called first.
    void doExport(void) {  }

    /// This function cleans up after the export is done. It must be called after the 
    /// ROP_FBXExporterWrapper::doExport() function.
    bool finishExport(void) { return false; }

    /// Retrieves the error manager for this wrapper.
    ROP_FBXErrorManager* getErrorManager(void) { return NULL; }

    /// Returns true if FBX is supported in the current Houdini build, false otherwise.
    static bool isSupported(void) { return false; }
    static void getVersions(TStringVector& versions_out) { }
};

#endif // FBX_SUPPORTED
/********************************************************************************************************/
#endif
