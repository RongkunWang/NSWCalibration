#include "NSWCalibration/MMTriggerCalib.h"

int main(int argc, char **argv) {
  auto calib = new nsw::MMTriggerCalib("MMTrackPulserTest");
  calib->setup("json:///afs/cern.ch/user/x/xjia/public/test.json");
  return 0;
}
