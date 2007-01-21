set(libvncserver_SRCS
   main.c
   rfbserver.c
   sraRegion.c
   auth.c
   sockets.c
   stats.c
   corre.c
   hextile.c
   rre.c
   translate.c
   cutpaste.c
   zlib.c
   tight.c
   httpd.c
   cursor.c
   font.c
   draw.c
   selbox.c
   d3des.c
   vncauth.c
   cargs.c)

kde4_add_library(libvncserver ${libvncserver_SRCS})

target_link_libraries(libvncserver)

install(TARGETS libvncserver DESTINATION ${LIB_INSTALL_DIR})
