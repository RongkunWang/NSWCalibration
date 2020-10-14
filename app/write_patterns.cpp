#include "NSWCalibration/MMTriggerCalib.h"

int main(int argc, char **argv) {
  auto calib = new nsw::MMTriggerCalib("MMTrackPulserTest");
  calib->setup("xjia@lxplus.cern.ch:/afs/cern.ch/user/x/xjia/public/*.json");
  return 0;
}
