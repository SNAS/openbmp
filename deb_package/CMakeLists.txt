INCLUDE(InstallRequiredSystemLibraries)

SET(CPACK_GENERATOR "DEB")

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
SET (CPACK_DEBIAN_PACKAGE_VERSION ${OPENBMPD_VERSION}-${OPENBMP_VER_RELEASE})
SET (CPACK_DEBIAN_PACKAGE_MAINTAINER "www.openbmp.org")
SET (CPACK_DEBIAN_PACKAGE_DESCRIPTION "Open BGP monitoring protocol (OpenBMP) collector server. Receives BMP messages and stores them for real-time and historical analytics.")
SET (CPACK_DEBIAN_PACKAGE_SECTION "main")

#
# Set the architecture
#
FIND_PROGRAM(DPKG_CMD dpkg)
IF(NOT DPKG_CMD)
  MESSAGE(STATUS "Can not find dpkg in your path, default to i386.")
  SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
ENDIF(NOT DPKG_CMD)
EXECUTE_PROCESS(COMMAND "${DPKG_CMD}" --print-architecture
  OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

#
# Define the depends
#
#set (CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6 (>= 4.6), libmysqlcppconn7 (>= 1.1.3), libc6 (>= 2.14)")
set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)


install(FILES openbmpd.conf.upstart
        PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
        DESTINATION /etc/init
        RENAME openbmpd.conf
)
install(FILES openbmpd.defaults
        PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
        DESTINATION /etc/default
        RENAME openbmpd.new
)
install(FILES openbmpd.init
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
        DESTINATION /etc/init.d
        RENAME openbmpd
)
install(FILES openbmpd.logrotate
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
        DESTINATION /etc/logrotate.d
        RENAME openbmpd
)


#
# Define the extras (post/pre install, etc.)
#
#SET (CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/deb_package/postinst;${CMAKE_CURRENT_SOURCE_DIR}/deb_package/prerm;" )

# 
# Set the package config filename
#
SET (CPACK_OUTPUT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/debPackageConfig.cmake")
SET (CPACK_SOURCE_OUTPUT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/debPackageSourceConfig.cmake")

# Define the inscript scripts, delimit with ;
SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/postinst;${CMAKE_CURRENT_SOURCE_DIR}/preinst")

# Define the package filename
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${OPENBMP_VERSION}-src")
SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${OPENBMPD_VERSION}")

add_custom_target(deb_package
    COMMAND "${CMAKE_CPACK_COMMAND}" 
    "--config" "${CMAKE_CURRENT_SOURCE_DIR}/debPackageConfig.cmake")


# 
# include cpack module last (loads the config)
#
INCLUDE(CPack)
 
