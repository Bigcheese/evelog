evelog
======

evelog is a C++ library for reading .lwb files output from the EVE Online
logserver.

Well, it's actually not a library yet. There is a prototype in the tools
directory called lwb-dump.cpp. The lwb reading bits of this will soon be split
out into a real library.

Build
=====

evelog uses CMake to create the build system. See:
http://www.cmake.org/cmake/help/runningcmake.html
