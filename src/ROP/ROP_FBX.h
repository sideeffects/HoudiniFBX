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

#include "ROP_API.h"
#include <GU/GU_DetailHandle.h>
#include "ROP_Node.h"

#include "ROP_FBXExporter.h"

#define STR_PARM(name, idx, vi, t) \
		{ evalString(str, name, vi, (float)t); }
#define INT_PARM(name, idx, vi, t) \
                { return evalInt(name, vi, t); }

class SOP_Node;

enum {
    ROP_FBX_RENDER,
    ROP_FBX_RENDER_CTRL,
    ROP_FBX_TRANGE,
    ROP_FBX_FRANGE,
//    ROP_FBX_TAKE,
    ROP_FBX_SOPOUTPUT,
//    ROP_FBX_INITSIM,
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

class ROP_API ROP_FBX : public ROP_Node {
public:
    virtual unsigned		 disableParms();

    static PRM_Template		*getTemplates();
    static PRM_Template		*getObsolete();
    static CH_LocalVariable	 myVariableList[];
    static OP_Node		*myConstructor(OP_Network *net, const char*name,
						OP_Operator *op);

    static void			 buildGeoSaveMenu(void *data, PRM_Name *,
						  int, const PRM_SpareData *,
						  PRM_Parm *);
    virtual void		 resolveObsoleteParms(
						PRM_ParmList *obsolete_parms);


    virtual fpreal		 getW() const;
    virtual fpreal		 getH() const;

    virtual void		 inputConnectChanged(int which);
    
    virtual void	 getNodeSpecificInfoText(OP_Context &context,
						 int verbose,
						 UT_WorkBuffer &text);
//    virtual bool	getGeometryHandle(const char *path, float t,
//				  GU_DetailHandle &gdh);

protected:
	     ROP_FBX(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~ROP_FBX();

    virtual int			 startRender(int nframes, float s, float e);
    virtual ROP_RENDER_CODE	 renderFrame(float time, UT_Interrupt *boss);
    virtual ROP_RENDER_CODE	 endRender();

private:
    void	OUTPUT(UT_String &str, float t)
		    {
		      if( getRenderMode() == RENDER_PRM )
		      { 
			  if (!getOutputOverride(str, t))
			  { STR_PARM("sopoutput",  2, 0, t) }
		      }
		      else
			str = getRenderOutput();
		    }

//    int		INITSIM(void)
//		    { INT_PARM("initsim", 3, 0, 0) }

    // Script commands
    void	PRERENDER(UT_String &str, float t)
		    { STR_PARM("prerender", 4, 0, t); }
    void	POSTRENDER(UT_String &str, float t)
		    { STR_PARM("postrender", 5, 0, t); }
    void	PREFRAME(UT_String &str, float t)
		    { STR_PARM("preframe", 6, 0, t); }
    void	POSTFRAME(UT_String &str, float t)
		    { STR_PARM("postframe", 7, 0, t); }

    #define ROP_FBX_NPARMS	8 // update this when parm list changes

    UT_String		 mySavePath;
    float		 myEndTime;

private:

    ROP_FBXExporter myFBXExporter;
    bool myDidCallExport;
};


#undef INT_PARM
#undef STR_PARM

#endif
