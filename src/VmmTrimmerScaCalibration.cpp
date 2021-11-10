#include "NSWCalibration/VmmTrimmerScaCalibration.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include <fmt/core.h>
#include <fmt/chrono.h>

#include <boost/property_tree/json_parser.hpp>

#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/Utility.h"

namespace pt = boost::property_tree;
namespace cm = nsw::CalibrationMath;

using namespace std::chrono_literals;

nsw::VmmTrimmerScaCalibration::VmmTrimmerScaCalibration(nsw::FEBConfig feb,
                                                        std::string outpath,
                                                        const std::size_t nSamples,
                                                        const std::size_t rmsFactor,
                                                        const std::size_t sector,
                                                        const int wheel,
                                                        const bool debug) :
  nsw::ScaCalibration::ScaCalibration(std::move(feb),
                                      std::move(outpath),
                                      nSamples,
                                      rmsFactor,
                                      sector,
                                      wheel,
                                      debug),
  m_chCalibData(fmt::format("{}/{}_calibration_data.txt", m_outPath, m_feName))
{}

void nsw::VmmTrimmerScaCalibration::runCalibration()
{
  readThresholds();

  std::this_thread::sleep_for(500ms);

  scaCalib();
}

void nsw::VmmTrimmerScaCalibration::readThresholds()
{
  std::ofstream thr_test(fmt::format("{}/{}_thresholds.txt", m_outPath, m_feName));
  thr_test.is_open();

  std::size_t bad_thr_tot{0};

  for (std::size_t vmmId{m_firstVmm}; vmmId < m_nVmms; vmmId++) {
    std::size_t dev_thr{0};

    for (std::size_t channelId{0}; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
      std::vector<short unsigned int> results;
      try {
        results = sampleVmmChThreshold(vmmId, channelId);
      } catch (const std::runtime_error& e) {
        nsw::VmmTrimmerScaCalibrationIssue issue(
          ERS_HERE,
          fmt::format("Skipping {} VMM{} Channel {} because threshold could not be sampled, "
                      "reason: ![{}]! Check status of the readout!",
                      m_feName,
                      vmmId,
                      channelId,
                      e.what()));
        ers::error(issue);
        results.clear();
        continue;
      }

      const auto sum = std::accumulate(results.begin(), results.end(), 0.f);
      const auto mean = [&sum, &results]() {
        if (!results.empty()) {
          return sum / static_cast<float>(results.size());
        }
        return sum;
      }();

      const auto strong_dev_samp =
        std::count_if(std::cbegin(results), std::cend(results), [&mean](const auto result) {
          return (std::abs(mean - result) > nsw::ref::THR_SAMPLE_CUTOFF);
        });

      if (cm::sampleTomV(mean, m_isStgc) < nsw::ref::THR_DEAD_CUTOFF) {
        nsw::VmmTrimmerScaCalibrationIssue issue(
          ERS_HERE,
          fmt::format("{} VMM{} channel - {} -> threshold might be dead = [{} mV]",
                      m_feName,
                      vmmId,
                      channelId,
                      cm::sampleTomV(mean, m_isStgc)));
        ers::warning(issue);
      }

      // Searching for max and min deviation in samples
      std::sort(results.begin(), results.end());

      const auto [max_dev, min_dev] = [&results]() -> std::tuple<std::size_t, std::size_t> {
        if (results.empty()) {
          return {0, 0};
        }
        return {results.back(), results.front()};
      }();

      const auto thr_dev = std::size_t{max_dev - min_dev};

      if (thr_dev > nsw::ref::THR_SAMPLE_CUTOFF &&
          static_cast<std::size_t>(strong_dev_samp) > (m_nSamples * nsw::ref::BASELINE_SAMP_FACTOR / 4)) {
        nsw::VmmTrimmerScaCalibrationIssue issue(
          ERS_HERE,
          fmt::format("{} VMM{} channel - {} -> high threshold deviation = [{} mV] from sample "
                      "mean={} mV), samples out of range = {}/{}",
                      m_feName,
                      vmmId,
                      channelId,
                      cm::sampleTomV(thr_dev, m_isStgc),
                      cm::sampleTomV(mean, m_isStgc),
                      strong_dev_samp,
                      m_nSamples * nsw::ref::BASELINE_SAMP_FACTOR));
        ers::warning(issue);

        dev_thr++;
      }

      std::stringstream outline;
      writeTabDelimitedLine(outline,
                            m_wheel,
                            m_sector,
                            m_feName,
                            vmmId,
                            channelId,
                            cm::sampleTomV(mean, m_isStgc),
                            cm::sampleTomV(max_dev, m_isStgc),
                            cm::sampleTomV(min_dev, m_isStgc));

      ERS_DEBUG(2, outline.str());

      thr_test << outline.rdbuf();
    }  // end loop over channels

    bad_thr_tot += dev_thr;
  }  // end loop over VMMs

  if (bad_thr_tot > m_quarterOfFebChannels) {
    nsw::VmmTrimmerScaCalibrationIssue issue(
      ERS_HERE,
      fmt::format(
        "Large number of strongly deviating thresholds for {} => ({})!", m_feName, bad_thr_tot));
    ers::warning(issue);
  }
  thr_test.close();
}

void nsw::VmmTrimmerScaCalibration::scaCalib()
{
  const auto start = std::chrono::high_resolution_clock::now();

  m_febOutJson.put("OpcServerIp", m_feb.getOpcServerIp());
  m_febOutJson.put("OpcNodeId", m_feName);

  m_chCalibData.is_open();

  ERS_INFO(fmt::format("{} Running SCA calibration", m_feName));

  // Run one calibration per VMM on this FEB
  for (std::size_t vmmId{m_firstVmm}; vmmId < m_nVmms; vmmId++) {
    calibrateVmm(vmmId);
  }

  // Write out the results from *all* VMMs
  writeOutScaVmmCalib();

  const auto finish = std::chrono::high_resolution_clock::now();
  const auto elapsed = finish - start;
  ERS_DEBUG(2, fmt::format("Elapsed time: {:%M:%S} [min]", elapsed));

  ERS_INFO(fmt::format("{}: All thresholds calibrated", m_feName));
}

void nsw::VmmTrimmerScaCalibration::calibrateVmm(const std::size_t vmmId)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};

  ERS_DEBUG(2, fmt::format("{} VMM{}: Calibrating baseline", m_feName, vmmId));

  const auto [vmm_median_samples, vmm_rms_samples, vmm_samples, ch_not_connected, n_over_cut] = getUnconnectedChannels(vmmId);

  ERS_INFO(fmt::format(" {} VMM{}: Number of unconnected channels={}", m_feName, vmmId, ch_not_connected));

  ERS_LOG(fmt::format("{} VMM{} samples collected {}", m_feName, vmmId, vmm_samples.size()));

  if ((ch_not_connected == nsw::vmm::NUM_CH_PER_VMM) or vmm_samples.empty()) {
    // Write the output for a fully disconnected VMM
    // thDac (nsw::ref::VMM_THDAC_MAX), trim (0), mask (1)
    setChTrimAndMask(vmmId, true);
    ERS_LOG(fmt::format("{} VMM{}: all channels not connected or empty sample vector, masking VMM", m_feName, vmmId));
  } else {
    const auto tmp_median = cm::takeMedian(vmm_samples);

    const auto mean_bad_bl_samples =
      std::accumulate(std::cbegin(n_over_cut), std::cend(n_over_cut), 0.f) /
      static_cast<float>(n_over_cut.size());
    const auto median_bad_bl_samples = cm::takeMedian(n_over_cut);

    ERS_LOG(fmt::format("{} VMM{} samples over RMS cutoff -> | MEAN: {}"
                        " | MEDIAN {} | tmp_median = {} | written channel baselines [{}]",
                        m_feName,
                        vmmId,
                        mean_bad_bl_samples,
                        median_bad_bl_samples,
                        tmp_median,
                        m_vmmTrimmerData.baselineMed.size()));

    // Set the channel masks, at this point unconnected channels are masked
    // As well as those with a baseline significantly larger than the median
    m_febTrimmerData.channelInfo.insert_or_assign(feb_vmm, maskChannels(vmmId, tmp_median));

    const auto hot_chan = std::get<1>(m_febTrimmerData.channelInfo.at(feb_vmm));
    const auto dead_chan = std::get<2>(m_febTrimmerData.channelInfo.at(feb_vmm));
    const auto bad_chan = hot_chan + dead_chan;

    if (bad_chan < nsw::vmm::NUM_CH_PER_VMM / 4 and bad_chan != 0) {
      ERS_LOG(fmt::format("{} VMM{} has a few BAD channels - (HOT - {})(DEAD - {})",
                          m_feName,
                          vmmId,
                          hot_chan,
                          dead_chan));
    } else if (bad_chan >= nsw::vmm::NUM_CH_PER_VMM / 4 and
               bad_chan < nsw::vmm::NUM_CH_PER_VMM / 2) {
      nsw::VmmTrimmerScaCalibrationIssue issue(
        ERS_HERE,
        fmt::format(
          "{} VMM{} has more than 1/4 of channels hot or dead ({})", m_feName, vmmId, bad_chan));
      ers::warning(issue);
    } else if (bad_chan >= nsw::vmm::NUM_CH_PER_VMM / 2) {
      nsw::VmmTrimmerScaCalibrationIssue issue(
        ERS_HERE,
        fmt::format(
          "{} VMM{}: SEVERE PROBLEM more than HALF BAD channels [{} (HOT - {})(DEAD - {})]"
          ", skipping this VMM. SUGGESTION - read full baseline to asseess "
          "severity of the problem.",
          m_feName,
          vmmId,
          bad_chan,
          hot_chan,
          dead_chan));
      ers::warning(issue);
    }

    // Calculate VMM median baseline and RMS
    const auto vmm_sum =
      std::accumulate(std::cbegin(vmm_median_samples), std::cend(vmm_median_samples), 0.f);
    // Take the mean of the medians of each channel
    const auto vmm_mean = vmm_sum / static_cast<float>(vmm_median_samples.size());
    // Take the median of the medians of each channel
    const auto vmm_median = cm::takeMedian(vmm_median_samples);
    // Take the RMS of the medians of each channel
    // const auto vmm_stdev = cm::takeRms(vmm_median_samples, vmm_mean);
    // Take the median of the RMS of each channel
    const auto vmm_stdev = cm::takeMedian(vmm_rms_samples);

    // Only place that these values are set, all other uses can be read-only
    m_febTrimmerData.baselineMed.insert_or_assign(feb_vmm, vmm_median);
    m_febTrimmerData.baselineRms.insert_or_assign(feb_vmm, vmm_stdev);

    ERS_LOG(fmt::format("{} VMM{} : Calculated global (tmp_vmm_median = {})(vmm_mean = "
                        "{})(vmm_median = {})(vmm_stdev = {})",
                        m_feName,
                        vmmId,
                        cm::sampleTomV(tmp_median, m_isStgc),
                        cm::sampleTomV(vmm_mean, m_isStgc),
                        cm::sampleTomV(vmm_median, m_isStgc),
                        cm::sampleTomV(vmm_stdev, m_isStgc)));

    // Global Threshold Calculations
    ERS_INFO(fmt::format("{} VMM{}: Calculating global threshold", m_feName, vmmId));
    const auto thDacConstants = [this, &vmmId, &feb_vmm](){
      try {
        return calculateGlobalThreshold(vmmId);
      } catch (const nsw::VmmTrimmerBadGlobalThreshold& e) {
        // This VMM failed, mask all channels
        m_febTrimmerData.channelMasks.at(feb_vmm) = nsw::calib::VmmChannelArray<std::size_t>{};

        // Ensure map values are set for any potential future access.
        // Set directly in calculateGlobalThreshold
        m_febTrimmerData.thDacConstants.insert_or_assign(feb_vmm, nsw::calib::GlobalThrConstants{});
        m_febTrimmerData.thDacValues.insert_or_assign(feb_vmm, nsw::ref::VMM_THDAC_MAX);
        // Set indirectly in calculateGlobalThreshold, via a call to calculateVmmGlobalThreshold
        m_febTrimmerData.midTrimMed.insert_or_assign(feb_vmm, -1.);
        m_febTrimmerData.midEffThresh.insert_or_assign(feb_vmm, -1.);

        return nsw::calib::GlobalThrConstants{};
      }}();

    const auto& thdac = m_febTrimmerData.thDacValues.at(feb_vmm);

    // Scanning trimmers
    scanTrimmers(vmmId);

    analyseTrimmers(vmmId, thdac, std::get<0>(thDacConstants));

    // Summary and output
    ERS_LOG(
      fmt::format("{} VMM{}: thDac written to the partial config is [{}]", m_feName, vmmId, thdac));

    const auto& best_channel_trim = m_vmmTrimmerData.best_channel_trim;
    const auto& eff_thr_w_best_trim = m_vmmTrimmerData.eff_thr_w_best_trim;

    const auto& channel_masks = m_febTrimmerData.channelMasks.at(feb_vmm);
    ERS_LOG(fmt::format("Data check to be written to disk: Side={} sector={} {}-VMM{}::size of "
                        "data to write: baselines ({}), VMM global baseline ({}), "
                        "best_channel_trim ({}), effective threshold ({}), channel mask ({})",
                        m_wheel,
                        m_sector,
                        m_feName,
                        vmmId,
                        m_vmmTrimmerData.baselineMed.size(),
                        m_febTrimmerData.baselineMed.size(),
                        best_channel_trim.size(),
                        eff_thr_w_best_trim.size(),
                        channel_masks.size()));

    // Write out analysis results to ptree/JSON for this VMM
    setChTrimAndMask(vmmId);

    ERS_INFO(fmt::format("{} VMM{} - Trimmers calculated & data written", m_feName, vmmId));

    std::this_thread::sleep_for(2000ms);

    const auto n_masked = std::accumulate(channel_masks.begin(), channel_masks.end(), 0.f);
    if (n_masked >= nsw::vmm::NUM_CH_PER_VMM / 4) {
      nsw::VmmTrimmerScaCalibrationIssue issue(
        ERS_HERE,
        fmt::format("{} VMM{}: Number of masked channels = {}", m_feName, vmmId, n_masked));
      ers::warning(issue);
    }
  }
}

// FIXME TODO rename
nsw::calib::VMMDisconnectedChannelInfo nsw::VmmTrimmerScaCalibration::getUnconnectedChannels(
  const std::size_t vmmId)
{
  // Return a vector of the median and RMS of the per-channel pruned
  // samples
  std::vector<std::size_t> vmm_ch_samples{};
  std::vector<std::size_t> vmm_ch_median_samples{};
  std::vector<std::size_t> vmm_ch_rms_samples{};

  std::size_t not_connected{0};
  std::vector<std::size_t> n_over_cut{};
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};

  auto defaultChannelBaseline = [this, &feb_vmm](const auto channelId) {
    // These are set in readBaseline and need to be set to a sensible
    // default in the case that we didn't successfully set them above
    const nsw::calib::FebChannelPair feb_ch{feb_vmm, channelId};
    if (m_vmmTrimmerData.baselineMed.find(feb_ch) != std::cend(m_vmmTrimmerData.baselineMed)) {
      m_vmmTrimmerData.baselineMed.insert_or_assign(feb_ch, -1.f);
    }
    if (m_vmmTrimmerData.baselineRms.find(feb_ch) != std::cend(m_vmmTrimmerData.baselineRms)) {
      m_vmmTrimmerData.baselineRms.insert_or_assign(feb_ch, -1.f);
    }
  };

  for (std::size_t channelId{0}; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
    if (!checkIfUnconnected(m_feName, vmmId, channelId)) {
      try {
        const auto [ch_samples, over_cut] = readBaseline(vmmId, channelId);
        n_over_cut.push_back(over_cut);

        const auto ch_median = cm::takeMedian(ch_samples);
        vmm_ch_median_samples.emplace_back(ch_median);
        vmm_ch_rms_samples.emplace_back(cm::takeRms(ch_samples, ch_median));

        std::move(std::begin(ch_samples), std::end(ch_samples), std::back_inserter(vmm_ch_samples));
      } catch (const nsw::ScaCalibrationIssue& ex) {
        ERS_INFO(
          fmt::format("{} VMM{}: channel {} baseline sampling failed - skipping: reason [{}]",
                      m_feName,
                      vmmId,
                      channelId,
                      ex.what()));
        not_connected++;
        defaultChannelBaseline(channelId);
        continue;
      }
    } else {
      ERS_INFO(fmt::format("{} VMM{}: channel {} is unconnected - skipping", m_feName, vmmId, channelId));
      not_connected++;
      defaultChannelBaseline(channelId);
      continue;
    }
  }

  return {vmm_ch_median_samples, vmm_ch_rms_samples, vmm_ch_samples, not_connected, n_over_cut};
}

std::pair<std::vector<short unsigned int>, std::size_t> nsw::VmmTrimmerScaCalibration::readBaseline(
  const std::size_t vmmId,
  const std::size_t channelId)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};
  const nsw::calib::FebChannelPair feb_ch{feb_vmm, channelId};

  const auto results = sampleVmmChMonDac(vmmId, channelId, nsw::ref::BASELINE_SAMP_FACTOR);

  const auto raw_median = cm::takeMedian(results);

  const auto far_outliers = std::count_if(
    std::cbegin(results),
    std::cend(results),
    [this, &vmmId, &channelId, &raw_median](const auto result) {
      if (cm::sampleTomV(std::abs(static_cast<int64_t>(result - raw_median)), m_isStgc) > nsw::ref::BASELINE_CUTOFF) {
        ERS_DEBUG(
          2,
          fmt::format("{} VMM{}: Outlier detected for channel {}: sample: {} mV | {} ADC",
                      m_feName,
                      vmmId,
                      channelId,
                      cm::sampleTomV(result, m_isStgc),
                      result));
        return true;
      }
      return false;
    });

  const auto results_pruned = [this, &raw_median, &results]() {
    auto output = decltype(results){};
    std::copy_if(std::cbegin(results),
                 std::cend(results),
                 std::back_inserter(output),
                 [this, &raw_median](const auto sample) {
                   return (cm::sampleTomV(std::abs(static_cast<int64_t>(sample - raw_median)), m_isStgc) <
                           nsw::ref::BASELINE_CUTOFF);
                 });
    return output;
  }();

  ERS_LOG(fmt::format("{} VMM{} channel {} pruned sample vector size ={}",
                      m_feName,
                      vmmId,
                      channelId,
                      results_pruned.size()));

  // Calculate channel level baseline median and RMS
  const auto sum = std::accumulate(std::cbegin(results_pruned), std::cend(results_pruned), 0.0f);
  const auto mean = sum / static_cast<float>(results_pruned.size());
  const auto stdev = cm::takeRms(results_pruned, mean);
  const auto median = cm::takeMedian(results_pruned);

  const auto mean_mV = cm::sampleTomV(mean, m_isStgc);
  const auto stdev_mV = cm::sampleTomV(stdev, m_isStgc);

  ERS_DEBUG(
    2,
    fmt::format("{} VMM{}, channel {}: mean = {}, stdev = {}], {} samples outside baseline cutoff ({}).",
                m_feName,
                vmmId,
                channelId,
                mean_mV,
                stdev_mV,
                far_outliers,
                nsw::ref::BASELINE_CUTOFF));

  // Add channel baseline median and RMS to (FEB, channel) map
  m_vmmTrimmerData.baselineMed.insert_or_assign(feb_ch, median);
  m_vmmTrimmerData.baselineRms.insert_or_assign(feb_ch, stdev);

  return {results_pruned, far_outliers};
}

void nsw::VmmTrimmerScaCalibration::setChTrimAndMask(const std::size_t vmmId, const bool reset)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};

  pt::ptree trimmerNode;
  pt::ptree maskNode;
  pt::ptree singleMask;
  pt::ptree singleTrim;

  const auto& channel_masks =
    reset ? nsw::calib::VmmChannelArray<std::size_t>{} : m_febTrimmerData.channelMasks.at(feb_vmm);

  const bool vmmMasked = (reset or std::all_of(std::cbegin(channel_masks),
                                               std::cend(channel_masks),
                                               [](const auto mask) { return mask == 1; }));

  const auto vmm_baseline_med =
    vmmMasked ? 0 : cm::sampleTomV(m_febTrimmerData.baselineMed.at(feb_vmm), m_isStgc);
  const auto vmm_baseline_rms =
    vmmMasked ? 0 : cm::sampleTomV(m_febTrimmerData.baselineRms.at(feb_vmm), m_isStgc);
  const auto vmm_median_trim_mid =
    vmmMasked ? 0 : cm::sampleTomV(m_febTrimmerData.midTrimMed.at(feb_vmm), m_isStgc);
  const auto vmm_eff_thresh =
    vmmMasked ? 0 : cm::sampleTomV(m_febTrimmerData.midEffThresh.at(feb_vmm), m_isStgc);
  const auto& thDac = vmmMasked ? nsw::ref::VMM_THDAC_MAX : m_febTrimmerData.thDacValues.at(feb_vmm);
  const auto& thDacSlope = vmmMasked ? 0 : std::get<0>(m_febTrimmerData.thDacConstants.at(feb_vmm));
  const auto& thDacOffset = vmmMasked ? 0 : std::get<1>(m_febTrimmerData.thDacConstants.at(feb_vmm));

  for (std::size_t ch{0}; ch < nsw::vmm::NUM_CH_PER_VMM; ch++) {
    // FIXME TODO if a channel was masked, it may not have populated the maps,
    // don't crash in such cases!
    // However, need to ensure that for all potentially failed
    // channels, it is masked, otherwise we have to create a different
    // mechanism
    const bool chMasked = (reset or (channel_masks.at(ch) == 1));
    const nsw::calib::FebChannelPair feb_ch{feb_vmm, ch};
    writeTabDelimitedLine(
      m_chCalibData,
      m_wheel,
      m_sector,
      m_feName,
      vmmId,
      ch,
      chMasked ? 0 : cm::sampleTomV(m_vmmTrimmerData.baselineMed.at(feb_ch), m_isStgc),
      chMasked ? 0 : cm::sampleTomV(m_vmmTrimmerData.baselineRms.at(feb_ch), m_isStgc),
      chMasked ? 0 : cm::sampleTomV(m_vmmTrimmerData.midEffThresh.at(feb_ch), m_isStgc),
      chMasked ? 0 : m_vmmTrimmerData.effThreshSlope.at(feb_ch),
      vmm_baseline_med,
      vmm_baseline_rms,
      vmm_median_trim_mid,
      vmm_eff_thresh,
      thDac,
      chMasked ? 0 : m_vmmTrimmerData.best_channel_trim.at(feb_ch),
      chMasked ? 0 : m_vmmTrimmerData.channel_trimmed_thr.at(feb_ch),
      chMasked ? 0 : m_vmmTrimmerData.eff_thr_w_best_trim.at(feb_ch),
      thDacSlope,
      thDacOffset,
      chMasked ? 1 : channel_masks.at(ch));

    singleTrim.put("", chMasked ? 0 : m_vmmTrimmerData.best_channel_trim.at(feb_ch));
    trimmerNode.push_back(std::make_pair("", singleTrim));

    singleMask.put("", reset ? 1 : channel_masks.at(ch));
    maskNode.push_back(std::make_pair("", singleMask));
  }

  // Writing all ptrees to the output JSON
  pt::ptree outJsonVmm;
  outJsonVmm.put("sdt_dac", thDac);
  outJsonVmm.add_child("channel_sd", trimmerNode);
  outJsonVmm.add_child("channel_sm", maskNode);

  m_febOutJson.add_child(fmt::format("vmm{}", vmmId), outJsonVmm);
}

nsw::calib::VMMChannelSummary nsw::VmmTrimmerScaCalibration::maskChannels(const std::size_t vmmId,
                                                                          const std::size_t median)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};

  nsw::calib::VmmChannelArray<std::size_t> channel_mask{};

  std::size_t noisy_chan{0}, hot_chan{0}, dead_chan{0};
  float sum_rms{0.f};

  for (std::size_t channelId{0}; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
    const nsw::calib::FebChannelPair feb_ch{feb_vmm, channelId};
    // FIXME TODO this is done in the calling scope, is it necessary
    // to repeat here?
    if (checkIfUnconnected(m_feName, vmmId, channelId)) {
      channel_mask.at(channelId) = 1;
    } else {
      const auto chnl_bln = m_vmmTrimmerData.baselineMed.at(feb_ch);
      const auto chnl_rms = m_vmmTrimmerData.baselineRms.at(feb_ch);

      // checking the noise
      const auto ch_noise = cm::sampleTomV(chnl_rms, m_isStgc);
      sum_rms += ch_noise;

      // FIXME RENAME ME
      if (ch_noise > nsw::ref::RMS_CUTOFF) {
        noisy_chan++;
      }

      // FIXME TODO currently uses BB5 analysis convention...
      // FIXME TODO factor of 3 is really high
      if (static_cast<float>(chnl_bln) > static_cast<float>(median) * nsw::ref::CHANNEL_HOT_FACTOR) {
        channel_mask.at(channelId) = 1;
        hot_chan++;
      } else if (static_cast<float>(chnl_bln) < static_cast<float>(median) * nsw::ref::CHANNEL_DEAD_FACTOR) {
        ERS_INFO(fmt::format("{} VMM{}: channel {} baseline = {} is below median.",
                             m_feName,
                             vmmId,
                             channelId,
                             cm::sampleTomV(chnl_bln, m_isStgc)));
      }
    }
  }

  const auto bad_bl = (noisy_chan >= nsw::vmm::NUM_CH_PER_VMM / 2);
  if (bad_bl) {
    nsw::VmmTrimmerScaCalibrationIssue issue(
      ERS_HERE,
      fmt::format("{} VMM{}: HALF or more channels have sample RMS > 30mV [{} channels], with average "
                  "channel RMS: [{:2.2f} mV] --- calibration might be flawed",
                  m_feName,
                  vmmId,
                  noisy_chan,
                  sum_rms / nsw::vmm::NUM_CH_PER_VMM));
    ers::warning(issue);
  }

  m_febTrimmerData.channelMasks.insert_or_assign(feb_vmm, channel_mask);

  return {noisy_chan, hot_chan, dead_chan, sum_rms, bad_bl};
}

nsw::calib::GlobalThrConstants nsw::VmmTrimmerScaCalibration::calculateGlobalThreshold(
  const std::size_t vmmId)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};
  const auto& vmmBlnRms = m_febTrimmerData.baselineRms.at(feb_vmm);
  const auto& vmmBlnMed = m_febTrimmerData.baselineMed.at(feb_vmm);
  const auto vmmBlnRms_mV = cm::sampleTomV(vmmBlnRms, m_isStgc);
  const auto vmmBlnMed_mV = cm::sampleTomV(vmmBlnMed, m_isStgc);

  const auto [thDacTargetValue_mV, expectedEffThreshold] =
    [this, vmmBlnMed_mV, vmmBlnRms_mV]() -> std::pair<float, float> {
    if (m_isStgc) {
      const auto isPfeb{m_feName.find("PFEB") != std::string::npos};
      if (isPfeb) {
        return {vmmBlnMed_mV + nsw::ref::PFEB_THDAC_TARGET_OFFSET,
                vmmBlnMed_mV + nsw::ref::PFEB_THDAC_TARGET_OFFSET};
      } else {
        return {vmmBlnMed_mV + nsw::ref::SFEB_THDAC_TARGET_OFFSET,
                vmmBlnMed_mV + nsw::ref::SFEB_THDAC_TARGET_OFFSET};
      }
    }
    return {(static_cast<float>(m_rmsFactor) * vmmBlnRms_mV) + vmmBlnMed_mV +
              cm::sampleTomV(nsw::ref::TRIM_MID, m_isStgc),
            (static_cast<float>(m_rmsFactor) * vmmBlnRms_mV)};
  }();

  ERS_DEBUG(1, fmt::format("{} VMM{}: THDAC target value - [{} (mV)]", m_feName, vmmId, thDacTargetValue_mV));

  auto mean{0.f};
  int thdac{0};
  nsw::calib::GlobalThrConstants thDacConstants;

  for (std::size_t th_try{1}; th_try <= nsw::MAX_ATTEMPTS; th_try++) {
    try {
      thDacConstants = calculateThrDacValue(vmmId, thDacTargetValue_mV);
    } catch (const nsw::VmmTrimmerBadDacSlope& e) {
      const nsw::VmmTrimmerBadGlobalThreshold issue(ERS_HERE, fmt::format("{}", e.what()));
      ers::error(issue);
      if (th_try == nsw::MAX_ATTEMPTS) {
        throw issue;
      } else {
        // FIXME would love to reissue message as different type
        // ers::warning(nsw::ScaVmmSamplingIssue(e));
        std::this_thread::sleep_for(30ms);
        continue;
      }
    } catch (const nsw::VmmTrimmerBadDacValue& e) {
      const nsw::VmmTrimmerBadGlobalThreshold issue(ERS_HERE, fmt::format("{}",e.what()));
      ers::error(issue);
      if (th_try == nsw::MAX_ATTEMPTS) {
        throw issue;
      } else {
        // FIXME would love to reissue message as different type
        // ers::warning(nsw::ScaVmmSamplingIssue(e));
        std::this_thread::sleep_for(30ms);
        continue;
      }
    }

    // At this point we have correctly sampled the DAC and obtained an
    // acceptable thdac value (within range)
    thdac = std::get<2>(thDacConstants);

    ERS_DEBUG(
      1,
      fmt::format("{} VMM{}: Calculated THDAC value -> {} DAC counts, [slope = {} : offset = {}]",
                  m_feName,
                  vmmId,
                  thdac,
                  std::get<0>(thDacConstants),
                  std::get<1>(thDacConstants)));

    const auto results = sampleVmmThDac(vmmId, thdac);

    const auto sum = std::accumulate(std::cbegin(results), std::cend(results), 0.f);
    mean = sum / static_cast<float>(results.size());
    const auto mean_mV = cm::sampleTomV(mean, m_isStgc);
    const auto thr_diff{mean_mV - vmmBlnMed_mV};

    if (!(thr_diff > 0)) {
      if (th_try == nsw::MAX_ATTEMPTS) {
        const nsw::VmmTrimmerBadGlobalThreshold issue(
          ERS_HERE,
          fmt::format("{} VMM{}: Global threshold DAC calibration yields the setting below the "
                      "baseline after {} attempts",
                      m_feName,
                      vmmId,
                      nsw::MAX_ATTEMPTS));
        throw issue;
      } else {
        ers::warning(nsw::ScaVmmSamplingIssue(
          ERS_HERE,
          m_feName,
          vmmId,
          fmt::format("Resulting threshold is {} mV BELOW baseline!", thr_diff)));

        std::this_thread::sleep_for(30ms);
        continue;
      }
    }

    // At this point we have determined that the setting will be above baseline
    const auto thdac_dev = std::abs(mean_mV - thDacTargetValue_mV) / thDacTargetValue_mV;

    if (thdac_dev >= nsw::ref::THR_DEV_FACTOR) {
      if (th_try == nsw::MAX_ATTEMPTS) {
        const nsw::VmmTrimmerBadGlobalThreshold issue(
          ERS_HERE,
          fmt::format("{} VMM{}: {:2.2f}% deviation in threshold calculation: [guess = {} -> calc. "
                      "= {:2.2f} mV] after {} attempts.",
                      m_feName,
                      vmmId,
                      thdac_dev * 100,
                      thDacTargetValue_mV,
                      mean_mV,
                      nsw::MAX_ATTEMPTS));
        throw issue;
      } else {
        ers::warning(nsw::ScaVmmSamplingIssue(
          ERS_HERE,
          m_feName,
          vmmId,
          fmt::format(
            "{:2.2f}% deviation in threshold calculation: [guess = {} -> calc. = {:2.2f} mV]",
            thdac_dev * 100,
            thDacTargetValue_mV,
            mean_mV)));
        std::this_thread::sleep_for(40ms);
        continue;
      }
    } else {
      ERS_INFO(fmt::format(
        "{} VMM{}: Global threshold is {}mV, deviation from desired value is [{:2.2f}]%",
        m_feName,
        vmmId,
        mean_mV,
        thdac_dev * 100));
      break;
    }
  }

  // Put the calculated value in the FEB data object, for failed VMMs,
  // we don't get here, so need to set appropriate defaults at the
  // site of failure.
  m_febTrimmerData.thDacConstants.insert_or_assign(feb_vmm, thDacConstants);
  m_febTrimmerData.thDacValues.insert_or_assign(feb_vmm, thdac);

  // FIXME TODO these come from the for/try loop above, but need to escape the scope... -> lambda!
  if ((cm::sampleTomV(mean, m_isStgc) - vmmBlnMed_mV) < 0) {
    ers::warning(
      nsw::VmmTrimmerFmtIssue(ERS_HERE, m_feName, vmmId, "Threshold below baseline!"));
  }

  // Get VMM-level averages.
  ERS_DEBUG(2,
            fmt::format("{} VMM{} - Calculating V_eff with trimmers at bit[14]", m_feName, vmmId));

  calculateVmmGlobalThreshold(vmmId);

  const auto& vmm_eff_thresh = m_febTrimmerData.midEffThresh.at(feb_vmm);

  // If effective thereshold on VMM level is less than 10 mV resample
  // thresholds with trimmers and an increased THDAC 30 ADC counts -
  // roughly 10 mV;

  // FIXME TODO triple check this, 30 mV is way too high!
  // 5mV for MM/fixed value above threshold for sTGC
  if (vmm_eff_thresh < nsw::ref::RMS_CUTOFF) {
    // if resulting RMS is too low to give at least 1 DAC count
    const auto addFactor = [vmmBlnRms_mV, &thDacConstants]() -> std::size_t {
      const auto fact = std::round(vmmBlnRms_mV / std::get<0>(thDacConstants));  // slope
      if (fact == 0) {
        return 1;
      } else {
        return fact;
      }
    }();

    const auto thdac_raised = std::size_t{thdac + addFactor};

    ERS_DEBUG(
      2,
      fmt::format(
        "{} VMM{} adding {{}} DAC counts to the global threshold", m_feName, vmmId, addFactor));

    ers::warning(nsw::VmmTrimmerScaCalibrationIssue(
      ERS_HERE,
      fmt::format("{} VMM{} effective threshold is less than 10 mV - raising THDAC by one noise "
                  "RMS -[{} + {}] = [{}]",
                  m_feName,
                  vmmId,
                  thdac,
                  addFactor,
                  thdac_raised)));

    calculateVmmGlobalThreshold(vmmId);

    ERS_DEBUG(2, fmt::format("{} VMM{}: new V_eff = {}", m_feName, vmmId, vmm_eff_thresh));

    m_febTrimmerData.thDacValues.insert_or_assign(feb_vmm, thdac_raised);
  }

  ERS_DEBUG(2,
            fmt::format("Threshold value in the thdac map for {} is [{}] DAC counts\n [slope - {}]",
                        m_feName,
                        m_febTrimmerData.thDacValues.at(feb_vmm),
                        std::get<0>(thDacConstants)));

  // Not modified from
  const auto& vmm_stdev = vmmBlnRms;
  const auto vmmEffThresh_mV = cm::sampleTomV(vmm_eff_thresh, m_isStgc);
  if ((vmmEffThresh_mV >= nsw::ref::EFF_THR_HIGH) or
      (vmmEffThresh_mV <= nsw::ref::EFF_THR_LOW)) {
    ers::warning(nsw::VmmTrimmerScaCalibrationIssue(
      ERS_HERE,
      fmt::format(
        "{} VMM{} high threshold [result: {:2.2f} mV, expected: {:2.2f} mV, noise: {:2.2f} mV]",
        m_feName,
        vmmId,
        vmmEffThresh_mV,
        expectedEffThreshold,
        cm::sampleTomV(vmm_stdev, m_isStgc))));
  }

  return thDacConstants;
}

nsw::calib::GlobalThrConstants nsw::VmmTrimmerScaCalibration::calculateThrDacValue(
  const std::size_t vmmId,
  const float thDacTargetValue_mV)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};

  std::vector<float> thDacGuessesSample{};
  const std::vector<std::size_t> thDacGuessVariations = {100, 150, 200, 250, 300, 350, 400};

  for (const auto& thDac : thDacGuessVariations) {
    const auto results = sampleVmmThDac(vmmId, thDac, nsw::ref::THDAC_SAMPLE_FACTOR);

    const auto thDac_med = cm::takeMedian(results);

    // Cleaning result vector from highly deviating samples
    nsw::calib::VMMSampleVector results_pruned{};
    std::copy_if(
      std::cbegin(results),
      std::cend(results),
      std::back_inserter(results_pruned),
      [this, &vmmId, &thDac_med](const auto result) {
        if (std::abs(static_cast<int64_t>(thDac_med - result)) > nsw::ref::THDAC_SAMPLE_MARGIN) {
          ERS_DEBUG(
            1,
            fmt::format("{} VMM{}: sample [{} ADC] strongly deviates from THDAC median value {}",
                        m_feName,
                        vmmId,
                        result,
                        thDac_med));
          return false;
        }
        return true;
      });

    const auto sum = std::accumulate(std::cbegin(results_pruned), std::cend(results_pruned), 0.f);
    const auto mean = sum / static_cast<float>(results_pruned.size());
    thDacGuessesSample.push_back(mean);

    ERS_DEBUG(1,
              fmt::format("{} VMM{}, thDac {}, thDac (mV) {}",
                          m_feName,
                          vmmId,
                          thDac,
                          cm::sampleTomV(mean, m_isStgc)));
  }

  // do fit to a line
  const auto thDacGuessMean =
    std::accumulate(thDacGuessVariations.begin(), thDacGuessVariations.end(), 0.f) /
    static_cast<float>(thDacGuessVariations.size());
  const auto thDacGuessSampleMean =
    std::accumulate(thDacGuessesSample.begin(), thDacGuessesSample.end(), 0.f) /
    static_cast<float>(thDacGuessesSample.size());

  auto num = 0.f;
  auto denom = 0.f;
  for (std::size_t i = 0; i < thDacGuessVariations.size(); i++) {
    const auto var = static_cast<float>(thDacGuessVariations.at(i));
    const auto sam = thDacGuessesSample.at(i);
    num += (var - thDacGuessMean) * (sam - thDacGuessSampleMean);
    denom += std::pow((var - thDacGuessMean), 2);
  }

  const auto thDacSlope = num / denom;
  const auto thDacIntercept = thDacGuessSampleMean - (thDacSlope * thDacGuessMean);

  // checking if calculated THDAC slope does not deviate
  // by more than 20% from targeted value
  if (thDacSlope > 4.f or thDacSlope < 1.f) {
    nsw::VmmTrimmerBadDacSlope issue(
      ERS_HERE,
      m_feName,
      vmmId,
      fmt::format(
        "THDAC calculation yields: [slope = {}] [offset = {}]", thDacSlope, thDacIntercept));
    ers::error(issue);
    throw issue;
  } else {
    ERS_INFO(fmt::format("{} VMM{} DAC slope OK! [approx 2 DAC/mV]", m_feName, vmmId));
  }

  // (y-b) / m = x
  const auto thdac =
    static_cast<std::size_t>((cm::mVtoSample(thDacTargetValue_mV, m_isStgc) - thDacIntercept) / thDacSlope);

  if (thdac > nsw::ref::VMM_THDAC_MAX) {
    nsw::VmmTrimmerBadDacValue issue(
      ERS_HERE,
      m_feName,
      vmmId,
      fmt::format("Resulting threshold {} mV is outside allowable range!", thdac));
    ers::error(issue);
    throw issue;
  }

  return std::make_tuple(thDacSlope, thDacIntercept, thdac);
}

void nsw::VmmTrimmerScaCalibration::calculateVmmGlobalThreshold(const std::size_t vmmId)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};

  std::vector<int> vmm_samples;

  for (std::size_t channelId = 0; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
    if (m_febTrimmerData.channelMasks.at(feb_vmm).at(channelId) != 0) {
      continue;
    }

    const auto ch_samples = sampleVmmChTrimDac(vmmId,
                                               channelId,
                                               m_febTrimmerData.thDacValues.at(feb_vmm),
                                               nsw::ref::TRIM_MID,
                                               nsw::ref::BASELINE_SAMP_FACTOR);

    std::move(std::begin(ch_samples), std::end(ch_samples), std::back_inserter(vmm_samples));
  }

  const auto [vmm_median_trim_mid, vmm_eff_thresh] = [this, &vmm_samples, &feb_vmm]() -> std::pair<float,float> {
    if (vmm_samples.empty()) {
      return {0.f, 0.f};
    }
    const auto median_trim_mid = cm::takeMedian(vmm_samples);
    return {median_trim_mid, median_trim_mid - m_febTrimmerData.baselineMed.at(feb_vmm)};
  }();

  m_febTrimmerData.midTrimMed.insert_or_assign(feb_vmm, vmm_median_trim_mid);
  m_febTrimmerData.midEffThresh.insert_or_assign(feb_vmm, vmm_eff_thresh);
}

void nsw::VmmTrimmerScaCalibration::scanTrimmers(const std::size_t vmmId)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};

  auto& channel_masks = m_febTrimmerData.channelMasks.at(feb_vmm);

  ERS_INFO(fmt::format("Scanning {} VMM{} trimmers", m_feName, vmmId));

  m_febTrimmerData.baselines_above_threshold.insert_or_assign(feb_vmm, 0);
  const auto& nch_base_above_thresh = m_febTrimmerData.baselines_above_threshold.at(feb_vmm);

  std::size_t good_chs{0};
  std::size_t tot_chs{nsw::vmm::NUM_CH_PER_VMM};

  const auto current_thdac = m_febTrimmerData.thDacValues.at(feb_vmm);

  for (std::size_t channelId = 0; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
    if (channel_masks.at(channelId) != 0) {
      ERS_DEBUG(
        2, fmt::format("{} VMM{} Channel {} masked, so skipping it", m_feName, vmmId, channelId));
      continue;
    }

    // check if channel has a weird RMS or baseline
    const nsw::calib::FebChannelPair feb_ch{feb_vmm, channelId};
    const auto& ch_baseline_rms = m_vmmTrimmerData.baselineRms.at(feb_ch);

    if (!cm::checkChannel(ch_baseline_rms, m_isStgc)) {
      ERS_DEBUG(2,
                fmt::format("{} VMM{}: channel {} baseline RMS ({}) out of range ({})",
                            m_feName,
                            vmmId,
                            channelId,
                            ch_baseline_rms,
                            nsw::ref::RMS_CUTOFF));
      // TODO FIXME first check, underlying value may be modified subsequently
      // Do we not also do tot_chs-- here, as later?

      // Necessary to have a value set in these maps for the non-masked case
      m_vmmTrimmerData.effThreshSlope.insert_or_assign(feb_ch, -1.f);
      m_vmmTrimmerData.trimmerMax.insert_or_assign(feb_ch, 0);

      // Set in findLinearRegionSlope
      m_vmmTrimmerData.minEffThresh.insert_or_assign(feb_ch, -1.f);
      m_vmmTrimmerData.midEffThresh.insert_or_assign(feb_ch, -1.f);
      m_vmmTrimmerData.maxEffThresh.insert_or_assign(feb_ch, -1.f);

      continue;
    }

    const auto [thresh_slope, trimmer_max] = findLinearRegionSlope(
      vmmId, channelId, current_thdac, {nsw::ref::TRIM_HI, nsw::ref::TRIM_MID, nsw::ref::TRIM_LO});

    m_vmmTrimmerData.effThreshSlope.insert_or_assign(feb_ch, thresh_slope);
    m_vmmTrimmerData.trimmerMax.insert_or_assign(feb_ch, trimmer_max);

    if (thresh_slope == 0) {
      tot_chs--;
      continue;
    }

    if (!cm::checkChannel(ch_baseline_rms, m_isStgc)) {
      // Second check, confirm underlying value hasn't changed in previous call
      tot_chs--;
      continue;
    }

    // trimmer range check
    const auto& min_eff_threshold = m_vmmTrimmerData.minEffThresh.at(feb_ch);
    const auto& max_eff_threshold = m_vmmTrimmerData.maxEffThresh.at(feb_ch);
    const auto& vmm_eff_thresh = m_febTrimmerData.midEffThresh.at(feb_vmm);
    ERS_DEBUG(2,
              fmt::format("{} VMM{}: [MIN_EF_TH = {}], [MAX_EF_TH = {}], [VMM_EF_TH = {}]",
                          m_feName,
                          vmmId,
                          cm::sampleTomV(min_eff_threshold, m_isStgc),
                          cm::sampleTomV(max_eff_threshold, m_isStgc),
                          cm::sampleTomV(vmm_eff_thresh, m_isStgc)));

    if (vmm_eff_thresh < min_eff_threshold || vmm_eff_thresh > max_eff_threshold) {
      ERS_DEBUG(2,
                fmt::format("{} VMM{} channel {} can't be equalized!", m_feName, vmmId, channelId));
    } else {
      good_chs++;
    }
  }  // end of channel loop

  if (nch_base_above_thresh > nsw::ref::CHAN_THR_CUTOFF) {
    nsw::VmmTrimmerScaCalibrationIssue issue(
      ERS_HERE,
      fmt::format("{} VMM{} has [{}] channels with V_eff below baseline at trim value 14 THDAC "
                  "might be increased later!",
                  m_feName,
                  vmmId,
                  nch_base_above_thresh));
    ers::warning(issue);
  }

  m_febTrimmerData.goodChannels.insert_or_assign(feb_vmm, good_chs);
  m_febTrimmerData.totalChannels.insert_or_assign(feb_vmm, tot_chs);
}

std::pair<float, std::size_t> nsw::VmmTrimmerScaCalibration::findLinearRegionSlope(
  const std::size_t vmmId,
  const std::size_t channelId,
  const std::size_t thdac,
  const nsw::calib::TrimPoints& trim_vals)
{
  auto trim_hi = std::get<0>(trim_vals);
  auto trim_mid = std::get<1>(trim_vals);
  auto trim_lo = std::get<2>(trim_vals);

  float avg_m{0.f};

  if (trim_hi <= trim_mid) {
    trim_hi = 0;
  } else if (trim_mid <= trim_lo) {
    trim_hi = 0;
  } else {
    const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};
    const nsw::calib::FebChannelPair feb_ch{feb_vmm, channelId};

    const auto& ch_baseline_med = m_vmmTrimmerData.baselineMed.at(feb_ch);
    const auto& bad_bl = std::get<4>(m_febTrimmerData.channelInfo.at(feb_vmm));

    auto& nch_base_above_thresh = m_febTrimmerData.baselines_above_threshold.at(feb_vmm);

    auto channel_mid_eff_thresh = 0.f;
    auto channel_max_eff_thresh = 0.f;
    auto channel_min_eff_thresh = 0.f;

    // Smples over all trim values
    for (const auto& trim : {trim_lo, trim_mid, trim_hi}) {
      const auto results =
        sampleVmmChTrimDac(vmmId, channelId, thdac, trim, nsw::ref::THDAC_SAMPLE_FACTOR);

      const auto median = cm::takeMedian(results);

      const auto dev_sam =
        std::count_if(std::cbegin(results), std::cend(results), [&median](const auto sam) {
          return (std::abs(static_cast<int64_t>(sam - median)) > nsw::ref::THDAC_SAMPLE_MARGIN);
        });

      if (dev_sam > std::floor((m_nSamples * nsw::ref::THDAC_SAMPLE_FACTOR) / 4)) {
        ERS_DEBUG(2,
                  fmt::format("{} VMM{} channel {}: {}/{} samples strongly deviate",
                              m_feName,
                              vmmId,
                              channelId,
                              dev_sam,
                              m_nSamples * nsw::ref::THDAC_SAMPLE_FACTOR));
      }

      const auto eff_thresh = median - ch_baseline_med;

      if (trim == trim_mid) {
        channel_mid_eff_thresh = eff_thresh;
        if (channel_mid_eff_thresh < 0) {
          nch_base_above_thresh++;

          ERS_DEBUG(2,
                    fmt::format("{} VMM{}: channel {} has negative effective thr. = {}",
                                m_feName,
                                vmmId,
                                channelId,
                                channel_mid_eff_thresh));
        }
      } else if (trim == trim_lo) {
        channel_max_eff_thresh = eff_thresh;
      } else if (trim == trim_hi) {
        channel_min_eff_thresh = eff_thresh;
      }

      ERS_DEBUG(2,
                fmt::format("{} VMM{}: channel {} Trim <<{}>>, sample median = {}, and effective thr [{}]",
                            m_feName,
                            vmmId,
                            channelId,
                            trim,
                            median,
                            eff_thresh));
    }

    // Highest trimmer value corresponds to the lowest set threshold! mc
    m_vmmTrimmerData.minEffThresh.insert_or_assign(feb_ch, channel_min_eff_thresh);
    m_vmmTrimmerData.midEffThresh.insert_or_assign(feb_ch, channel_mid_eff_thresh);
    m_vmmTrimmerData.maxEffThresh.insert_or_assign(feb_ch, channel_max_eff_thresh);

    ERS_DEBUG(
      2,
      fmt::format(
        "{} VMM{}: slope calc params -> ( Min|Mid|Max thr: {} | {} | {})(Hi|Mid|Lo trims: {} | {} | {})",
        m_feName,
        vmmId,
        channel_min_eff_thresh,
        channel_mid_eff_thresh,
        channel_max_eff_thresh,
        trim_hi,
        trim_mid,
        trim_lo));

    const auto [m1, m2] = cm::getSlopes(channel_min_eff_thresh,
                                        channel_mid_eff_thresh,
                                        channel_max_eff_thresh,
                                        nsw::ref::TRIM_HI,
                                        nsw::ref::TRIM_MID,
                                        nsw::ref::TRIM_LO);

    avg_m = (m1 + m2) / 2.;

    ERS_DEBUG(2,
              fmt::format("{} VMM{}: channel {} trim {} - ([avg_m {}], [m1 {}], [m2 {}])",
                          m_feName,
                          vmmId,
                          channelId,
                          trim_hi,
                          avg_m,
                          m1,
                          m2));

    const auto check = cm::checkSlopes(m1, m2, nsw::ref::SLOPE_CHECK);
    if (!check) {
      ERS_DEBUG(
        2,
        fmt::format("{} VMM{}: low-middle & middle-high slope comparison gives {}, slopes are not same",
                    m_feName,
                    vmmId,
                    check));

      return findLinearRegionSlope(vmmId, channelId, thdac, {trim_hi - 2, trim_mid, trim_lo});
    }
  }

  return std::make_pair(avg_m, trim_hi);
}

void nsw::VmmTrimmerScaCalibration::analyseTrimmers(const std::size_t vmmId,
                                                    const std::size_t thdac,
                                                    const float thDacSlope,
                                                    const bool recalc)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};

  const auto& channel_masks = m_febTrimmerData.channelMasks.at(feb_vmm);
  const auto& good_channels = m_febTrimmerData.goodChannels.at(feb_vmm);
  const auto& tot_channels = m_febTrimmerData.totalChannels.at(feb_vmm);

  ERS_DEBUG(1,
            fmt::format(
              "{} VMM{}: {} out of {} channels are OK!", m_feName, vmmId, good_channels, tot_channels));

  // value to add to THDAC if one of the channels with negative threshold is unmasked
  // check for trimmer performance vector of effective threshold
  const auto short_trim =
    std::count_if(std::cbegin(m_vmmTrimmerData.trimmerMax),
                  std::cend(m_vmmTrimmerData.trimmerMax),
                  [this](const auto trim_max) { return (trim_max.second <= nsw::ref::TRIM_MID); });

  if (short_trim >= nsw::ref::MAX_NUM_BAD_SWOOSH) {
    ers::warning(nsw::VmmTrimmerScaCalibrationIssue(
      ERS_HERE,
      fmt::format("{} VMM{}: {}/64 trimmers with half of defined operation range",
                  m_feName,
                  vmmId,
                  short_trim)));
  }

  for (std::size_t channelId = 0; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
    if (channel_masks.at(channelId) != 0) {
      continue;
    }

    std::this_thread::sleep_for(5ms);

    analyseChannelTrimmers(vmmId, channelId, thdac, recalc);
  }

  const auto bad_trim = std::size_t{tot_channels - good_channels};

  if (!recalc) {
    // check if there are unmasked channels above threshold, if this is the
    // case - recalculate values with updated thdac
    // FIXME TODO can any entries in this vector ever be negative?
    std::vector<std::size_t> add_dac;
    for (std::size_t i = 0; i < nsw::vmm::NUM_CH_PER_VMM; i++) {
      const nsw::calib::FebChannelPair feb_ch{feb_vmm, i};
      const auto extraDAC = [this, feb_ch]() {
        try {
          return m_vmmTrimmerData.dac_to_add.at(feb_ch);
        } catch (const std::out_of_range& ex) {
          // dac_to_add doesn't have a matching key
          return std::size_t{0};
        }
      }();

      // half of trimmer working range, 1mV per DAC unit on the VMM, *not* per SCA ADC unit
      if (extraDAC > 0 and cm::sampleTomV(extraDAC, m_isStgc) <= nsw::ref::TRIM_CIRCUIT_MID) {
        add_dac.push_back(extraDAC);
      } else {
        continue;
      }
    }

    if (!add_dac.empty()) {
      std::sort(add_dac.begin(), add_dac.end());
      // FIXME TODO this shouldn't ever be negative
      const int plus_dac = std::round(add_dac.back() / thDacSlope) + 1;
      const auto thdac_new = thdac + plus_dac;

      ERS_INFO(fmt::format("{} VMM{} New THDAC value is set to - ({}) by adding ({} + 1) [DAC] -> "
                           "recalculating channel threshold values",
                           m_feName,
                           vmmId,
                           thdac_new,
                           plus_dac));

      // Recursively call again, with updated thdac, but only for a
      // maximum of 2 total calls, because recalc is set to true here
      analyseTrimmers(vmmId, thdac_new, thDacSlope, true);
    } else {
      if (bad_trim >= nsw::vmm::NUM_CH_PER_VMM/4) {
        nsw::VmmTrimmerScaCalibrationIssue issue(
          ERS_HERE,
          fmt::format(
            "{} VMM{} - Number of channels with bad trimmers ({})!", m_feName, vmmId, bad_trim));
        ers::warning(issue);
      }
    }
  } else {
    const int plus_dac = thdac - m_febTrimmerData.thDacValues.at(feb_vmm);
    if ((bad_trim >= nsw::vmm::NUM_CH_PER_VMM/4) or recalc) {
      nsw::VmmTrimmerScaCalibrationIssue issue(
        ERS_HERE,
        fmt::format(
          "{} VMM{}: Threshold raised by {} DAC, OR many badly trimmed channels - [{}/64](try 2)",
          m_feName,
          vmmId,
          plus_dac,
          bad_trim));
      ers::warning(issue);
    }

    // FIXME TODO but doesn't modify the thDacConstants...?
    m_febTrimmerData.thDacValues.insert_or_assign(feb_vmm, thdac);
  }
}

void nsw::VmmTrimmerScaCalibration::analyseChannelTrimmers(const std::size_t vmmId,
                                                           const std::size_t channelId,
                                                           const std::size_t thdac_i,
                                                           const bool recalc)
{
  const nsw::calib::FebVmmPair feb_vmm{m_feName, vmmId};
  const nsw::calib::FebChannelPair feb_ch{feb_vmm, channelId};

  const auto& eff_thresh_slope = m_vmmTrimmerData.effThreshSlope.at(feb_ch);
  const auto& ch_baseline_med = m_vmmTrimmerData.baselineMed.at(feb_ch);

  // FIXME TODO add proper slope check here
  const auto slope_ok = (std::abs(eff_thresh_slope) > std::pow(10, -9.));

  // Get desired trimmer value
  const auto delta =
    m_vmmTrimmerData.midEffThresh.at(feb_ch) - m_febTrimmerData.midEffThresh.at(feb_vmm);

  auto& best_channel_trim = m_vmmTrimmerData.best_channel_trim;

  if (slope_ok) {
    const std::size_t trim_target =
      nsw::ref::TRIM_MID + std::round(delta / eff_thresh_slope);

    ERS_DEBUG(2,
              fmt::format("{} VMM{} channel {} trim information (ch_slope = {})(delta = {})(trim target = {})",
                          m_feName,
                          vmmId,
                          channelId,
                          eff_thresh_slope,
                          delta,
                          trim_target));

    best_channel_trim.insert_or_assign(
      feb_ch,
      std::max(std::size_t{0}, std::min(trim_target, m_vmmTrimmerData.trimmerMax.at(feb_ch))));
  } else {
    ERS_DEBUG(2,
              fmt::format("{} VMM{} channel {} slope is unuseful, value={}",
                          m_feName,
                          vmmId,
                          channelId,
                          eff_thresh_slope));
    best_channel_trim.insert_or_assign(feb_ch, nsw::ref::TRIM_LO);
  }

  // FIXME TODO what to do if this call is allowed to throw rather than return an empty vector?
  const auto results = sampleVmmChTrimDac(vmmId, channelId, thdac_i, best_channel_trim.at(feb_ch));

  const auto [median, eff_thresh] =
    [this, &vmmId, &channelId, &results, &ch_baseline_med]() -> std::pair<float, float> {
    if (results.empty()) {
      ERS_LOG(fmt::format("{} VMM{} channel {} returned an empty vector, unable to calculate "
                          "median or effective threshold.",
                          m_feName,
                          vmmId,
                          channelId));
      return {0.f, 0.f};
    }
    const auto tmp_median = cm::takeMedian(results);
    return {tmp_median, tmp_median - ch_baseline_med};
  }();

  ERS_DEBUG(
    2,
    fmt::format("{} VMM{} channel {}: THR(eff) = {} ADC", m_feName, vmmId, channelId, eff_thresh));

  if (eff_thresh < 0) {
  ERS_LOG(
    fmt::format("{} VMM{} channel {}: effective threshold is below 0", m_feName, vmmId, channelId));
    // Trimmed median is smaller than baseline median for this channel
    auto& channel_masks = m_febTrimmerData.channelMasks.at(feb_vmm);
    const auto mask = (channel_masks.at(channelId) != 0);
    if (!mask) {
      auto& dac_to_add = m_vmmTrimmerData.dac_to_add;
      dac_to_add.insert_or_assign(feb_ch, std::abs(eff_thresh));
      // FIXME TODO 55 is magic!!
      if (recalc or std::abs(eff_thresh) > 55) {
        // cutting of around 20mV
        // if negative eff thr is less than approx 30mV, raise the thdac
        // FIXME REDUNDANT duplicates line above, probably only need to mask here.
        dac_to_add.insert_or_assign(feb_ch, std::abs(eff_thresh));
        channel_masks.at(channelId) = 1;
      }
    } else {
      ERS_DEBUG(
        2,
        fmt::format(
                    "{} VMM{}: ignoring masked channel {} with eff_thr {}<0", m_feName, vmmId, channelId, eff_thresh));
    }
  }

  auto& channel_trimmed_thr = m_vmmTrimmerData.channel_trimmed_thr;
  auto& eff_thr_w_best_trim = m_vmmTrimmerData.eff_thr_w_best_trim;

  eff_thr_w_best_trim.insert_or_assign(feb_ch, eff_thresh);
  channel_trimmed_thr.insert_or_assign(feb_ch, median);
}

void nsw::VmmTrimmerScaCalibration::writeOutScaVmmCalib()
{
  m_chCalibData.close();

  const auto fNameJson = fmt::format("{}/partial_config.json", m_outPath, m_feName);
  pt::write_json(fNameJson, m_febOutJson);

  std::ifstream filecheck;
  filecheck.open(fNameJson, std::ios::in);
  if (!(filecheck.peek() == std::ifstream::traits_type::eof())) {
    nsw::VmmTrimmerScaCalibrationIssue issue(ERS_HERE,
                                             fmt::format("{} file was not written", fNameJson));
    ers::warning(issue);
  }
  filecheck.close();
}
