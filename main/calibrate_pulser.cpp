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
#

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"

#include "boost/foreach.hpp"
#include "boost/program_options.hpp"

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
//	std::string io_config_path = "../../NSWCalibration/lxplus_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
//	std::string io_config_path = "../../NSWCalibration/bb5_sectA14_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
//	std::string io_config_path = "/afs/cern.ch/user/v/vplesano/public/calib_repo/NSWCalibration/lxplus_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
	//std::string io_config_path = "/afs/cern.ch/user/v/vplesano/public/calib_repo/NSWCalibration/bb5_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
	std::string io_config_path = "/afs/cern.ch/user/v/vplesano/public/calib_repo/NSWCalibration/vs_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
	pt::read_json(io_config_path, input_data);
//-------------------------------------------------------------------
	std::string	def_config = input_data.get<std::string>("configuration_json");
 	std::string base_folder = input_data.get<std::string>("config_dir");
	std::string cl_file = input_data.get<std::string>("report_log");

    std::string description = "\tProgramm allows to configure/calibrate MM FEBs\n\t declare what you want to do, type:\n\t --init_conf - to load inital VMM configuration or final configuration with resulting json file\n\t --threshold - to read thresholds of the particular VMM\n\t --cal_thresholds - to calibrate threshold and trimmer DAC on FEB scale\n\t --merge_config - to merge separate board .json files into one\t default name -> generated_config.json (name changing option -j)\n\n\t!!!\n\tIMPORTANT: programm requires existance of the input configuration .json file with necessary output file location paths, OPC server name and associated communication port\n\t!!!";

		bool debug;
    int N_FEB;
    int n_samples;
 
		std::set<std::string> board_set;
		std::vector<int> board_set_int;
    std::string config_filename;
    std::string dw_layer;
    std::string fe_name;
    po::options_description desc(description);
    desc.add_options()
    ("help,h", "produce help message")
    ("config,c", po::value<std::string>(&config_filename)->default_value(base_folder+def_config),"Configuration .json file. If not specified choses from input_data.json - [configuration_json] ")
    ("layer_dw,L", po::value<std::string>(&dw_layer)->default_value(""),"Layer in the DW to configure/calibrate, type in -> L1/L2/L3/L4")
		("samples,s", po::value<int>(&n_samples)->
        default_value(10), "Number of ADC samples to read per channel >>> For the baselines a multiplication factor of x10 is implemented")
    ("boards,b", po::value<int>(&N_FEB)->
        default_value(2), "Number of boards to configure >>> Set the value according to number of borads one wants to configure,\n Limited by the number of FEBs includeed in .json file")
		("debug", po::bool_switch()->default_value(false), "Enable detailed <<cout<<  debug output (((Preferably to be used for single board calibration)))\n"" =>> default value - false")
     ;
//------------ input of the board names ------------------------
		po::parsed_options parsed_options = po::command_line_parser(ac,av)
				.options(desc)
				.run(); 
			
//		BOOST_FOREACH(po::option &o, parsed_options.options)
//		{
//			if(o.string_key == "names" && o.value.empty()){o.value.push_back(0);}
//		}
//--------------------------------------------------------------
    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);

	debug   			 = vm["debug"]    		.as<bool>();

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
	calibrep<<"MAIN INPUT PARAMETERS: [samples:"<<n_samples<<"]\n"<<std::endl;
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

		unsigned int nfebs=0;
		calibrep<<"\n\t\t_____Calibrating Pulser DAC_____"<<std::endl;
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
					
				if(debug){std::cout<<"| FEB | VMM | DAC | SAMPLE | SAMPLE(mV) |"<<std::endl;}
				for(unsigned int l=0; l<fe_names_v.size(); l++)
				{
					if(fe_names_v[l].find(dw_layer)!=std::string::npos)
					{
						nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
						conf_threads[ifeb] = std::thread(&nsw::CalibrationSca::calib_pulserDAC, calib_ptr, frontend_configs, io_config_path, fe_names_v.at(l), n_samples, l, debug);
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
				std::cout<<"Pulser DAC done!"<<std::endl;
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
					if(debug){std::cout<<"| FEB | VMM | DAC | SAMPLE | SAMPLE(mV) |"<<std::endl;}
					for(int i=0; i<N_FEB; i++)
					{
						nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
						conf_threads[i] = std::thread(&nsw::CalibrationSca::calib_pulserDAC, calib_ptr, frontend_configs, io_config_path, *ct_it, n_samples, i, debug);
//						if(threshold){conf_threads[i] = std::thread(&nsw::CalibrationSca::read_thresholds, calib_ptr, config_filename, frontend_configs, io_config_path, n_samples, i, debug, *ct_it);}
//						if(baseline){conf_threads[i] = std::thread(&nsw::CalibrationSca::read_baseline_full, calib_ptr, config_filename, frontend_configs, io_config_path, n_samples, i, *ct_it, conn_check);}
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
	std::this_thread::sleep_for(std::chrono::seconds(1));

	auto finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = finish - start;
	std::cout << "\n Elapsed time: " << elapsed.count() << " s\n";
	
	calibrep<<"\n\t\t______Complete operation took "<<std::setprecision(2)<<(elapsed.count())/60<<" minutes_____"<<std::endl;
	calibrep.close();
	

return 0;
	
}
