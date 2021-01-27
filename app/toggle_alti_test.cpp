#include <string>

#include "RunControl/Common/OnlineServices.h"
#include "RunControl/RunControl.h"
#include "RunControl/Common/RunControlCommands.h"
#include "ipc/core.h"


int main(int argc, char*[])
{
    const std::string app_name = "Alti_RCD";
    const std::string cmd_name = [argc] () {if (argc == 1) return "StopPatternGenerator"; else return "StartPatternGenerator";}();
    const std::string partition_name = "part-VS-1MHzTest";
    //char* argv[7] = {"/afs/cern.ch/work/n/nswdaq/public/nswdaq/tdaq-09-02-01/nswdaq/installed/x86_64-centos7-gcc8-opt/bin/NSWConfigRc_main", "-n", "VS-Config", "-P", "VerticalSliceTests", "-s" ,"VerticalSliceTests"};
    //int nargs = 7;
    //IPCCore::init(nargs, argv);
    IPCCore::init({{"-n", "VS-Config"}, {"-P", "VerticalSliceTests"}, {"-s" ,"VerticalSliceTests"}});
    const daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(partition_name, "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    return 0;
}
