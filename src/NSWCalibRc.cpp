#include <utility>  // make_pair                                                                                                                                                                                    
#include <string>                                                                                                                                                                                                   
#include <memory>                                                                                                                                                                                                   
                                                                                                                                                                                                                    
// Header to the RC online services                   

#include "RunControl/Common/OnlineServices.h"                                                                                                                                                                       
                                                                                                                                                                                                                    
#include "NSWCalibration/NSWCalibRc.h" 
#include "NSWCalibrationDal/NSWCalibApplication.h"                                                                                                                                                                  
#include "NSWCalibration/CalibAlg.h"                     
#include "NSWConfiguration/NSWConfig.h"                     
#include "NSWCalibration/PDOCalib.h"                     
 
using boost::property_tree::ptree;

nsw::NSWCalibRc::NSWCalibRc(bool simulation):m_simulation {simulation} { 
    ERS_LOG("Constructing NSWCalibRc instance");  
    if (m_simulation) { 
        ERS_INFO("Running in simulation mode, no configuration will be sent"); 
    }                                      
 
}                                 
 
void nsw::NSWCalibRc::configure(const daq::rc::TransitionCmd& cmd) { 
    ERS_INFO("CalibHandler - configure - Start");          
    
    //Retrieve config DB
    daq::rc::OnlineServices& rcSvc = daq::rc::OnlineServices::instance();
    const daq::core::RunControlApplicationBase& rcBase = rcSvc.getApplication();
    const nsw::dal::NSWCalibApplication* nswApp = rcBase.cast<nsw::dal::NSWCalibApplication>();
    auto dbcon = nswApp->get_dbConnection();
    reset_vmm = nswApp->get_resetVMM();
    reset_tds = nswApp->get_resetTDS();

    //Retrieve ipc partition
    m_ipcpartition = rcSvc.getIPCPartition();
    
    //getting IS dictionary for this partition
    is_dictionary = new ISInfoDictionary(m_ipcpartition);
    std::string g_calibration_type="";
    std::string g_info_server_name="";
    const std::string stateInfoName = g_info_server_name + ".CurrentCalibState";
    const std::string calibInfoName = g_info_server_name + "." + g_calibration_type + "CalibInfo";

     // Announce the current calibType, and
    // publish metadata to IS (in progress)
    m_calibType = calibTypeFromIS();
    int init_trgCalKey = -1;
    publish4swrod(init_trgCalKey);
    sleep(5);
    m_NSWConfig = std::make_unique<NSWConfig>(m_simulation);
    m_NSWConfig->readConf(nswApp);

    ERS_LOG("End");       
}                                       

void nsw::NSWCalibRc::connect(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");
		m_calibType = calibTypeFromIS(); 
    //getting configuration from ptree 
    ptree conf = m_NSWConfig->getConf();
    write_xml(std::cout, conf);

    //Send configuratoin for this run
    m_NSWConfig->substituteConf(conf);
    //send out configuration
    m_NSWConfig->configureRc();
    ERS_INFO("End");

}

void nsw::NSWCalibRc::disconnect(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");
    m_NSWConfig->unconfigureRc();
    ERS_INFO("End");

}

void nsw::NSWCalibRc::unconfigure(const daq::rc::TransitionCmd& cmd) { 
    ERS_INFO("Start"); 
    ERS_INFO("End"); 
} 
 
               
void nsw::NSWCalibRc::prepareForRun(const daq::rc::TransitionCmd& cmd) { 
    ERS_LOG("Start"); 
    ERS_INFO("initialization/prepare_for_run step here"); 
    end_of_run = 0;
    handler_thread = std::async(std::launch::async, &nsw::NSWCalibRc::handler, this);
    ERS_LOG("End");                     
}                                                
                                     
void nsw::NSWCalibRc::stopRecording(const daq::rc::TransitionCmd& cmd) { 
    ERS_LOG("Start");
    end_of_run = 1;
    ERS_LOG("End");                                                            
}                                                
                                                               
void nsw::NSWCalibRc::user(const daq::rc::UserCmd& usrCmd) { 
  ERS_LOG("User command received: " << usrCmd.commandName());      
} 

void nsw::NSWCalibRc::subTransition(const daq::rc::SubTransitionCmd& cmd) {
  auto main_transition = cmd.mainTransitionCmd();
  auto sub_transition = cmd.subTransition();
  ERS_LOG("Subtransition command:"<< sub_transition <<"[MAIN transition - "<< main_transition <<"]");
}

void nsw::NSWCalibRc::handler(){

  ERS_INFO("NSWCalibration::handler::Starting the calibration run");

    daq::rc::OnlineServices& rcSvc = daq::rc::OnlineServices::instance();
    const daq::core::RunControlApplicationBase& rcBase = rcSvc.getApplication();
    const nsw::dal::NSWCalibApplication* nswApp = rcBase.cast<nsw::dal::NSWCalibApplication>();
    std::string config = nswApp->get_dbConnection();
 
  bool pdo = false;
  bool tdo = false;
  bool all_chan = true;
  int Nchan = 1;

  if(m_calibType == "PDOCalib"){
    pdo=true;
    //tdo=false;
    //Nchan=1;
  }
  if(m_calibType == "PDOCalibSingleChan"){
    pdo=true;
    //tdo=false;
    Nchan=64;
    all_chan=false;
  }
  if(m_calibType == "TDOCalib"){
    tdo=true;
    //pdo=false;
   // Nchan=1;
  }
  if(m_calibType == "TDOCalibSingleChan"){
    tdo=true;
    //pdo=false;
    Nchan=64;
    all_chan=false;
  }

  calib = 0;
//--------- add IS publication/reading when main loop operates -----
   ERS_INFO("Calibration type:"<<m_calibType);
   if(m_calibType=="PDOCalib" ||
 		 m_calibType=="TDOCalib" || 
 		 m_calibType=="PDOCalibSingleChan" || 
 		 m_calibType=="TDOCalibSingleChan") 
   {
     calib = std::make_unique<PDOCalib>(m_calibType);
     ERS_INFO("unique pointer to PDOCalib made based on the IS calib type entry = " << m_calibType);
     if(calib!=0){ ERS_INFO("Calib pointer status is non zero");}
   }
   else{ 
      std::string msg = m_calibType + "-> calibration type is not supported or does not exist";
 	   nsw::NSWCalibIssue issue(ERS_HERE, msg);
   	 ers::error(issue);
 		 throw std::runtime_error(msg); 
   }

   try{
      calib->setup(config);
   }catch(std::exception &ex){
      nsw::NSWCalibIssue issue(ERS_HERE, ex.what());
   	  ers::error(issue);
 		  throw std::runtime_error(ex.what()); 
   } 
 
    std::vector<std::string> v_hex_data_sr = { "0x8" };
    std::vector<std::string> v_hex_data_ecr = { "0x2" };
    std::vector<std::string> v_hex_data_bcr = { "0x1" };
    
//    std::vector<int> tpdacs = {200,300,400,500,600,700,800,900,1000};// testing full range
    std::vector<int> tpdacs = {200,300,400,500};
//    std::vector<int> delays = {0,1,2,3,4,5,6,7};// full range of delays
    std::vector<int> delays = {0,1,2,3,4};
//    int i_counter = 0;

    int n_delays = delays.size();
    int n_dacs = tpdacs.size();
    
    int loop_max;
    if(pdo){loop_max = n_dacs;}
    if(tdo){loop_max = n_delays;}
///===================================================

    sleep(15);
    alti_stop_pat();
//    chan=0;
    sleep(5);
    ERS_INFO("Setting 6ns delay");
  	publish4swrod(2);
    sleep(5);
    calib->configure(2, pdo, tdo, Nchan, all_chan);
       alti_send_reset(v_hex_data_sr);
       alti_send_reset(v_hex_data_bcr);
       alti_send_reset(v_hex_data_ecr);

    sleep(5);
    alti_start_pat();
    sleep(10);
    alti_stop_pat();
    sleep(10);
    ERS_INFO("Setting 12ns delay");
    calib->unconfigure();
  	publish4swrod(4);
    calib->configure(4, pdo, tdo, Nchan, all_chan);
       alti_send_reset(v_hex_data_sr);
       alti_send_reset(v_hex_data_bcr);
       alti_send_reset(v_hex_data_ecr);

    alti_start_pat();
    sleep(10);
    alti_stop_pat();

///===================================================
//   alti_stop_pat();
//   for(int i_step=0; i_step<loop_max; i_step++){
//
//    for(int chan=0; chan<Nchan; chan++){
//
//       int i_par;
//       if(i_counter==0){alti_stop_pat();}
//       if(pdo){
//         i_par = tpdacs[i_step];
//         ERS_INFO("NSWCalib::handler::Calibrating PDO with pulser DAC = "<<tpdacs[i_step]);
//       }
//       if(tdo){
//         i_par = delays[i_step];
//         ERS_INFO("NSWCalib::handler::Calibrating TDO with delay = "<<delays[i_step]*3 <<" [ns]");
//       }
// 
//        ERS_INFO("CHECK: ALLchan{"<<all_chan<<"} i_par{"<<i_par<<"} tdo{"<<tdo<<"} pdo{"<<pdo<<"}");
//        sleep(5);
//    	 publish4swrod(i_par);
//       if(pdo){
//         try{
//          calib->configure(tpdacs[i_step], pdo, tdo, chan, all_chan);
//          ERS_INFO("PDO calib config sent");
//         }catch(std::exception &ex){
//       	  nsw::NSWCalibIssue issue(ERS_HERE, ex.what());
//   	      ers::error(issue);
// 		      throw std::runtime_error(ex.what()); 
//         } 
//       }
//       if(tdo){
//         try{
//          calib->configure(delays[i_step], pdo, tdo, chan, all_chan);
//         }catch(std::exception &ex){
//       	  nsw::NSWCalibIssue issue(ERS_HERE, ex.what());
//   	      ers::error(issue);
// 		      throw std::runtime_error(ex.what()); 
//         }
//         ERS_INFO("TDO calib config sent");
//       }
//       else{
//         ERS_INFO("Something went wrong...");
//         break;
//       } 
//      
////       ERS_INFO("A");
////    	 publish4swrod(i_par);
////       ERS_INFO("B");
//       sleep(3);   
//       alti_send_reset(v_hex_data_sr);
//       alti_send_reset(v_hex_data_bcr);
//       alti_send_reset(v_hex_data_ecr);
//       int sleeptime = 5;
//       ERS_INFO("C");
//       ERS_INFO("NSWCalibRc::handler::Recording data for [ "<<sleeptime<<" ] sec");
//       alti_start_pat();
//  
////       ERS_INFO("D");
//       sleep(sleeptime);
//  
//       alti_stop_pat();
//
////       ERS_INFO("E");
//       ERS_INFO("NSWCalib::handler:: done with channel:"<<chan);
//       i_counter++;
//      }// channel loop end here
//   
//   }// DAC/Delay loop vector ends
   end_of_run = 1;
   ERS_INFO("NSWCalibRc::handler::calibrations done, pattern generator stopped");
}

// strating and stopping pattern generator explicitly
void nsw::NSWCalibRc::alti_stop_pat(){
    ERS_INFO("ALTI - stopped pattern generator");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "StopPatternGenerator";
    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    usleep(100e3);
}
void nsw::NSWCalibRc::alti_start_pat(){
    ERS_INFO("ALTI - started pattern generator");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "StartPatternGenerator";
    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    usleep(100e3);
}
/////////////////////////////////////////////////////////////

void nsw::NSWCalibRc::alti_send_reset(std::vector<std::string> hex_data){
    ERS_INFO("ALTI - SENDING reset signals to FEBs");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "SendAsyncShortCommand";
    std::vector<std::string> cmd_args = {hex_data};
    daq::rc::UserCmd cmd(cmd_name, cmd_args);
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    sleep(2);
}
//---------------- borrowed from master branch --------------------------
void nsw::NSWCalibRc::publish4swrod(int i_par) {
  //
  // Commented out for now.
  // This is powerful and risky.
  //
   if (calib) {
     //is_dictionary->checkin(m_calibCounter, ISInfoInt(calib->counter()));
     is_dictionary->checkin(m_calibCounter, ISInfoInt(i_par));
     ERS_INFO("Publishing following parameter to swrod plugin >>" << i_par);
   } else {
     is_dictionary->checkin(m_calibCounter, ISInfoInt(-1));
     ERS_INFO("Updated triggerCalibrationKey w value [-1]");
   }
   sleep(2);
   wait4swrod();
   sleep(2);
}

void nsw::NSWCalibRc::wait4swrod() {
  if(calib){
    ISInfoInt trigger_par;
    try{
      is_dictionary->getValue(m_calibCounter, trigger_par);
      ERS_INFO("Reading back updated triggerCalibrationKey is >> "<< trigger_par);
    }catch(daq::is::Exception &ex){
       ers::error(ex);
    }
  }
  if (!calib->wait4swrod())
    return; 
  //ISInfoInt counter(-1);
 // ISInfoInt counter(-1);
  //ERS_INFO("calib waiting for swROD...");
//  int attempt_i = 0;
//  int attempts_max = 5;
//  //while (counter.getValue() != calib->counter()) {
//  while (counter.getValue() != calib->counter()) {
//    try {
//      is_dictionary->getValue(m_calibCounter_readback, counter);
//    } catch(daq::is::Exception& ex) {
//      ers::error(ex);
//    }
//    // usleep(100e3);
//    ERS_INFO("calib waiting for swROD, attempt " << attempt_i);
//    usleep(100e3);
//    // usleep(1e6);
//    attempt_i++;
//    if (attempt_i >= attempts_max)
//      throw std::runtime_error("Waiting for swROD failed");
//  }
}

std::string nsw::NSWCalibRc::calibTypeFromIS() {
  // Grab the calibration type string from IS
  // Can manually write to this variable from the command line:
  // > is_write -p part-BB5-Calib -n Setup.NSW.calibType -t String  -v MMARTPhase -i 0
  // > is_ls -p part-BB5-Calib -R ".*NSW.cali.*" -v
  // Currently supported options are written in the `handler` function.
  std::string calibType;
  if(is_dictionary->contains("Setup.NSW.calibType") ){
    ISInfoDynAny calibTypeFromIS;
    is_dictionary->getValue("Setup.NSW.calibType", calibTypeFromIS);
    calibType = calibTypeFromIS.getAttributeValue<std::string>(0);
    ERS_INFO("Calibration type from IS: " << calibType);
    if (m_calibType != "" && calibType != m_calibType) {
      std::string msg = "Found a new calibType. Was " + m_calibType + ", is now " + calibType;
      nsw::NSWCalibIssue issue(ERS_HERE, msg);
      ers::warning(issue);
    }
  } else {
    calibType = "MMARTConnectivityTest";
    nsw::NSWCalibIssue issue(ERS_HERE, "Calibration type not found in IS. Defaulting to: " + calibType);
    ers::warning(issue);
  }
  ISInfoDynAny runParams;
  is_dictionary->getValue("RunParams.RunParams", runParams);
  runParams.setAttributeValue<std::string>(4,"Calibration");
  runParams.setAttributeValue<std::string>(8,calibType);
  is_dictionary->update("RunParams.RunParams", runParams);
  return calibType;
}

