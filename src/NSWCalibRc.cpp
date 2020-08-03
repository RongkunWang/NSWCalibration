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

    //////////////////////////////////////////////////////////////////////////////////////////////
    ISInfoDynAny calibTypeFromIS;
//    if(is_dictionary->contains("Setup.NSW.calibType")){
//      is_dictionary->getValue("Setup.NSW.calibType", calibTypeFromIS);
//      m_calibType = calibTypeFromIS.getAttributeValue<std::string>(0);
//      ERS_INFO("Calibration type obtained from IS >> "<< m_calibType);    
//    }
//    else{
//      m_calibType = "MMARTConnectivityTest";
//      nsw::NSWConfigIssue issue(ERS_HERE, "Calibration type not found in IS, default type: " + m_calibType);
//      ers::warning(issue);
//    }
//`
//    is_infodictionary.insert()
    

    m_NSWConfig = std::make_unique<NSWConfig>(m_simulation);
    m_NSWConfig->readConf(nswApp);

    ERS_LOG("End");       
}                                       

void nsw::NSWCalibRc::connect(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");
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
    // hardcore is bad but...
  std::string fname = "/afs/cern.ch/user/n/nswdaq/workspace/public/vlad/vlad_calibdev/NSWCalibrationData/config_files/for_vs_pulsing_sdsm_app_RMS_9.json";
  //std::string fname = "/afs/cern.ch/user/n/nswdaq/workspace/public/vlad/vlad_calibdev/NSWCalibrationData/config_files/vs_test_conf_no_pulser_enebled.json";

  ERS_INFO("Imported the Front-End Configuration");
    std::unique_ptr<CalibAlg> calib = 0;
//--------- add IS publication/reading when main loop operates -----
//    ERS_INFO("Calibration type:"<<m_calibType);
//    if(m_calibType=="PDOCalib" or m_calibType=="TDOCalib"){
//      calib = std::make_unique<PDOCalib>(m_calibType);
//    }
//    else{
//      throw std::runtime_error("calibration type is other than PDO/TDO or not found"); 
//    }
//
//   m_calibType ="PDOCalib";

    calib = std::make_unique<PDOCalib>(m_calibType);
    calib->setup(fname);

//    std::string reset_ecr = "rc_sender -p NSWCalibRcSender -n ALTI_RCD -c USER SendShortAsyncCommand 0x2";
//    std::string reset_bcr = "rc_sender -p NSWCalibRcSender -n ALTI_RCD -c USER SendShortAsyncCommand 0x1";

    std::string hex_data_bcr = "0x1";
    std::string hex_data_ecr = "0x2";
    std::vector<std::string> v_hex_data_sr = { "0x8" };
    std::vector<std::string> v_hex_data_ecr = { "0x2" };
    std::vector<std::string> v_hex_data_bcr = { "0x1" };
    
    std::vector<int> tpdacs = {200,300,400};
    //std::vector<int> tpdacs = {200,300,400,500};
  //  int tpdacSize = tpdacs.size();
  //  bool next_dac = true;
    int dac_counter = 0;
///======== a small test before main loop ============
//    alti_hold_trg();
//    alti_stop_pat();
//    sleep(20);
//    alti_start_pat();
//    alti_resume_trg();
///===================================================
   for(long unsigned int i_tpdac=0; i_tpdac<tpdacs.size(); i_tpdac++){
//     alti_hold_trg();
//     alti_stop_pat();
     ERS_INFO("NSWCalib::handler::Calibrating PDO with DAC = "<<tpdacs[dac_counter]);
//     sleep(3);
     calib->configure(tpdacs[dac_counter]);
//     sleep(20);
//     alti_resume_trg();
     alti_send_reset(v_hex_data_sr);
     alti_send_reset(v_hex_data_bcr);
     alti_send_reset(v_hex_data_ecr);
     sleep(5);
     ERS_INFO("NSWCalibRc::handler::Sleeping 5 sec");
     alti_start_pat();
//    alti_resume_trg();

//     calib->unconfigure();
//     sleep(2);
//   alti_resume_trg();
     sleep(30);
   
//     alti_stop_pat();
     calib->unconfigure();
     ERS_INFO("NSWCalib::handler::end of iteration nr "<<dac_counter);
     dac_counter++;
   }
//   alti_stop_pat();
   end_of_run = 1;
   ERS_INFO("NSWCalibRc::handler::calibrations done, pattern generator stopped");

}

void nsw::NSWCalibRc::alti_hold_trg(){
    ERS_INFO("ALTI - HOLDING triggers");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "HOLD";
    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    usleep(100e3);
    //sleep(60);
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
    //sleep(60);
}
void nsw::NSWCalibRc::alti_start_pat(){
    ERS_INFO("ALTI - started pattern generator");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "StartPatternGenerator";
    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    usleep(100e3);
    //sleep(60);
}
/////////////////////////////////////////////////////////////
void nsw::NSWCalibRc::alti_resume_trg(){
    ERS_INFO("ALTI - RESUMING triggers");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "RESUME";
    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    usleep(100e3);
//    sleep(6);
}

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
