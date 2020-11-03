tdaq_package()

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
    src/CalibrationMath.cpp
    src/CalibrationSca.cpp
    src/CalibAlg.cpp
    src/MMTriggerCalib.cpp
    src/MMTPInputPhase.cpp
    src/sTGCTriggerCalib.cpp
    src/sTGCStripsTriggerCalib.cpp
    src/sTGCSFEBToRouter.cpp
    src/sTGCRouterToTP.cpp
    src/sTGCPadTriggerToSFEB.cpp
    src/NSWCalibRc.cpp
  LINK_LIBRARIES nswconfig NSWConfig nswcalibdal
    nswopcclient
    Boost::boost
    rt
    Protobuf
    tdaq::AltiModule
)


#--------- calibration exe --------------------------
tdaq_add_executable(calibrate app/Calibrate.cpp
  LINK_LIBRARIES nswcalib
    nswconfig
    nswopcclient
    Boost::program_options
)

tdaq_add_executable(calibrate_pulser app/calibrate_pulser.cpp
  LINK_LIBRARIES nswcalib
    nswconfig
    nswopcclient
    Boost::program_options
)
tdaq_add_executable(calibratex3 app/calibrate_3offsets.cpp
  LINK_LIBRARIES nswcalib
    nswconfig
    nswopcclient
    Boost::program_options
)

tdaq_add_executable(NSWCalibRc_main app/NSWCalibRc_main.cpp src/NSWCalibRc.cpp
  LINK_LIBRARIES nswcalib tdaq::daq-df-dal tdaq::rc_ItemCtrl
    tdaq-common::ers Boost::program_options nswcalibdal
    tdaq::AltiModule
)

tdaq_add_executable(nsw_write_patterns app/write_patterns.cpp
  LINK_LIBRARIES nswcalib tdaq::daq-df-dal tdaq::rc_ItemCtrl
    tdaq-common::ers Boost::program_options nswcalibdal
    tdaq::AltiModule
)

tdaq_add_schema(schema/NSWCalib.schema.xml)
