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
    ROP_FBX_SOPOUTPUT,
    ROP_FBX_MKPATH,
//    ROP_FBX_INITSIM,

    ROP_FBX_STARTNODE,
    ROP_FBX_CREATESUBNETROOT,
    ROP_FBX_EXPORTASCII,
    ROP_FBX_SDKVERSION,
    ROP_FBX_VCFORMAT,
    ROP_FBX_INVISOBJ,
    ROP_FBX_POLYLOD,
    ROP_FBX_DETECTCONSTPOINTOBJS,
    ROP_FBX_CONVERTSURFACES,
    ROP_FBX_CONSERVEMEM,
    ROP_FBX_DEFORMSASVCS,
    ROP_FBX_FORCEBLENDSHAPE,
    ROP_FBX_FORCESKINDEFORM,

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
    virtual bool		 updateParmsFlags();

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
    virtual void		 resolveObsoleteParms(
						PRM_ParmList *obsolete_parms);


    virtual fpreal		 getW() const;
    virtual fpreal		 getH() const;

    virtual void		 inputConnectChanged(int which);
    
    virtual void		 getNodeSpecificInfoText(OP_Context &context,
					OP_NodeInfoParms &iparms);
    virtual void		 fillInfoTreeNodeSpecific(UT_InfoTree &tree, 
					const OP_NodeInfoTreeParms &parms);

    SOP_Node *			 getSopNode() const;

protected:
	     ROP_FBX(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~ROP_FBX();

    virtual int			 startRender(int nframes, fpreal s, fpreal e);
    virtual ROP_RENDER_CODE	 renderFrame(fpreal time, UT_Interrupt *boss);
    virtual ROP_RENDER_CODE	 endRender();

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

    int VCFORMAT(void)
    { INT_PARM("vcformat", 0, 0) }

    int INVISOBJ(void)
    { INT_PARM("invisobj", 0, 0) }

    void STARTNODE(UT_String& str)
    { STR_PARM("startnode",  0, 0); }

    void SDKVERSION(UT_String& str)
    { STR_PARM("sdkversion",  0, 0); }
    
    bool CREATESUBNETROOT(fpreal t) const
    { INT_PARM("createsubnetroot", 0, t); }

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
