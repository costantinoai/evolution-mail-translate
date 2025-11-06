# Evolution Translate Extension build script

cmake_minimum_required(VERSION 3.15)
cmake_policy(VERSION 3.15)

project(evolution-translate-extension C)

include(GNUInstallDirs)

#
# Base bits
#
set(GETTEXT_PACKAGE "translate-module")
set(RELEASE_NAME "Evolution Translate Extension")

# Determine project version (single source of truth)
# Priority: explicitly provided -DVERSION => debian/changelog => fallback
if(NOT DEFINED VERSION)
  find_program(DPKG_PARSECHANGELOG dpkg-parsechangelog)
  if(DPKG_PARSECHANGELOG)
    execute_process(
      COMMAND ${DPKG_PARSECHANGELOG} -S Version
      OUTPUT_VARIABLE DEB_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(DEB_VERSION)
      set(VERSION "${DEB_VERSION}")
    else()
      set(VERSION "1.0.0")
    endif()
  else()
    set(VERSION "1.0.0")
  endif()
endif()

set(VERSION_INFO "Production Release")
set(LANGUAGE_SUPPORT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/share/locale)

add_definitions(-DGETTEXT_PACKAGE=\"${GETTEXT_PACKAGE}\")

# Filenames for tarball
set(ARCHIVE_BASE_NAME ${CMAKE_PROJECT_NAME}-${VERSION})
set(ARCHIVE_FULL_NAME ${ARCHIVE_BASE_NAME}.tar.xz)

# Lowered to match local environment (build tested with 3.52.x)
set(REQUIRE_EVOLUTION_VERSION 3.52.0)

find_package(PkgConfig REQUIRED)

function(pkg_check_variable _output_name _pkg _name)
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=${_name} ${_pkg}
                    OUTPUT_VARIABLE _pkg_result
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    set("${_output_name}" "${_pkg_result}" CACHE STRING "pkg-config variable ${_name} of ${_pkg}")
endfunction()

pkg_check_modules(EVOLUTION_SHELL REQUIRED evolution-shell-3.0>=${REQUIRE_EVOLUTION_VERSION})
pkg_check_modules(EVOLUTION_CALENDAR REQUIRED evolution-calendar-3.0>=${REQUIRE_EVOLUTION_VERSION})
pkg_check_modules(EVOLUTION_MAIL REQUIRED evolution-mail-3.0>=${REQUIRE_EVOLUTION_VERSION})
pkg_check_modules(LIBECAL REQUIRED libecal-2.0>=${REQUIRE_EVOLUTION_VERSION})
pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)

pkg_check_variable(EVOLUTION_MODULE_DIR evolution-shell-3.0 moduledir)

# Evolution only loads modules from /usr/lib*/evolution/modules/
# Always use the system module directory from pkg-config

include(CheckCCompilerFlag)
check_c_compiler_flag(-Wno-deprecated-declarations HAVE_NO_DEPRECATED_DECLARATIONS)

if(HAVE_NO_DEPRECATED_DECLARATIONS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
endif()

# uninstall target
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Dist
# This generates the dist tarball from a git clone
add_custom_target(
	dist
	COMMAND git archive --prefix=${ARCHIVE_BASE_NAME}/ HEAD | xz -z > ${CMAKE_BINARY_DIR}/${ARCHIVE_FULL_NAME}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_subdirectory(src)

# Install GSettings schemas, if present
if(EXISTS ${CMAKE_SOURCE_DIR}/data)
  add_subdirectory(data)
endif()

# Install Python translation tools (arch-independent) under share
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/tools/translate/
  DESTINATION share/evolution-translate/translate
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "*.py"
  PATTERN "__pycache__" EXCLUDE
  PATTERN "*.pyc" EXCLUDE
)

# Install setup helper into /usr/bin
install(
  PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/evolution-translate-setup.sh
  DESTINATION bin
  RENAME evolution-translate-setup
)

# Install manual page for evolution-translate-setup
install(
  FILES ${CMAKE_SOURCE_DIR}/docs/man/evolution-translate-setup.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)

# Ensure the man page is compressed with gzip for Debian policy compliance
install(CODE [[
  # gzip the manpage in-place within the DESTDIR tree
  # Try both /usr/share/man/man1 and /usr/man/man1, depending on CMAKE_INSTALL_MANDIR
  set(_candidates)
  list(APPEND _candidates "${CMAKE_INSTALL_PREFIX}/share/man/man1/evolution-translate-setup.1")
  list(APPEND _candidates "${CMAKE_INSTALL_PREFIX}/man/man1/evolution-translate-setup.1")
  foreach(_man IN LISTS _candidates)
    if(DEFINED ENV{DESTDIR})
      set(_dest "$ENV{DESTDIR}${_man}")
    else()
      set(_dest "${_man}")
    endif()
    if(EXISTS "${_dest}")
      execute_process(COMMAND gzip -9n -f "${_dest}" RESULT_VARIABLE _gz_rv)
      if(NOT _gz_rv EQUAL 0)
        message(WARNING "Failed to gzip man page: ${_dest}")
      endif()
      break()
    endif()
  endforeach()
]])

# CPack configuration for Debian package generation
set(CPACK_PACKAGE_NAME "evolution-translate-extension")
set(CPACK_PACKAGE_VERSION "${VERSION}")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "Andrea Costantino <andreaivancostantino@outlook.it>")
set(CPACK_PACKAGE_VENDOR "Andrea Costantino")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Translation extension for Evolution Mail")
set(CPACK_PACKAGE_DESCRIPTION "This extension adds translation capabilities to the GNOME Evolution
email client, enabling users to translate email message bodies directly within
the application using Argos Translate for offline, privacy-focused translation.")

# Debian-specific settings
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "evolution (>= ${REQUIRE_EVOLUTION_VERSION}), python3 (>= 3.8), python3-pip, python3-venv, python3-bs4")
set(CPACK_DEBIAN_PACKAGE_SECTION "gnome")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/debian/postinst")

# Provide a Homepage field to reduce lintian noise
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/costantinoai/evolution-mail-translate")

# Use dpkg-shlibdeps to generate proper shared library dependencies
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Help dpkg-shlibdeps find Evolution private libraries
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "/usr/lib/x86_64-linux-gnu/evolution;/usr/lib/evolution")

# Install Debian-format changelog and copyright
install(FILES ${CMAKE_SOURCE_DIR}/debian/changelog
        DESTINATION share/doc/${CPACK_PACKAGE_NAME}
        RENAME changelog.Debian)
install(CODE [[
  # gzip the changelog.Debian in DESTDIR if present (package doc dir)
  if(DEFINED ENV{DESTDIR})
    set(_root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
  else()
    set(_root "${CMAKE_INSTALL_PREFIX}")
  endif()
  file(GLOB _candidates "${_root}/share/doc/*/changelog.Debian")
  foreach(_cl IN LISTS _candidates)
    if(EXISTS "${_cl}")
      execute_process(COMMAND gzip -9n -f "${_cl}" RESULT_VARIABLE _cl_rv)
      if(NOT _cl_rv EQUAL 0)
        message(WARNING "Failed to gzip changelog: ${_cl}")
      endif()
    endif()
  endforeach()
]])
install(FILES ${CMAKE_SOURCE_DIR}/debian/copyright
        DESTINATION share/doc/${CPACK_PACKAGE_NAME})

# Strip binaries during packaging to avoid unstripped-object warnings
install(CODE [[
  # Strip the plugin library if strip is available
  find_program(_STRIP_EXEC strip)
  if(_STRIP_EXEC)
    # Look for the installed module in typical locations under the DESTDIR+prefix
    if(DEFINED ENV{DESTDIR})
      set(_root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
    else()
      set(_root "${CMAKE_INSTALL_PREFIX}")
    endif()
    file(GLOB _cand
      "${_root}/lib/evolution/modules/libtranslate-module.so"
      "${_root}/lib/*/evolution/modules/libtranslate-module.so"
      "${_root}/lib64/evolution/modules/libtranslate-module.so"
    )
    foreach(_dest IN LISTS _cand)
      if(EXISTS "${_dest}")
        execute_process(COMMAND "${_STRIP_EXEC}" --strip-unneeded "${_dest}" RESULT_VARIABLE _strip_rv)
        if(NOT _strip_rv EQUAL 0)
          message(WARNING "Failed to strip plugin library: ${_dest}")
        endif()
      endif()
    endforeach()
  endif()
]])

# Normalize directory permissions to 0755 (umask-safe) to appease lintian
install(CODE [[
  if(DEFINED ENV{DESTDIR})
    set(_root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
  else()
    set(_root "${CMAKE_INSTALL_PREFIX}")
  endif()
  execute_process(COMMAND sh -c "chmod -R a+rX,go-w \"${_root}\"")
]])

# Package file name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}_${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)
# Avoid embedding build/install RPATH/RUNPATH globally (module is dlopen'ed)
set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_SKIP_INSTALL_RPATH TRUE)
set(CMAKE_BUILD_RPATH_USE_ORIGIN FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
