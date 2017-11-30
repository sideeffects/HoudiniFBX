-------------------------------------------------------------------------------
Building the FBX ROP on Windows with Cygwin:
-------------------------------------------------------------------------------

To build the FBX ROP on Windows the HFS environment variable needs to be set to the houdini install directory,
and the MSVCDir environment must be set to the VC subdirectory of your Visual Studio installation.

To do so, first open a cygwin shell and go to the Houdini X.Y.ZZZ directory and source the Houdini environment:

cd C:/Program\ Files/Side\ Effects\ Software/Houdini\ X.Y.ZZZ
source ./houdini_setup

Then set your MSVCDir environment variable to the VC subdirectory of your Visual Studio installation:

export MSVCDir="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC"

You can then go to the FBX ROP src folder and build it by calling:
make ROP_FBX.dll



-------------------------------------------------------------------------------
Building the FBX ROP on Linux or Mac OS X:
-------------------------------------------------------------------------------

First, make sure the HFS environment variable is properly set to your houdini install directory.

Then go to the FBX ROP src folder.

- For linux users, run: make ROP_FBX.so
- For OSX users, run: make ROP_FBX.dylib
