

# This is a skeleton CMakeLists.txt file, auto-generated on
# Mon Mar 25 21:46:48 UTC 2024.  The developer(s) of this package should delete
# this comment as well as adding dependent targets, packages,
# etc. specific to the package. For details on how to write a package,
# please see
# https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/

cmake_minimum_required(VERSION 3.12)
project(cibmodules VERSION 1.0.1)

find_package(daq-cmake REQUIRED)

daq_setup_environment()


find_package(appfwk REQUIRED)
find_package(opmonlib REQUIRED)
find_package(Boost COMPONENTS unit_test_framework REQUIRED)
# Other modules that are needed for this specific case
find_package(rcif REQUIRED)
find_package(logging REQUIRED)
find_package(ers REQUIRED)
find_package(hsilibs REQUIRED)

##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_codegen

daq_codegen(cibmodule.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2)
daq_codegen(cibmoduleinfo.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2)

##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_library

#daq_add_library( LINK_LIBRARIES ) # Any source files and/or dependent libraries to link in not yet determined

##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_python_bindings


#daq_add_python_bindings(*.cpp LINK_LIBRARIES ${PROJECT_NAME} ) # Any additional libraries to link in beyond the main library not yet determined


##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_plugin

daq_add_plugin(CIBModule duneDAQModule LINK_LIBRARIES hsilibs::hsilibs appfwk::appfwk) # Replace appfwk library with a more specific library when appropriate

##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_application

daq_add_application(cib_monitor cib_monitor.cxx LINK_LIBRARIES ) # Any libraries to link in not yet determined
#daq_add_application(cib_simulator cib_simulator.cxx LINK_LIBRARIES ) # Any libraries to link in not yet determined

##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_unit_test

#daq_add_unit_test(Placeholder_test LINK_LIBRARIES)  # Placeholder_test should be replaced with real unit tests

##############################################################################

daq_install()

