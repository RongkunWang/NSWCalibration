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
**In case build exits with pkill error or One of the libraries is not linking properly** - try repeating last step with lesser nr of cores: {make -j5}. For sure it helps when NSWConfigRc.cpp linking is not working properly.

After this NSWCalibration will be installed and will use appropriate libraties from NSWConfiguration
-----
As the last step, one should run shell script to setup the calibration data readout directory:

```bash
./set_dir.sh absolute/path/to/desired/directory/ opc.server-name.cern.ch
``
this command will create **[lxplus_input_data.json]** file with all directory references and opc server to access. What remains is to insert desired configuration file name in the node "configuration_json". The file paths can be changed at any time. In general changes in the input .json file do not require recompilation of the SW itself.
-----
_Now the installation is complete.(whop, whop!)_

# Operation description

Script itself allows to:

* Configure frontends;
* Read channel baseline and thresholds;
* Modify configuration .json files;

In more detail, use following options (that can be viewed by using option -h):

## General function calls
-------------------------------------------------------------------------

	* --init_conf 	-	-> send configuration to FEBs;
	* --baseline 	-	-> read channel baseline;
	* --threshold	-	-> read channel thresholds;
	* --cal_thresholds	-> calibrate global and trimmer DAC;
	* --merge_config	-> merge append channel masking/trimmer values to the initial .json file;
			(if Nr. of FEBs in operation is >1, function is called automatically)

Every aforementioned option writes notifications in the CalibReport.txt in user defined directory. In case there were no calculations with strong deviations or any misbehaving channels the log woill just have the start message with date-time stamp and overall procedure time.

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

with appropriate additional options;

**Few warnings:**

	* !Always! put search string in the -L option that will match the name in working .json file - otherwise >> exception;
	* -b option sorts FEB names from .json file and starts count from 0 to entered number;
	* --baseline and --threshold can not be called simultaneously;
	* if only one FEB was calibrated - one needs to manually call --merge_config and -j options;
	* in case Nr. of FEBs to be calibrated is >64 -> call --threshold and --cal_thresholds as a separate, consequen processes - otherwise memory gets overloaded and programm flips out.(to be fixed)


**Usefull links:**

	[Data plotter](https://gitlab.cern.ch/vplesano/nswcalibrationdataplotter/tree/master)
	[Corresponding NSWConfig branch](https://gitlab.cern.ch/atlas-muon-nsw-daq/NSWConfiguration/tree/vlad_calib_devmerged)


