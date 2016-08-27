# lua_flatbuffers
a lua google flatbuffers encode/decode module base on google flatbuffers reflection  
See more about parson at http://google.github.io/flatbuffers/  

FlatBuffers release 1.4.0
https://github.com/google/flatbuffers/releases/tag/v1.4.0

#libflatbuffers.so
    change CMakeLists.txt at line 151  
    if(FLATBUFFERS_BUILD_FLATLIB)  
    add_library(flatbuffers STATIC ${FlatBuffers_Library_SRCS})  
    add_library(flatbuffers_shared SHARED ${FlatBuffers_Library_SRCS})  
    set_target_properties(flatbuffers_shared PROPERTIES OUTPUT_NAME flatbuffers)  
    endif()  

if(FLATBUFFERS_INSTALL)  
  install(DIRECTORY include/flatbuffers DESTINATION include)  
  if(FLATBUFFERS_BUILD_FLATLIB)  
    install(TARGETS flatbuffers DESTINATION lib)  
    install(TARGETS flatbuffers_shared DESTINATION lib)  
  endif()  
  if(FLATBUFFERS_BUILD_FLATC)  
    install(TARGETS flatc DESTINATION bin)  
  endif()  
endif()  

#note
1. include other schema test
2. namespace ?
