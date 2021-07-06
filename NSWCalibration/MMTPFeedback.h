#ifndef MMTPFEEDBACK_H_
#define MMTPFEEDBACK_H_

#include <fstream>
#include <string>
#include <vector>
#include <future>

#include "NSWConfiguration/FEBConfig.h"

#include "ers/Issue.h"

#include <boost/property_tree/ptree.hpp>
#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"

namespace nsw {

  struct MMTPChannelRate {
    std::string mmfe8;
    int vmm;
    int ch;
    int rate;
    int layer;
    int pos;
  };

  struct MMTPVMMRate {
    std::string mmfe8;
    int vmm;
    std::vector<int> rates;
    int median_rate;
    int layer;
    int pos;
  };

  class MMTPFeedback {

  public:
    MMTPFeedback();
    ~MMTPFeedback() = default;

    void SetDebug(bool val) {m_debug = val;}
    void SetChannelRateDataFile(const std::string& val) {m_fname_data     = val;}
    void SetChannelRateRootFile(const std::string& val) {m_fname_root_i   = val;}
    void SetOutputRootFile     (const std::string& val) {m_fname_root_o   = val;}
    void SetChannelRateTreeName(const std::string& val) {m_tree_name      = val;}
    void SetInputJsonFile      (const std::string& val) {m_fname_json_i   = val;}
    void SetOutputJsonFile     (const std::string& val) {m_fname_json_o   = val;}
    void SetNoisyChannelFactor (const int val)          {m_factor_channel = val;}
    void SetNoisyVMMFactor     (const int val)          {m_factor_vmm     = val;}
    void SetThresholdIncrease  (const int val)          {m_threshold_incr = val;}
    void SetMaxThreads         (const int val)          {m_max_threads    = val;}
    void SetLoop               (const int val)          {m_loop           = val;}
    void SetSimulation         (const bool val)         {m_sim            = val;}
    void DecodeChannelRateDataFile();
    void LoadChannelRateRootFile();
    void AnalyzeNoise();
    void AnalyzeTTree();
    void InitializeRates();
    void LoadRates();
    void MakeListOfNoisyChannels();
    void MakeListOfNoisyVMMs();
    void SendConfigurations();
    void LoadConfigs();
    void SetMaskedChannels();
    void SetVMMThresholds();
    void SendFEBConfigs();
    void SendFEBConfig(const nsw::FEBConfig& feb);
    void UpdatePtree();
    void UpdatePtreeVmmThreshold();
    void UpdatePtreeChannelMask();
    void WriteOutputJsonFile();
    void WriteOutputRootFile();
    void Summarize();

    bool Debug()      const {return m_debug;}
    bool Simulation() const {return m_sim;}
    std::string ChannelRateDataFile() const {return m_fname_data;}
    std::string ChannelRateRootFile() const {return m_fname_root_i;}
    std::string OutputRootFile()      const {return m_fname_root_o;}
    std::string ChannelRateTreeName() const {return m_tree_name;}
    std::string InputJsonFile()       const {return m_fname_json_i;}
    std::string OutputJsonFile()      const {return m_fname_json_o;}
    std::string MakeMMFE8Name();
    int NoisyChannelFactor()          const {return m_factor_channel;}
    int NoisyVMMFactor()              const {return m_factor_vmm;}
    int ThresholdIncrease()           const {return m_threshold_incr;}
    int MaxThreads()                  const {return m_max_threads;}
    int Loop()                        const {return m_loop;}
    int GetMedian(std::vector<int>& v);
    bool TooManyThreads();
    size_t ActiveThreads();
    std::vector<nsw::FEBConfig> MakeFEBConfigs();

  private:
    bool m_sim;
    bool m_debug;
    std::string m_fname_data;
    std::string m_fname_root_i;
    std::string m_fname_root_o;
    std::string m_fname_json_i;
    std::string m_fname_json_o;
    int m_factor_channel;
    int m_factor_vmm;
    int m_loop;
    

  private:

    std::vector<MMTPChannelRate> m_rates;
    std::vector<MMTPChannelRate> m_noisy_channels;
    std::vector<MMTPVMMRate>     m_noisy_vmms;

  private:

    std::vector<nsw::FEBConfig> m_febs;
    const std::string m_threshold    = "sdt_dac";
    const std::string m_channel_mask = "channel_sm";
    int m_threshold_incr;
    size_t m_max_threads;
    std::unique_ptr<std::vector<std::future<void> > > m_threads;
    boost::property_tree::ptree m_feb_config;

  private:

    size_t m_entries;

    std::string m_tree_name = "decodedData";
    std::unique_ptr<TFile> m_rfile = 0;
    std::shared_ptr<TTree> m_rtree = 0;

    TBranch* b_fiber;           //!
    TBranch* b_mmfe8;           //!
    TBranch* b_cycle;           //!
    TBranch* b_channel_rate;    //!
    TBranch* b_octupletLayer;   //!
    TBranch* b_quadrupletLayer; //!
    TBranch* b_rightLeft;       //!
    TBranch* b_pcb;             //!
    TBranch* b_boardPosition;   //!
    TBranch* b_vmmPosition;     //!
    TBranch* b_chPosition;      //!
    TBranch* b_vmm;             //!
    TBranch* b_ch;              //!

    Double_t m_fiber;
    Double_t m_mmfe8;
    Double_t m_cycle;
    Double_t m_octupletLayer;
    Double_t m_quadrupletLayer;
    Double_t m_rightLeft;
    Double_t m_pcb;
    Double_t m_boardPosition;
    std::unique_ptr< std::vector<int> > m_channel_rate   = 0;
    std::unique_ptr< std::vector<int> > m_vmmPosition    = 0;
    std::unique_ptr< std::vector<int> > m_chPosition     = 0;
    std::unique_ptr< std::vector<int> > m_ch             = 0;
    std::unique_ptr< std::vector<int> > m_vmm            = 0;

  };

}

#endif
