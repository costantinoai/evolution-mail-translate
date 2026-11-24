list(APPEND SOURCES
	translate-module.c
	translate-shell-view-extension.h
	translate-shell-view-extension.c
	translate-mail-ui.h
	translate-mail-ui.c
	translate-browser-extension.h
	translate-browser-extension.c
	translate-dom.h
	translate-dom.c
	translate-content.h
	translate-content.c
	translate-preferences.h
	translate-preferences.c
	translate-utils.h
	translate-utils.c
	translate-common.h
	translate-common.c
	m-utils.h
	m-utils.c
	providers/translate-provider.h
	providers/translate-provider.c
	providers/translate-provider-argos.h
	providers/translate-provider-argos.c
	providers/translate-provider-google.h
	providers/translate-provider-google.c
	providers/translate-provider-mymemory.h
	providers/translate-provider-mymemory.c
	providers/translate-provider-libre.h
	providers/translate-provider-libre.c
)

add_library(translate-module MODULE ${SOURCES})

target_compile_definitions(translate-module PRIVATE
	G_LOG_DOMAIN=\"translate-module\"
	TRANSLATE_HELPER_DIR=\"${CMAKE_INSTALL_FULL_DATADIR}/evolution-translate/translate\"
)

target_include_directories(translate-module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/providers)
target_link_libraries(translate-module PRIVATE PkgConfig::EVOLUTION)

install(TARGETS translate-module
	DESTINATION ${EVOLUTION_MODULE_DIR}
)
