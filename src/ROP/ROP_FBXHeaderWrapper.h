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

#ifndef __ROP_FBXHeaderWrapper_h__
#define __ROP_FBXHeaderWrapper_h__

#include <SYS/SYS_Pragma.h>

SYS_PRAGMA_PUSH_WARN()
SYS_PRAGMA_DISABLE_DEPRECATED()
SYS_PRAGMA_DISABLE_IGNORED_QUALIFIERS()
SYS_PRAGMA_DISABLE_OVERLOADED_VIRTUAL()
SYS_PRAGMA_DISABLE_UNUSED_FUNCTION()
SYS_PRAGMA_DISABLE_UNUSED_VALUE()
SYS_PRAGMA_DISABLE_UNUSED_VARIABLE()
SYS_PRAGMA_DISABLE_COMMENT_WARNING()
SYS_PRAGMA_DISABLE_MISSING_FIELD_INITIALIZERS()

// FBX has several PropertyNotify functions, which is fine unless
// you have also included X11/X.h which defines PropertyNotify as
// the number 28. Undefine it here.
#ifdef PropertyNotify
    #undef PropertyNotify
#endif

#include <fbx/fbxsdk.h>

// Ingeniously, FBX SDK defines strdup as a macro,
// so we undo the damage here.
#undef strdup
#undef getpid
#undef finite

SYS_PRAGMA_POP_WARN()

#endif
