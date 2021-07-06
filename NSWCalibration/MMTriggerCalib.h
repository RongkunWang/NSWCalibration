#ifndef MMTRIGGERCALIB_H_
#define MMTRIGGERCALIB_H_

//
// Derived class for NSW ART input phase calib
//

#include <future>
#include <string>
#include <vector>

#include "NSWCalibration/CalibAlg.h"

#include "NSWConfiguration/ADDCConfig.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/TPConfig.h"

#include "TFile.h"
#include "TTree.h"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWMMTriggerCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class MMTriggerCalib: public CalibAlg {

  public:
    MMTriggerCalib(const std::string& calibType);
    virtual ~MMTriggerCalib() = default;
    void setup(const std::string& db) override;
    void configure() override;
    void unconfigure() override;

  public:
    boost::property_tree::ptree patterns() const;
    template <class T>
      std::vector<T> make_objects(const std::string& cfg, std::string element_type, std::string name = "");
    int pattern_number(const std::string& name) const;
    int configure_febs_from_ptree(const boost::property_tree::ptree& tr, bool unmask);
    int configure_addcs_from_ptree(const boost::property_tree::ptree& tr);
    int configure_vmms(nsw::FEBConfig feb, const boost::property_tree::ptree& febpatt, bool unmask) const;
    int configure_art_input_phase(nsw::ADDCConfig addc, uint phase) const;
    int configure_tps(const boost::property_tree::ptree& tr);
    int addc_tp_watchdog();

    //
    // Read the hit counters of all ART ASICs.
    // The output is saved to a ROOT file, where
    //  each entry of the TTree is 32 counters read from 1 ASIC.
    // On first iteration, create the ROOT file and tree.
    // On last iteration, close the file.
    //
    int read_arts_counters();

    //
    // https://espace.cern.ch/ATLAS-NSW-ELX/Shared%20Documents/ART/art2_registers_v.xlsx
    //
    std::vector<uint32_t> read_art_counters(const nsw::ADDCConfig& addc, int art) const;
    int wait_until_done();
    int announce(const std::string& name, const boost::property_tree::ptree& tr, bool unmask) const;

  private:
    std::string                    m_calibType = "";

    std::vector<nsw::FEBConfig>    m_febs   = {};
    std::vector<nsw::ADDCConfig>   m_addcs  = {};
    std::vector<nsw::TPConfig>     m_tps    = {};
    std::vector<int>               m_phases = {};

    bool m_connectivity = false;
    bool m_tracks = false;
    bool m_noise = false;
    bool m_latency = false;
    bool m_staircase = false;
    bool m_dry_run = false;
    bool m_reset_vmm = false;
    boost::property_tree::ptree m_patterns;
    std::unique_ptr<std::vector<std::future<int> > > m_threads = nullptr;
    std::future<int> m_watchdog;
    mutable std::atomic<bool> m_tpscax_busy = false;

    std::unique_ptr<TFile> m_art_rfile;
    std::shared_ptr<TTree> m_art_rtree;
    std::string m_addc_address;
    std::string m_art_name;
    std::string m_art_now;
    int m_art_event;
    int m_art_index;
    std::unique_ptr<std::vector<uint32_t> > m_art_hits;

  };

}

#endif
