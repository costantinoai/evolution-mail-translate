set(GSETTINGS_DIR "${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas")

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/gschema/org.gnome.evolution.translate.gschema.xml
  DESTINATION ${GSETTINGS_DIR}
)

message(STATUS "GSettings schemas will be installed to: ${GSETTINGS_DIR}")

