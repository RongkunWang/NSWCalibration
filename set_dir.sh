mkdir -p $1/NSWCalibrationData/{config_files,calib_data,calib_json,thresholds,baselines,test_pulse_dac}
printf "This file will contain warning reports of the NSWCalibration\n">CalibReport.txt
mv CalibReport.txt $1/NSWCalibrationData/
