find_package(Doxygen)

# Find all the public headers
# get_target_property(CAT_CUTIFIER_PUBLIC_HEADER_DIR CatCutifier INTERFACE_INCLUDE_DIRECTORIES)
# jfile(GLOB_RECURSE CAT_CUTIFIER_PUBLIC_HEADERS ${CAT_CUTIFIER_PUBLIC_HEADER_DIR}/*.h)

if (DOXYGEN_FOUND)
    
    set(DOXYGEN_INPUT_DIR ${PROJECT_SOURCE_DIR})
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
    set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/html/index.html)
    set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen/Doxyfile.out)
    
    message("Doxygen found. Generating doxygen docs into ${DOXYGEN_OUTPUT_DIR}. HTML index file is at ${DOXYGEN_INDEX_FILE}")
    message("Doxyfile.out is at ${DOXYFILE_OUT}")
    #Replace variables inside @@ with the current values
    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

    file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}) #Doxygen won't create this for us
    # Also make sphinx dir under docs in build for sphinx later
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sphinx)
    add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
                       COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
                       MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
                       COMMENT "Generating docs...")

    add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})
else (DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation. Skipping doxygen docs generation...")
endif (DOXYGEN_FOUND)