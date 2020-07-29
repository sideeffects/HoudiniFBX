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

#ifndef __ROP_FBX_h__
#define __ROP_FBX_h__

#include <GU/GU_DetailHandle.h>
#include <ROP/ROP_Node.h>

#include "ROP_FBXExporterWrapper.h"

#define FBX_FLOAT_PARM(name, vi, t)	\
		{ return evalFloat(name, vi, t); }
#define STR_PARM(name, vi, t) \
		{ evalString(str, name, vi, t); }
#define INT_PARM(name, vi, t) \
                { return evalInt(name, vi, t); }

class SOP_Node;

enum {
    ROP_FBX_RENDER,
    ROP_FBX_RENDER_CTRL,
    ROP_FBX_TRANGE,
    ROP_FBX_FRANGE,
    ROP_FBX_TAKE,
    ROP_FBX_STARTNODE,
    ROP_FBX_CREATESUBNETROOT,
    ROP_FBX_SOPOUTPUT,
    ROP_FBX_MKPATH,
    ROP_FBX_BUILDFROMPATH,
    ROP_FBX_PATHATTRIB,

    ROP_FBX_SWITCHER,
    ROP_FBX_EXPORTASCII,
    ROP_FBX_SDKVERSION,
    ROP_FBX_VCFORMAT,
    ROP_FBX_INVISOBJ,
    ROP_FBX_AXISSYSTEM,
    ROP_FBX_CONVERTAXIS,
    ROP_FBX_CONVERTUNITS,
    ROP_FBX_POLYLOD,
    ROP_FBX_DETECTCONSTPOINTOBJS,
    ROP_FBX_CONVERTSURFACES,
    ROP_FBX_CONSERVEMEM,
    ROP_FBX_DEFORMSASVCS,
    ROP_FBX_FORCEBLENDSHAPE,
    ROP_FBX_FORCESKINDEFORM,
    ROP_FBX_EXPORTENDEFFECTORS,
    ROP_FBX_EXPORTCLIPS,
    ROP_FBX_NUMCLIPS,

    ROP_FBX_TPRERENDER,
    ROP_FBX_PRERENDER,
    ROP_FBX_LPRERENDER,
    ROP_FBX_TPREFRAME,
    ROP_FBX_PREFRAME,
    ROP_FBX_LPREFRAME,
    ROP_FBX_TPOSTFRAME,
    ROP_FBX_POSTFRAME,
    ROP_FBX_LPOSTFRAME,
    ROP_FBX_TPOSTRENDER,
    ROP_FBX_POSTRENDER,
    ROP_FBX_LPOSTRENDER,

    ROP_FBX_MAXPARMS
};

class ROP_FBX : public ROP_Node
{
public:
    bool                         updateParmsFlags() override;

    static PRM_Template		*getTemplates();
    static PRM_Template		*getObsolete();
    static CH_LocalVariable	 myVariableList[];
    static OP_Node		*myConstructor(OP_Network *net, const char*name,
						OP_Operator *op);

    static void			 buildGeoSaveMenu(void *data, PRM_Name *,
						  int, const PRM_SpareData *,
						  const PRM_Parm *);
    static void			 buildVersionsMenu(void *data, PRM_Name *,
						  int, const PRM_SpareData *,
						  const PRM_Parm *);
    static void			 buildAxisMenu(void *data, PRM_Name *,
                                               int, const PRM_SpareData *,
                                               const PRM_Parm *);

    void                         resolveObsoleteParms(
                                        PRM_ParmList *obsolete_parms) override;


    fpreal                       getW() const override;
    fpreal                       getH() const override;

    void                         inputConnectChanged(int which) override;
    
    void                         getNodeSpecificInfoText(
                                        OP_Context &context,
					OP_NodeInfoParms &iparms) override;
    void                         fillInfoTreeNodeSpecific(
                                        UT_InfoTree &tree, 
					const OP_NodeInfoTreeParms &parms
                                        ) override;

    SOP_Node *			 getSopNode() const;

protected:
	     ROP_FBX(OP_Network *net, const char *name, OP_Operator *op);
            ~ROP_FBX() override;

    int                          startRender(
                                        int nframes,
                                        fpreal s,
                                        fpreal e) override;
    ROP_RENDER_CODE              renderFrame(
                                        fpreal time,
                                        UT_Interrupt *boss) override;
    ROP_RENDER_CODE              endRender() override;

private:
    void	OUTPUT(UT_String &str, fpreal t)
		    { getOutputOverrideEx(str, t, "sopoutput", "mkpath"); }

//    int		INITSIM(void)
//		    { INT_PARM("initsim", 0, 0) }

    int EXPORTASCII(void)
	{ INT_PARM("exportkind", 0, 0) }

    float POLYLOD(void)
	{ FBX_FLOAT_PARM("polylod", 0, 0) }

    int DETECTCONSTOBJS(void)
	{ INT_PARM("detectconstpointobjs", 0, 0) }

    int CONVERTSURFACES(void)
	{ INT_PARM("convertsurfaces", 0, 0) }

    int DEFORMSASVCS(void)
    { INT_PARM("deformsasvcs", 0, 0) }

    int CONSERVEMEM(void)
    { INT_PARM("conservemem", 0, 0) }

    int FORCEBLENDSHAPE(void)
    { INT_PARM("forceblendshape", 0, 0) }

    int FORCESKINDEFORM(void)
    { INT_PARM("forceskindeform", 0, 0) }

    int EXPORTENDEFFECTORS(void)
    { INT_PARM("exportendeffectors", 0, 0) }

    int VCFORMAT(void)
    { INT_PARM("vcformat", 0, 0) }

    int INVISOBJ(void)
    { INT_PARM("invisobj", 0, 0) }

    int AXISSYSTEM(fpreal t) const
    { INT_PARM("axissystem", 0, t) }

    bool CONVERTAXIS(fpreal t) const
    { INT_PARM("convertaxis", 0, t) }

    bool CONVERTUNITS(fpreal t) const
    { INT_PARM("convertunits", 0, t) }

    void STARTNODE(UT_String& str)
    { STR_PARM("startnode",  0, 0); }

    void SDKVERSION(UT_String& str)
    { STR_PARM("sdkversion",  0, 0); }
    
    bool CREATESUBNETROOT(fpreal t) const
    { INT_PARM("createsubnetroot", 0, t); }

    bool BUILD_FROM_PATH(fpreal t) const
    { INT_PARM("buildfrompath", 0, t); }

    int EXPORTCLIPS(void)
    {
	INT_PARM("exportclips", 0, 0)
    }

    int  NUM_CLIPS(fpreal time) const
    {
	return evalInt("numclips", 0, time);
    }

    void CLIP_NAME(UT_String &str, int idx, fpreal time) const
    {
	evalStringInst("clipname#", &idx, str, 0, time);
    }

    int  CLIP_START(int idx, fpreal time) const
    {
	return evalIntInst("clipframerange#", &idx, 0, time);
    }

    int  CLIP_END(int idx, fpreal time) const
    {
	return evalIntInst("clipframerange#", &idx, 1, time);
    }

    UT_StringHolder PATH_ATTRIB(fpreal t) const
    {
        UT_StringHolder attrib;
        evalString(attrib, "pathattrib", 0, t);
        return attrib;
    }


    // Script commands
    void	PRERENDER(UT_String &str, fpreal t)
		    { STR_PARM("prerender", 0, t); }
    void	POSTRENDER(UT_String &str, fpreal t)
		    { STR_PARM("postrender", 0, t); }
    void	PREFRAME(UT_String &str, fpreal t)
		    { STR_PARM("preframe", 0, t); }
    void	POSTFRAME(UT_String &str, fpreal t)
		    { STR_PARM("postframe", 0, t); }

    #define ROP_FBX_NPARMS	8 // update this when parm list changes

    UT_String		 mySavePath;
    fpreal		 myEndTime;

private:

    ROP_FBXExporterWrapper myFBXExporter;
    bool myDidCallExport;
};


#undef INT_PARM
#undef STR_PARM

#endif
