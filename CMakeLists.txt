tdaq_package() #This will make a package called NSWCalibration

remove_definitions(-DERS_NO_DEBUG)

tdaq_add_executable(send_config_and_ltp_testpulse app/send_config_and_ltp_testpulse.cpp
  LINK_LIBRARIES Boost::program_options nswconfig
)

