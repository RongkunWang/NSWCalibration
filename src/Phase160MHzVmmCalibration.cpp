#include <fstream>

#include "NSWCalibration/Phase160MHzVmmCalibration.h"
#include "NSWCalibration/BaseCalibration.h"

#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/ConfigConverter.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/I2cMasterConfig.h"

Phase160MHzVmmCalibration::Phase160MHzVmmCalibration(nsw::FEBConfig t_config, const std::vector<int>& t_input={}) : m_config(t_config),
                                                                                                                    m_inputValues(getInputVals(t_input))
{
}

[[nodiscard]] ValueMap Phase160MHzVmmCalibration::getInputVals(const std::vector<int>& t_input) const
{

    const auto valsePllPhase160MHz = [&t_input]() {
        if (t_input.empty())
        {
            // 40MHz clock has 128 possible different values
            const int nEntries{32};

            // 40MHz has values 0 - 127
            const int clock160MHzStart{0};

            auto vec = std::vector<int>(nEntries);

            std::generate(vec.begin(), vec.end(), [i = clock160MHzStart]() mutable { return i++; });

            return vec;
        }
        else
        {
            return t_input;
        }
    }();

    for (const auto val : valsePllPhase160MHz)
    {
        std::cout << val << ' ';
    }
    std::cout << '\n';

    return {{"ePllVmm0.ePllPhase160MHz_0", valsePllPhase160MHz},
            {"ePllVmm0.ePllPhase160MHz_1", valsePllPhase160MHz},
            {"ePllVmm0.ePllPhase160MHz_2", valsePllPhase160MHz},
            {"ePllVmm0.ePllPhase160MHz_3", valsePllPhase160MHz},
            {"ePllVmm1.ePllPhase160MHz_0", valsePllPhase160MHz},
            {"ePllVmm1.ePllPhase160MHz_1", valsePllPhase160MHz},
            {"ePllVmm1.ePllPhase160MHz_2", valsePllPhase160MHz},
            {"ePllVmm1.ePllPhase160MHz_3", valsePllPhase160MHz}};
}

std::string indent2(int level) {
  std::string s; 
  for (int i=0; i<level; i++) s += "  ";
  return s; 
} 

void printTree2 (ptree &pt, int level) {
  if (pt.empty()) {
    std::cout << "\""<< pt.data()<< "\"";
  }

  else {
    if (level) std::cout << std::endl; 

    std::cout << indent2(level) << "{" << std::endl;     

    for (ptree::iterator pos = pt.begin(); pos != pt.end();) {
      std::cout << indent2(level+1) << "\"" << pos->first << "\": "; 

      printTree2(pos->second, level + 1); 
      ++pos; 
      if (pos != pt.end()) {
        std::cout << ","; 
      }
      std::cout << std::endl;
    } 

   std::cout << indent2(level) << " }";     
  }

  return; 
}

void Phase160MHzVmmCalibration::setRegisters(const int t_iteration) const
{
    nsw::ConfigSender configSender;
    const auto ptree = BaseCalibration<Phase160MHzVmmCalibration>::createPtree(m_inputValues, t_iteration);
    const auto configConverter = ConfigConverter(ptree, ConfigConverter::RegisterAddressSpace::ROC_ANALOG, ConfigConverter::ConfigType::VALUE_BASED);
    auto translatedPtree = configConverter.getRegisterBasedConfigWithoutSubregisters(m_config.getRocAnalog());
    printTree2(translatedPtree, 0);
    const auto partialConfig = nsw::I2cMasterConfig(translatedPtree, ROC_ANALOG_NAME, ROC_ANALOG_REGISTERS, true);
    const auto opcIp = m_config.getOpcServerIp();
    const auto scaAddress = m_config.getAddress();

    //std::cout << "BEFORE BASIC REG64\n " << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, 64)) << '\n';
    //std::cout << "BEFORE BASIC REG68\n " << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, 68)) << '\n';
    for (auto regNumber : {64, 65, 66, 67, 68, 69, 80, 81, 82, 83, 84, 85})
    {
        std::cout << "BEFORE BASIC REG " << regNumber << '\n' << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, regNumber)) << '\n';
        //std::cout << "FINAL REG68\n " << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, 68)) << '\n';
    }
    BaseCalibration<Phase160MHzVmmCalibration>::checkVmmCaptureRegisters(m_config);
    BaseCalibration<Phase160MHzVmmCalibration>::basicConfigure(m_config);
    //std::cout << "AFTER BASIC REG64\n " << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, 64)) << '\n';
    //std::cout << "AFTER BASIC REG68\n " << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, 68)) << '\n';
    for (auto regNumber : {64, 65, 66, 67, 68, 69, 80, 81, 82, 83, 84, 85})
    {
        std::cout << "AFTER BASIC REG " << regNumber << '\n' << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, regNumber)) << '\n';
        //std::cout << "FINAL REG68\n " << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, 68)) << '\n';
    }
    BaseCalibration<Phase160MHzVmmCalibration>::checkVmmCaptureRegisters(m_config);
    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocCoreResetN", 0);
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

    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocCoreResetN", 1);

    for (auto regNumber : {64, 65, 66, 67, 68, 69, 80, 81, 82, 83, 84, 85, 116})
    {
        std::cout << "FINAL REG " << regNumber << '\n' << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, regNumber)) << '\n';
        //std::cout << "FINAL REG68\n " << static_cast<int>(configSender.readBackRocAnalog(opcIp, scaAddress, 68)) << '\n';
    }
    BaseCalibration<Phase160MHzVmmCalibration>::checkVmmCaptureRegisters(m_config);
}

void Phase160MHzVmmCalibration::saveBestSettings(const int t_bestIteration, const std::string &t_filename) const
{
    const auto bestPhase160MHz = m_inputValues.at("ePllVmm0.ePllPhase160MHz_0").at(t_bestIteration);
    std::cout << "Best values (iteration) " << t_bestIteration << '\n'
              << "\t160MHz: " << bestPhase160MHz << '\n';
    std::ofstream outfile;
    outfile.open(t_filename);
    for (const auto& [registerName, values] : m_inputValues)
    {
        outfile << "rocCoreAnalog." << registerName << ':' << values[t_bestIteration] << '\n';
    }
    outfile.close();
}

std::size_t Phase160MHzVmmCalibration::getNumberOfConfigurations() const
{
    return m_inputValues.begin()->second.size();
}

int Phase160MHzVmmCalibration::getValueOfIteration(const int t_iteration) const
{
    return m_inputValues.at("ePllVmm0.ePllPhase160MHz_0").at(t_iteration);
}
