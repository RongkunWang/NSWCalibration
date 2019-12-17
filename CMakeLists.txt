tdaq_package()

#If (DEFINED ENV{NSWCONFIG_LIB})
#    set(NSWCONFIG_LIB $ENV{NSWCONFIG_LIB})
#    message("NSWCONFIG_LIB: " ${NSWCONFIG_LIB})
#Else()
#    message( FATAL_ERROR "NSWCONFIG_LIB environment variable not defined!" )
#Endif()
#include_directories(${NSWCONFIG_LIB}/NSWConfiguration)

#--------- nswcalibration lib -----------------------
tdaq_add_library(nswcalib
						src/CalibrationMath.cpp
						src/CalibrationSca.cpp
	INCLUDE_DIRECTORIES #PRIVATE ${NSWCONFIG_LIB}
											PRIVATE ${OPC_GENERATED_CLIENT_SRCS}
	LINK_LIBRARIES nswconfig 
								 nswopcclient 
								 Boost::boost
			
)
##----- opc client lib ------------------------------
#include_directories(UaoClientForOpcUaSca/include)
#
#if (DEFINED ENV{OPC_OPEN62541_PATH})
set(OPC_OPEN62541_PATH $ENV{OPC_OPEN62541_PATH})
#    message("OPC_OPEN62541_PATH: " ${OPC_OPEN62541_PATH})
#else()
#    message( FATAL_ERROR "OPC_OPEN62541_PATH environment variable not defined!" )
#endif()
#
#set()
#include_directories(${NSWCONFIG_LIB}/UaoClientForOpcUaSca/include)
include_directories(${OPC_OPEN62541_PATH}/include)
include_directories(${OPC_OPEN62541_PATH}/build/open62541/)
#
file(GLOB OPC_GENERATED_CLIENT_SRCS UaoClientForOpcUaSca/src/*.cpp)
#--------- calibration exe --------------------------
tdaq_add_executable(calibrate main/Calibrate.cpp
#		INCLUDE_DIRECTORIES PRIVATE ${NSWCONFIG_LIB}
		LINK_LIBRARIES nswcalib
										nswconfig
										nswopcclient
										Boost::program_options
)


