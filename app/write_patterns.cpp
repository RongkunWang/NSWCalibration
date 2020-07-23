#include "NSWCalibration/MMTriggerCalib.h"

int main(int argc, char **argv) {
  auto calib = new nsw::MMTriggerCalib("MMARTPhase");
  calib->setup("json://dummy");
  return 0;
}
