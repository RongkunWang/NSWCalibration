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

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"

#include "boost/foreach.hpp"
#include <boost/program_options.hpp>
#include "boost/property_tree/json_parser.hpp"

#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/CalibrationSca.h"

namespace po = boost::program_options;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int ac, const char* av[]){

  std::cout<<"========================================================================================"<<std::endl;
  std::cout<<"====================== HELLO THERE! ===================================================="<<std::endl;
  std::cout<<"========================================================================================"<<std::endl;

  auto start= std::chrono::high_resolution_clock::now();
  //--------------------------------------------------------------
  namespace pt = boost::property_tree;
  pt::ptree input_data;
  std::string io_config_path = "../../NSWCalibration/lxplus_input_data.json"; //<<---- default input_data.json path file location!!!!

  //    std::string io_config_path = "../../NSWCalibration/bb5_sectA14_input_data.json"; //<<---- can have another path file in same direcroty but then recompile with default file path commented out and new file location in
  pt::read_json(io_config_path, input_data);
  //-------------------------------------------------------------------
  std::string   def_config = input_data.get<std::string>("configuration_json");
  std::string base_folder = input_data.get<std::string>("config_dir");
  std::string cl_file = input_data.get<std::string>("report_log");

  std::string description = "\tProgramm allows to configure/calibrate MM FEBs\n\t declare what you want to do, type:\n\t --init_conf - to load inital VMM configuration or final configuration with resulting json file\n\t --threshold - to read thresholds of the particular VMM\n\t --cal_thresholds - to calibrate threshold and trimmer DAC on FEB scale\n\t --merge_config - to merge separate board .json files into one\t default name -> generated_config.json (name changing option -j)\n\n\t!!!\n\tIMPORTANT: programm requires existance of the input configuration .json file with necessary output file location paths, OPC server name and associated communication port\n\t!!!";

  bool init_conf;
  bool threshold;
  bool cal_thresholds;
  bool merge_config;
  bool split_config;
  bool baseline;
  bool debug;
  //    bool stgc;

  //            bool pFEB; //swithch for pFEBs
  bool conn_check; //swithch baseline checks
  int N_FEB;
  int n_samples;
  int rms;

  std::set<std::string> board_set;
  std::vector<int> board_set_int;
  std::string config_filename;
  std::string mod_json;
  std::string dw_layer;
  std::string fe_name;
  po::options_description desc(description);
  desc.add_options()
    ("help,h", "produce help message")
    ("config,c", po::value<std::string>(&config_filename)->default_value(base_folder+def_config),"Configuration .json file. If not specified choses from input_data.json - [configuration_json] ")
    ("new_json,j", po::value<std::string>(&mod_json)->default_value("generated_config"),"Generated json file name, type in -> example_config.json")
    ("layer_dw,L", po::value<std::string>(&dw_layer)->default_value(""),"Layer in the DW to configure/calibrate, type in -> L1/L2/L3/L4")
    ("samples,s", po::value<int>(&n_samples)->
     default_value(10), "Number of ADC samples to read per channel >>> For the baselines a multiplication factor of x10 is implemented")
    ("boards,b", po::value<int>(&N_FEB)->
     default_value(2), "Number of boards to configure >>> Set the value according to number of borads one wants to configure,\n Limited by the number of FEBs includeed in .json file")
    ("rms,R", po::value<int>(&rms)->
     default_value(9), "RMS factor")
    ("init_conf", po::bool_switch()->default_value(false), "Send initial configuration to FEBs\n""Input parameters =>> -c, -b ")
    ("threshold", po::bool_switch()->default_value(false), "Read channel thresholds the FEBs\n""Input parameters =>> -c, -b,-s, --debug")
    ("cal_thresholds", po::bool_switch()->default_value(false), "Commence FEB calibration (baseline, thdac, trimmers)\n""Input parameters =>> -c, -b or -L, -s, -r, --debug")
    ("merge_config", po::bool_switch()->default_value(false), "Merge separate configuration child files into one \n""Input parameters =>> -j")
    ("split_config", po::bool_switch()->default_value(false), "Create separate config files for HO and IP sides for layers 1,2 and 3,4 \n""Input parameters =>> deffault value - false")
    ("debug", po::bool_switch()->default_value(false), "Enable detailed <<cout<<  debug output (((Preferably to be used for single board calibration)))\n"" =>> default value - false")
    ("baseline", po::bool_switch()->default_value(false), "Read baseline of the specified boards \n"" =>> input parameters: -s, --conn_check")
    ("conn_check", po::bool_switch()->default_value(false), "Check the baseline for poorely connected or hot channels. If certain ammount of chennels do not fulfill the prerequesites calibration thread of this FEB is terminated \n"" =>> default value - false ")
    //          ("stgc", po::bool_switch()->default_value(false), "Modify data processing for stgc \n"" =>> default value - false ")
    ;
  //------------ input of the board names ------------------------
  po::parsed_options parsed_options = po::command_line_parser(ac,av)
    .options(desc)
    .run();

  //            BOOST_FOREACH(po::option &o, parsed_options.options)
  //            {
  //                    if(o.string_key == "names" && o.value.empty()){o.value.push_back(0);}
  //            }
  //--------------------------------------------------------------
  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);
  init_conf      = vm["init_conf"]     .as<bool>();
  threshold              = vm["threshold"]     .as<bool>();
  cal_thresholds = vm["cal_thresholds"]    .as<bool>();
  merge_config   = vm["merge_config"]  .as<bool>();
  split_config   = vm["split_config"]  .as<bool>();
  debug                          = vm["debug"]                  .as<bool>();
  baseline               = vm["baseline"]    .as<bool>();
  conn_check            = vm["conn_check"]              .as<bool>();

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  //---------- thing to see available core number -----------------
  {
    unsigned int c = std::thread::hardware_concurrency();
    std::cout << "\n Available number of cores: " << c <<"\n"<< std::endl;;
  }

  std::time_t run_start = std::chrono::system_clock::to_time_t(start);

  std::ofstream calibrep(cl_file, std::ofstream::out|std::ofstream::app);
  calibrep.is_open();
  calibrep<<"\n------------------ Calibration run log -------------------------\n";
  calibrep<<"\t\t   "<<std::ctime(&run_start);
  calibrep<<"-----------------------------------------------------------------\n";
  if(baseline or threshold){calibrep<<"MAIN INPUT PARAMETERS: [samples:"<<n_samples<<"([x10]\n"<<std::endl;}
  if(cal_thresholds){calibrep<<"MAIN INPUT PARAMETERS: [samples:"<<n_samples<<"([bl:x10][trims:x1][thdac:x2])]-[RMS:"<<rms<<"]\n"<<std::endl;}
  if(merge_config){calibrep<<"MAIN INPUT PARAMETERS: [new config file name - "<<mod_json<<"_sdsm_app_"<<rms<<"]\n"<<std::endl;}
  else{calibrep<<"MAIN INPUT PARAMETERS: [config file - "<<config_filename<<"]\n"<<std::endl;}
  //--------------------------------------------------------------------------------------------------------------------------------

  //------- using input data json file (mainly paths to folders)--------------------------------

  std::thread conf_threads[N_FEB];

  //nsw::CalibrationMath cm;
  nsw::CalibrationSca sca;

  //========================== sending initial configuration ======================================

  std::set<std::string> frontend_names;
  std::vector<std::string> fe_names_v;
  std::vector<nsw::FEBConfig> frontend_configs;
  bool full_set = true;

  //============================================================================================/
  if(init_conf)
    {
      unsigned int nfebs=0;
      calibrep<<"\n\t\t_____Configuring boards_____"<<std::endl;
      if(dw_layer.length()>0)
        {
          try{
            full_set=false;
            sca.read_config(config_filename, fe_name, full_set, frontend_names, fe_names_v, frontend_configs);
            int ifeb=0;
            std::cout<<"\nsearching for FEBs with naming pattern "<<dw_layer<<std::endl;
            for(unsigned int l=0; l<fe_names_v.size();l++)
              {
                //              std::cout<<fe_names_v[l]<<std::endl;
                if(fe_names_v[l].find(dw_layer)!=std::string::npos)
                  {
                    std::cout<<"found - "<<fe_names_v[l]<<std::endl;
                    nfebs++;
                  }
                else{continue;}
              }
            std::cout<<"\n{"<<nfebs<<"} will be configured"<<std::endl;
            calibrep<<"{"<<nfebs<<"} FEBs will be configured"<<std::endl;
            std::thread conf_threads[nfebs];

            for(long unsigned int b=0; b<fe_names_v.size(); b++)
              {
                if(fe_names_v.at(b).find(dw_layer)!= std::string::npos)
                  {
                    nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
                    conf_threads[ifeb] = std::thread(&nsw::CalibrationSca::configure_feb, calib_ptr, frontend_configs, b);
                    ifeb++;
                  }
                else{continue;}
              }
            for(unsigned int j=0; j<nfebs; j++)
              {
                conf_threads[j].join();
              }
          }catch(std::exception &e)
            {
              std::cout<<"ERROR - configuration failed: ["<<e.what()<<"]"<<std::endl;
              calibrep<<"\nERROR interupt: "<<e.what()<<std::endl;
            }
        }
      //----------------------------further is executed if no specific board set is specified-----------------------------------------------------------------------------------------
      else
        {
          try{
            sca.read_config(config_filename, fe_name, full_set, frontend_names, fe_names_v, frontend_configs);
            calibrep<<"\t{"<<N_FEB<<"} FEBs will be configured"<<std::endl;
            for(int i=0; i<N_FEB; i++)
              {
                nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
                conf_threads[i] = std::thread(&nsw::CalibrationSca::configure_feb, calib_ptr, frontend_configs, i);
              }
            for(int j=0; j<N_FEB; j++)
              {
                conf_threads[j].join();
              }
          }catch(std::exception &e){
            std::cout<<"ERROR - configuration failed: ["<<e.what()<<"]"<<std::endl;
            calibrep<<"\nERROR interupt: "<<e.what()<<std::endl;}
        }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  //========================== reading thresholds/baselines ==================================================
  if(threshold or baseline)
    {
      unsigned int nfebs=0;
      if(threshold){calibrep<<"\n\t\t_____Reading channel thresholds_____"<<std::endl;}
      if(baseline){calibrep<<"\n\t\t_____Reading channel baselines_____"<<std::endl;}
      if(dw_layer.length()>0)
        {
          try{
            //---------------- threshold reading here ---------------------------------
            full_set=false;
            sca.read_config(config_filename, fe_name, full_set, frontend_names, fe_names_v, frontend_configs);
            int ifeb=0;
            std::cout<<"\nsearching for FEBs with naming pattern "<<dw_layer<<std::endl;

            for(unsigned int l=0; l<fe_names_v.size();l++)
              {
                if(fe_names_v[l].find(dw_layer)!=std::string::npos)
                  {
                    std::cout<<"found - "<<fe_names_v[l]<<std::endl;
                    nfebs++;
                  }
                else{continue;}
              }
            std::cout<<"\n"<<nfebs<<"FEB(s) will be calibrated"<<std::endl;
            std::thread conf_threads[nfebs];

            if(debug and threshold){std::cout<<"| FEB | VMM | CHANNEL | MEAN(mV) | MAX_SAMPLE(mV) | MIN_SAMPLE(mV) |"<<std::endl;}
            if(conn_check and baseline){std::cout<<"| FEB | VMM | CHANNEL | MEDIAN(mV) | RMS(mV) | MAX_SAMPLE_SPREAD(mV) |"<<std::endl;}
            for(unsigned int l=0; l<fe_names_v.size(); l++)
              {
                if(fe_names_v[l].find(dw_layer)!=std::string::npos)
                  {
                    nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
                    if(baseline){conf_threads[ifeb] = std::thread(&nsw::CalibrationSca::read_baseline_full, calib_ptr, config_filename, frontend_configs, io_config_path, n_samples, l, fe_names_v.at(l),conn_check);}
                    if(threshold){conf_threads[ifeb] = std::thread(&nsw::CalibrationSca::read_thresholds, calib_ptr, config_filename, frontend_configs, io_config_path, n_samples, l, debug, fe_names_v.at(l));}
                    ifeb++;
                  }
                else{continue;}

              }
            for(unsigned int l=0; l<nfebs; l++)
              {
                if(conf_threads[l].joinable())
                  {
                    conf_threads[l].join();
                  }
              }
            if(baseline){std::cout<<"Baselines done!"<<std::endl;}
            if(threshold){std::cout<<"Thresholds done!"<<std::endl;}
          }catch(std::exception &e)
            {
              std::cout<<"Error on thread: ["<<e.what()<<"]"<<std::endl;
              calibrep<<"ERROR interupt: ["<<e.what()<<"]"<<std::endl;
            }
        }
      //----------------------------further is executed if no specific board set is specified-----------------------------------------------------------------------------------------
      else
        {
          sca.read_config(config_filename, fe_name, full_set, frontend_names, fe_names_v, frontend_configs);
          try{
            std::set<std::string>::iterator ct_it=frontend_names.begin();
            if(debug and threshold){std::cout<<"| FEB | VMM | CHANNEL | MEAN(mV) | MAX_SAMPLE(mV) | MIN_SAMPLE(mV) |"<<std::endl;}
            if(conn_check and baseline){std::cout<<"| FEB | VMM | CHANNEL | MEDIAN(mV) | RMS(mV) | MAX_SAMPLE_SPREAD(mV) |"<<std::endl;}
            for(int i=0; i<N_FEB; i++)
              {
                nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
                if(threshold){conf_threads[i] = std::thread(&nsw::CalibrationSca::read_thresholds, calib_ptr, config_filename, frontend_configs, io_config_path, n_samples, i, debug, *ct_it);}
                if(baseline){conf_threads[i] = std::thread(&nsw::CalibrationSca::read_baseline_full, calib_ptr, config_filename, frontend_configs, io_config_path, n_samples, i, *ct_it, conn_check);}
                ct_it++;
              }

            for(int i=0; i<N_FEB; i++)
              {
                conf_threads[i].join();
              }
            std::cout<<"\nthreads joined!"<<std::endl;
          }catch(std::exception &e){
            std::cout<<"Error on thread: ["<<e.what()<<"]"<<std::endl;
            calibrep<<"\nERROR interupt: "<<e.what()<<std::endl;}
        }
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
  //========================== THRESHOLD CALIBRATION ===========================================================================
  //    std::this_thread::sleep_for(std::chrono::seconds(10));
  if(cal_thresholds){
    unsigned int nfebs=0;
    calibrep<<"\n\t\t_____Calibrating thresholds_____"<<std::endl;

    //------------------------ creating configuration threads -------------------------------------------------------------
    if(dw_layer.length()>0)
      {
        full_set=false;
        int ifeb=0;
        sca.read_config(config_filename, fe_name, full_set, frontend_names, fe_names_v, frontend_configs);
        std::cout<<"\nsearching for FEBs with naming pattern ->"<<dw_layer<<std::endl;
        for(unsigned int l=0; l<fe_names_v.size();l++)
          {
            if(fe_names_v[l].find(dw_layer)!=std::string::npos)
              {
                std::cout<<"found - "<<fe_names_v[l]<<std::endl;
                nfebs++;
              }
            else{continue;}
          }
        std::cout<<"\n"<<nfebs<<"FEB(s) will be calibrated"<<std::endl;
        std::thread conf_threads[nfebs];

        try{
          for(long unsigned int b = 0; b < fe_names_v.size(); b++)
            {
              if(fe_names_v[b].find(dw_layer)!=std::string::npos)
                {
                  nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
                  conf_threads[ifeb] = std::thread(&nsw::CalibrationSca::sca_calib, calib_ptr, config_filename, frontend_configs, fe_names_v[b], io_config_path, b, n_samples, debug, rms);
                  ifeb++;
                }
              else{continue;}
            }

          for(unsigned int j=0;j<nfebs;j++)
            {
              if(conf_threads[j].joinable())
                {
                  conf_threads[j].join();
                }
            }
          printf("\nthreads joined!\n");

          std::this_thread::sleep_for(std::chrono::seconds(1));
          if(nfebs>1){sca.merge_json(mod_json, io_config_path, config_filename, rms, split_config);} //generate new .json file
        }
        catch(std::exception & e)
          {
            std::cout<<"\nERROR on thread: "<<e.what()<<std::endl;
            calibrep<<"\nERROR interupt: "<<e.what()<<std::endl;
          }
      }
    //--------------- in case of need to configure whole set of boards -----------------------------
    else
      {
        sca.read_config(config_filename, fe_name, full_set, frontend_names, fe_names_v, frontend_configs);
        try{
          std::set<std::string>::iterator ct_it=frontend_names.begin();
          for(int i = 0; i < N_FEB; i++)
            {
              nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
              conf_threads[i] = std::thread(&nsw::CalibrationSca::sca_calib, calib_ptr, config_filename, frontend_configs, *ct_it, io_config_path, i, n_samples, debug, rms);
              ct_it++;
            }
          for(int j=0;j<N_FEB;j++)
            {
              conf_threads[j].join();
            }
          printf("\nthreads joined!\n");
          std::this_thread::sleep_for(std::chrono::seconds(1));
          if(N_FEB>1){sca.merge_json(mod_json, io_config_path, config_filename, rms, split_config);} //generate new .json file
        }
        catch(std::exception &e){
          std::cout<<"Error on thread: ["<<e.what()<<"]"<<std::endl;
          calibrep<<"\nERROR interupt: "<<e.what()<<std::endl;}
      }
  }
  //================== Merging separate board configs ===============================================
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if(merge_config){
    calibrep<<"\n\t\t_____Merging json files_____"<<std::endl;
    try{
      sca.merge_json(mod_json, io_config_path, config_filename, rms, split_config);
    }catch(std::exception &e){
      std::cout<<"Couldn`t merge .json files, reason: "<<e.what()<<std::endl;
      calibrep<<"\nERROR interupt: "<<e.what()<<std::endl;
    }
  } //generate new .json file

  //------- calculate elapsed time --------------------------------------------------
  auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;
  std::cout << "\n Elapsed time: " << elapsed.count() << " s\n";

  calibrep<<"\n\t\t______Complete operation took "<<std::setprecision(2)<<(elapsed.count())/60<<" minutes_____"<<std::endl;
  calibrep.close();

  return 0;
}
