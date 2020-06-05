// Main application to run in the partition

#include <string>
#include <memory>

#include "boost/program_options.hpp"

#include "ers/ers.h"
#include "ipc/core.h"
#include "NSWCalibration/NSWCalibRc.h"
#include "RunControl/RunControl.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
    try {
        IPCCore::init(argc, argv);
    }
    catch(daq::ipc::CannotInitialize& e) {
        ers::fatal(e);
        abort();
    }
    catch(daq::ipc::AlreadyInitialized& e) {
        ers::warning(e);
    }

    std::string name("");
    bool simulation;

    po::options_description desc("This application controls the NSWCalibRc.");
    desc.add_options()
        ("simulation", po::bool_switch(&simulation)->default_value(false),
        "Run in simulation mode, don't send any configuration to FEs");
    try {
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
        po::notify(vm);

        // Parser for the ItemCtrl
        daq::rc::CmdLineParser cmdParser(argc, argv, true);

        // Instantiate ItemCtrl with proper Controllable
        daq::rc::ItemCtrl itemCtrl(cmdParser, std::shared_ptr<daq::rc::Controllable>(new nsw::NSWCalibRc(simulation)));
        itemCtrl.init();
        itemCtrl.run();
    }
    catch(daq::rc::CmdLineHelp& ex) {
        std::cout << desc << std::endl;
        std::cout << ex.message() << std::endl;
    }
    catch(ers::Issue& ex) {
        ers::fatal(ex);
        return EXIT_FAILURE;
    }
    catch(boost::program_options::error& ex) {
        ers::fatal(daq::rc::CmdLineError(ERS_HERE, ex.what(), ex));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
