workdir=$1NSWCalibrationData/
#opc=$2
mkdir -p $workdir/{config_files,calib_data,calib_json,thresholds,baselines,test_pulse_dac,test_pulse_dac2}
#mkdir -p $1/NSWCalibrationData/{config_files,calib_data,calib_json,thresholds,baselines,test_pulse_dac}
printf "This file will contain warning reports of the NSWCalibration\n">CalibReport.txt
#mv CalibReport.txt $1/NSWCalibrationData/
mv CalibReport.txt $workdir

#workdir=$1/NSWCalibration
#opc=$2

printf "{\n\"config_dir\":\"$workdir/config_files/\",\n\"configuration_json\":\"integration_config_ala_bb5.json\",\n\"cal_data_output\":\"$workdir/calib_data/\",\n\"thr_data_output\":\"$workdir/thresholds/\",\n\"json_data_output\":\"$workdir/calib_json/\",\n\"opc_server_name\":\"$opc\",\n\"opc_server_port\":\":48020\",\n\"bl_data_output\":\"$workdir/baselines/\",\n\"tp_data_output\":\"$workdir/test_pulse_dac/\",\n\"tp_data_output2\":\"$workdir/test_pulse_dac2/\",\n\"report_log\":\"$workdir/CalibReport.txt,\"\n\"archive\":\"$workdir/archive/\n\"}">lxplus_input_data.json

mkdir -p $workdir/archive/{RootFiles,TextFiles}
