tdaq_package()

#--------- nswcalibration lib -----------------------
tdaq_add_library(nswcalib
						src/CalibrationMath.cpp
						src/CalibrationSca.cpp
	INCLUDE_DIRECTORIES	PRIVATE ${OPC_GENERATED_CLIENT_SRCS}
	LINK_LIBRARIES nswconfig 
								 nswopcclient 
								 Boost::boost
			
)
##----- opc client lib ------------------------------
set(OPC_OPEN62541_PATH $ENV{OPC_OPEN62541_PATH})

#set()
include_directories(${OPC_OPEN62541_PATH}/include)
include_directories(${OPC_OPEN62541_PATH}/build/open62541/)
#
file(GLOB OPC_GENERATED_CLIENT_SRCS UaoClientForOpcUaSca/src/*.cpp)
#--------- calibration exe --------------------------
tdaq_add_executable(calibrate main/Calibrate.cpp
		LINK_LIBRARIES nswcalib
										nswconfig
										nswopcclient
										Boost::program_options
)

tdaq_add_executable(pulse main/small_pulsing_test.cpp
		LINK_LIBRARIES nswcalib
										nswconfig
										nswopcclient
										Boost::program_options
)

