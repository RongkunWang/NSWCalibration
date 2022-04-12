cmake_minimum_required(VERSION 3.14.0)

add_subdirectory(extern)

tdaq_package()
set(TDAQ_DB_PROJECT muons)

option(ENABLE_NSW_DEBUG "Compile with all DEBUG messages" OFF)
if(ENABLE_NSW_DEBUG)
  remove_definitions(-DERS_NO_DEBUG)
endif()

install (DIRECTORY DESTINATION ${CMAKE_INSTALL_PREFIX}/${BINARY_TAG}/bin/)
tdaq_add_scripts(bin/nsw_art_input_phase.py             python/nsw_art_input_phase.py)
tdaq_add_scripts(bin/nsw_increase_threshold_in_json.py  python/nsw_increase_threshold_in_json.py)
tdaq_add_scripts(bin/nsw_mmtp_channelrates.py           python/nsw_mmtp_channelrates.py)
tdaq_add_scripts(bin/nsw_mmtp_diagnostics.py            python/nsw_mmtp_diagnostics.py)
tdaq_add_scripts(bin/nsw_mmtp_event_display.py          python/nsw_mmtp_event_display.py)
tdaq_add_scripts(bin/nsw_plot_art_hits.py               python/nsw_plot_art_hits.py)
tdaq_add_scripts(bin/nsw_padtrigger_inputdelays.py      python/nsw_padtrigger_inputdelays.py)
tdaq_add_scripts(bin/nsw_pads_hit_rate.py               python/nsw_pads_hit_rate.py)
tdaq_add_scripts(bin/nsw_trigger_mapping.py             python/nsw_trigger_mapping.py)

find_package(ROOT REQUIRED COMPONENTS Core Tree RIO)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

tdaq_generate_dal(schema/NSWCalib.schema.xml
  NAMESPACE nsw::dal
  INCLUDE_DIRECTORIES DFConfiguration dal
  INCLUDE NSWCalibrationDal
  CPP_OUTPUT dal_cpp_srcs
)

# Build a C++ library out of the generated files
tdaq_add_library(nswcalibdal DAL
  ${dal_cpp_srcs}
  LINK_LIBRARIES tdaq::config tdaq::daq-core-dal tdaq::daq-df-dal
)

#--------- nswcalibration lib -----------------------
tdaq_add_library(nswcalib
    src/PDOCalib.cpp
    src/ScaCalibration.cpp
    src/VmmTrimmerScaCalibration.cpp
    src/VmmBaselineScaCalibration.cpp
    src/THRCalib.cpp
    src/Utility.cpp
    src/CalibrationMath.cpp
    src/CalibAlg.cpp
    src/MMTriggerCalib.cpp
    src/MMTPInputPhase.cpp
    src/sTGCTriggerCalib.cpp
    src/sTGCPadVMMTDSChannels.cpp
    src/sTGCStripsTriggerCalib.cpp
    src/sTGCSFEBToRouter.cpp
    src/sTGCPadTriggerToSFEB.cpp
    src/sTGCPadTriggerInputDelays.cpp
    src/sTGCPadsControlPhase.cpp
    src/sTGCPadsL1DDCFibers.cpp
    src/sTGCPadsHitRateL1a.cpp
    src/sTGCPadsHitRateSca.cpp
    src/NSWCalibRc.cpp
    src/MMTPFeedback.cpp
    src/MMTPStatusRegisters.cpp
    src/RocPhaseCalibrationBase.cpp
    src/RocPhase40MhzCore.cpp
    src/RocPhase160MhzCore.cpp
    src/RocPhase160MhzVmm.cpp
  INCLUDE_DIRECTORIES
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/omni/include>
  LINK_LIBRARIES nswconfig  NSWConfig nswcalibdal
    nswhwinterface
    nswopcclient
    LogIt
    Boost::boost
    rt
    Protobuf
    tdaq::AltiModule
    ROOT::Core
    ROOT::Tree
    ROOT::RIO
    PRIVATE
      stdc++fs
)

## TODO these lines, and the associated files, are to be removed if no
## one complains about missing them
#--------- calibration exe --------------------------
# tdaq_add_executable(calibrate app/Calibrate.cpp
#   LINK_LIBRARIES nswcalib
#     nswconfig
#     nswopcclient
#     Boost::program_options
# )

# tdaq_add_executable(calibrate_pulser app/calibrate_pulser.cpp
#   LINK_LIBRARIES nswcalib
#     nswconfig
#     nswopcclient
#     Boost::program_options
# )

tdaq_add_executable(NSWCalibRc_main app/NSWCalibRc_main.cpp src/NSWCalibRc.cpp
  INCLUDE_DIRECTORIES
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/omni/include>
  LINK_LIBRARIES nswcalib tdaq::daq-df-dal tdaq::rc_ItemCtrl
    tdaq-common::ers Boost::program_options nswcalibdal
    tdaq::AltiModule
)

tdaq_add_executable(nsw_art_hit_counters app/art_hit_counters.cpp
  LINK_LIBRARIES nswcalib tdaq-common::ers Boost::program_options
)

tdaq_add_executable(nsw_mmtp_diagnostics app/mmtp_diagnostics.cpp
  LINK_LIBRARIES nswcalib tdaq-common::ers Boost::program_options
)

tdaq_add_executable(nsw_mmtp_feedback app/mmtp_feedback.cpp
  LINK_LIBRARIES nswcalib tdaq-common::ers Boost::program_options
)

tdaq_add_schema(schema/NSWCalib.schema.xml)
