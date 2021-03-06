
to compile :
--------------
- you need ANDROID NDK


make all     : build all the programs 
make install : install all the programs
make clean   : clean all

to test :

cd 7za
make test

look at the makefile to see other tests




III. Debuggable versus Release builds:
--------------------------------------

In NDK r5, ndk-build has been modified to make it easier to switch between
release and debug builds. This is done by using the NDK_DEBUG variable.
For example:

  $NDK/ndk-build NDK_DEBUG=1  => forces the generation of debug binaries
  $NDK/ndk-build NDK_DEBUG=0  => forces the generation of release binaries

If you don't specify NDK_DEBUG, ndk-build will keep its default behaviour,
which is to inspect the AndroidManifest.xml, if any, and see if its
<application> element has android:debuggable="true".

IMPORTANT: If you use the build tools of SDK r8 (or higher), you
           won't need to touch your AndroidManifest.xml file at all!

           That's because if you build a debug package (e.g. with
           "ant debug" or the corresponding option of the ADT plugin),
           the tool will  automatically pick the native debug files
           generated with NDK_DEBUG=1.

Also, as a convenience, the release and debug object files generated by the
NDK are now stored in different directories (e.g. obj/local/<abi>/objs and
obj/local/<abi>/objs-debug). This avoids having to recompile all your sources
when you switch between these two modes (even when you only modified one or
two source files).

