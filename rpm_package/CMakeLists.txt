INCLUDE(InstallRequiredSystemLibraries)

SET(CPACK_GENERATOR "RPM")

IF(NOT UNIX)
  MESSAGE(FATAL_ERROR "Can only be run under Unix.")
ENDIF(NOT UNIX)

#
# Define standard control properties
#
SET (CPACK_PACKAGE_NAME "openbmp")
SET (CPACK_PACKAGE_VERSION_MAJOR ${OPENBMP_VER_MAJOR})
SET (CPACK_PACKAGE_VERSION_MINOR ${OPENBMP_VER_MINOR})
SET (CPACK_PACKAGE_VERSION_PATCH ${OPENBMP_VER_PATCH})
SET (CPACK_RPM_PACKAGE_VERSION ${OPENBMP_VER_MAJOR}.${OPENBMP_VER_MINOR}.${OPENBMP_VER_PATCH})
SET (CPACK_RPM_PACKAGE_RELEASE ${OPENBMP_VER_RELEASE})

SET (CPACK_RPM_PACKAGE_GROUP "bgp")

SET (CPACK_RPM_PACKAGE_LICENSE "Eclipse Public License v1.0  http://www.eclipse.org/legal/epl-v10.html")

SET (CPACK_RPM_PACKAGE_VENDOR "www.openbmp.org")
SET (CPACK_RPM_PACKAGE_URL "www.openbmp.org")

SET (CPACK_RPM_PACKAGE_SUMMARY "Open BGP monitoring protocol (OpenBMP) collector server.")
SET (CPACK_RPM_PACKAGE_DESCRIPTION "Open BGP monitoring protocol (OpenBMP) collector server. Receives BMP messages and stores them for real-time and historical analytics.")

#
# Set the architecture
#
FIND_PROGRAM(ARCH_CMD arch)
IF(NOT ARCH_CMD)
  MESSAGE(STATUS "Can not find arch in your path, default to x86_64")
  SET(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
ENDIF(NOT ARCH_CMD)
EXECUTE_PROCESS(COMMAND "${ARCH_CMD}"
  OUTPUT_VARIABLE CPACK_RPM_PACKAGE_ARCHITECTURE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

#
# Define the depends
#
SET (CPACK_RPM_PACKAGE_REQUIRES "openssl >= 1")
SET (CPACK_RPM_PACKAGE_REQUIRES "cyrus-sasl-lib >= 2")
set (CPACK_RPM_PACKAGE_SUGGESTS ON)


#
# Define the extras (post/pre install, etc.)
#

#
# Define the user file list to ingore files - this is only needed for cmake 2.8 since 3.x has auto exclude
#
SET(CPACK_RPM_SPEC_MORE_DEFINE "%define ignore \#")
SET(CPACK_RPM_USER_FILELIST "%ignore /usr" "%ignore /usr/bin")


# 
# Set the package config filename
#
SET (CPACK_OUTPUT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpmPackageConfig.cmake")
SET (CPACK_SOURCE_OUTPUT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpmPackageSourceConfig.cmake")

# Define the package filename

SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${OPENBMP_VERSION}-src")
SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${OPENBMPD_VERSION}")

add_custom_target(rpm_package
    COMMAND "${CMAKE_CPACK_COMMAND}" 
    "--config" "${CMAKE_CURRENT_SOURCE_DIR}/rpmPackageConfig.cmake")

# 
# include cpack module last (loads the config)
#
INCLUDE(CPack)
 
