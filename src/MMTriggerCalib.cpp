#include "NSWCalibration/MMTriggerCalib.h"
using boost::property_tree::ptree;

nsw::MMTriggerCalib::MMTriggerCalib(std::string calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::MMTriggerCalib::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run   = 0;
  m_reset_vmm = 0;
  m_threads = std::make_unique< std::vector< std::future<int> > >();
  m_threads->clear();

  if (m_calibType=="MMARTConnectivityTest" ||
      m_calibType=="MMARTConnectivityTestAllChannels") {
    m_phases = {-1};
    m_connectivity = true;
    m_tracks       = false;
    m_noise        = false;
    m_latency      = false;
  } else if (m_calibType=="MMTrackPulserTest") {
    m_phases = {-1};
    m_connectivity = false;
    m_tracks       = true;
    m_noise        = false;
    m_latency      = false;
  } else if (m_calibType=="MMCableNoise") {
    m_phases = {-1};
    m_connectivity = false;
    m_tracks       = false;
    m_noise        = true;
    m_latency      = false;
  } else if (m_calibType=="MMARTPhase") {
    m_phases = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    m_connectivity = true;
    m_tracks       = false;
    m_noise        = false;
    m_latency      = false;
  } else if (m_calibType=="MML1ALatency") {
    m_phases = {-1};
    m_connectivity = false;
    m_tracks       = false;
    m_noise        = false;
    m_latency      = true;
  } else {
    throw std::runtime_error("Unknown calibration request. Can't set up MMTriggerCalib: " + m_calibType);
  }

  m_patterns = patterns();
  write_json("test.json", m_patterns);
  setTotal((int)(m_patterns.size()));
  setToggle(1);
  setWait4swROD(0);
  if (m_latency)
    setToggle(0);

  m_febs   = make_objects<nsw::FEBConfig> (db, "MMFE8");
  m_addcs  = make_objects<nsw::ADDCConfig>(db, "ADDC");
  m_tps    = make_objects<nsw::TPConfig>  (db, "TP");

  ERS_INFO("Found " << m_febs.size()     << " MMFE8s");
  ERS_INFO("Found " << m_addcs.size()    << " ADDCs");
  ERS_INFO("Found " << m_tps.size()      << " TPs");
  ERS_INFO("Found " << m_phases.size()   << " ART input phases");
  ERS_INFO("Found " << m_patterns.size() << " patterns");

  m_watchdog = std::async(std::launch::async, &nsw::MMTriggerCalib::addc_tp_watchdog, this);
}

void nsw::MMTriggerCalib::configure() {

  for (auto toppattkv : m_patterns) {

    int ipatt = pattern_number(toppattkv.first);
    if (ipatt != counter())
      continue;
    auto tr = toppattkv.second;

    ERS_INFO("Configure " << toppattkv.first
             << " with ART phase = " << tr.get<int>("art_input_phase")
             << " and TP L1A latency = " << tr.get<int>("tp_latency")
             );

    // enable test pulse
    configure_febs_from_ptree(tr, true);

    // set addc phase
    configure_addcs_from_ptree(tr);

    // send TP config ("ECR")
    configure_tps(tr);

    // record some data?
    if (m_latency)
      sleep(5);
  }

}

void nsw::MMTriggerCalib::unconfigure() {

  for (auto toppattkv : m_patterns) {

    int ipatt = pattern_number(toppattkv.first);
    if (ipatt != counter())
      continue;
    auto tr = toppattkv.second;

    ERS_INFO("Un-configure " << toppattkv.first);

    // disable test pulse
    configure_febs_from_ptree(tr, false);
  }

}

int nsw::MMTriggerCalib::wait_until_done() {
  for (auto& thread : *m_threads)
    thread.get();
  m_threads->clear();
  return 0;
}

int nsw::MMTriggerCalib::pattern_number(std::string name) {
  return std::stoi( std::regex_replace(name, std::regex("pattern_"), "") );
}

int nsw::MMTriggerCalib::configure_febs_from_ptree(ptree tr, bool unmask) {
  //
  // if unmask and first art phase: send configuration
  // if   mask and  last art phase: send configuration
  //
  auto phase = tr.get<int>("art_input_phase");
  if (unmask) {
    if (phase != m_phases.front()) {
      return 0;
    }
  } else {
    if (phase != m_phases.back()) {
      return 0;
    }
  }

  for (auto pattkv : tr) {
    auto febpatt_n = pattkv.first;
    auto febtr     = pattkv.second;
    if (febpatt_n.find("febpattern_") == std::string::npos)
      continue;
    announce(febpatt_n, febtr, unmask);
    for (auto febkv : febtr) {
      for (auto & feb : m_febs) {
        if (febkv.first != feb.getAddress())
          continue;
        m_threads->push_back(std::async(std::launch::async,
                                        &nsw::MMTriggerCalib::configure_vmms, this,
                                        feb, febkv.second, unmask));
        break;
      }
    }
    wait_until_done();
  }
  return 0;
}

int nsw::MMTriggerCalib::configure_addcs_from_ptree(ptree tr) {
  auto phase = tr.get<int>("art_input_phase");
  if (phase != -1) {
    if (phase == m_phases.front())
      std::cout << "ART phase: " << std::endl;
    std::cout << std::hex << phase << std::dec << std::flush;
    if (phase == m_phases.back())
      std::cout << std::endl;
    for (auto & addc : m_addcs)
      m_threads->push_back(std::async(std::launch::async,
                                      &nsw::MMTriggerCalib::configure_art_input_phase, this,
                                      addc, phase));
    wait_until_done();
  }
  return 0;
}

int nsw::MMTriggerCalib::configure_tps(ptree tr) {
  auto latency = tr.get<int>("tp_latency");
  for (auto & tp : m_tps) {
    if (latency != -1)
      tp.setARTWindowCenter(latency);
    auto cs = std::make_unique<nsw::ConfigSender>();
    while (m_tpscax_busy)
      usleep(1e5);
    m_tpscax_busy = 1;
    if (!m_dry_run)
      cs->sendTpConfig(tp);
    m_tpscax_busy = 0;
  }
  return 0;
}

int nsw::MMTriggerCalib::announce(std::string name, ptree tr, bool unmask) {
  std::cout << " Configure MMFE8s (" << (unmask ? "unmask" : "mask") << ") with " << name << std::endl << std::flush;
  for (auto febkv : tr) {
    std::cout << "  " << febkv.first;
    for (auto vmmkv : febkv.second) {
      std::cout << " " << vmmkv.first;
      for (auto chkv : vmmkv.second)
        std::cout << "/" << chkv.second.get<unsigned>("");
    }
    std::cout << std::endl << std::flush;
  }
  return 0;
}

int nsw::MMTriggerCalib::configure_vmms(nsw::FEBConfig feb, ptree febpatt, bool unmask) {
  //
  // Example febpatt ptree:
  // {
  //   "0": ["0", "1", "2"],
  //   "1": ["0"],
  //   "2": ["0"]
  // }
  //
  auto cs = std::make_unique<nsw::ConfigSender>();
  int vmmid, chan;
  std::set<int> vmmids = {};
  for (auto vmmkv : febpatt) {
    for (auto chkv : vmmkv.second) {
      vmmid = std::stoi(vmmkv.first);
      chan  = chkv.second.get<int>("");
      feb.getVmm(vmmid).setChannelRegisterOneChannel("channel_st", unmask ? 1 : 0, chan);
      feb.getVmm(vmmid).setChannelRegisterOneChannel("channel_sm", unmask ? 0 : 1, chan);
      vmmids.emplace(vmmid);
    }
  }
  if (m_reset_vmm) {
    for (auto vmmid : vmmids) {
      auto & vmm = feb.getVmm(vmmid);
      auto orig = vmm.getGlobalRegister("reset");
      vmm.setGlobalRegister("reset", 3);
      if (!m_dry_run)
        cs->sendVmmConfigSingle(feb, vmmid);
      vmm.setGlobalRegister("reset", orig);
    }
  }
  if (vmmids.size() == 8) {
    if (!m_dry_run)
      cs->sendVmmConfig(feb);
  } else {
    for (auto vmmid : vmmids)
      if (!m_dry_run)
        cs->sendVmmConfigSingle(feb, vmmid);
  }
  return 0;
}

int nsw::MMTriggerCalib::configure_art_input_phase(nsw::ADDCConfig addc, uint phase) {
  auto cs = std::make_unique<nsw::ConfigSender>();
  if (phase > std::pow(2, 4))
    throw std::runtime_error("Gave bad phase to configure_art_input_phase: " + std::to_string(phase));
  size_t art_size = 2;
  uint8_t art_data[] = {0x0, 0x0};
  auto opc_ip   = addc.getOpcServerIp();
  auto sca_addr = addc.getAddress();
  uint8_t this_phase = phase + (phase << 4);
  // std::cout << "Setting input phase of " << sca_addr << " to be 0x"
  //           << std::hex << (uint)(this_phase) << std::dec << std::endl;
  for (auto art : addc.getARTs()) {
    auto name = sca_addr + "." + art.getName() + "Ps" + "." + art.getName() + "Ps";
    for (auto reg : { 6,  7,  8,  9,
          21, 22, 23, 24,
          36, 37, 38, 39,
          51, 52, 53, 54,
          }) {
      art_data[0] = (uint8_t)(reg);
      art_data[1] = this_phase;
      if (!m_dry_run)
        cs->sendI2cRaw(opc_ip, name, art_data, art_size);
    }
  }
  return 0;
}

ptree nsw::MMTriggerCalib::patterns() {
  ptree patts;
  int ipatts   = 0;
  int ifebpatt = 0;

  if (m_noise) {
    //
    // cable noise loop: no patterns
    //
    int npatts = 1000;
    for (int i = 0; i < npatts; i++) {
      ptree feb_patt;
      ptree top_patt;
      top_patt.put("tp_latency", -1);
      top_patt.put("art_input_phase", -1);
      top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
      patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
      ipatts++;
      ifebpatt++;
    }
  } else if (m_latency) {
    //
    // latency loop: incrementing latency
    //               no FEB or ADDC patterns
    //
    int npatts = 100;
    for (int i = 0; i < npatts; i++) {
      ptree feb_patt;
      ptree top_patt;
      top_patt.put("tp_latency", i);
      top_patt.put("art_input_phase", -1);
      top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
      patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
      ipatts++;
      ifebpatt++;
    }
  } else if (m_connectivity) {
    //
    // connectivity max-parallel loop
    //
    bool even;
    int nvmm = 8;
    int nchan = 64;
    int pcb = 0;
    for (int pos = 0; pos < 8; pos++) {
      even = pos % 2 == 0;
      pcb  = pos / 2 + 1;
      auto pcbstr       = std::to_string(pcb);
      auto pcbstr_plus4 = std::to_string(pcb+4);
      for (int chan = 0; chan < nchan; chan++) {
        if (m_calibType == "MMARTConnectivityTest" && chan % 10 != 0)
          continue;
        if (m_calibType == "MMARTPhase"            && chan % 10 != 0)
          continue;
        ptree feb_patt;
        for (auto name : {
              "MMFE8_L1P" + pcbstr       + "_HO" + (even ? "R" : "L"),
              "MMFE8_L2P" + pcbstr       + "_HO" + (even ? "L" : "R"),
              "MMFE8_L3P" + pcbstr       + "_HO" + (even ? "R" : "L"),
              "MMFE8_L4P" + pcbstr       + "_HO" + (even ? "L" : "R"),
              "MMFE8_L4P" + pcbstr       + "_IP" + (even ? "R" : "L"),
              "MMFE8_L3P" + pcbstr       + "_IP" + (even ? "L" : "R"),
              "MMFE8_L2P" + pcbstr       + "_IP" + (even ? "R" : "L"),
              "MMFE8_L1P" + pcbstr       + "_IP" + (even ? "L" : "R"),
              "MMFE8_L1P" + pcbstr_plus4 + "_HO" + (even ? "R" : "L"),
              "MMFE8_L2P" + pcbstr_plus4 + "_HO" + (even ? "L" : "R"),
              "MMFE8_L3P" + pcbstr_plus4 + "_HO" + (even ? "R" : "L"),
              "MMFE8_L4P" + pcbstr_plus4 + "_HO" + (even ? "L" : "R"),
              "MMFE8_L4P" + pcbstr_plus4 + "_IP" + (even ? "R" : "L"),
              "MMFE8_L3P" + pcbstr_plus4 + "_IP" + (even ? "L" : "R"),
              "MMFE8_L2P" + pcbstr_plus4 + "_IP" + (even ? "R" : "L"),
              "MMFE8_L1P" + pcbstr_plus4 + "_IP" + (even ? "L" : "R"),
              }) {
          ptree febtree;
          for (int vmmid = 0; vmmid < nvmm; vmmid++) {

            //if (vmm_of_interest >= 0 && vmmid != vmm_of_interest)
            //  continue;

            // this might seem stupid, and it is, but
            //   it allows to write a vector of channels per VMM
            ptree vmmtree;
            ptree chantree;
            chantree.put("", chan);
            vmmtree.push_back(std::make_pair("", chantree));
            febtree.add_child(std::to_string(vmmid), vmmtree);
          }
          feb_patt.add_child(name, febtree);
        }

        for (auto art_phase : m_phases) {
          ptree top_patt;
          top_patt.put("tp_latency", -1);
          top_patt.put("art_input_phase", art_phase);
          top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
          patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
          ipatts++;
        }

        ifebpatt++;
      }
    }
  } else if (m_tracks) {
/*
        //
        // track-like loop
        //
        bool even;
        int nvmm = 8;
        int nchan = 64;
        int pcb = 0;
        for (int pos = 0; pos < 16; pos++) {
            even = pos % 2 == 0;
            pcb  = pos / 2 + 1;
            auto pcbstr = std::to_string(pcb);
            for (int vmmid = 0; vmmid < nvmm; vmmid++) {
                // if (vmm_of_interest >= 0 && vmmid != vmm_of_interest)
                //     continue;
                for (int chan = 0; chan < nchan; chan++) {
                    if (chan % 10 != 0)
                        continue;
                    ptree feb_patt;
                    for (auto name : {"MMFE8_L1P" + pcbstr + "_HO" + (even ? "R" : "L"),
                                "MMFE8_L2P" + pcbstr + "_HO" + (even ? "L" : "R"),
                                "MMFE8_L3P" + pcbstr + "_HO" + (even ? "R" : "L"),
                                "MMFE8_L4P" + pcbstr + "_HO" + (even ? "L" : "R"),
                                "MMFE8_L4P" + pcbstr + "_IP" + (even ? "R" : "L"),
                                "MMFE8_L3P" + pcbstr + "_IP" + (even ? "L" : "R"),
                                "MMFE8_L2P" + pcbstr + "_IP" + (even ? "R" : "L"),
                                "MMFE8_L1P" + pcbstr + "_IP" + (even ? "L" : "R")}) {
                        ptree febtree;
                        // this might seem stupid, and it is, but
                        //   it allows to write a vector of channels per VMM
                        ptree vmmtree;
                        ptree chantree;
                        chantree.put("", chan);
                        vmmtree.push_back(std::make_pair("", chantree));
                        febtree.add_child(std::to_string(vmmid), vmmtree);
                        feb_patt.add_child(name, febtree);
                    }
                    for (auto art_phase : m_phases) {
                      ptree top_patt;
                      top_patt.put("tp_latency", -1);
                      top_patt.put("art_input_phase", art_phase);
                      top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
                      patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
                      ifebpatt++;
                      ipatts++;
                    }
                }
            }
        }
*/
        //
        // homogeneously distributted perpendicular tracks
        //

        bool even;
        int nvmm = 8;
        int nchan = 64;
        int countId[8192][4];
        for (int pos = 0; pos < 16; pos++) {
            even = pos % 2 == 0;
            for (int vmmid = 0; vmmid < nvmm; vmmid++) {
                for (int chan = 0; chan < nchan; chan++) {
                    int count = (even ? ((pos+1)*512-vmmid*64-chan-1) : (pos*512+vmmid*64+chan));
                    countId[count][0] = count;
                    countId[count][1] = pos;
                    countId[count][2] = vmmid;
                    countId[count][3] = chan;
                }
            }
        }
        for (int cx = 0; cx < 8192; cx++) {
            if (cx % 100 != 0)
                continue;
            for (int dif = -3000; dif < 3001; dif++) {
                if (dif % 100 != 0)
                    continue;
                int posx = countId[cx][1];
                int vmmidx = countId[cx][2];
                int chanx = countId[cx][3];
                bool evenx = posx % 2 == 0;
                int pcbx  = posx / 2 + 1;
                auto pcbstrx = std::to_string(pcbx);
                ptree feb_patt;
                for (auto name : {"MMFE8_L1P" + pcbstrx + "_HO" + (evenx ? "R" : "L"),
                            "MMFE8_L2P" + pcbstrx + "_HO" + (evenx ? "L" : "R"),
                            "MMFE8_L2P" + pcbstrx + "_IP" + (evenx ? "R" : "L"),
                            "MMFE8_L1P" + pcbstrx + "_IP" + (evenx ? "L" : "R")}) {
                    ptree febtree;
                    ptree vmmtree;
                    ptree chantree;
                    chantree.put("", chanx);
                    vmmtree.push_back(std::make_pair("", chantree));
                    febtree.add_child(std::to_string(vmmidx), vmmtree);
                    feb_patt.add_child(name, febtree);
                }
                int cu = cx + dif;
                if (cu >= 0 && cu < 8192){
                    int posu = countId[cu][1];
                    int vmmidu = countId[cu][2];
                    int chanu = countId[cu][3];
                    bool evenu = posu % 2 == 0;
                    int pcbu  = posu / 2 + 1;
                    auto pcbstru = std::to_string(pcbu);
                    for (auto name : {"MMFE8_L4P" + pcbstru + "_HO" + (evenu ? "L" : "R"),
                                "MMFE8_L3P" + pcbstru + "_IP" + (evenu ? "L" : "R")}) {
                        ptree febtree;
                        ptree vmmtree;
                        ptree chantree;
                        chantree.put("", chanu);
                        vmmtree.push_back(std::make_pair("", chantree));
                        febtree.add_child(std::to_string(vmmidu), vmmtree);
                        feb_patt.add_child(name, febtree);
                    }
                }
                int cv = cx - dif;
                if (cv >= 0 && cv < 8192){
                    int posv = countId[cv][1];
                    int vmmidv = countId[cv][2];
                    int chanv = countId[cv][3];
                    bool evenv = posv % 2 == 0;
                    int pcbv  = posv / 2 + 1;
                    auto pcbstrv = std::to_string(pcbv);
                    for (auto name : {"MMFE8_L3P" + pcbstrv + "_HO" + (evenv ? "L" : "R"),
                                "MMFE8_L4P" + pcbstrv + "_IP" + (evenv ? "L" : "R")}) {
                        ptree febtree;
                        ptree vmmtree;
                        ptree chantree;
                        chantree.put("", chanv);
                        vmmtree.push_back(std::make_pair("", chantree));
                        febtree.add_child(std::to_string(vmmidv), vmmtree);
                        feb_patt.add_child(name, febtree);
                    }
                }
                for (auto art_phase : m_phases) {
                      ptree top_patt;
                      top_patt.put("tp_latency", -1);
                      top_patt.put("art_input_phase", art_phase);
                      top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
                      patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
                      ifebpatt++;
                      ipatts++;
                }
            }
        }
    }
  return patts;
}

std::string nsw::MMTriggerCalib::strf_time() {
  std::stringstream ss;
  std::string out;
  std::time_t result = std::time(nullptr);
  std::tm tm = *std::localtime(&result);
  ss << std::put_time(&tm, "%Y_%m_%d_%Hh%Mm%Ss");
  ss >> out;
  return out;
}

int nsw::MMTriggerCalib::addc_tp_watchdog() {
  //
  // Be forewarned: this function reads TP SCAX registers.
  // Dont race elsewhere.
  //

  if (m_addcs.size() == 0)
    return 0;

  nsw::ConfigSender cs;

  // sleep time
  size_t slp = 1;

  // collect all TPs from the ARTs
  std::set< std::pair<std::string, std::string> > tps;
  for (auto & addc : m_addcs)
    for (auto art : addc.getARTs())
      tps.emplace(std::make_pair(art.getOpcServerIp_TP(), art.getOpcNodeId_TP()));
  auto regAddrVec = nsw::hexStringToByteVector("0x02", 4, true);

  // output file and announce
  std::string fname = "addc_alignment_" + strf_time() + ".txt";
  std::ofstream myfile;
  myfile.open(fname);
  ERS_INFO("ADDC-TP watchdog. Output: " << fname << ". Sleep: " << slp << "s");

  // monitor
  while (counter() < total()) {
    myfile << "Time " << strf_time() << std::endl;
    for (auto tp : tps) {
      while (m_tpscax_busy)
        usleep(1e5);
      m_tpscax_busy = 1;
      auto outdata = m_dry_run ? std::vector<uint8_t>(4) :
        cs.readI2cAtAddress(tp.first, tp.second, regAddrVec.data(), regAddrVec.size(), 4);
      m_tpscax_busy = 0;
      for (auto & addc : m_addcs) {
        for (auto art : addc.getARTs()) {
          if (art.IsMyTP(tp.first, tp.second)) {
            auto aligned = art.IsAlignedWithTP(outdata);
            std::stringstream result;
            result << addc.getAddress()         << " "
                   << art.getName()             << " "
                   << art.TP_GBTxAlignmentBit() << " "
                   << aligned << std::endl;
            myfile << result.str();
          }
        }
      }
    }
    sleep(slp);
  }

  // close
  ERS_INFO("Closing " << fname);
  myfile.close();
  return 0;
}

template <class T>
std::vector<T> nsw::MMTriggerCalib::make_objects(std::string cfg, std::string element_type, std::string name) {

  // create config reader
  nsw::ConfigReader reader1(cfg);
  try {
    auto config1 = reader1.readConfig();
  }
  catch (std::exception & e) {
    std::cout << "Make sure the json is formed correctly. "
              << "Can't read config file due to : " << e.what() << std::endl;
    std::cout << "Exiting..." << std::endl;
    exit(0);
  }

  // parse input names
  std::set<std::string> names;
  if (name != "") {
    if (std::count(name.begin(), name.end(), ',')) {
      std::istringstream ss(name);
      while (!ss.eof()) {
        std::string buf;
        std::getline(ss, buf, ',');
        if (buf != "")
          names.emplace(buf);
      }
    } else {
      names.emplace(name);
    }
  } else {
    names = reader1.getAllElementNames();
  }

  // make objects
  std::vector<T> configs;
  std::cout << "Adding:" << std::endl;
  for (auto & nm : names) {
    try {
      if (nsw::getElementType(nm) == element_type) {
        configs.emplace_back(reader1.readConfig(nm));
        std::cout << " " << nm;
        if (configs.size() % 4 == 0)
          std::cout << std::endl;
      }
    }
    catch (std::exception & e) {
      std::cout << nm << " - ERROR: Skipping this FE!"
                << " - Problem constructing configuration due to : " << e.what() << std::endl;
    }
  }
  std::cout << std::endl;

  return configs;

}

