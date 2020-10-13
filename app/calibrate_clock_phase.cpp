#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <exception>
#include <chrono>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"


namespace po = boost::program_options;


enum Mode
{
    none=-1,
    clockPhase,
};


// struct holding command line arguments
struct Args
{
    const std::string configFile{""};
    const bool dryRun{false};
    const std::vector<std::string> names{};
    //const std::string valuesFilename{""};
    const Mode mode{Mode::none};
    const bool help{false};
};


using ValueMap = std::map<std::string, std::map<std::string, std::vector<std::string>>>;


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
                //vm["values"].as<std::string>(),
                mode
                };
}


//[[nodiscard]] std::map<std::string, std::vector<std::string>> parseInputVals(const std::string& t_filename)
[[nodiscard]] ValueMap getInputVals(const Mode& t_mode)
{
    //using boost::property_tree::ptree;
    //ptree pt;
    //read_json(t_filename, pt);
    //std::map<std::string, std::vector<std::string>> result;

    //// parse pt to map
    //try
    //{
    //    for (const auto& [registerName, subpt] : pt)
    //    {
    //        result[registerName] = {};
    //        result[registerName].reserve(subpt.size());
    //        for (const auto& [dummy, value] : subpt)
    //        {
    //            result[registerName].push_back(value.data());
    //        }
    //    }
    //}
    //catch (const boost::property_tree::ptree_error&)
    //{
    //    std::cerr << "Wrong format of value json. Example format: "
    //              << "{\n\t\"reg1\" : [1, 2, 3],\n"
    //              << "\t\"reg2\" : [10, 12, 13],\n"
    //              << "}\n";
    //    throw;
    //}

    //// check size
    //const auto required_size = result.begin()->second.size();
    //if (not std::all_of(result.begin(), result.end(), [required_size] (const auto& pair) { return pair.second.size() == required_size; }))
    //{
    //    throw std::runtime_error("Not all lists in values json have the same length");
    //}
    if (t_mode == Mode::clockPhase)
    {
        // 160MHz clock has 32 possible different values
        const int nEntries{32};

        const auto valsePllPhase40MHz = [nEntries] () {
            // 40MHz has values 112 - 127 (two times)
            const int clock40MhzStart{112};

            auto vec = std::vector<std::string>(nEntries);

            // two times the same values -> generate half first
            const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )
            std::generate(vec.begin(), middle, [i=clock40MhzStart] () mutable { return std::to_string(i++); });

            // copy first half into second half
            std::copy(vec.begin(), middle, middle);

            return vec;
        }();

        const auto valsePllPhase160MHz_4 = [nEntries] () {
            auto vec = std::vector<std::string>(nEntries);

            const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )

            // first half fourth bit is 0, then 1
            std::generate(vec.begin(), middle, [] () { return "0"; });
            std::generate(middle, vec.end(), [] () { return "1"; });

            return vec;
        }();

        const auto valsePllPhase160MHz_3_0 = [nEntries] () {
            auto vec = std::vector<std::string>(nEntries);

            // range: 0-15 (two times)
            const int clock160MhzStart{0};

            const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )

            // first half fourth bit is 0, then 1
            std::generate(vec.begin(), middle, [i=clock160MhzStart] () mutable { return std::to_string(i++); });

            // copy first half into second half
            std::copy(vec.begin(), middle, middle);

            return vec;
        }();

        return {{"reg115",
                    {{"ePllPhase160MHz_0[4]", valsePllPhase160MHz_4},
                     {"ePllPhase40MHz_0", valsePllPhase40MHz}}},
                {"reg116",
                    {{"ePllPhase160MHz_1[4]", valsePllPhase160MHz_4},
                     {"ePllPhase40MHz_1", valsePllPhase40MHz}}},
                {"reg117",
                    {{"ePllPhase160MHz_2[4]", valsePllPhase160MHz_4},
                     {"ePllPhase40MHz_2", valsePllPhase40MHz}}},
                {"reg118",
                    {{"ePllPhase160MHz_0[3:0]", valsePllPhase160MHz_3_0},
                     {"ePllPhase160MHz_1[3:0]", valsePllPhase160MHz_3_0}}},
                {"reg119",
                    {{"ePllPhase160MHz_2[3:0]", valsePllPhase160MHz_3_0}}}};
    }
    return {};
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


[[nodiscard]] nsw::FEBConfig adaptConfig(const nsw::FEBConfig& t_config, const ValueMap t_vals, int i)
{
    // creates a copy...
    auto config = t_config.getConfig();

    // loop over registers
    for (const auto& [registerName, settings] : t_vals)
    {
        // loop over names in registers
        for (const auto& [key, values] : settings)
        {
            // Set value TODO: digital?
            const std::string analogName{"rocPllCoreAnalog"};
            const std::string settingName{analogName + '.' + registerName + '.' + key};
            // Check if value exists
            try
            {
                config.get<std::string>(settingName);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Did not find key " << settingName << "in config\n";
                throw;
            }

            config.put(settingName, values[i]);
        }
    }

    // TODO: delete me
    //std::cout << i << '\n';
    //boost::property_tree::write_json(std::cout, config);

    return nsw::FEBConfig{config};
}


void configure(const nsw::FEBConfig& t_config)
{
    nsw::ConfigSender configSender;
    configSender.sendRocConfig(t_config);
}


[[nodiscard]] std::array<uint8_t, 8> checkVmmCaptureRegisters(const nsw::FEBConfig& t_config)
{
    std::array<uint8_t, 8> result;
    const auto opcIp = t_config.getOpcServerIp();
    // TODO: magic numbers
    const int vmmCaptureAddressInitial = 32; 
    nsw::ConfigSender configSender;
    for (int vmmId = 0; vmmId <= 7; vmmId++)
    {
        // see NSWConfiguration/app/vmm_capture_status.cpp
        const auto vmmCaptureStatus = configSender.readBackRoc(opcIp,
                                                               t_config.getAddress() + ".gpio.bitBanger",
                                                               17, // scl line
                                                               18, // sda line
                                                               static_cast<uint8_t>(vmmCaptureAddressInitial + vmmId), // reg number
                                                               2); // delay
        result[vmmId] = vmmCaptureStatus;
    }

    return result;
}


void printResult(const std::array<uint8_t, 8>& t_result)
{
    int counter = 0;
    const int vmmCaptureAddressInitial = 32; 
    for (const auto& val : t_result)
    {
        std::cout << "Register " << vmmCaptureAddressInitial + counter++ << " : " << val << " (";
        
        // Check registers (https://espace.cern.ch/ATLAS-NSW-ELX/_layouts/15/WopiFrame.aspx?sourcedoc=/ATLAS-NSW-ELX/Shared%20Documents/ROC/ROC_Reg_digital_analog_combined_annotated.xlsx&action=default)
        const auto fifo_bit{0b0001'0000};
        const auto coherency_bit{0b0001'0000};
        const auto decoder_bit{0b0001'0000};
        const auto misalignment_bit{0b0001'0000};
        const auto alignment_bit{0b0001'0000};
        if (val & fifo_bit)
        {
            std::cout << "FIFO full error, ";
        }
        if (val & coherency_bit)
        {
            std::cout << "Coherency error, ";
        }
        if (val & decoder_bit)
        {
            std::cout << "Decoder error, ";
        }
        if (val & misalignment_bit)
        {
            std::cout << "Misalignment error, ";
        }
        if (not (val & alignment_bit))
        {
            std::cout << "VMM not aligned";
        }
        std::cout << ")\n";
    }
    std::cout << '\n';
}


void run(const Args& args)
{
    // parse the input json containing the values for the registers which are set
    // const auto inputVals = parseInputVals(args.valuesFilename);
    const auto inputVals = getInputVals(args.mode);

    // loop over all boards
    for (const auto& config : splitConfigs(args.configFile, args.names))
    {
        // iterate through settings (vector in map of map)
        for (std::size_t counter=0; counter<inputVals.begin()->second.begin()->second.size(); counter++)
        {
            // change setting in config
            const auto adaptedConfig = adaptConfig(config, inputVals, counter);

            // do nothing if dry run
            if (args.dryRun)
            {
                continue;
            }

            // configure the board
            configure(adaptedConfig);

            // wait for one second
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            // check the result
            const auto result = checkVmmCaptureRegisters(adaptedConfig);

            // print TODO: make this nice
            printResult(result);
        }
    }
}


int main(int argc, char* argv[])
{
    const auto args = parseArgs(argc, argv);
    if (args.help)
    {
        return 0;
    }

    run(args);

    return 0;
}
