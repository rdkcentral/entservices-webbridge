# - Try to find JSONCPP

find_path( JSONCPP_INCLUDE_DIR NAMES json/json.h PATH_SUFFIXES jsoncpp )
find_library( JSONCPP_LIBRARY NAMES libjsoncpp.so jsoncpp )

include( FindPackageHandleStandardArgs )

# Handle the QUIETLY and REQUIRED arguments and set the JSONCPP_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args( JSONCPP DEFAULT_MSG
        JSONCPP_LIBRARY JSONCPP_INCLUDE_DIR )

mark_as_advanced( JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY )

if( JSONCPP_FOUND )
    set( JSONCPP_LIBRARIES ${JSONCPP_LIBRARY} )
    set( JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR} )
endif()

if( JSONCPP_FOUND AND NOT TARGET JsonCpp::JsonCpp )
    add_library( JsonCpp::JsonCpp SHARED IMPORTED )
    set_target_properties( JsonCpp::JsonCpp PROPERTIES
            IMPORTED_LOCATION "${JSONCPP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIRS}" )
endif()