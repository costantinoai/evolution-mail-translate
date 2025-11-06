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
set(VERSION "1.0.0")
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

# Install Python translation tools
# Use fixed lib path since Python scripts are architecture-independent
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/tools/translate/
  DESTINATION lib/evolution-translate/translate
  FILES_MATCHING
  PATTERN "*.py"
  PATTERN "__pycache__" EXCLUDE
  PATTERN "*.pyc" EXCLUDE
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

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
    "${CMAKE_SOURCE_DIR}/debian/postinst;${CMAKE_SOURCE_DIR}/debian/prerm;${CMAKE_SOURCE_DIR}/debian/postrm")

# Package file name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}_${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)
