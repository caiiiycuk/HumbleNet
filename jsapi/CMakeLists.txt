add_executable(jsapi
    src/main.cpp
)
target_link_options(jsapi PRIVATE
    -s EXPORTED_FUNCTIONS=['_malloc','_free']
    -sDISABLE_EXCEPTION_CATCHING=0
    -sASSERTIONS=1
    -sMODULARIZE
    -sEXIT_RUNTIME=0
    -sEXPORT_NAME="HumbleNet"
    -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$intArrayFromString'
)
target_link_libraries(jsapi humblenet)
set_target_properties(jsapi PROPERTIES SUFFIX ".mjs")