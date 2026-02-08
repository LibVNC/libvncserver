# CMake generated Testfile for 
# Source directory: /home/runner/work/libvncserver/libvncserver
# Build directory: /home/runner/work/libvncserver/libvncserver/_codeql_build_dir
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(cargs "/home/runner/work/libvncserver/libvncserver/_codeql_build_dir/test/cargstest")
set_tests_properties(cargs PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/libvncserver/libvncserver/CMakeLists.txt;745;add_test;/home/runner/work/libvncserver/libvncserver/CMakeLists.txt;0;")
add_test(includetest_server "/home/runner/work/libvncserver/libvncserver/test/includetest.sh" "/usr/local/include" "/usr/bin/gmake" "rfb/rfb.h")
set_tests_properties(includetest_server PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/libvncserver/libvncserver/CMakeLists.txt;749;add_test;/home/runner/work/libvncserver/libvncserver/CMakeLists.txt;0;")
add_test(includetest_client "/home/runner/work/libvncserver/libvncserver/test/includetest.sh" "/usr/local/include" "/usr/bin/gmake" "rfb/rfbclient.h")
set_tests_properties(includetest_client PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/libvncserver/libvncserver/CMakeLists.txt;752;add_test;/home/runner/work/libvncserver/libvncserver/CMakeLists.txt;0;")
add_test(wstest "/home/runner/work/libvncserver/libvncserver/_codeql_build_dir/test/wstest")
set_tests_properties(wstest PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/libvncserver/libvncserver/CMakeLists.txt;759;add_test;/home/runner/work/libvncserver/libvncserver/CMakeLists.txt;0;")
