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

      //void configure();

      void configure(int i_par, bool pdo, bool tdo);

      void unconfigure();

      //===== pure pdocalib funcs ====================================  

     public:
      std::vector<nsw::FEBConfig> ReadPulsingConfig(std::string db_conf);

      //void send_pulsing_configs(std::vector<nsw::FEBConfig> &frontend_configs, std::set<std::string> &frontend_names, int tpdac);     
      void send_pulsing_configs(int i_par, bool pdo, bool tdo);     
      
      void disable_pulser();     
      
      void setup_pulses(int which_feb, int i_tpdac);

      void setup_pulse_delay(int which_feb, int i_delay);

      void turn_off_pulses(int which_feb);

     // void launch_PDO_calib();

     private:
      std::string m_calibType = "";
        
      std::vector<nsw::FEBConfig> feconfigs = {};
      std::set<std::string> fenames = {};

      std::vector<std::thread> conf_threads = {};
      //std::vector<int> tpdacs = {200,300,400,500};

  
  }; //class brack

} //namespace brack

#endif
