# Evolution Translate Extension build script

cmake_minimum_required(VERSION 3.15)
cmake_policy(VERSION 3.15)

project(evolution-translate-extension C)

include(GNUInstallDirs)
set(CMAKE_SKIP_RPATH TRUE)
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

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
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
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

pkg_check_modules(EVOLUTION_SHELL REQUIRED evolution-shell-3.0>=${REQUIRE_EVOLUTION_VERSION})
pkg_check_modules(EVOLUTION REQUIRED IMPORTED_TARGET
    evolution-mail-3.0>=${REQUIRE_EVOLUTION_VERSION}
    evolution-shell-3.0>=${REQUIRE_EVOLUTION_VERSION}
    json-glib-1.0)
# Evolution's pkg-config files explicitly inject -Wl,-R paths. The host already
# loads its private libraries; retain other link options without embedding paths.
get_target_property(EVOLUTION_LINK_OPTIONS PkgConfig::EVOLUTION INTERFACE_LINK_OPTIONS)
if(EVOLUTION_LINK_OPTIONS)
  list(FILTER EVOLUTION_LINK_OPTIONS EXCLUDE REGEX "^-Wl,(-R|-rpath|--rpath)(,|=|/|$)")
  set_property(TARGET PkgConfig::EVOLUTION PROPERTY INTERFACE_LINK_OPTIONS "${EVOLUTION_LINK_OPTIONS}")
endif()
pkg_get_variable(EVOLUTION_MODULE_DIR evolution-shell-3.0 moduledir)
if(NOT IS_ABSOLUTE "${EVOLUTION_MODULE_DIR}")
  message(FATAL_ERROR "Evolution did not provide an absolute module install directory")
endif()

# Check if Evolution >= 3.56 (uses new EUIManager API instead of GtkUIManager)
if(EVOLUTION_SHELL_VERSION VERSION_GREATER_EQUAL "3.56.0")
  message(STATUS "Evolution >= 3.56 detected: using EUIManager API")
  add_definitions(-DHAVE_EUI_MANAGER=1)
else()
  message(STATUS "Evolution < 3.56 detected: using legacy GtkUIManager API")
endif()

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
install(FILES requirements.txt requirements-online.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/evolution-translate)

install(
  PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/evolution-translate-setup.sh
  DESTINATION bin
  RENAME evolution-translate-setup
)

# Generate compressed install files before installation so the manifest records
# their final names and uninstall removes exactly what this project installed.
find_program(GZIP_EXEC gzip)
if(NOT GZIP_EXEC)
  message(FATAL_ERROR "gzip is required to prepare the manual and changelog")
endif()
function(compress_install_file source output)
  execute_process(COMMAND "${GZIP_EXEC}" -9n -c "${source}"
                  OUTPUT_FILE "${output}" RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not compress ${source}")
  endif()
endfunction()
compress_install_file("${CMAKE_SOURCE_DIR}/docs/man/evolution-translate-setup.1"
                      "${CMAKE_BINARY_DIR}/evolution-translate-setup.1.gz")
compress_install_file("${CMAKE_SOURCE_DIR}/debian/changelog"
                      "${CMAKE_BINARY_DIR}/changelog.Debian.gz")
install(FILES "${CMAKE_BINARY_DIR}/evolution-translate-setup.1.gz"
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

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

install(FILES "${CMAKE_BINARY_DIR}/changelog.Debian.gz" "${CMAKE_SOURCE_DIR}/debian/copyright"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${CPACK_PACKAGE_NAME})
set(CPACK_STRIP_FILES TRUE)

# Package file name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}_${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)
