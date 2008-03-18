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

#include "ROP_FBX.h"
#include <CH/CH_LocalVariable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Director.h>
#include <OBJ/OBJ_Node.h>
#include <GEO/GEO_Vertex.h>
#include <SIM/SIM_Object.h>
#include <SIM/SIM_Geometry.h>
#include <DOP/DOP_Parent.h>
#include <DOP/DOP_FullPathData.h>
#include "ROP_Error.h"
#include "ROP_Templates.h"

CH_LocalVariable	ROP_FBX::myVariableList[] = { {0, 0, 0} };

static PRM_Name		sopOutput("sopoutput",	"Output File");
static PRM_Default	sopOutputDefault(0, "$HIP/$F.fbx");
static PRM_ChoiceList	sopOutputMenu(PRM_CHOICELIST_REPLACE,
					 &ROP_FBX::buildGeoSaveMenu);

static PRM_Template	 geoTemplates[] = {
    PRM_Template(PRM_FILE,  1, &sopOutput, &sopOutputDefault, NULL),
};

static PRM_Template	geoObsolete[] = {
    PRM_Template()
};

PRM_Template *
ROP_FBX::getTemplates()
{
    static PRM_Template	*theTemplate = 0;

    if (theTemplate)
	return theTemplate;

    theTemplate = new PRM_Template[ROP_FBX_MAXPARMS+1];
    theTemplate[ROP_FBX_RENDER] = theRopTemplates[ROP_RENDER_TPLATE];
    theTemplate[ROP_FBX_RENDER_CTRL] = theRopTemplates[ROP_RENDERDIALOG_TPLATE];
    theTemplate[ROP_FBX_TRANGE] = theRopTemplates[ROP_TRANGE_TPLATE];
    theTemplate[ROP_FBX_FRANGE] = theRopTemplates[ROP_FRAMERANGE_TPLATE];
//    theTemplate[ROP_FBX_TAKE] = theRopTemplates[ROP_TAKENAME_TPLATE];
    theTemplate[ROP_FBX_SOPOUTPUT] = geoTemplates[0];
//    theTemplate[ROP_FBX_INITSIM] = theRopTemplates[ROP_IFD_INITSIM_TPLATE];
    theTemplate[ROP_FBX_TPRERENDER] = theRopTemplates[ROP_TPRERENDER_TPLATE];
    theTemplate[ROP_FBX_PRERENDER] = theRopTemplates[ROP_PRERENDER_TPLATE];
    theTemplate[ROP_FBX_LPRERENDER] = theRopTemplates[ROP_LPRERENDER_TPLATE];
    theTemplate[ROP_FBX_TPREFRAME] = theRopTemplates[ROP_TPREFRAME_TPLATE];
    theTemplate[ROP_FBX_PREFRAME] = theRopTemplates[ROP_PREFRAME_TPLATE];
    theTemplate[ROP_FBX_LPREFRAME] = theRopTemplates[ROP_LPREFRAME_TPLATE];
    theTemplate[ROP_FBX_TPOSTFRAME] = theRopTemplates[ROP_TPOSTFRAME_TPLATE];
    theTemplate[ROP_FBX_POSTFRAME] = theRopTemplates[ROP_POSTFRAME_TPLATE];
    theTemplate[ROP_FBX_LPOSTFRAME] = theRopTemplates[ROP_LPOSTFRAME_TPLATE];
    theTemplate[ROP_FBX_TPOSTRENDER] = theRopTemplates[ROP_TPOSTRENDER_TPLATE];
    theTemplate[ROP_FBX_POSTRENDER] = theRopTemplates[ROP_POSTRENDER_TPLATE];
    theTemplate[ROP_FBX_LPOSTRENDER] = theRopTemplates[ROP_LPOSTRENDER_TPLATE];
    theTemplate[ROP_FBX_MAXPARMS] = PRM_Template();

    UT_ASSERT(PRM_Template::countTemplates(theTemplate) == ROP_FBX_MAXPARMS);

    return theTemplate;
}

PRM_Template *
ROP_FBX::getObsolete()
{
    return geoObsolete;
}

OP_Node *
ROP_FBX::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new ROP_FBX(net, name, op);
}

unsigned
ROP_FBX::disableParms()
{
    bool		hasinput = CAST_SOPNODE(getInput(0)) ? true : false;
    int			changed = 0;
   
    return changed + ROP_Node::disableParms();
}

ROP_FBX::ROP_FBX(OP_Network *net, const char *name, OP_Operator *entry)
	: ROP_Node(net, name, entry)
{

}

void
ROP_FBX::resolveObsoleteParms(PRM_ParmList *obsolete_parms)
{
    UT_String	net, node, path, blank;
    ROP_Node::resolveObsoleteParms(obsolete_parms);
}

ROP_FBX::~ROP_FBX()
{
}

class ropFBX_AutoCookRender {
public:
    ropFBX_AutoCookRender(OP_Node *sop)
    {
	if (myObj = sop->getParent())
	{
	    myPrev = myObj->isCookingRender();
	    myObj->setCookingRender(1);
	}
    }
    ~ropFBX_AutoCookRender()
    {
	if (myObj)
	    myObj->setCookingRender(myPrev);
    }
private:
    OP_Node	*myObj;
    int		 myPrev;
};

int
ROP_FBX::startRender(int /*nframes*/, float tstart, float tend)
{
    int			 rcode = 1;

//    if (INITSIM())
    {
        initSimulationOPs();
	OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(1);
    }

    if (error() < UT_ERROR_ABORT)
    {
	if( !executePreRenderScript(tstart) )
	    return 0;
    }

    OUTPUT(mySavePath, tstart);
    ROP_FBXExportOptions export_options;
    myFBXExporter.initializeExport((const char*)mySavePath, tstart, tend, &export_options);
    myDidCallExport = false;

    return rcode;
}

ROP_RENDER_CODE
ROP_FBX::renderFrame(float time, UT_Interrupt *)
{
    if( !executePreFrameScript(time) )
	return ROP_ABORT_RENDER;

    // We only need to call this once, even if we're rendering a frame range.
    if(!myDidCallExport)
    {
	myFBXExporter.doExport();
	myDidCallExport = true;
    }

    if (error() < UT_ERROR_ABORT)
    {
	if( !executePostFrameScript(time) )
	    return ROP_ABORT_RENDER;
    }


    return ROP_CONTINUE_RENDER;
}

ROP_RENDER_CODE
ROP_FBX::endRender()
{
    myFBXExporter.finishExport();

//    if (INITSIM())
	OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(-1);

    if (error() < UT_ERROR_ABORT)
    {
	if( !executePostRenderScript(myEndTime) )
	    return ROP_ABORT_RENDER;
    }
    return ROP_CONTINUE_RENDER;
}


//------------------------------------------------------------------------------

static void
setMenu(PRM_Name &menu, const char *token, const char *label)
{
    menu.setToken(token);
    menu.setLabel(label);
}

void
ROP_FBX::buildGeoSaveMenu(void *, PRM_Name *menu, int,
			       const PRM_SpareData *, PRM_Parm *)
{
    int		i;
    i = 0;
    setMenu(menu[i++], "$HIP/$F.fbx",	"HIP: binary FBX file");
    setMenu(menu[i++], "$JOB/$F.fbx",	"JOB: binary FBX file");

    setMenu(menu[i], 0, 0);
}

void
ROP_FBX::inputConnectChanged(int which)
{
    ROP_Node::inputConnectChanged(which);
}
    
fpreal
ROP_FBX::getW() const
{
    return 4 * getFlagWidth() + getNodeButtonWidth();
}

fpreal
ROP_FBX::getH() const
{
    return getNodeHeight();
}

void
ROP_FBX::getNodeSpecificInfoText(OP_Context &context,
				      int verbose,
				      UT_WorkBuffer &text)
{
    SOP_Node		*sop;
    UT_String		 out;
    UT_String		 soppath;
    
    ROP_Node::getNodeSpecificInfoText(context, verbose, text);

    sop = CAST_SOPNODE(getInput(0));
    if( sop )
    {
	// If we have an input, get the full path to that SOP.
	sop->getFullPath(soppath);
    }
    else
    {
	UT_ASSERT(0);
    }

    if(soppath.isstring())
    {
	text.append("Render SOP        ");
	text.append(soppath);
	text.append("\n");
    }

    evalStringRaw(out, "sopoutput", 0, 0.0f);
    text.append("Write to          ");
    text.append(out);
}

