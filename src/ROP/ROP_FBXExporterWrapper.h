/*
 * Copyright (c) 2017
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
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

#ifndef __ROP_FBXExporterWrapper_h__
#define __ROP_FBXExporterWrapper_h__

#include "ROP_FBXCommon.h"
#include "ROP_FBXErrorManager.h"

#ifdef FBX_ENABLED

class ROP_FBXExporter;
/********************************************************************************************************/
class ROP_FBXExporterWrapper
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
    static void getVersions(TStringVector& versions_out);

private:

    ROP_FBXExporter* myFBXExporter;
};
/********************************************************************************************************/
#else // FBX_ENABLED

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

    static void getVersions(TStringVector& versions_out) { }
};

#endif // FBX_ENABLED
/********************************************************************************************************/
#endif
