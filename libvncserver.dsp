# Microsoft Developer Studio Project File - Name="libvncserver" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libvncserver - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "libvncserver.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "libvncserver.mak" CFG="libvncserver - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "libvncserver - Win32 Release" (basierend auf  "Win32 (x86) Static Library")
!MESSAGE "libvncserver - Win32 Debug" (basierend auf  "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libvncserver - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "zlib" /I "libjpeg" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libvncserver - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libvncserver___Win32_Debug"
# PROP BASE Intermediate_Dir "libvncserver___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "libvncserver___Win32_Debug"
# PROP Intermediate_Dir "libvncserver___Win32_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "zlib" /I "libjpeg" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libvncserver - Win32 Release"
# Name "libvncserver - Win32 Debug"
# Begin Group "Sources"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "auth"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\auth.c
# End Source File
# Begin Source File

SOURCE=.\d3des.c
# End Source File
# Begin Source File

SOURCE=.\storepasswd.c
# End Source File
# Begin Source File

SOURCE=.\vncauth.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\cargs.c
# End Source File
# Begin Source File

SOURCE=.\corre.c
# End Source File
# Begin Source File

SOURCE=.\cursor.c
# End Source File
# Begin Source File

SOURCE=.\cutpaste.c
# End Source File
# Begin Source File

SOURCE=.\draw.c
# End Source File
# Begin Source File

SOURCE=.\font.c
# End Source File
# Begin Source File

SOURCE=.\hextile.c
# End Source File
# Begin Source File

SOURCE=.\httpd.c
# End Source File
# Begin Source File

SOURCE=.\main.c
# End Source File
# Begin Source File

SOURCE=.\rfbserver.c
# End Source File
# Begin Source File

SOURCE=.\rre.c
# End Source File
# Begin Source File

SOURCE=.\selbox.c
# End Source File
# Begin Source File

SOURCE=.\sockets.c
# End Source File
# Begin Source File

SOURCE=.\sraRegion.c
# End Source File
# Begin Source File

SOURCE=.\stats.c
# End Source File
# Begin Source File

SOURCE=.\tight.c
# End Source File
# Begin Source File

SOURCE=.\translate.c
# End Source File
# Begin Source File

SOURCE=.\zlib.c
# End Source File
# End Group
# Begin Group "Headers"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "auth Nr. 1"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\d3des.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\default8x16.h
# End Source File
# Begin Source File

SOURCE=.\keysym.h
# End Source File
# Begin Source File

SOURCE=.\radon.h
# End Source File
# Begin Source File

SOURCE=.\region.h
# End Source File
# Begin Source File

SOURCE=.\rfb.h
# End Source File
# Begin Source File

SOURCE=.\rfbproto.h
# End Source File
# Begin Source File

SOURCE=.\sraRegion.h
# End Source File
# End Group
# End Target
# End Project
