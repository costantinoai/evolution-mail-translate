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

# Ensure the linker knows where to find Evolution libraries without injecting RPATH
target_link_directories(translate-module PRIVATE
    ${EVOLUTION_SHELL_LIBRARY_DIRS}
    ${EVOLUTION_CALENDAR_LIBRARY_DIRS}
    ${EVOLUTION_MAIL_LIBRARY_DIRS}
    ${LIBECAL_LIBRARY_DIRS}
    ${JSON_GLIB_LIBRARY_DIRS}
)

target_link_libraries(translate-module
    ${EVOLUTION_SHELL_LIBRARIES}
    ${EVOLUTION_CALENDAR_LIBRARIES}
    ${EVOLUTION_MAIL_LIBRARIES}
    ${LIBECAL_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
)

# Propagate any non-library link flags but filter out potential RPATH/RUNPATH injectors
set(_PKG_LDFLAGS_OTHER
    ${EVOLUTION_SHELL_LDFLAGS_OTHER}
    ${EVOLUTION_CALENDAR_LDFLAGS_OTHER}
    ${EVOLUTION_MAIL_LDFLAGS_OTHER}
    ${LIBECAL_LDFLAGS_OTHER}
    ${JSON_GLIB_LDFLAGS_OTHER}
)

if(_PKG_LDFLAGS_OTHER)
    # Remove any -Wl,-rpath,* and -Wl,-R* occurrences defensively
    foreach(flag IN LISTS _PKG_LDFLAGS_OTHER)
        if(flag MATCHES "^-Wl,-rpath,.*" OR flag MATCHES "^-Wl,-R.*")
            list(REMOVE_ITEM _PKG_LDFLAGS_OTHER ${flag})
        endif()
    endforeach()
    if(_PKG_LDFLAGS_OTHER)
        target_link_options(translate-module PRIVATE ${_PKG_LDFLAGS_OTHER})
    endif()
endif()

# Ensure no RPATH/RUNPATH is embedded to satisfy Debian lintian
set_target_properties(translate-module PROPERTIES
    SKIP_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH ""
    BUILD_RPATH ""
)

install(TARGETS translate-module
	DESTINATION ${EVOLUTION_MODULE_DIR}
)
