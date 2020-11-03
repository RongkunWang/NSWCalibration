#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <exception>
#include <chrono>
#include <thread>
#include <fstream>

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
    const bool help{false};
};


// struct holding best settings
struct Settings
{
    std::string ePllPhase40MHz{""};
    std::string ePllPhase160MHz_3_0{""};
    std::string ePllPhase160MHz_4{""};
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


[[nodiscard]] ValueMap getInputVals(const Mode& t_mode, nsw::FEBConfig t_config)
{
    auto config = t_config.getConfig();
    const auto analog = config.get_child("rocPllCoreAnalog");
    if (t_mode == Mode::clockPhase)
    {
        // 160MHz clock has 32 possible different values
        const int nEntries{32};

        const auto valsePllPhase40MHz = [nEntries, &analog] () {
            // 40MHz has values 112 - 127 (two times)
            const auto clock40MHzDefault = analog.get<int>("reg115.ePllPhase40MHz_0");
            const int clock40MHzStart{clock40MHzDefault & 0b1111'0000}; // xxxx 0000 (set last 4 bits to 0)

            auto vec = std::vector<std::string>(nEntries);

            // two times the same values -> generate half first
            const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )
            std::generate(vec.begin(), middle, [i=clock40MHzStart] () mutable { return std::to_string(i++); });

            // copy first half into second half
            std::copy(vec.begin(), middle, middle);

            //return std::vector<std::string>{"124"};
            return vec;
        }();

        const auto valsePllPhase160MHz_4 = [nEntries] () {
            auto vec = std::vector<std::string>(nEntries);

            const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )

            // first half fourth bit is 0, then 1
            std::generate(vec.begin(), middle, [] () { return "0"; });
            std::generate(middle, vec.end(), [] () { return "1"; });

            //return std::vector<std::string>{"1"};
            return vec;
        }();

        const auto valsePllPhase160MHz_3_0 = [nEntries] () {
            auto vec = std::vector<std::string>(nEntries);

            // range: 0-15 (two times)
            const int clock160MHzStart{0};

            const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )

            // first half fourth bit is 0, then 1
            std::generate(vec.begin(), middle, [i=clock160MHzStart] () mutable { return std::to_string(i++); });

            // copy first half into second half
            std::copy(vec.begin(), middle, middle);

            //return std::vector<std::string>{"12"};
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
    if (t_mode == Mode::clockPhase40MHz)
    {
        const int nEntries{16};
        const auto valsePllPhase40MHz = [nEntries, &analog] () {
            const auto clock40MHzDefault = analog.get<int>("reg115.ePllPhase40MHz_0");
            const int clock40MHzStart{clock40MHzDefault & 0b0000'1111}; // 0000 xxxx (set first 4 bits to 0)

            auto vec = std::vector<std::string>(nEntries);

            // change first 4 bits and add last 4 bits of default value
            std::generate(vec.begin(), vec.end(), [i=0, clock40MHzStart] () mutable { return std::to_string(clock40MHzStart + (i << 4)); });

            return vec;
        }();
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

    return nsw::FEBConfig{config};
}


void configure(nsw::FEBConfig t_config)
{
    nsw::ConfigSender configSender;
    t_config.dump();
    configSender.sendConfig(t_config);
    //configSender.sendRocConfig(t_config);
    // TEST
//std::vector<uint8_t> nsw::ConfigSender::readI2cAtAddress(std::string opcserver_ipport,
//    std::string node, uint8_t* address, size_t address_size, size_t number_of_bytes) {
    const auto full_node_name = t_config.getAddress() + "." + t_config.getRocAnalog().getName() + "." + "reg115";  // Full I2C address
    auto regAddrVec = nsw::hexStringToByteVector("0x02", 4, true);
    const auto dataread1 = configSender.readI2c(t_config.getOpcServerIp(), full_node_name, 1);
    const auto dataread = configSender.readI2c(t_config.getOpcServerIp(), full_node_name, 1);
    //const auto dataread = configSender.readI2cAtAddress(t_config.getOpcServerIp(), full_node_name, regAddrVec.data(), regAddrVec.size(), 4);
    std::cout << "SIZE " << dataread.size() << " " << full_node_name << std::endl;
    for (const auto c : dataread)
    {
        std::cout <<  unsigned(c) << std::endl;
    }
    const auto result1 = configSender.readBackRoc(t_config.getOpcServerIp(),
                                                           t_config.getAddress() + ".gpio.bitBanger",
                                                           17, // scl line
                                                           18, // sda line
                                                           static_cast<uint8_t>(155), // reg number
                                                           2); // delay
    const auto result = configSender.readBackRoc(t_config.getOpcServerIp(),
                                                           t_config.getAddress() + ".gpio.bitBanger",
                                                           17, // scl line
                                                           18, // sda line
                                                           static_cast<uint8_t>(155), // reg number
                                                           2); // delay
    std::cout << "RESULT " << unsigned(result) << std::endl;
    exit(0);


    const bool m_resetvmm{false};
    if (m_resetvmm) {
        std::vector<unsigned> reset_ori;
        for (auto& vmm : t_config.getVmms()) {
            reset_ori.push_back(vmm.getGlobalRegister("reset"));  // Set reset bits to 1
            vmm.setGlobalRegister("reset", 3);  // Set reset bits to 1
        }
        configSender.sendVmmConfig(t_config);

        size_t i = 0;
        for (auto& vmm : t_config.getVmms()) {
            vmm.setGlobalRegister("reset", reset_ori[i++]);  // Set reset bits to original
        }
        configSender.sendVmmConfig(t_config);
    }
}

//void configure(const nsw::FEBConfig& t_config, const std::string& t_registerName)
//{
//    const auto full_node_name = t_config.getAddress() + "." + t_config.getName() + "." + t_registerName;  // Full I2C address
//    const auto cfg = t_config.getRocAnalog();
////void nsw::ConfigSender::sendI2cMasterSingle(std::string opcserver_ipport, std::string topnode,
////                                            const nsw::I2cMasterConfig& cfg, std::string reg_address) {
//}

[[nodiscard]] std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>> checkVmmCaptureRegisters(const nsw::FEBConfig& t_config)
{
    std::array<uint8_t, 8> result;
    std::array<uint8_t, 8> resultParity;
    const auto opcIp = t_config.getOpcServerIp();
    // TODO: magic numbers
    const int vmmCaptureAddressInitial = 32;
    const int vmmParityCounterAddressInitial = 45;
    nsw::ConfigSender configSender;
    for (int dummy = 0; dummy < 2; dummy++)
    {
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

            const auto parityCounter = configSender.readBackRoc(opcIp,
                                                                t_config.getAddress() + ".gpio.bitBanger",
                                                                17, // scl line
                                                                18, // sda line
                                                                static_cast<uint8_t>(vmmParityCounterAddressInitial + vmmId), // reg number
                                                                2); // delay
            resultParity[vmmId] = parityCounter;
        }
    }

    return {result, resultParity};
}


void printResult(const std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>& t_result, int i)
{
    std::ofstream outfile;
    outfile.open("log_myreadout.txt", std::ios_base::app);
    outfile << "Iteration: " << i << '\n';
    const auto status = t_result.first;
    const auto parity = t_result.second;
    for (int vmmId=0; vmmId < status.size(); vmmId++)
    {
        outfile << "VMM Status/Parity" << vmmId << " : " << unsigned(status[vmmId]) << '/' << unsigned(parity[vmmId]) << " (";
        
        // Check registers (https://espace.cern.ch/ATLAS-NSW-ELX/_layouts/15/WopiFrame.aspx?sourcedoc=/ATLAS-NSW-ELX/Shared%20Documents/ROC/ROC_Reg_digital_analog_combined_annotated.xlsx&action=default)
        const auto fifo_bit{0b0001'0000};
        const auto coherency_bit{0b0000'1000};
        const auto decoder_bit{0b0000'0100};
        const auto misalignment_bit{0b000'0010};
        const auto alignment_bit{0b0000'0001};
        if (status[vmmId] & fifo_bit)
        {
            outfile << "FIFO full error, ";
        }
        if (status[vmmId] & coherency_bit)
        {
            outfile << "Coherency error, ";
        }
        if (status[vmmId] & decoder_bit)
        {
            outfile << "Decoder error, ";
        }
        if (status[vmmId] & misalignment_bit)
        {
            outfile << "Misalignment error, ";
        }
        if (not (status[vmmId] & alignment_bit))
        {
            outfile << "VMM not aligned, ";
        }
        if (parity[vmmId] > 0)
        {
            outfile << "Parity counter error = " << unsigned(parity[vmmId]);
        }
        outfile << ")\n";
    }
    outfile << '\n';
    outfile.close();
}


int analyzeResults(const std::vector<std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>>& t_results)
{
    std::vector<bool> testResults;
    testResults.reserve(t_results.size());
    std::transform(std::begin(t_results), std::end(t_results), std::back_inserter(testResults),
        [] (const std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>& t_result)
        {
            const auto status{t_result.first};
            const auto parity{t_result.second};
            const auto statusOk = std::all_of(std::begin(status), std::end(status),
                [noError = 0b0000'0001] (const auto t_val) { return t_val & noError; });
            const auto parityOk = std::all_of(std::begin(parity), std::end(parity),
                [] (const auto t_val) { return t_val == 0; });
            return statusOk and parityOk;
        }
    );

    const auto index = std::distance(std::begin(testResults), std::find_if(std::begin(testResults), std::end(testResults),
        [firstElement = testResults.at(0)] (const bool val)
        {
            return val != firstElement;
        }
    ));

    for (const auto& el : testResults)
    {
        std::cout << el << " ";
    }
    std::cout << std::endl;

    // wrap around....
    std::rotate(std::begin(testResults), std::begin(testResults) + index, std::end(testResults));

    int counterGood = 0;
    int maxCounterGood = 0;
    int endGoodRegion = 0;
    for (std::size_t i=0; i<testResults.size(); i++)
    {
        const auto& testResult = testResults[i];
        if (testResult)
        {
            counterGood++;
        }
        if (not testResult or i == testResults.size() - 1)
        {
            if (counterGood > maxCounterGood)
            {
                maxCounterGood = counterGood;
                endGoodRegion = i;
            }
            counterGood = 0;
        }
    }

    // This definetely does not need any explanantion
    return (endGoodRegion - maxCounterGood / 2 + index) % testResults.size();
}


Settings getBestSettings(const ValueMap& t_inputValues, const int t_bestIteration)
{
    return Settings{t_inputValues.at("reg115").at("ePllPhase40MHz_0").at(t_bestIteration),
                    t_inputValues.at("reg118").at("ePllPhase160MHz_0[3:0]").at(t_bestIteration),
                    t_inputValues.at("reg115").at("ePllPhase160MHz_0[4]").at(t_bestIteration)};
}


void run(const Args& args)
{
    // loop over all boards
    int debug_board_number = 0;

    for (const auto& config : splitConfigs(args.configFile, args.names))
    {
        // parse the input json containing the values for the registers which are set
        // const auto inputVals = parseInputVals(args.valuesFilename);
        const auto inputVals = getInputVals(args.mode, config);

        std::vector<std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>> allResults;
        std::cout << "WORKING ON BORD NUMBER " << debug_board_number++ << std::endl;
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

            //readVMMROC_Status(adaptedConfig, counter);
            // check the result
            const auto result = checkVmmCaptureRegisters(adaptedConfig);
            allResults.push_back(result);

            // print
            printResult(result, counter);
        }
        const auto bestIteration = analyzeResults(allResults);
        const auto bestSettings = getBestSettings(inputVals, bestIteration);
        std::cout << "Best values (iteration) " << bestIteration << '\n'
                  << "\t40MHz: " << bestSettings.ePllPhase40MHz << '\n'
                  << "\t160MHz[3:0]: " << bestSettings.ePllPhase160MHz_3_0 << '\n'
                  << "\t160MHz[4]: " << bestSettings.ePllPhase160MHz_4 << '\n';
        const auto adaptedConfig = adaptConfig(config, inputVals, bestIteration);
        configure(adaptedConfig);
        break;
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
