# The HFS Environment variable needs to be set before calling make
# Windows users should also define their MSVCDir environnment varaibale

# Define the name of the ROP lib
ifdef WINDOWS
DSONAME = ROP_FBX.dll
else ifdef MBSD
DSONAME = ROP_FBX.dylib
else
DSONAME = ROP_FBX.so
endif

# List of C++ source files to build.
SOURCES = \
	ROP_FBX.C \
	ROP_FBXExporter.C \
	ROP_FBXExporterWrapper.C \
	ROP_FBXActionManager.C \
	ROP_FBXAnimVisitor.C \
	ROP_FBXBaseAction.C \
	ROP_FBXBaseVisitor.C \
	ROP_FBXCommon.C \
	ROP_FBXDerivedActions.C \
	ROP_FBXErrorManager.C \
	ROP_FBXMainVisitor.C \
	ROP_FBXUtil.C

# Additional include directories.
INCDIRS = \
        -I$(HFS)/toolkit/include \
        -I$(HFS)/toolkit/include/fbx

# These are needed for FBX SDK to link properly	
INCEXTRA = -DFBXSDK_SHARED

HDEFINES += -DEXPORT_FBX

#ifdef MBSD
#LDFLAGS = -undefined dynamic_lookup
#endif

# For GNU make, use this line:
#      include $(HFS)/toolkit/makefiles/Makefile.gnu
# For Microsoft Visual Studio's nmake use this line instead
#      !INCLUDE $(HFS)/toolkit/makefiles/Makefile.nmake
#
include $(HFS)/toolkit/makefiles/Makefile.gnu