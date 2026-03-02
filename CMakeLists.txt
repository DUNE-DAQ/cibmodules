#
# CIBmodules package: dune-daq interface for the Calibration Interface Board
#
cmake_minimum_required(VERSION 3.12)
project(cibmodules VERSION 2.0.0)

find_package(daq-cmake REQUIRED)

daq_setup_environment()


find_package(appfwk REQUIRED)
find_package(opmonlib REQUIRED)
# Other modules that are needed for this specific case
find_package(logging REQUIRED)
find_package(ers REQUIRED)
find_package(hsilibs REQUIRED)
find_package(timinglibs REQUIRED)
find_package(daqconf REQUIRED)
find_package(Boost COMPONENTS unit_test_framework REQUIRED)
##############################################################################
# Protobuf code generation for data structures
daq_protobuf_codegen( opmon/*.proto )
##############################################################################

daq_add_library( *.cpp LINK_LIBRARIES 
                 opmonlib::opmonlib ers::ers appfwk::appfwk logging::logging stdc++fs hsilibs::hsilibs )

daq_add_plugin(CIBModule duneDAQModule            LINK_LIBRARIES cibmodules )

# timinglibs::dal is needed to avoid a link error; it can be removed once timinglibs #155 is addressed
daq_add_unit_test(CIB_test LINK_LIBRARIES cibmodules timinglibs::dal)

daq_install()
