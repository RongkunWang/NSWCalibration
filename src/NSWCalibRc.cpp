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
    publish4swrod();
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

  ERS_INFO("Starting the PDO calibration run");
  //std::string fname = "/afs/cern.ch/user/n/nswdaq/workspace/public/vlad/vlad_calibdev/NSWCalibrationData/config_files/vs_test_conf_no_pulser_enebled.json";
    daq::rc::OnlineServices& rcSvc = daq::rc::OnlineServices::instance();
    const daq::core::RunControlApplicationBase& rcBase = rcSvc.getApplication();
    const nsw::dal::NSWCalibApplication* nswApp = rcBase.cast<nsw::dal::NSWCalibApplication>();
    std::string config = nswApp->get_dbConnection();
 
  ERS_INFO("Imported the Front-End Configuration");

  bool pdo = false;
  bool tdo = false;

  if(m_calibType == "PDOCalib"){pdo=true;}
  if(m_calibType == "TDOCalib"){tdo=true;}

  calib = 0;
//--------- add IS publication/reading when main loop operates -----
   ERS_INFO("Calibration type:"<<m_calibType);
   if(m_calibType=="PDOCalib" ||
 		 m_calibType=="TDOCalib") {
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
//   std::string config = dbcon;
//  ERS_INFO("Mark1: dbcon << "<< dbcon);
  ERS_INFO("Mark1.5: config << "<< config);
//    calib = std::make_unique<PDOCalib>(m_calibType);
   // calib->setup(dbcon);
    calib->setup(config);
//    ERS_INFO("calib wait4swrod: " << calib->wait4swrod());

//    std::string reset_ecr = "rc_sender -p NSWCalibRcSender -n ALTI_RCD -c USER SendShortAsyncCommand 0x2";
//    std::string reset_bcr = "rc_sender -p NSWCalibRcSender -n ALTI_RCD -c USER SendShortAsyncCommand 0x1";

  ERS_INFO("Mark2");
    std::vector<std::string> v_hex_data_sr = { "0x8" };
    std::vector<std::string> v_hex_data_ecr = { "0x2" };
    std::vector<std::string> v_hex_data_bcr = { "0x1" };
    
    std::vector<int> tpdacs = {200,300,400,500,600,700,800,900,1000};// testing full range
    //std::vector<int> tpdacs = {200,300,400};
    std::vector<int> delays = {0,1,2,3,4,5,6,7};// full range of delays
    //std::vector<int> delays = {2,4,6};
    int i_counter = 0;
    int n_delays = delays.size();
    int n_dacs = tpdacs.size();
    
    int loop_max;
    if(pdo){loop_max = n_dacs;}
    if(tdo){loop_max = n_delays;}
  ERS_INFO("Mark3");
///======== a small test before main loop ============
//    alti_hold_trg();
//    sleep(20);
//
//    ERS_INFO("Trying to stop pattern generator");
//    alti_stop_pat();
//
//    alti_send_reset(v_hex_data_sr);
//    sleep(2);
//    alti_send_reset(v_hex_data_bcr);
//    sleep(2);
//    alti_send_reset(v_hex_data_ecr);
//    sleep(2);
//
//    alti_start_pat();

///===================================================
  // for(long unsigned int i_tpdac=0; i_tpdac<tpdacs.size(); i_tpdac++){
   for(int i_step=0; i_step<loop_max; i_step++){

     if(i_counter==0){alti_stop_pat();}
     if(pdo){ERS_INFO("NSWCalib::handler::Calibrating PDO with pulser DAC = "<<tpdacs[i_step]);}
     if(tdo){ERS_INFO("NSWCalib::handler::Calibrating TDO with delay = "<<delays[i_step]*3 <<" [ns]");}
		 
//		 publish4swrod();
     if(pdo){calib->configure(tpdacs[i_step], pdo, tdo);}
     else if(tdo){calib->configure(delays[i_step], pdo, tdo);}
     else{
       ERS_INFO("Something went wrong...");
       break;
     } 

     alti_send_reset(v_hex_data_sr);
		 sleep(2);
     alti_send_reset(v_hex_data_bcr);
		 sleep(2);
     alti_send_reset(v_hex_data_ecr);
     int sleeptime = 10;
     ERS_INFO("NSWCalibRc::handler::Sleeping "<<sleeptime<<" sec");
     alti_start_pat();

     sleep(sleeptime);

     alti_stop_pat();
//     sleep(0.5);
//     calib->unconfigure();
     ERS_INFO("NSWCalib::handler::end of iteration nr "<<i_counter+1);
     i_counter++;
   }
//   alti_stop_pat();
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
//    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::UserCmd cmd(cmd_name, cmd_args);
    //daq::rc::UserCmd cmd(cmd_name, hex_data);
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
//    usleep(100e3);
    sleep(2);
}
//---------------- borrowed from master branch --------------------------
void nsw::NSWCalibRc::publish4swrod() {
  //
  // Commented out for now.
  // This is powerful and risky.
  //
  // if (calib) {
  //   is_dictionary->checkin(m_calibCounter, ISInfoInt(calib->counter()));
  // } else {
  //   is_dictionary->checkin(m_calibCounter, ISInfoInt(-1));
  // }
  // wait4swrod();
}

void nsw::NSWCalibRc::wait4swrod() {
  if (!calib)
    return;
  if (!calib->wait4swrod())
    return;
  ISInfoInt counter(-1);
  ERS_INFO("calib waiting for swROD...");
  int attempt_i = 0;
  int attempts_max = 5;
  while (counter.getValue() != calib->counter()) {
    try {
      is_dictionary->getValue(m_calibCounter_readback, counter);
    } catch(daq::is::Exception& ex) {
      ers::error(ex);
    }
    // usleep(100e3);
    ERS_INFO("calib waiting for swROD, attempt " << attempt_i);
    usleep(100e3);
    // usleep(1e6);
    attempt_i++;
    if (attempt_i >= attempts_max)
      throw std::runtime_error("Waiting for swROD failed");
  }
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
