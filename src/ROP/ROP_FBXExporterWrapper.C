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

#ifdef FBX_SUPPORTED

#include "ROP_FBXHeaderWrapper.h"
#include <UT/UT_Taint.h>
#include "ROP_FBXActionManager.h"
#include "ROP_FBXExporter.h"
#include "ROP_FBXExporterWrapper.h"

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
    if(ROP_FBXExporterWrapper::isSupported())
	return myFBXExporter->initializeExport(output_name, tstart, tend, options);
    else
    {
	if(this->getErrorManager())
	    this->getErrorManager()->addError("FBX Export is only supported in Escape and Master versions.", true);
	return false;
    }
}
/********************************************************************************************************/
void 
ROP_FBXExporterWrapper::doExport(void)
{
    if(ROP_FBXExporterWrapper::isSupported())
	myFBXExporter->doExport();
}
/********************************************************************************************************/
bool 
ROP_FBXExporterWrapper::finishExport(void)
{
    if(ROP_FBXExporterWrapper::isSupported())
	return myFBXExporter->finishExport();
    else
	return false;
}
/********************************************************************************************************/
ROP_FBXErrorManager* 
ROP_FBXExporterWrapper::getErrorManager(void)
{
    return myFBXExporter->getErrorManager();
}
/********************************************************************************************************/
UT_String* 
ROP_FBXExporterWrapper::getVersions(void)
{
    return ROP_FBXExporter::getVersions();
}
/********************************************************************************************************/
bool 
ROP_FBXExporterWrapper::isSupported(void) 
{ 
    // FBX export is only present in complete versions of Houdini (and in
    // the educational versions, as well):
    if (!UTisTainted()) //  && !UTignoreTaint())
	return true; 
    else
	return false;
}
/********************************************************************************************************/
#endif // FBX_SUPPORTED
