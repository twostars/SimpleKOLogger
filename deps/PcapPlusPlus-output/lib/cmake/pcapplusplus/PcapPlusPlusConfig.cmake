# ~~~
# - Config file for the PcapPlusPlus package
# It defines the following variables
#  PcapPlusPlus_INCLUDE_DIRS - include directories for PcapPlusPlus
#  PcapPlusPlus_LIBRARIES    - libraries to link against
# ~~~


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was PcapPlusPlusConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/modules")

include(CMakeFindDependencyMacro)

set_and_check(PcapPlusPlus_INCLUDE_DIR
              ${PACKAGE_PREFIX_DIR}/include/pcapplusplus)

if(UNIX)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()
find_dependency(Threads)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

if(NOT PCAP_FOUND)
find_dependency(PCAP)
endif()
if(NOT Packet_FOUND)
find_dependency(Packet)
endif()


include("${CMAKE_CURRENT_LIST_DIR}/PcapPlusPlusTargets.cmake")

check_required_components(PcapPlusPlus)
