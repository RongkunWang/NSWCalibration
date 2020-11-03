#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <exception>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWCalibration/BaseCalibration.h"
#include "NSWCalibration/Phase160MHzCalibration.h"

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace po = boost::program_options;


enum Mode
{
    none=-1,
    clockPhase,
    clockPhase40MHz,
};


// struct holding command line arguments
struct Args
{
    const std::string configFile{""};
    const bool dryRun{false};
    const std::vector<std::string> names{};
    //const std::string valuesFilename{""};
    const Mode mode{Mode::none};
    const std::string outputFilename{""};
    const bool help{false};
};


[[nodiscard]] Args parseArgs(int argc, char* argv[])
{
    // Command line parsing
    po::options_description desc("Calibrate the phases of the ROC clocks");
    desc.add_options()
        ("help,h", "show this help message")
        ("config_file,c", po::value<std::string>()->
            default_value("/afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/config-files/config_json/VS/vs_1mhz_phase2_8MMFE8_masked.json"),
            "Configuration file path")
        ("dry-run", po::bool_switch()->
            default_value(false), "Option to NOT send configurations")
        ("name,n", po::value<std::vector<std::string>>()->multitoken()->default_value(std::vector<std::string>(), ""),
            "The names of frontend to read ROC register.\n If this option is left empty, "
            "all front end elements in the config file will be calibrated.")
        //("values", po::value<std::string>()->required(),
        //    "JSON file containing a map {'registername1' : [values1], 'registername2' : [values2]}. "
        //    "All values for are changed simultaneously for all registernames. This means all value lists must have the same length");
        ("mode", po::value<std::string>()->required(),
            "Choose which calibration to run. Available options: [ClockPhase]");
        ("output,o", po::value<std::string>()->
            default_value("best_settings.txt"),"Name of outputfile for best settings")

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
        std::cout << desc << '\n';
        return Args{.help=true};
    }

    po::notify(vm);

    const auto mode = [&vm] () {
        const auto cfgValMode = vm["mode"].as<std::string>();
        if (cfgValMode == "ClockPhase")
        {
            return Mode::clockPhase;
        }
        else
        {
            throw std::runtime_error("Value for mode '" + cfgValMode + "' is not accepted");
        }
    }();

    return Args{vm["config_file"].as<std::string>(),
                vm["dry-run"].as<bool>(),
                vm["name"].as<std::vector<std::string>>(),
                mode,
                vm["output"].as<std::string>()
                };
}


[[nodiscard]] std::vector<nsw::FEBConfig> splitConfigs(const std::string& t_configFile, const std::vector<std::string>& t_names)
{
    // takes either vector or set...
    nsw::ConfigReader reader("json://" + t_configFile);
    const auto func = [&reader] (const auto& t_names) {
        std::vector<nsw::FEBConfig> feb_configs;
        // TODO: Adapt this???
        for (const auto& name : t_names)
        {
            std::cout << name << std::endl;
            if (name == "MMFE8_0000")
            {
                std::cout << "GOOD" << std::endl;
            }
            try
            {
                if (nsw::getElementType(name) == "MMFE8")
                {
                    feb_configs.emplace_back(reader.readConfig(name));
                    std::cout << "Adding: " << name << '\n';
                }
                else
                {
                    std::cout << "Skipping: " << name
                              << " because its a " << nsw::getElementType(name)
                              << '\n';
                }
            }
            catch (const std::exception& e)
            {
                std::cout << "WARNING! Skipping FE " << name
                          << " - Problem constructing configuration due to : " << e.what() << '\n';
                throw;
            }
        }
        return feb_configs;
    };

    if (t_names.empty())
    {
        return func(reader.getAllElementNames());
    }
    else
    {
        return func(t_names);
    }

}

int main(int argc, char* argv[])
{
    const auto args = parseArgs(argc, argv);
    if (args.help)
    {
        return 0;
    }

    for (const auto& config : splitConfigs(args.configFile, args.names))
    {
        if (args.mode == Mode::clockPhase)
        {
            BaseCalibration<Phase160MHzCalibration> calibrator(config);
            calibrator.run(args.dryRun, args.outputFilename);
        }
        break;
    }

    return 0;
}
