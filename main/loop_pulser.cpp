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
	std::string io_config_path = "../../NSWCalibration/lxplus_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
//	std::string io_config_path = "../../NSWCalibration/bb5_sectA14_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
//	std::string io_config_path = "/afs/cern.ch/user/v/vplesano/public/calib_repo/NSWCalibration/lxplus_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
	//std::string io_config_path = "/afs/cern.ch/user/v/vplesano/public/calib_repo/NSWCalibration/bb5_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
//	std::string io_config_path = "/afs/cern.ch/user/v/vplesano/public/calib_repo/NSWCalibration/vs_input_data.json"; //<<---- change this path according to input_data.json location path!!!!
	pt::read_json(io_config_path, input_data);
//-------------------------------------------------------------------
	std::string	def_config = input_data.get<std::string>("configuration_json");
 	std::string base_folder = input_data.get<std::string>("config_dir");
	std::string cl_file = input_data.get<std::string>("report_log");

    std::string description = "\tProgram executes VMM internal pulser loop going from pulser DAC 200 to 1000 in steps of 100 with global threshold set at 180(default)\n";

		bool debug;
		bool task;
    int N_FEB;
    int thdac;
    int alti_chan;
		std::string expect_file;

    std::string config_filename;
    std::string dw_layer;
    std::string fe_name;
    po::options_description desc(description);
    desc.add_options()
    ("help,h", "produce help message")
    ("config,c", po::value<std::string>(&config_filename)->default_value(base_folder+def_config),"Configuration .json file. If not specified choses from input_data.json - [configuration_json] ")
    ("expect,e", po::value<std::string>(&expect_file)->default_value("/afs/cern.ch/user/n/nswdaq/public/alti/ALTI_oneshot_pattern.expect"),"ALTI .expect file with appropriate tigger pattern ")
    ("alti_chan,a", po::value<int>(&alti_chan)->default_value(11),"slot in VME crate where ALTI sits")
    ("thdac, t", po::value<int>(&thdac)->default_value(180),"Threshold DAC to set for pulsing - default 180")
    ("layer_dw,L", po::value<std::string>(&dw_layer)->default_value(""),"Select febs by their naming - type in -> L1/L2/L3/L4 to pulse FEBs in these layers or HO/IP to pulse whole side of DW or be more specific by entering i.e. MMFE8_L1P1_HOL and pulse only on FEB")
		("task", po::bool_switch()->default_value(false), "Enable TTC commands =>> default value - false")
		("debug", po::bool_switch()->default_value(false), "Enable detailed <<cout<<  debug output (((Preferably to be used for single board calibration)))\n"" =>> default value - false")
     ;
//------------ input of the board names ------------------------
		po::parsed_options parsed_options = po::command_line_parser(ac,av)
				.options(desc)
				.run(); 
//--------------------------------------------------------------
    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);

	debug   			 = vm["debug"]    		.as<bool>();
	task   			 = vm["task"]    		.as<bool>();

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
	}

	const char* host = std::getenv("HOSTNAME");
	std::string this_host = host;

	if(this_host.find("sbcnsw-ttc-01")!=std::string::npos){
		std::cout<<"\n Using VS SBC & ALTI - VME channel 9"<<std::endl;
		alti_chan = 9;
	}
	if(this_host.find("sbcatlnswb1")!=std::string::npos){
		std::cout<<"\n Using BB5 SBC & ALTI - VME channel 11"<<std::endl;
		alti_chan = 11;
	}
	if(this_host.find("sbcl1ct-191")!=std::string::npos){
		std::cout<<"\n Using 191 SBC & ALTI - VME channel 11"<<std::endl;
		alti_chan = 11;
	}
	else{std::cout<<"Running on unknown SBC or without it"<<std::endl;}
//---------- thing to see available core number -----------------
	{
	   unsigned int c = std::thread::hardware_concurrency();
	   std::cout << "\n Available number of cores: " << c <<"\n"<< std::endl;;
	}
	
	std::string vme_chan = std::to_string(alti_chan);
	expect_file += " "+vme_chan;

	std::cout<<"Expect file to be used - << "<<expect_file<<" >>"<<std::endl;
	std::time_t run_start = std::chrono::system_clock::to_time_t(start);

	std::ofstream calibrep(cl_file, std::ofstream::out|std::ofstream::app);
	calibrep.is_open();
	calibrep<<"\n------------------ Calibration run log -------------------------\n";
	calibrep<<"\t\t   "<<std::ctime(&run_start);
	calibrep<<"-----------------------------------------------------------------\n";
	calibrep<<"MAIN INPUT PARAMETERS: [expect file:"<<expect_file<<"]\n"<<std::endl;

	std::thread conf_threads[N_FEB];
	nsw::CalibrationSca sca;

//========================== sending initial configuration ======================================
	std::set<std::string> frontend_names;
	std::vector<std::string> fe_names_v;
	std::vector<nsw::FEBConfig> frontend_configs;
	bool full_set = true;
	
	std::vector<int> tpdacs = {200,300,400,500,600,700,800,900,1000};

		unsigned int nfebs=0;
		calibrep<<"\n\t\t_____Pulsing_Channels_____"<<std::endl;
		if(dw_layer.length()>0)
		{	
			try{
			//---------------- threshold reading here ---------------------------------
				full_set=false;	
				sca.read_config(config_filename, fe_name, full_set, frontend_names, fe_names_v, frontend_configs);
//				int ifeb=0;
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
				std::cout<<"\n"<<nfebs<<"FEB(s) will be pulsed"<<std::endl;
				std::thread conf_threads[nfebs];
//----------------------------------------------------------------------------------
//					Here two system calls should be made for the ttc sr and ecr commands
//-----------------------------------------------------------------------------------
//			 if(!task){system("echo sr 11 executes && echo ecr 11 executes");}
			 std::string ttc_com = "sr "+std::to_string(alti_chan)+" && ecr "+std::to_string(alti_chan);
	//		 if(task){system("sr 11 && sleep 2 && ecr 11");}
//			 if(task){system(ttc_com.c_str());}
			 sleep(2);
//------------------ sending intial config ----------------------------------------------
					int ifeb=0;
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
					std::cout<<" threads launched"<<std::endl;
					for(unsigned int j=0; j<nfebs; j++)
					{
						conf_threads[j].join();
					}
					
					std::cout<<" threads joined"<<std::endl;
//----------------------------------------------------------------------------------------------------------
			 for(unsigned int i = 0; i < tpdacs.size(); i++)
			 {
//--------------CONFIGURING FEBS TO SEND TEST PULSES-------------------------------------------------------------------------			
				int ifeb_on = 0;
					for(unsigned int l=0; l<fe_names_v.size(); l++)
					{
						if(fe_names_v[l].find(dw_layer)!=std::string::npos)
						{
							nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
							conf_threads[ifeb_on] = std::thread(&nsw::CalibrationSca::send_pulses, calib_ptr, frontend_configs, l, thdac, tpdacs[i], debug);
							ifeb_on++;
							if(debug){std::cout<<"\nstarted thread ["<<ifeb_on<<"] for = "<<fe_names_v[l]<<" with settings (THDAC="<<thdac<<"|TPDAC="<<tpdacs[i]<<")"<<std::endl;}
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
//---------EXECUTING TTC COMMANDS--------------------------------------------------------------------------------
				if(!task){system("echo ttc0");}
				if(task){system(expect_file.c_str());}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if(debug){std::cout<<"TTC command happens here for <<"<<i+1<<">> time"<<std::endl;}
//----------DISABLING PUSLES-----------------------------------------------------------------------------			
				int ifeb_off = 0;
					for(unsigned int l=0; l<fe_names_v.size(); l++)
					{
						if(fe_names_v[l].find(dw_layer)!=std::string::npos)
						{
							nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
							conf_threads[ifeb_off] = std::thread(&nsw::CalibrationSca::turn_off_pulser, calib_ptr, frontend_configs, l, debug);
							ifeb_off++;
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
//------------REPEATING FOR ENXT PULSE DAC VALUE-----------------------------------------------------------------------------
			}

			std::cout<<"Pulsing done!"<<std::endl;
			}catch(std::exception &e)
			{
				std::cout<<"Error on thread: ["<<e.what()<<"]"<<std::endl;
				calibrep<<"ERROR interupt: ["<<e.what()<<"]"<<std::endl;
			}
		}
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 /////////////////////////////  later need to adjust lower part ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//----------------------------further is executed if no specific board set is specified-----------------------------------------------------------------------------------------
		else
		{
				sca.read_config(config_filename, fe_name, full_set, frontend_names, fe_names_v, frontend_configs);
				try{

					 if(!task){system("echo sr 11 executes && echo ecr 11 executes");}
					 if(task){system("sr 11 && ecr 11");}

					for(unsigned int i=0; i< tpdacs.size(); i++)
					{
						std::cout<<"setting all feb pulsers to ["<<tpdacs[i]<<"] DAC counts"<<std::endl;
				//--------------------sending-pulses-----------------------------------------
					//	std::set<std::string>::iterator ct_it=frontend_names.begin();
						if(debug){std::cout<<"\nstarting threads with settings (THDAC="<<thdac<<"|TPDAC="<<tpdacs[i]<<")"<<std::endl;}
						for(int i=0; i<N_FEB; i++)
						{
							nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
							conf_threads[i] = std::thread(&nsw::CalibrationSca::send_pulses, calib_ptr, frontend_configs, i, thdac, tpdacs[i], debug);
					//		ct_it++;
						}
						for(int i=0; i<N_FEB; i++)
						{
							conf_threads[i].join();
						}
				//----------------------executing-ttc-commands----------------------------------------
					if(task){system(expect_file.c_str());}
					usleep(100000);		
					if(debug){std::cout<<"TTC command happens here for <<"<<i+1<<">> time"<<std::endl;}	
				//--------------------disbling-pulses------------------------------------------
					//	std::set<std::string>::iterator ct_it=frontend_names.begin();
						for(int i=0; i<N_FEB; i++)
						{
							nsw::CalibrationSca * calib_ptr = new nsw::CalibrationSca;
							conf_threads[i] = std::thread(&nsw::CalibrationSca::turn_off_pulser, calib_ptr, frontend_configs, i, debug);
					//		ct_it++;
						}
						for(int i=0; i<N_FEB; i++)
						{
							conf_threads[i].join();
						}
				//---------------------repeat-----------------------------------------
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
