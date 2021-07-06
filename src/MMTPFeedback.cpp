#include "NSWCalibration/MMTPFeedback.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Constants.h"

#include <unistd.h>
#include <stdexcept>

#include <boost/property_tree/json_parser.hpp>

#include "ers/ers.h"

nsw::MMTPFeedback::MMTPFeedback() {
  m_sim            = false;
  m_debug          = false;
  m_fname_data     = "";
  m_fname_root_i   = "";
  m_fname_root_o   = "";
  m_fname_json_i   = "";
  m_fname_json_o   = "";
  m_factor_channel = 10;
  m_factor_vmm     = 10;
  m_threshold_incr = 10;
  m_max_threads    = 100;
  m_loop           = 0;
  m_threads        = std::make_unique< std::vector< std::future<void> > >();
}

void nsw::MMTPFeedback::AnalyzeNoise() {
  DecodeChannelRateDataFile();
  LoadChannelRateRootFile();
  AnalyzeTTree();
  SendConfigurations();
  WriteOutputRootFile();
  Summarize();
}

void nsw::MMTPFeedback::AnalyzeTTree() {
  InitializeRates();
  LoadRates();
  MakeListOfNoisyChannels();
  MakeListOfNoisyVMMs();
}

void nsw::MMTPFeedback::SendConfigurations() {
  UpdatePtree();
  LoadConfigs();
  // SetMaskedChannels();
  // SetVMMThresholds();
  SendFEBConfigs();
}

void nsw::MMTPFeedback::InitializeRates() {
  m_rates = std::vector<nsw::MMTPChannelRate>();
}

void nsw::MMTPFeedback::LoadRates() {
  //
  // Read channel rates from the TTree
  //
  for (size_t ent = 0; ent < m_entries; ent++) {
    //
    // Consider the latest measurement only
    //
    if (m_entries - ent > nsw::mmfe8::MMFE8_PER_SECTOR) {
      continue;
    }
    m_rtree->GetEntry(ent);
    auto mmfe8 = MakeMMFE8Name();
    for (size_t it = 0; it < m_ch->size(); it++) {
      MMTPChannelRate measurement;
      measurement.mmfe8 = mmfe8;
      measurement.vmm   = static_cast<int>(m_vmm         ->at(it));
      measurement.ch    = static_cast<int>(m_ch          ->at(it));
      measurement.rate  = static_cast<int>(m_channel_rate->at(it));
      measurement.layer = static_cast<int>(m_octupletLayer);
      measurement.pos   = static_cast<int>(m_boardPosition);
      m_rates.push_back(measurement);
    }
  }
  if (m_rates.size() != nsw::mmfe8::NUM_CH_PER_SECTOR) {
    throw std::runtime_error("Problem: m_rates.size() = "
                             + std::to_string(m_rates.size()));
  }

}

void nsw::MMTPFeedback::MakeListOfNoisyChannels() {
  //
  // Is "noisy" relative or absolute? Lets do absolute for now.
  //
  // Absolute:
  // "Noisy" if channel rate > m_factor_channel
  //
  // Relative:
  // "Noisy" if channel rate > m_factor_channel * VMM median rate
  //
  m_noisy_channels = std::vector<nsw::MMTPChannelRate>();
  for (const auto& meas: m_rates) {
    if (meas.rate > NoisyChannelFactor()) {
      m_noisy_channels.push_back(meas);
      std::cout << "Noisy channel:"
                << " " << meas.mmfe8
                << " " << "Layer "    << meas.layer
                << " " << "FEB Pos. " << meas.pos
                << " " << "VMM "      << meas.vmm
                << " " << "CH "       << meas.ch
                << " " << "Rate "     << meas.rate
                << std::endl;
    }
  }
}

void nsw::MMTPFeedback::MakeListOfNoisyVMMs() {
  m_noisy_vmms    = std::vector<nsw::MMTPVMMRate>();
  auto vmm_rates  = std::vector<nsw::MMTPVMMRate>();

  //
  // initialize VMM rates
  //
  for (const auto& meas: m_rates) {
    if (meas.ch == 0) {
      MMTPVMMRate vmm_meas;
      vmm_meas.mmfe8       = meas.mmfe8;
      vmm_meas.vmm         = meas.vmm;
      vmm_meas.rates       = std::vector<int>();
      vmm_meas.median_rate = 0;
      vmm_meas.layer       = meas.layer;
      vmm_meas.pos         = meas.pos;
      vmm_rates.push_back(vmm_meas);
    }
  }

  //
  // get VMM rates from channel rates
  //
  for (const auto& meas: m_rates) {
    for (auto& vmm_meas: vmm_rates) {
      if (meas.layer == vmm_meas.layer &&
          meas.pos   == vmm_meas.pos   &&
          meas.vmm   == vmm_meas.vmm) {
        vmm_meas.rates.push_back(meas.rate);
        break;
      }
    }
  }

  //
  // get VMM median rate and assess noisiness
  //
  for (auto& vmm_meas: vmm_rates) {
    vmm_meas.median_rate = GetMedian(vmm_meas.rates);
    if (vmm_meas.median_rate > NoisyVMMFactor()) {
      m_noisy_vmms.push_back(vmm_meas);
      std::cout << "Noisy VMM:"
                << " " << vmm_meas.mmfe8
                << " " << "Layer "    << vmm_meas.layer
                << " " << "FEB Pos. " << vmm_meas.pos
                << " " << "VMM "      << vmm_meas.vmm
                << " " << "Rate "     << vmm_meas.median_rate
                << std::endl;
    }
  }

}

std::string nsw::MMTPFeedback::MakeMMFE8Name() {
  //
  // Looks like: MMFE8_L1P1_IPL
  //
  std::string quad = (m_octupletLayer < 4) ? "IP" : "HO";
  std::string side = (m_rightLeft == 0)    ? "R" : "L";
  std::string quadlayer = std::to_string(static_cast<size_t>(m_quadrupletLayer));
  std::string pcb       = std::to_string(static_cast<size_t>(m_pcb));
  return "MMFE8_L" + quadlayer + "P" + pcb + "_" + quad + side;
}

void nsw::MMTPFeedback::DecodeChannelRateDataFile() {
  //
  // This will be replaced by something which can run in ATLAS
  //
  std::cout << "Decoding " << ChannelRateDataFile() << std::endl;
  const std::string clone   = "/afs/cern.ch/work/t/tuna/public/nswtriggerdatadecoder";
  const std::string decoder = clone + "/scripts/NSWTriggerDataDecoder.py";
  const std::string args_i  = " -i " + ChannelRateDataFile();
  const std::string args_o  = " -o " + ChannelRateRootFile();
  const std::string args_d  = " -d mmchannelrates ";
  const std::string args    = args_i + args_o + args_d;
  const std::string cmd     = decoder + args;
  std::cout << "Running:\n" << cmd << std::endl;
  auto ret = std::system(cmd.c_str());
  if (ret != 0) {
    throw std::runtime_error("Failed: exit code " + std::to_string(ret));
  }
}

void nsw::MMTPFeedback::LoadChannelRateRootFile() {
  //
  // Load ROOT TTree
  //
  m_rfile = std::make_unique< TFile >(ChannelRateRootFile().c_str(), "read");
  m_rtree = std::shared_ptr< TTree >(
    static_cast<TTree*>(m_rfile->Get(ChannelRateTreeName().c_str()))
  );

  m_channel_rate   = std::make_unique< std::vector<int> >();
  m_vmmPosition    = std::make_unique< std::vector<int> >();
  m_chPosition     = std::make_unique< std::vector<int> >();
  m_vmm            = std::make_unique< std::vector<int> >();
  m_ch             = std::make_unique< std::vector<int> >();

  m_rtree->SetMakeClass(1);
  m_rtree->SetBranchAddress("fiber",           &m_fiber,           &b_fiber);
  m_rtree->SetBranchAddress("mmfe8",           &m_mmfe8,           &b_mmfe8);
  m_rtree->SetBranchAddress("cycle",           &m_cycle,           &b_cycle);
  m_rtree->SetBranchAddress("octupletLayer",   &m_octupletLayer,   &b_octupletLayer);
  m_rtree->SetBranchAddress("quadrupletLayer", &m_quadrupletLayer, &b_quadrupletLayer);
  m_rtree->SetBranchAddress("rightLeft",       &m_rightLeft,       &b_rightLeft);
  m_rtree->SetBranchAddress("pcb",             &m_pcb,             &b_pcb);
  m_rtree->SetBranchAddress("boardPosition",   &m_boardPosition,   &b_boardPosition);
  m_rtree->SetBranchAddress("v_channel_rate",  &m_channel_rate,    &b_channel_rate);
  m_rtree->SetBranchAddress("v_vmmPosition",   &m_vmmPosition,     &b_vmmPosition);
  m_rtree->SetBranchAddress("v_chPosition",    &m_chPosition,      &b_chPosition);
  m_rtree->SetBranchAddress("v_vmm",           &m_vmm,             &b_vmm);
  m_rtree->SetBranchAddress("v_ch",            &m_ch,              &b_ch);
  std::cout << "File    : " << ChannelRateRootFile() << std::endl;
  std::cout << "Entries : " << m_rtree->GetEntries() << std::endl;
  m_entries = m_rtree->GetEntries();
  if (m_entries < nsw::mmfe8::MMFE8_PER_SECTOR) {
    throw std::runtime_error("Problem: TTree entries < N(MMFE8s)");
  }

}

int nsw::MMTPFeedback::GetMedian(std::vector<int>& v) {
  size_t n = v.size() / 2;
  std::nth_element(v.begin(), v.begin()+n, v.end());
  float median = v[n];
  return median;
}

void nsw::MMTPFeedback::LoadConfigs() {
  //
  // Load FEBConfig objects
  //
  m_febs = MakeFEBConfigs();
  if (Debug()) {
    for (const auto& feb: m_febs) {
      std::cout << "Found "
                << feb.getAddress()
                << " @ "
                << feb.getOpcServerIp()
                << std::endl;
    }
  }
}

void nsw::MMTPFeedback::SetMaskedChannels() {
  for (const auto& meas: m_noisy_channels) {
    for (auto& feb: m_febs) {
      if (meas.mmfe8 == feb.getAddress()) {
        if (Debug()) {
          std::cout << "Masking"
                    << " "     << meas.mmfe8
                    << " VMM " << meas.vmm
                    << " CH "  << meas.ch
                    << std::endl;
        }
        feb.getVmm(meas.vmm)
          .setChannelRegisterOneChannel("channel_sm", 1, meas.ch);
        break;
      }
    }
  }
}

void nsw::MMTPFeedback::SetVMMThresholds() {
  for (const auto& vmm_meas: m_noisy_vmms) {
    for (auto& feb: m_febs) {
      if (vmm_meas.mmfe8 == feb.getAddress()) {
        auto& vmm = feb.getVmm(vmm_meas.vmm);
        auto threshold = vmm.getGlobalRegister(m_threshold);
        vmm.setGlobalRegister(m_threshold, threshold + ThresholdIncrease());
        if (Debug()) {
          std::cout << "Increasing threshold: "
                    << " "     << vmm_meas.mmfe8
                    << " VMM " << vmm_meas.vmm
                    << " "     << threshold
                    << " -> "  << threshold + ThresholdIncrease()
                    << std::endl;
        }
        break;
      }
    }
  }
}

std::vector<nsw::FEBConfig> nsw::MMTPFeedback::MakeFEBConfigs() {

  //
  // load the ptree
  //
  nsw::ConfigReader reader(m_feb_config);
  auto cfg   = reader.readConfig();
  auto names = reader.getAllElementNames();

  //
  // get FEBs
  //
  const std::string& element_type = "MMFE8";
  std::vector<nsw::FEBConfig> configs;
  for (const auto & nm : names) {
    if (nsw::getElementType(nm) == element_type) {
      configs.emplace_back(reader.readConfig(nm));
    }
  }

  return configs;
}

void nsw::MMTPFeedback::SendFEBConfigs() {
  //
  // Launch threads to configure FEBs
  //
  m_threads->clear();
  for (const auto& feb : m_febs) {
    while (TooManyThreads()) {
      usleep(500000);
    }
    m_threads->push_back(std::async(std::launch::async,
                                    &nsw::MMTPFeedback::SendFEBConfig,
                                    this,
                                    feb));
  }
  for (auto& thread : *m_threads) {
    thread.get();
  }
  m_threads->clear();
}

void nsw::MMTPFeedback::SendFEBConfig(const nsw::FEBConfig& feb) {
  //
  // Configure the VMMs of 1 FEB
  //
  auto sender = std::make_unique<nsw::ConfigSender>();
  if (!Simulation()) {
    sender->sendVmmConfig(feb);
  }
}

bool nsw::MMTPFeedback::TooManyThreads() {
  //
  // Check how many threads are active
  //
  size_t nthreads = ActiveThreads();
  bool decision = (nthreads >= m_max_threads);
  if (decision) {
    std::cout << "Too many active threads ("
              << nthreads
              << "), waiting for fewer than "
              << m_max_threads << std::endl;
  }
  return decision;
}

size_t nsw::MMTPFeedback::ActiveThreads() {
  //
  // Count the number of active threads
  //
  size_t nfinished = 0;
  for (auto& thread : *m_threads)
    if (thread.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      nfinished++;
  return m_threads->size() - nfinished;
}

void nsw::MMTPFeedback::UpdatePtree() {
  //
  // Im sorrying navigating ptrees is complicated
  //

  //
  // read
  //
  boost::property_tree::read_json(InputJsonFile(), m_feb_config);

  //
  // raise thresholds
  //
  UpdatePtreeVmmThreshold();

  //
  // mask channels
  //
  UpdatePtreeChannelMask();

  //
  // write to file
  //
  WriteOutputJsonFile();

}

void nsw::MMTPFeedback::UpdatePtreeVmmThreshold() {
  //
  // NB: vmm json object must have existing sdt_dac
  //
  for (const auto& vmm_meas: m_noisy_vmms) {

    for (auto& feb_kv : m_feb_config) {

      //
      // match noisy VMM to FEBConfig
      //
      if (feb_kv.first != vmm_meas.mmfe8) {
        continue;
      }

      for (auto& vmm_kv: feb_kv.second) {

        //
        // match noisy VMM to VMM ptree
        //
        if (vmm_kv.first != "vmm" + std::to_string(vmm_meas.vmm)) {
          continue;
        }

        //
        // increase threshold
        //
        auto threshold = vmm_kv.second.get<int>(m_threshold);
        vmm_kv.second.put(m_threshold, threshold + ThresholdIncrease());

        std::cout << "Increasing threshold (json):"
                  << " "     << vmm_meas.mmfe8
                  << " VMM " << vmm_meas.vmm
                  << " "     << threshold
                  << " -> "  << threshold + ThresholdIncrease()
                  << std::endl;

        break;

      }

      break;

    }

  }

}

void nsw::MMTPFeedback::UpdatePtreeChannelMask() {
  //
  // NB: VMM json object need not have channel_sm
  //
  for (const auto& meas: m_noisy_channels) {

    for (auto& feb_kv : m_feb_config) {

      //
      // match noisy channel to FEBConfig
      //
      if (feb_kv.first != meas.mmfe8) {
        continue;
      }

      for (auto& vmm_kv: feb_kv.second) {

        //
        // match noisy channel to VMM ptree
        //
        if (vmm_kv.first != "vmm" + std::to_string(meas.vmm)) {
          continue;
        }

        auto& vmm_tree = vmm_kv.second;
        auto mask = std::vector<unsigned>(nsw::vmm::NUM_CH_PER_VMM);

        if (vmm_tree.count(m_channel_mask) == 0) {
          //
          // If channel mask child doesnt exist:
          //   Assume all channels are unmasked (0).
          //
        } else {
          //
          // Else, get channel mask child
          //
          auto ptemp = vmm_tree.get_child(m_channel_mask);
          if (ptemp.empty()) {
            //
            // There is a single value for register
            //   -> all channels have the same value
            //
            auto val = vmm_tree.get<unsigned>(m_channel_mask);
            for (size_t it = 0; it < mask.size(); it++) {
              mask.at(it) = val;
            }
          } else {
            //
            // There is a array
            // -> each channel has different value
            //
            int this_ch = 0;
            for (auto& ch_kv: ptemp) {
              mask.at(this_ch) = ch_kv.second.get<unsigned>("");
              this_ch++;
            }
          }
        }

        //
        // mask this channel and write to ptree
        //
        mask.at(meas.ch) = 1;
        auto temp = nsw::buildPtreeFromVector(mask);
        vmm_tree.erase(m_channel_mask);
        vmm_tree.add_child(m_channel_mask, temp);
        break;

      }

    }

  }

}

void nsw::MMTPFeedback::WriteOutputJsonFile() {
  std::ostringstream oss;
  boost::property_tree::write_json(oss, m_feb_config);
  std::cout << std::endl;
  std::cout << "Writing updated config to " << OutputJsonFile() << std::endl;
  std::cout << std::endl;
  std::ofstream file;
  file.open(OutputJsonFile());
  file << oss.str();
  file.close();
}

void nsw::MMTPFeedback::WriteOutputRootFile() {

  //
  // output
  //
  std::cout << std::endl;
  std::cout << "Writing output ROOT file: " << std::endl;
  std::cout << OutputRootFile() << std::endl;
  std::cout << std::endl;
  auto rfile = std::make_unique< TFile >(OutputRootFile().c_str(), "recreate");
  auto rtree = std::make_shared< TTree >("nsw", "nsw");

  //
  // declarations for TTree
  //
  std::string now = "";
  uint32_t noisy_channel_factor = -1;
  uint32_t noisy_channel_n      = -1;
  auto noisy_channel_mmfe8 = std::make_unique< std::vector<std::string> >();
  auto noisy_channel_vmm   = std::make_unique< std::vector<uint32_t> >();
  auto noisy_channel_ch    = std::make_unique< std::vector<uint32_t> >();
  auto noisy_channel_rate  = std::make_unique< std::vector<uint32_t> >();
  auto noisy_channel_layer = std::make_unique< std::vector<uint32_t> >();
  auto noisy_channel_pos   = std::make_unique< std::vector<uint32_t> >();

  //
  // define TTree branches
  //
  rtree->Branch("time",                 &now);
  rtree->Branch("noisy_channel_factor", &noisy_channel_factor);
  rtree->Branch("noisy_channel_n",      &noisy_channel_n);
  rtree->Branch("noisy_channel_mmfe8",   noisy_channel_mmfe8.get());
  rtree->Branch("noisy_channel_vmm",     noisy_channel_vmm.get());
  rtree->Branch("noisy_channel_ch",      noisy_channel_ch.get());
  rtree->Branch("noisy_channel_rate",    noisy_channel_rate.get());
  rtree->Branch("noisy_channel_layer",   noisy_channel_layer.get());
  rtree->Branch("noisy_channel_pos",     noisy_channel_pos.get());

  //
  // fill branches
  //
  now = nsw::calib::utils::strf_time();
  noisy_channel_factor = NoisyChannelFactor();
  noisy_channel_n      = m_noisy_channels.size();
  for (const auto& meas: m_noisy_channels) {
    noisy_channel_mmfe8->push_back(meas.mmfe8);
    noisy_channel_vmm  ->push_back(meas.vmm);
    noisy_channel_ch   ->push_back(meas.ch);
    noisy_channel_rate ->push_back(meas.rate);
    noisy_channel_layer->push_back(meas.layer);
    noisy_channel_pos  ->push_back(meas.pos);
  }

  //
  // fill TTree and close
  //
  rtree->Fill();
  rfile->cd();
  rtree->Write();
  rfile->Close();

}

void nsw::MMTPFeedback::Summarize() {
  std::cout << std::endl;
  std::cout << "Noisy channel factor       :: " << NoisyChannelFactor()    << std::endl;
  std::cout << "Noisy VMM factor           :: " << NoisyVMMFactor()        << std::endl;
  std::cout << "---------------------------------------------------------" << std::endl;
  std::cout << "N(channels to mask)        :: " << m_noisy_channels.size() << std::endl;
  std::cout << "N(VMMs to raise threshold) :: " << m_noisy_vmms.size()     << std::endl;
  std::cout << std::endl;
}

