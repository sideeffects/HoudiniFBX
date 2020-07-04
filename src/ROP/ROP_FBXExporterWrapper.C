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
 * COMMENTS:	Class for FBX output.
 *
 */

#ifdef FBX_ENABLED

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXActionManager.h"
#include "ROP_FBXExporter.h"
#include "ROP_FBXExporterWrapper.h"

using namespace std;

/********************************************************************************************************/
ROP_FBXExporterWrapper::ROP_FBXExporterWrapper()
{
    myFBXExporter = new ROP_FBXExporter();
}
/********************************************************************************************************/
ROP_FBXExporterWrapper::~ROP_FBXExporterWrapper()
{
    delete myFBXExporter;
    myFBXExporter = NULL;
}
/********************************************************************************************************/
bool 
ROP_FBXExporterWrapper::initializeExport(const char* output_name, fpreal tstart, fpreal tend, ROP_FBXExportOptions* options)
{
    if(myFBXExporter)
	return myFBXExporter->initializeExport(output_name, tstart, tend, options);
    else
    {
	if(this->getErrorManager())
	    this->getErrorManager()->addError("FBX export is not supported in Houdini Apprentice.", true);
	return false;
    }
}
/********************************************************************************************************/
void 
ROP_FBXExporterWrapper::doExport()
{
    if(myFBXExporter)
	myFBXExporter->doExport();
}
/********************************************************************************************************/
bool 
ROP_FBXExporterWrapper::finishExport()
{
    if(myFBXExporter)
	return myFBXExporter->finishExport();
    else
	return false;
}
/********************************************************************************************************/
ROP_FBXErrorManager* 
ROP_FBXExporterWrapper::getErrorManager()
{
    return myFBXExporter->getErrorManager();
}
/********************************************************************************************************/
void
ROP_FBXExporterWrapper::getVersions(TStringVector& versions_out)
{
    return ROP_FBXExporter::getVersions(versions_out);
}
/********************************************************************************************************/
#endif // FBX_ENABLED
