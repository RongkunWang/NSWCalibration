#ifndef PDOCALIB_H_
#define PDOCALIB_H_
//
// Class that handles PDO/TDO calibration
//
#include <iostream> 
#include <thread> 
#include <sys/types.h>
#include <chrono>
#include <string>
#include <cstring>
#include <vector>
#include <iomanip>
#include <fstream>
#include <thread>
#include <numeric>
#include <set>
#include <map>
#include <stdio.h>
#include <dirent.h>
#include <ctime>
#include <mutex>
#include <math.h>

#include "ers/ers.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"

#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/CalibrationSca.h"

#include "NSWCalibration/CalibAlg.h"

namespace nsw {

  class PDOCalib: public CalibAlg {
  
     public:
      PDOCalib(std::string calibType);
        ~PDOCalib(){};

      //===== inherited from calibalg =================================

      void setup(std::string db);

      void configure(); //have to do smth about this func. name conflict...

      void configure(int i_par, bool pdo, bool tdo, int chan, bool all_chan);

      void unconfigure();

      //===== pure pdocalib funcs ====================================  

     public:
      std::vector<nsw::FEBConfig> ReadPulsingConfig(std::string db_conf);

      void send_pulsing_configs(int i_par, bool pdo, bool tdo, int pulse_this_chan, bool all_chan);     
      
      void disable_pulser();     
      
      void setup_pulses(int which_feb, int i_tpdac, int pulse_this_chan, bool all_chan);

      //void setup_pulse_delay(int which_feb, int i_delay);
      void setup_pulse_delay(int which_feb, int i_delay, int pulse_this_chan, bool all_chan);

      void turn_off_pulses(int which_feb);

     private:
      std::string m_calibType = "";
        
      std::vector<nsw::FEBConfig> feconfigs = {};
      std::set<std::string> fenames = {};

      std::vector<std::thread> conf_threads = {};
  
  }; //class brack

} //namespace brack

#endif
