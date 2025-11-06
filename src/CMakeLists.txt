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
)

add_library(translate-module MODULE ${SOURCES})

target_compile_definitions(translate-module PRIVATE
	G_LOG_DOMAIN=\"translate-module\"
)

target_compile_options(translate-module PUBLIC
	${EVOLUTION_SHELL_CFLAGS}
	${EVOLUTION_CALENDAR_CFLAGS}
	${EVOLUTION_MAIL_CFLAGS}
	${LIBECAL_CFLAGS}
	${JSON_GLIB_CFLAGS}
)

target_include_directories(translate-module PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/providers
	${EVOLUTION_SHELL_INCLUDE_DIRS}
	${EVOLUTION_CALENDAR_INCLUDE_DIRS}
	${EVOLUTION_MAIL_INCLUDE_DIRS}
	${LIBECAL_INCLUDE_DIRS}
	${JSON_GLIB_INCLUDE_DIRS}
)

target_link_libraries(translate-module
	${EVOLUTION_SHELL_LDFLAGS}
	${EVOLUTION_CALENDAR_LDFLAGS}
	${EVOLUTION_MAIL_LDFLAGS}
	${LIBECAL_LDFLAGS}
	${JSON_GLIB_LDFLAGS}
)

install(TARGETS translate-module
	DESTINATION ${EVOLUTION_MODULE_DIR}
)
