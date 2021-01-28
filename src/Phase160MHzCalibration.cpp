#include <fstream>

#include "NSWCalibration/Phase160MHzCalibration.h"
#include "NSWCalibration/BaseCalibration.h"

#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/ConfigConverter.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/I2cMasterConfig.h"

Phase160MHzCalibration::Phase160MHzCalibration(nsw::FEBConfig t_config, const std::vector<int>& t_input={}) : m_config(t_config),
                                                                                                              m_inputValues(getInputVals(t_input))
{
}

[[nodiscard]] ValueMap Phase160MHzCalibration::getInputVals(const std::vector<int>& t_input) const
{
    const auto valsePllPhase40MHz = [&t_input]() {
        if (t_input.empty())
        {
            // 40MHz clock has 128 possible different values
            const int nEntries{128};

            // 40MHz has values 0 - 127
            const int clock40MHzStart{0};

            auto vec = std::vector<int>(nEntries);

            std::generate(vec.begin(), vec.end(), [i = clock40MHzStart]() mutable { return i++; });

            return vec;
        }
        else
        {
            return t_input;
        }
    }();

    const auto valsePllPhase160MHz = [&valsePllPhase40MHz]() {
        auto vec = std::vector<int>(valsePllPhase40MHz.size());

        std::transform(valsePllPhase40MHz.begin(), valsePllPhase40MHz.end(), std::begin(vec),
                       [] (const auto t_val) { return t_val % 32; });

        return vec;
    }();

    for (const auto val : valsePllPhase40MHz)
    {
        std::cout << val << ' ';
    }
    std::cout << '\n';

    for (const auto val : valsePllPhase160MHz)
    {
        std::cout << val << ' ';
    }
    std::cout << '\n';

    return {{"FIXME.ePllPhase160MHz_0", valsePllPhase160MHz},
            {"FIXME.ePllPhase160MHz_1", valsePllPhase160MHz},
            {"FIXME.ePllPhase160MHz_2", valsePllPhase160MHz},
            {"FIXME.ePllPhase40MHz_0", valsePllPhase40MHz},
            {"FIXME.ePllPhase40MHz_1", valsePllPhase40MHz},
            {"FIXME.ePllPhase40MHz_2", valsePllPhase40MHz}};
}

std::string indent(int level) {
  std::string s; 
  for (int i=0; i<level; i++) s += "  ";
  return s; 
} 

void printTree (ptree &pt, int level) {
  if (pt.empty()) {
    std::cout << "\""<< pt.data()<< "\"";
  }

  else {
    if (level) std::cout << std::endl; 

    std::cout << indent(level) << "{" << std::endl;     

    for (ptree::iterator pos = pt.begin(); pos != pt.end();) {
      std::cout << indent(level+1) << "\"" << pos->first << "\": "; 

      printTree(pos->second, level + 1); 
      ++pos; 
      if (pos != pt.end()) {
        std::cout << ","; 
      }
      std::cout << std::endl;
    } 

   std::cout << indent(level) << " }";     
  }

  return; 
}

void Phase160MHzCalibration::setRegisters(const int t_iteration) const
{
    nsw::ConfigSender configSender;
    const auto ptree = BaseCalibration<Phase160MHzCalibration>::createPtree(m_inputValues, t_iteration);
    const auto configConverter = ConfigConverter(ptree, ConfigConverter::RegisterAddressSpace::ROC_ANALOG, ConfigConverter::ConfigType::VALUE_BASED);
    //auto translatedPtree = configConverter.getRegisterBasedConfigWithoutSubregisters<ConfigConverter::RegisterAddressSpace::ROC_ANALOG>(m_config.getOpcServerIp(), m_config.getAddress());
    auto translatedPtree = configConverter.getRegisterBasedConfigWithoutSubregisters(m_config.getRocAnalog());
    const auto partialConfig = nsw::I2cMasterConfig(translatedPtree, ROC_ANALOG_NAME, ROC_ANALOG_REGISTERS, true);
    std::cout << "HEELO FND ME IN THIS BULLSHIUT OUTPUT\n";
    printTree(translatedPtree, 0);
    const auto opcIp = m_config.getOpcServerIp();
    const auto scaAddress = m_config.getAddress();

    BaseCalibration<Phase160MHzCalibration>::basicConfigure(m_config);
    std::cout << "BEFORE REG 19 \n" <<  static_cast<unsigned int>(configSender.readBackRocDigital(opcIp, scaAddress, 19)) << '\n';
    //configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocCoreResetN", 0);
    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocPllResetN", 0);
    //configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocSResetN", 0);

    //configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocSResetN", 1);

    configSender.sendI2cMasterConfig(opcIp, scaAddress, partialConfig);
    //for (const auto& entry : m_inputValues)
    //{
    //    std::cout << "SET REG " << entry.first << std::endl;
    //    configSender.sendI2cMasterSingle(opcIp, scaAddress, analog, entry.first);
    //    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    //}

    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocPllResetN", 1);

    bool roc_locked = 0;
    while (!roc_locked) {
        bool rPll1 = configSender.readGPIO(opcIp, scaAddress + ".gpio.rocPllLocked");
        bool rPll2 = configSender.readGPIO(opcIp, scaAddress + ".gpio.rocPllRocLocked");
        roc_locked = rPll1 & rPll2;
    }

    //configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocCoreResetN", 1);

    std::cout << "REG 116 \n" << static_cast<unsigned int>(configSender.readBackRocAnalog(opcIp, scaAddress, 116)) << '\n';
    std::cout << "REG 64 \n" <<  static_cast<unsigned int>(configSender.readBackRocAnalog(opcIp, scaAddress, 64)) << '\n';
    std::cout << "REG 19 \n" <<  static_cast<unsigned int>(configSender.readBackRocDigital(opcIp, scaAddress, 19)) << '\n';
}

void Phase160MHzCalibration::saveBestSettings(const int t_bestIteration, const std::string &t_filename) const
{
    const auto bestPhase40MHz = m_inputValues.at("FIXME.ePllPhase40MHz_0").at(t_bestIteration);
    const auto bestPhase160MHz = m_inputValues.at("FIXME.ePllPhase160MHz_0").at(t_bestIteration);
    std::cout << "Best values (iteration) " << t_bestIteration << '\n'
              << "\t40MHz:  " << bestPhase40MHz << '\n'
              << "\t160MHz: " << bestPhase160MHz << '\n';
    std::ofstream outfile;
    outfile.open(t_filename);
    for (const auto& [registerName, values] : m_inputValues)
    {
        outfile << "rocCoreAnalog." << registerName << ':' << values[t_bestIteration] << '\n';
    }
    outfile.close();
}

std::size_t Phase160MHzCalibration::getNumberOfConfigurations() const
{
    return m_inputValues.begin()->second.size();
}

int Phase160MHzCalibration::getValueOfIteration(const int t_iteration) const
{
    return m_inputValues.at("FIXME.ePllPhase40MHz_0").at(t_iteration);
}
