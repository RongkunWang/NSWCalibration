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
    LINK_LIBRARIES tdaq::config tdaq::daq-core-dal tdaq::daq-df-dal nswconfig
)


#--------- nswcalibration lib -----------------------
tdaq_add_library(nswcalib
        src/CalibrationMath.cpp
        src/CalibrationSca.cpp
        src/NSWCalibRc.cpp
        src/BaseCalibration.cpp
        src/Phase160MHzCalibration.cpp
        src/Phase160MHzVmmCalibration.cpp
        src/Phase40MHzVmmCalibration.cpp
    INCLUDE_DIRECTORIES PRIVATE ${OPC_GENERATED_CLIENT_SRCS}
    LINK_LIBRARIES nswconfig
        nswopcclient
        Boost::boost
        rt
        Protobuf
        tdaq::rc_ItemCtrl
        tdaq-common::ers
)


##----- opc client lib ------------------------------
set(OPC_OPEN62541_PATH $ENV{OPC_OPEN62541_PATH})

include_directories(../NSWConfiguration/UaoClientForOpcUaSca/include)
include_directories(${OPC_OPEN62541_PATH}/include)
include_directories(${OPC_OPEN62541_PATH}/build/open62541/)
include_directories(${OPC_OPEN62541_PATH}/build/LogIt/include)

file(GLOB OPC_GENERATED_CLIENT_SRCS UaoClientForOpcUaSca/src/*.cpp)


#--------- calibration exe --------------------------
tdaq_add_executable(calibrate app/Calibrate.cpp
    LINK_LIBRARIES nswcalib
        nswconfig
        nswopcclient
        Boost::program_options
)

tdaq_add_executable(toggle_alti_test app/toggle_alti_test.cpp
    LINK_LIBRARIES nswcalib
        nswconfig
        nswopcclient
        tdaq::AltiModule
)

tdaq_add_executable(pulse app/small_pulsing_test.cpp
    LINK_LIBRARIES nswcalib
        nswconfig
        nswopcclient
        Boost::program_options
)

tdaq_add_executable(run_calibration app/run_calibration.cpp
    LINK_LIBRARIES nswcalib
        nswconfig
        nswopcclient
        Boost::program_options
)

tdaq_add_executable(NSWCalibRc_main app/NSWCalibRc_main.cpp src/NSWCalibRc.cpp
    LINK_LIBRARIES nswcalib tdaq::daq-df-dal tdaq::rc_ItemCtrl
        tdaq-common::ers Boost::program_options nswcalibdal nswconfig
)

tdaq_add_schema(schema/NSWCalib.schema.xml)

