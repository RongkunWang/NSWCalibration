# NSWCalibration

## Intro

NSWCalibration has dependencies on the NSWConfiguration libraries of _UaoClientForOpcUa_ and _NSWConfiguration_ - thus installation of NSWConfiguration is a pre-requisite. In long-term perspective a standalone build should be available.

## Installation

NSWCalibration is insatlled as a part of the nswdaq package and should work out of the box.

## Class functionality

### NSWCalibRc

<!--This is basicly a placeholder for later description of the ACCURATE NSWCalibRc functionality -->
Class responsible for opreating the specialized calibration applications in the TDAQ partition
Basic strucutre and usage of function follows the partition transitions e.g. `INITIALIZE`, `CONNECT`, `CONFIGURE` etc.
User enables the desired calibration class by specifyong calibration type in the information Server (IS). The main handler of the calibration classes in NSWCalibRc is the nsw::NSWCalibRc::handler function.
During `INITIALIZE` and `CONFIGURE` steps the configuration is received from database and sent to hardware in use by `NSWConfigRc.cpp`.
Upon receiving the calibration type from IS after `PREPARE_FOR_RUN` transition unique pointer to desired class is made.
Later the handler function itrates through configuration steps that are common to all classes in the NSWCalibration.
The end of calibration loop in handler is signified by the `ERS_INFO` message.

### CalibAlg

`CalibAlg` is the base class for all NSW calibrations.
The current calibrations are described by the implementations below.

#### THRCalib

Class desiganted for VMM threshold calibration and runs only from TDAQ
partition. Built in options allow to read front-end baselines to
assess the noise, calibrate global VMM thresholds with respect to the
desired number of samples per channel and desired RMS threshold offset
provided by user. During the calibration cycle the channel trimmer
DACs are also calibrated. Information about calibrated DAC values is
being stored in per-frontend partial `.json` config files and at the end
of the calibration cycle merged into a single file to be upladed to
the conditions database.

To run threshold calibration user does following:

1. Start TDAQ partition

2. Upload the threshold calibration type to the IS using following
   command line command

   ```bash
   is_write -p <partition_name> -n NswParams.Calib.calibType -t String -v THRCalib -i 0
   ```

3. Declare desired threshold calibration routine

   For pure baseline readout and internal pulser DAC calibration (done
   one after another):

   ```bash

   is_write -p <partition_name> -n NswParams.Calib.calibParams -t String -v BLN,<nr.of samplesx10>,<RMSfactor>,<debug flag> -i 0
   ```

   as an example:

   ```bash
   is_write -p <partition_name> -n NswParams.Calib.calibParams -t String -v BLN,10,9,0 -i 0

   ```

   where -v option holds following comma separated input parameters in
   exactly this order:
   * BLN - marks the baseline readout operation (has to be specifically
     these three capital characters)
   * 10 - 100 samples per channel (has a x10 multiplier in the script -
     so to have 100 samples one enters 10)
   * 9 - RMS factor for the threshold calibration (not needed for
     baselines but entry must not be empty anyway)
   * 0 - debug output minimised in the RC application log files (1 -
     enables more detailed information per channel)

   Or complete threshold calibration cycle by specifying first entry with
   `THR` flag as the first parameter in the IS input string:

   ```bash
   is_write -p <partition_name> -n NswParams.Calib.calibParams -t String -v THR,10,9,0 -i 0

   ```

4. Continue with `INITIALIZE`, `CONFIGURE`, `START` transitions during
   which the infrastructure is initialized, the frontends are
   configured and data acquisition starts. User will be notified when
   the calibration has succeeded.

Data acquisition for the `THRCalib` does not depend on the TDAQ
partition `record data` setting, and in both states the data `.txt`
files are written.

Script will calibrate thresholds for those front-ends that are
specified in the `.json` configuration generated from database of
manually assembled.

Data from THRCalib is being written to the directory specified in the
`schema/NSWCalib.schema.xml`

```xml
<attribute name="CalibOutput" description="Directory to write threshold calibration data" type="string" init-value="/afs/cern.ch/user/v/vplesano/vladwork/threshold_data/" is-not-null="yes"/>
```

that should be modified at the partition generation to hold the user
desired output directory.

```bash
MMFE8_L1P1_IPR_calibration_data.txt            #calibration data output file
MMFE8_L1P1_IPR_thresholds.txt                  #untrimmed threshold data file
MMFE8_L1P1_IPR_partial_config.json             #json files that hold mask and trimmer values
MMFE8_L1P1_IPR_baseline_samples.txt            #sampled baseline file
MMFE8_L1P1_IPR_TPDAC_samples.txt               #pulser dac calibration file
```

The complete data volume for a single MM double wedge should not
exceed 900 Mb.  All aforementioned files are used by the
`NSWCalibrationDataPlotter` package to plot/analyse the calibration
data (`.txt`), and generation of the modified frontend configuration
that holds calibrated global threshold and trimmed regiser values
(`.json`).

#### PDOCalib

This class is designated for PDO and TDO calibration i.e. to read VMM
digital data to be later converted from PDO to fC and from TDO to
ns. Class is written to run from run controll application in TDAQ
partition. Performance of this class highly depends on the swROD and
ALTI configuration correctness for a specific setup.  In short,
PDOCalib pulses the VMM channels at a user defined pulser DAC
`sdp_dac` or ROC test pulse delay registers
`RocAnalog:reg073ePllVmm#:tp_phase_#` in several iterations depending
on the selected channel groups to be pulsed. Application iterates
through each entered register value for on channel(channel group) and
moves to next and iterates throug register values again and so
on. Possibility to pulse all channels at the same time is available.

Class contains dedicated warninigs if the connection to some
front-ends was lost before and after configuration with new register
values. It waits few seconds in 3 attempts for OPC server to
reestablish connection and if its sucessfull continues. If connection
was not reestablished - then major malfunction was experienced in the
system and felixcore and OPC server must be restarted and whole
procedure rerun.

The data recording in this calibration type is handled by the swROD
and data recording in the TDAQ partition settings must be enabled with
a desired tier0 name. the calibration tag will be added automatically
from is calibration type entry.

To run PDO/TDO calibration using this class user does following:

1. Starts TDAQ partition created by `nswdaq/NSWPartitionMaker` package

2. Declares the calibration type in the IS to enable the `PDOCalib`
   class in the run controll application in following way:

   ```bash
   is_write -p <partition_name> -n NswParams.Calib.calibType -t String -v PDOCalib(/TDOCalib)-i 0
   ```

   where depending on the -v entry either TDO (-v TDOCalib) OR PDO (-v
   PDOCalib) will be calibrated.

3. Declare the calibration run parameters:
   ```bash
   is_write -p <partition_name> -n NswParams.Calib.calibParams -t String -v <channel-group>,<reg-val-1>,<reg-val-2>,...,<reg-val-n>,*<data-coll-time[ms]>* -i 0
   ```

   example:

   ```bash
   is_write -p <partition_name> -n NswParams.Calib.calibType -t String -v PDOCalib -i 0

   is_write -p <partition_name> -n NswParams.Calib.calibParams -t String -v 8,200,300,400,*6000* -i 0
   ```

   in which user tells application to calibrate PDO with following
   input parameters:

   * 8 - Group of channels to be pulsed (supported groups are 1,2,4,8
     channels at a time, to enable all 64 VMM channels user writes any
     odd integer)
   * 200,300,400 - internal Pulser DAC register values to pulse (only
     limitation - VMM register range from 0-1024 DAC)
   * 6000 - pulsing time of 6000 milliseconds (to adjust for different
     pattern file rates)

   Note: *First entry in the -v option MUST be the channel group*, all
   other parameters can be swapped

4. Proceed with the `INITIALIZE`, `CONFIGURE`, and `START`
   transitions. Calibration starts and ends automatically

   As in the THRCalib class case all front-ends specified in the
   configuration .json file will be calibrated.

   Few pro tips in resolving problems with run

#### MMTPFeedback

#### MMTPInputPhase

#### MMTriggerCalib

#### sTGCPadTriggerToSFEB

#### sTGCRouterToTP

#### sTGCSFEBToRouter

#### sTGCStripsTriggerCalib

#### sTGCTriggerCalib

#### CalibrationSca [[deprecated]]

This class is a pre-decessor of the `THRCalib` class that basicly
works from the command line. All the functionality (except the pulser
DAC calibration) is provided by `app/Calibrate.cpp` executable. This
script allows user to :

* Configure frontends (function applicable for SCA path calibration only);
* Read channel baseline and thresholds;
* Modify configuration .json files;
* Calibrate internal pulser DAC;
(pulse.cpp, description will follow later...)

For example, lets inspect Calibate.cpp that does SCA path calibrations
(executable name - ./calibrate) that has following options (can be
viewed by using option -h):

##### General function calls (flags)
-------------------------------------------------------------------------

	* --init_conf 	-	-> send configuration to FEBs (use only for SCA calibrations - for L1 data taking use configure_frontend.cpp from NSWConfiguration);
	* --baseline 	-	-> read channel baseline;
	* --threshold	-	-> read channel thresholds;
	* --cal_thresholds	-> calibrate global and trimmer DAC;
	* --merge_config	-> merge append channel masking/trimmer values to the initial .json file;
	* --split_config	-> (for bb5/191 sites) split configuration into 4 separate .json files for HO_L1L2, HO_L3L$, IP_L1L2, and IP_L3L4 front-end mapping;
			(if Nr. of FEBs in operation is >1, function is called automatically)

Aforementioned options (except split_config) write notifications in
the CalibReport.txt in user defined directory. In case there were no
calculations with strong deviations or any misbehaving channels the
log woill just have the start message with date-time stamp and overall
procedure time.

##### Other options
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

##### Typical use cases are:

```bash
./calibrate -L MM --init_conf			#configure all FEBs which name (in.xml/.json files) have MM in their naming;
./calibrate -L HO --baseline			#read baseline of HO side FEBs with 10(x10) samples per channel;
./calibrate -L L1 --threshold			#read channel thresholds of L1(layer one) FEBs on the DW;
./calibrate -b 2 -s 5 -R 9 --cal_thresholds 	#calibrate threshold and trimmer DAC on the first two FEB VMMs in the .json file;

```

In general the sequence for the full cycle would be:

```
 --init_conf -> --threshold(OR --baseline, then got to start) -> --cal_thresholds -> --init_conf
```

with appropriate additional options.

IMPORTANT - In case of small scale operations (less than 96 FEBs) on
can give multiple function calls (flags) in one execution. IF
operation scale is > 96 FEBs it is advised to execute each function
call separately (otherwise memory is overwhelmed and process fails
with pkill error).

**Few warnings:**

	* !Always! put search string in the -L option that will match the name in working .json file - otherwise >> exception;
	* -b option sorts FEB names from .json file and starts count from 0 to entered number;
	* --baseline and --threshold can not be called simultaneously;
	* if only one FEB was calibrated - one needs to manually call --merge_config and -j options;
	* in case Nr. of FEBs to be calibrated is >64 -> call --threshold and --cal_thresholds as a separate, consequen processes - otherwise memory gets overloaded and programm flips out.(to be fixed)
	* Text file archivation is temporarily suspended to decease disk space usage (from 12.02.2020)

### Utility

#### CalibrationMath

Class holds basic mathematical expressions to transform VMM ADC/DAC
sample values to mV and backwards.  In addition it stores the
calculation constants, VMM parameters, cutoff limits for the readout
sample in a speciffic `nsw::ref_val` namespace that are use in
THRCalib and PDOCalib classes.


**Useful links:**

        [Link to plotter][data_plotter]
	[data_plotter]:https://gitlab.cern.ch/vplesano/nswcalibrationdataplotter/tree/master
