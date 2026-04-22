configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/jeode.nsi.in
    ${CMAKE_BINARY_DIR}/jeode.nsi
    @ONLY)

find_program(MAKENSIS makensis)
if(MAKENSIS)
  add_custom_target(installer
    COMMAND ${MAKENSIS} -NOCD ${CMAKE_BINARY_DIR}/jeode.nsi
    DEPENDS winhttp_proxy jeode
    COMMENT "Building Installer")
endif()
