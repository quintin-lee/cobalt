# vcpkg portfile for Cobalt
# https://github.com/quintin-lee/cobalt

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO quintin-lee/cobalt
    REF "v2.0.0"
    SHA512 "PLACEHOLDER"
    HEAD_REFS
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_register_package(Cobalt)

vcpkg_copy_pdbs()

file(COPY "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${SOURCE_PATH}/vcpkg-portconfig-revisions.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
