tdaq_package()

if (DEFINED ENV{NSWCONFIG_LIB})
    set(NSWCONFIG_LIB $ENV{NSWCONFIG_LIB})
    message("NSWCONFIG_LIB: " ${NSWCONFIG_LIB})
else()
    message( FATAL_ERROR "NSWCONFIG_LIB environment variable not defined!" )
endif()
include_directories(${NSWCONFIG_LIB}/NSWConfiguration)

tdaq_add_header_directory(${NSWCONFIG_LIB}/NSWConfiguration/NSWConfiguration)

tdaq_add_library(nswconfig
#	${NSWCONFIG_LIB}/src/*.cpp	
	${NSWCONFIG_LIB}/src/ConfigSender.cpp	
	${NSWCONFIG_LIB}/src/ConfigReader.cpp
	${NSWCONFIG_LIB}/src/ConfigReaderApi.cpp
	${NSWCONFIG_LIB}/src/FEConfig.cpp	
	${NSWCONFIG_LIB}/src/FEBConfig.cpp	
	${NSWCONFIG_LIB}/src/VMMConfig.cpp	
	${NSWCONFIG_LIB}/src/ROCConfig.cpp	
	${NSWCONFIG_LIB}/src/VMMCodec.cpp	
	${NSWCONFIG_LIB}/src/Utility.cpp
	${NSWCONFIG_LIB}/src/I2cMasterConfig.cpp
	${NSWCONFIG_LIB}/src/OpcClient.cpp
	${NSWCONFIG_LIB}/src/ARTConfig.cpp
	${NSWCONFIG_LIB}/src/ADDCConfig.cpp
#	${NSWCONFIG_LIB}/src/ADDCConfig.cpp
#	${NSWCONFIG_LIB}/src/ADDCConfig.cpp
#	${NSWCONFIG_LIB}/src/ADDCConfig.cpp
	INCLUDE_DIRECTORIES PRIVATE ${NSWCONFIG_LIB}
	LINK_LIBRARIES 	Boost::boost
									nswopcclient	
									tdaq-common::ers
)

#tdaq_add_library(nswconfig
#						${NSWCONFIG_SO}libnswconfig.so
#)
#--------- nswcalibration stuff -----------------------
tdaq_add_library(nswcalib
						src/CalibrationMath.cpp
						src/CalibrationSca.cpp
	INCLUDE_DIRECTORIES PRIVATE ${NSWCONFIG_LIB}
											PRIVATE ${OPC_GENERATED_CLIENT_SRCS} 
	LINK_LIBRARIES nswconfig 
								 nswopcclient 
								 Boost::boost
			
)
##----- opcua related ------------------------------
include_directories(UaoClientForOpcUaSca/include)

if (DEFINED ENV{OPC_OPEN62541_PATH})
    set(OPC_OPEN62541_PATH $ENV{OPC_OPEN62541_PATH})
    message("OPC_OPEN62541_PATH: " ${OPC_OPEN62541_PATH})
else()
    message( FATAL_ERROR "OPC_OPEN62541_PATH environment variable not defined!" )
endif()

include_directories(${NSWCONFIG_LIB}/UaoClientForOpcUaSca/include)
include_directories(${OPC_OPEN62541_PATH}/include)
include_directories(${OPC_OPEN62541_PATH}/build/open62541/)

file(GLOB OPC_GENERATED_CLIENT_SRCS UaoClientForOpcUaSca/src/*.cpp)

tdaq_add_library(nswopcclient
        ${NSWCONFIG_LIB}/src/OpcClient.cpp
        ${NSWCONFIG_LIB}/UaoClientForOpcUaSca/src/ClientSessionFactory.cpp
        ${OPC_GENERATED_CLIENT_SRCS}
	INCLUDE_DIRECTORIES PRIVATE ${NSWCONFIG_LIB}
  LINK_LIBRARIES
        Boost::boost
        tdaq-common::ers
        ${OPC_OPEN62541_PATH}/build/libopen62541-compat.so
        ${OPC_OPEN62541_PATH}/build/open62541/libopen62541.a
        rt
)

tdaq_add_executable(calibrate main/Calibrate.cpp
		INCLUDE_DIRECTORIES PRIVATE ${NSWCONFIG_LIB}
		LINK_LIBRARIES nswcalib
										nswconfig
										nswopcclient
										Boost::program_options
)


