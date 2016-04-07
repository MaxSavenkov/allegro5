# - Find OpenGLES2
# Find the native OpenGLES2 includes and libraries
#
#  MIR_INCLUDE_DIR - where to find Mir headers
#  MIR_LIBRARIES   - List of libraries when using Mir.
#  MIR_FOUND       - True if OpenGLES found.

pkg_check_modules (PKG_MIR QUIET mirclient)

set (MIR_FOUND ${PKG_MIR_FOUND})
set (MIR_INCLUDE_DIR ${PKG_MIR_INCLUDE_DIRS})
set (MIR_LIBRARIES   ${PKG_MIR_LIBRARIES})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MIR DEFAULT_MSG
    MIR_INCLUDE_DIR MIR_LIBRARIES)

mark_as_advanced(MIR_INCLUDE_DIR)
mark_as_advanced(MIR_LIBRARIES)
