if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND EMSCRIPTEN)
    message(FATAL_ERROR "Build type must be Release (-DCMAKE_BUILD_TYPE=Release)")
endif()

add_executable(jsapi
    src/main.cpp
)
target_link_options(jsapi PRIVATE
    -sINITIAL_MEMORY=16MB
    -sMAXIMUM_MEMORY=16MB
    -sALLOW_MEMORY_GROWTH=0
    -sEXPORTED_FUNCTIONS=['_malloc','_free']
    -sDISABLE_EXCEPTION_CATCHING=0
    -sASSERTIONS=0
    -sMODULARIZE
    -sEXIT_RUNTIME=0
    -sEXPORT_NAME="HumbleNet"
    -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$intArrayFromString'
)
target_link_libraries(jsapi humblenet)
set_target_properties(jsapi PROPERTIES SUFFIX ".mjs")