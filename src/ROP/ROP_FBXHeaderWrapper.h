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

#ifndef __ROP_FBXHeaderWrapper_h__
#define __ROP_FBXHeaderWrapper_h__

#include <fbx/fbxsdk.h>
// Ingeniously, FBX SDK defines strdup as a macro,
// so we undo the damage here.
#undef strdup

#endif
