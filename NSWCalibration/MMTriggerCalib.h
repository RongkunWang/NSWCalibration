#ifndef MMTRIGGERCALIB_H_
#define MMTRIGGERCALIB_H_

//
// Derived class for NSW ART input phase calib
//

#include <future>
#include <string>
#include <vector>

#include "NSWCalibration/CalibAlg.h"

#include "NSWConfiguration/hw/MMTP.h"

#define R__HAS_STD_SPAN
#include "TFile.h"
#include "TTree.h"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWMMTriggerCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {
  namespace hw {
    class ADDC;
  }

  class MMTriggerCalib: public CalibAlg {

  public:
    MMTriggerCalib(std::string calibType, const hw::DeviceManager& deviceManager);

    void setup(const std::string& db) override;
    void configure() override;
    void acquire() override;
    void unconfigure() override;
    [[nodiscard]]
    nsw::commands::Commands getAltiSequences() const override;
    void setCalibParamsFromIS(const ISInfoDictionary& is_dictionary, const std::string& is_db_name) override;

  public:
    boost::property_tree::ptree patterns() const;
    template <class T>
      std::vector<T> make_objects(const std::string& cfg, std::string element_type, std::string name = "");
    int pattern_number(const std::string& name) const;
    int configure_febs_from_ptree(const boost::property_tree::ptree& tr, bool unmask);
    int configure_addcs_from_ptree(const boost::property_tree::ptree& tr);
    int configure_vmms(nsw::hw::FEB feb, const boost::property_tree::ptree& febpatt, bool unmask) const ;
    int configure_art_input_phase(const nsw::hw::ADDC& addc, uint phase) const;
    int configure_tps(const boost::property_tree::ptree& tr);
    int addc_tp_watchdog();
    void recordPattern();

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
    std::vector<uint32_t> read_art_counters(const nsw::hw::ART& art) const;
    int wait_until_done();
    int announce(const std::string& name, const boost::property_tree::ptree& tr, bool unmask) const;

  private:
    std::string                    m_trackPatternFile;
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
    std::future<int>  m_watchdog;
    std::future<void> m_writePattern;
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
