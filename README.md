# NSWCalibration 

## Intro 

Calibration/configuration module is tied to the input file of .json format to manage data writing and further processing(plotting).
Input file contains all data output paths, default names of the OPC server and communication port, self location path,
channel mask and trimmer register json files, and desired default configuration .json file, etc.

**Mandatory!**

NSWCalibration has dependencies on the NSWConfiguration libraries of _UaoClientForOpcUa_ and _NSWConfiguration_ - thus installation of NSWConfiguration is a pre-requisite. In long-term perspective a standalone build should be available.

## Installation

Move to the directory where NSWConfiguration is installed and:

```bash
git clone --recursive https://:@gitlab.cern.ch:8443/atlas-muon-nsw-daq/NSWCalibration.git
```
Now there sould be this structure in your work directory, before compilation:

* CMakeLists.txt
* NSWConfiguration
* NSWCalibration

Next step is to compile both sw with following commands:

```bash
cmake_config
#after cmake configured move to build directory under appropriate hw tag
cd x64...
make -j
```
In case build exits with pkill error or One of the libraries is not linking properly - try repeating last step with lesser nr of cores: {make -j5}. For sure it helps when NSWConfigRc.cpp linking is not working properly. Sometimes one must delete the hardware tagged directory (x64_86...) and repeat from cmake_config command.

After this NSWCalibration will be installed and will use appropriate libraties from NSWConfiguration

As the last step, one should run shell script to setup the calibration data readout directory:

```bash
./set_dir.sh absolute/path/to/desired/directory/ opc.server-name.cern.ch
```
This shell script has twofold purpose. First is creation of the **[lxplus_input_data.json]** file with all directory references and opc server to access. The file looks like this:

```
# Absolute paths have to be specified
# mainly keys point to the absolute path of some specific output/input files
{
"config_dir":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/config_files/",		#location of the configuration files
"configuration_json":"integration_config_ala_bb5.json",						#configuration file of choice in the config_files
"cal_data_output":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/calib_data/",		#calibration data output file
"thr_data_output":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/thresholds/",		#untrimmed threshold data file 
"json_data_output":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/calib_json/",	#json files that hold mask and trimmer values
"opc_server_name":"hefr40.physik.unifreiburg.de",						#reserved but not used yet, opc server hostname
"opc_server_port":":48020",									#reserved but not used yet, communication port
"bl_data_output":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/baselines/",		#sampled baseline out. file
"tp_data_output":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/test_pulse_dac/",	#pulser dac calibration out. file
"tp_data_output2":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/test_pulse_dac2/",	#reserved for future dev.
"report_log":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/CalibReport.txt",		#Report log file
"archive":"/afs/cern.ch/user/v/vplesano/public/NSWCalibrationData/archive/"			#archive
}
```
The second action of the shell script is to create output file directories defined by user in the .json fiel above with following structure(examplary):

```
NSWCalibrationData/
├── archive
│   ├── RootFiles
│   │   ├── baseline_tree_17-12-2019_10-02-20.root
│   │   ├── baseline_tree_17-12-2019_10-02-47.root
│   │   ├── baseline_tree_17-12-2019_10-03-18.root
│   │   ├── data_tree_17-12-2019_10-02-20.root
│   │   ├── data_tree_17-12-2019_10-02-47.root
│   │   ├── data_tree_17-12-2019_10-03-18.root
│   │   ├── data_tree_17-12-2019_11-26-44.root
│   │   ├── threshold_tree_17-12-2019_10-02-20.root
│   │   ├── threshold_tree_17-12-2019_10-02-47.root
│   │   ├── threshold_tree_17-12-2019_10-03-18.root
│   │   └── threshold_tree_17-12-2019_11-26-44.root
│   └── TextFiles
│       ├── Baselines_17-12-2019_10-02-19.txt
│       ├── Baselines_17-12-2019_10-02-47.txt
│       ├── Baselines_17-12-2019_10-03-18.txt
│       ├── Calib_data_17-12-2019_10-02-20.txt
│       ├── Calib_data_17-12-2019_10-02-47.txt
│       ├── Calib_data_17-12-2019_10-03-18.txt
│       ├── Calib_data_17-12-2019_11-26-44.txt
│       ├── Untrimmed_thresholds_17-12-2019_10-02-20.txt
│       ├── Untrimmed_thresholds_17-12-2019_10-02-47.txt
│       ├── Untrimmed_thresholds_17-12-2019_10-03-18.txt
│       └── Untrimmed_thresholds_17-12-2019_11-26-44.txt
├── baselines							
│   ├── MMFE8-0000_full_bl.txt
│   └── MMFE8-0001_full_bl.txt
├── calib_data
│   ├── MMFE8-0000_data.txt
│   └── MMFE8-0001_data.txt
├── calib_json
│   ├── MMFE8-0000_config_test2.json
│   └── MMFE8-0001_config_test2.json
├── CalibReport.txt
├── config_files
│   ├── generated_config_sdsm_appended.json
│   └── integration_config_ala_bb5.json
├── test_pulse_dac
└── thresholds
    ├── MMFE8-0000_thresholds.txt
    └── MMFE8-0001_thresholds.txt

```
Files for separate FEBs in the baselines/, calib_data/, calib_json/, thresholds/ and test_pulse_dac/ are overwritten each time one starts a new calibration run and their merged copy is held in the archive (created by the NSWCalibrationDataPlotter)

*What remains is to insert desired configuration file name in the node "configuration_json". The file paths can be changed at any time. In general changes in the input .json file do not require recompilation of the SW itself. Of course one can still call the desitred configuration file using conventional -c option*

_Now the installation is complete.(whop, whop!)_

# Operation description

Script itself allows to:

* Configure frontends (function applicable for SCA path calibration only);
* Read channel baseline and thresholds;
* Modify configuration .json files;
(Calibrate.cpp)
* Calibrate internal pulser DAC;
(pulse.cpp, description will follow later...)

For example, lets inspect Calibate.cpp that does SCA path calibrations (executable name - ./calibrate) that has following options (can be viewed by using option -h):

## General function calls (flags)
-------------------------------------------------------------------------

	* --init_conf 	-	-> send configuration to FEBs (use only for SCA calibrations - for L1 data taking use configure_frontend.cpp from NSWConfiguration);
	* --baseline 	-	-> read channel baseline;
	* --threshold	-	-> read channel thresholds;
	* --cal_thresholds	-> calibrate global and trimmer DAC;
	* --merge_config	-> merge append channel masking/trimmer values to the initial .json file;
	* --split_config	-> (for bb5/191 sites) split configuration into 4 separate .json files for HO_L1L2, HO_L3L$, IP_L1L2, and IP_L3L4 front-end mapping;
			(if Nr. of FEBs in operation is >1, function is called automatically)

Aforementioned options (except split_config) write notifications in the CalibReport.txt in user defined directory. In case there were no calculations with strong deviations or any misbehaving channels the log woill just have the start message with date-time stamp and overall procedure time.

## Other options
----------------------------------------------------------------------------------

	* -L -> desired FEB or FEB group to be used (default - "") - input string is compared with the FEB names in the .json file and in the case of a match thread will be started;
	* -s -> Nr. of samples per channel (default - 10 (for baselines and threshold calibration multipliers should be considered: BL- x10, THDAC - x2));
	* -R -> desired RMS factor (default - x8);
	* -c -> initial configuration file (default - defined in the input json file);
	* -j -> name of the merged .json file(default - generated_conf.json);
	* -b -> total Nr. of FEBs to be used (default - 2)(in case of absent overriding -L option);

	* --debug -> verbose the detailed output for --threshold and --cal_thresholds options;
	* --conn_check -> same, but for --baseline option
-------------------------------------------------------------------------------------------------

# Typical use cases are:

```bash
./calibrate -L MM --init_conf			#configure all FEBs which name (in.xml/.json files) have MM in their naming;
./calibrate -L HO --baseline			#read baseline of HO side FEBs with 10(x10) samples per channel;
./calibrate -L L1 --threshold			#read channel thresholds of L1(layer one) FEBs on the DW;
./calibrate -b 2 -s 5 -R 9 --cal_thresholds 	#calibrate threshold and trimmer DAC on the first two FEB VMMs in the .json file;

```

In general the sequence for the full cycle would be:

 --init_conf -> --threshold(OR --baseline, then got to start) -> --cal_thresholds -> --init_conf

with appropriate additional options.

IMPORTANT - In case of small scale operations (less than 96 FEBs) on can give multiple function calls (flags) in one execution. IF opeation scale is > 96 FEBs it is advised to execute each function call separately (otherwise memory is overwhelmed and process fails with pkill error).

**Few warnings:**
	
	* !Always! put search string in the -L option that will match the name in working .json file - otherwise >> exception;
	* -b option sorts FEB names from .json file and starts count from 0 to entered number;
	* --baseline and --threshold can not be called simultaneously;
	* if only one FEB was calibrated - one needs to manually call --merge_config and -j options;
	* in case Nr. of FEBs to be calibrated is >64 -> call --threshold and --cal_thresholds as a separate, consequen processes - otherwise memory gets overloaded and programm flips out.(to be fixed)

**Usefull links:**

	[Data plotter](https://gitlab.cern.ch/vplesano/nswcalibrationdataplotter/tree/master)
	[Corresponding NSWConfig branch](https://gitlab.cern.ch/atlas-muon-nsw-daq/NSWConfiguration/tree/vlad_calib_devmerged)


