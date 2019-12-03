# NSW Calibration

This is a repository of calibration scripts which 
need libraries from multiple NSW DAQ repos.

**Small intro about calibrate_thresholds.cpp**

Calibration/configuration module is tied to the input file of .json format to manage data writing and further processing(plotting).
Input file contains all data output paths, default names of the OPC server and communication port, self location path,
channel mask and trimmer register json files, and desired default configuration .json file, etc.
Pathsd have to be setup once and forgotten about. Thing like OPC server/port or json file can be altered without recompilation of the software.

Script itself allows to:

* Configure frontends;
* Read channel baseline and thresholds;
* Modify configuration .json files;

In more detail, use following options (that can be viewed by using option -h):

------ general function calls -------------------------------------------------------------------

* --init_conf 	-	-> send configuration to FEBs;
* --baseline 	-	-> read channel baseline;
* --threshold	-	-> read channel thresholds;
* --cal_thresholds	-> calibrate global and trimmer DAC;
* --merge_config	-> merge append channel masking/trimmer values to the initial .json file;
			(if Nr. of FEBs in operation is >1, function is called automatically)

Every aforementioned option writes notifications in the CalibReport.txt in user defined directory.
In case there were no calculations with strong deviations or any misbehaving channels the log woill just have the start 
message with date-time stamp and overall procedure time.
 
------ other options ----------------------------------------------------------------------------

* -L -> desired FEB or FEB group to be used (default - "") - input string is compared with the FEB names in the .json file and in the case of a match thread will be started;
* -s -> Nr. of samples per channel (default - 10 (for baselines and threshold calibration multipliers should be considered: BL- x10, THDAC - x2));
* -R -> desired RMS factor (default - x8);
* -c -> initial configuration file (default - defined in the input json file);
* -j -> name of the merged .json file(default - generated_conf.json);
* -b -> total Nr. of FEBs to be used (default - 2)(in case of absent overriding -L option);

* --debug -> verbose the detailed output for --threshold and --cal_thresholds options;
* --conn_check -> same, but for --baseline option
-------------------------------------------------------------------------------------------------

So a typical way to use the script would be:

```bash
./calibrate_thresholds -L MM --init_conf		#configure all FEBs which name (in.xml/.json files) have MM in their naming;
./calibrate_thresholds -L HO --baseline			#read baseline of HO side FEBs with 10(x10) samples per channel;
./calibrate_thresholds -L L1 --threshold		#read channel thresholds of L1(layer one) FEBs on the DW;
./calibrate_thresholds -b 2 -s 5 -R 9 --cal_thresholds 	#calibrate threshold and trimmer DAC on the first two FEB VMMs in the .json file;

```
In general the sequence for the full cycle would be: --init_conf -> --threshold(OR --baseline, then got to start) -> --cal_thresholds -> --init_conf
with appropriate additional options;

**Few warnings:**
	- **Always** put search string in the -L option that will match the name in working .json file - otherwise >> exception;
	- -b option sorts FEB names from .json file and starts count from 0 to entered number;
	- --baseline and --threshold can not be called simultaneously;
	- if only one FEB was calibrated - one needs to manually call --merge_config and -j options;

