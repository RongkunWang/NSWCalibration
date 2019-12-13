mkdir -p $1/NSWCalibrationData/{config_files,calib_data,calib_json,thresholds,baselines,test_pulse_dac}
printf "This file will contain warning reports of the NSWCalibration\n">CalibReport.txt
mv CalibReport.txt $1/NSWCalibrationData/

folders=$1/NSWCalibration
opc=$2

printf "{\n\"config_dir\":\"$folders/config_files/\",\n\"configuration_json\":\"integration_config_ala_bb5.json\",\n\"cal_data_output\":\"$folders/calib_data/\",\n\"thr_data_output\":\"$folders/thresholds/\",\n\"json_data_output\":\"$folders/calib_json/\",\n\"opc_server_name\":\"$opc\",\n\"opc_server_port\":\":48020\",\n\"bl_data_output\":\"$folders/baselines/\",\n\"tp_data_output\":\"$folders/test_pulse_dac/\",\n\"tp_data_output2\":\"$folders/test_pulse_dac2/\",\n\"report_log\":\"folders/CalibReport.txt\"\n}">lxpus_input_data.json
