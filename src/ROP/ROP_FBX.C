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

static PRM_Name	vcType[] =
{
    PRM_Name("mayaformat",	"Maya Compatible (MC)"),
    PRM_Name("maxformat",	"3DS MAX Compatible (PC2)"),
    PRM_Name(0),
};

static PRM_Name	invisObj[] =
{
    PRM_Name("nullnodes",	"As Hidden Null Nodes"),
    PRM_Name("fullnodes",	"As Hidden Full Nodes"),
    PRM_Name(0),
};

CH_LocalVariable	ROP_FBX::myVariableList[] = { {0, 0, 0} };

static PRM_Name		sopOutput("sopoutput",	"Output File");
static PRM_Name		startNode("startnode", "Start At");
static PRM_Name		exportKind("exportkind", "Export in ASCII Format");
static PRM_Name		detectConstPointObjs("detectconstpointobjs", "Detect Constant Point Count Dynamic Objects");
static PRM_Name		deformsAsVcs("deformsasvcs", "Export Deforms as Vertex Caches");
static PRM_Name		polyLOD("polylod", "Conversion Level of Detail");
static PRM_Name		vcTypeName("vcformat", "Vertex Cache Format");
static PRM_Name		invisObjTypeName("invisobj", "Export Invisible Objects");

static PRM_Range	polyLODRange(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 5);

static PRM_Default	exportKindDefault(1);
static PRM_Default	detectConstPointObjsDefault(1);
static PRM_Default	deformsAsVcsDefault(0);
static PRM_Default	polyLODDefault(1.0);
static PRM_Default	startNodeDefault(0, "/obj");
static PRM_Default	sopOutputDefault(0, "$HIP/$F.fbx");
static PRM_ChoiceList	sopOutputMenu(PRM_CHOICELIST_REPLACE,
					 &ROP_FBX::buildGeoSaveMenu);

static PRM_ChoiceList	vcTypeMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE
				| PRM_CHOICELIST_REPLACE), vcType);
static PRM_ChoiceList	invisObjMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE
				   | PRM_CHOICELIST_REPLACE), invisObj);



static PRM_Template	 geoTemplates[] = {
    PRM_Template(PRM_FILE,  1, &sopOutput, &sopOutputDefault, NULL),
    PRM_Template(PRM_STRING,  PRM_TYPE_DYNAMIC_PATH, 1, &startNode, &startNodeDefault, NULL),
    PRM_Template(PRM_TOGGLE,  1, &exportKind, &exportKindDefault, NULL),
    PRM_Template(PRM_FLT,  1, &polyLOD, &polyLODDefault, NULL, &polyLODRange),
    PRM_Template(PRM_TOGGLE,  1, &detectConstPointObjs, &detectConstPointObjsDefault, NULL),
    PRM_Template(PRM_TOGGLE,  1, &deformsAsVcs, &deformsAsVcsDefault, NULL),
    PRM_Template(PRM_ORD,  PRM_Template::PRM_EXPORT_TBX, 1, &vcTypeName, 0, &vcTypeMenu),
    PRM_Template(PRM_ORD,  PRM_Template::PRM_EXPORT_TBX, 1, &invisObjTypeName, 0, &invisObjMenu),
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
    theTemplate[ROP_FBX_TAKE] = theRopTemplates[ROP_TAKENAME_TPLATE];
    theTemplate[ROP_FBX_SOPOUTPUT] = geoTemplates[0];
//    theTemplate[ROP_FBX_INITSIM] = theRopTemplates[ROP_IFD_INITSIM_TPLATE];

    theTemplate[ROP_FBX_STARTNODE] = geoTemplates[1];
    theTemplate[ROP_FBX_EXPORTASCII] = geoTemplates[2];
    theTemplate[ROP_FBX_VCFORMAT] = geoTemplates[6];
    theTemplate[ROP_FBX_INVISOBJ] = geoTemplates[7];
    theTemplate[ROP_FBX_POLYLOD] = geoTemplates[3];
    theTemplate[ROP_FBX_DETECTCONSTPOINTOBJS] = geoTemplates[4];
    theTemplate[ROP_FBX_DEFORMSASVCS] = geoTemplates[5];

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
    int			changed = 0;

    changed += enableParm("deformsasvcs", DORANGE());
   
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

    UT_String str_start_node(UT_String::ALWAYS_DEEP);

    OUTPUT(mySavePath, tstart);
    STARTNODE(str_start_node);

    if(str_start_node.length() <= 0)
	str_start_node = "/obj";

    // Set export options
    ROP_FBXExportOptions export_options;

    int vc_format = VCFORMAT();
    if(vc_format == 0)
	export_options.setVertexCacheFormat(ROP_FBXVertexCacheExportFormatMaya);
    else
	export_options.setVertexCacheFormat(ROP_FBXVertexCacheExportFormat3DStudio);

    export_options.setInvisibleNodeExportMethod((ROP_FBXInvisibleNodeExportType)((int)INVISOBJ()));
    
    export_options.setExportInAscii(EXPORTASCII());
    export_options.setPolyConvertLOD(POLYLOD());
    export_options.setDetectConstantPointCountObjects(DETECTCONSTOBJS());
    export_options.setExportDeformsAsVC(DEFORMSASVCS());
    export_options.setStartNodePath((const char*)str_start_node);
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

    // Add any messages we might have had
    if(myFBXExporter.getErrorManager())
    {
	ROP_FBXError* error_ptr;
	int curr_error, num_errors = myFBXExporter.getErrorManager()->getNumItems();
	for(curr_error = 0; curr_error < num_errors; curr_error++)
	{
	    error_ptr = myFBXExporter.getErrorManager()->getError(curr_error);
	    if(error_ptr->getIsCritical())
	    {
		// Error		
		addError(ROP_MESSAGE, error_ptr->getMessage());
	    }
	    else
	    {
		// Warning
		addWarning(ROP_MESSAGE, error_ptr->getMessage());
	    }
	}
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

