#ifndef CALIBTYPES_H
#define CALIBTYPES_H

#include <unordered_map>
#include <string>
#include <vector>
#include <tuple>

#include "NSWConfiguration/Constants.h"

namespace nsw {
  namespace calib {

    /*!
     * \brief FebVmm defines a pair of front-end board name and VMM index
     *
     * \param std::string Key : front-end name from `feb.getAddress()`
     * \param std::size_t VMM index (0--7)
     */
    using FebVmmPair = std::pair<std::string, std::size_t>;

    /*!
     * \brief VmmChannelArray defines an array with one entry per VMM channel
     *
     * \tparam T Value: type of value to store in the array
     */

    template<class T>
    using VmmChannelArray = std::array<T, nsw::vmm::NUM_CH_PER_VMM>;

    /*!
     * \brief FebChannel defines a pair of front-end board name and VMM channel ID
     *
     * \param FebVmmPair Key : front-end name from `feb.getAddress()` coupled with `vmm{vmmID}`
     * \param int VMM channel in the VMM loop (0-63)
     */
    using FebChannelPair = std::pair<FebVmmPair, std::size_t>;

    /*!
     * \brief Define the hash function for using \c FebVmmPair as a key in an std::unordered_map
     */
    struct FebVmmHash {
      template<class T1, class T2>
      std::size_t operator()(const std::pair<T1, T2>& p) const
      {
        const auto h1 = std::hash<T1>{}(p.first);
        const auto h2 = std::hash<T2>{}(p.second);

        return h1 ^ h2;
      }
    };

    /*!
     * \brief FebVmmMap defines a map between a FebVmm and calibration parameter
     *
     * \param FebVmmPair Key: a VMM of a front-end board
     * \tparam T Value: any calibration parameter corresponding to the selected VMM
     */

    template<class T>
    using FebVmmMap = std::unordered_map<FebVmmPair, T, FebVmmHash>;

    /*!
     * \brief Define the hash function for using \c FebChannelPair as a key in an std::unordered_map
     */
    struct FebChannelHash {
      template<class T1, class T2>
      std::size_t operator()(const std::pair<std::pair<T1, T2>, T2>& p) const
      {
        const auto h1 = std::hash<T1>{}(p.first.first);
        const auto h2 = std::hash<T2>{}(p.first.second);
        const auto h3 = std::hash<T2>{}(p.second);

        return h1 ^ h2 ^ h3;
      }
    };

    /*!
     * \brief FebChannelMap defines a map between a FebChannel and calibration parameter
     *
     * \param FebChannelPair Key: a VMM channel of a front-end board
     * \tparam T Value: any calibration parameter corresponding to the selected channel
     */

    template<class T>
    using FebChannelMap = std::unordered_map<FebChannelPair, T, FebChannelHash>;

    /*!
     * \brief GlobalThrConstants defines an `std::tuple` that stores VMM global threshold fit information
     *
     * \param float the calibrated threshold DAC slope
     * \param float the calibrated threshold DAC offset
     * \param std::size_t the calibrated threshold DAC TODO clamped to VMM DAC range 0x3ff?
     */
    using GlobalThrConstants = std::tuple<float, float, std::size_t>;

    /*!
     * \brief VMMSampleVector defines an `std::vector` of the VMM samples
     */
    using VMMSampleVector = std::vector<short unsigned int>;

    /*!
     * \brief VMMDisconnectedChannelInfo defines an `std::tuple` that stores:
     *
     * \param std::vector<float> The per-channel median of the pruned baseline sampling data for all channels on this VMM
     * \param std::vector<float> The per-channel RMS of the pruned baseline sampling data for all channels on this VMM
     * \param std::vector<std::size_t> The pruned baseline sampling data for all channels on this VMM
     * \param std::vector<std::size_t> The baseline sampling data for all channels on this VMM
     * \param std::size_t The number of disconnected channels
     * \param std::vector<std::size_t> A count of the sampling outliers for each VMM channel
     *
     * \todo Rename this as it is nominally an object storing the baseline information
     */
    using VMMDisconnectedChannelInfo = std::tuple<std::vector<float>,
                                                  std::vector<float>,
                                                  std::vector<std::size_t>,
                                                  std::size_t,
                                                  std::vector<std::size_t>>;

    /*!
     * \brief VMMChannelSummary defines an `std::tuple` that stores:
     *
     * \param std::size_t The number of noisy channels
     * \param std::size_t The number of hot channels
     * \param std::size_t The number of dead channels
     * \param float The sum RMS noise of all channels
     * \param bool Flag reporting whether the baselines are
     *        considered bad, i.e., more than half are noisy
     */
    using VMMChannelSummary = std::tuple<std::size_t, std::size_t, std::size_t, float, bool>;

    /*!
     * \brief FEBVMMConstants defines an ``std::tuple`` that stores
     *
     * \param std::size_t The number of VMMs connected to the FEB
     * \param std::size_t The index of the first VMM
     * \param std::size_t The number of 1/4 of all channels connected to the FEB
     */
    using FEBVMMConstants = std::tuple<std::size_t, std::size_t, std::size_t>;

    /*!
     * \brief TrimPoints defines an ``std::tuple`` that stores
     *
     * \param std::size_t The high trimmer DAC point
     * \param std::size_t The middle trimmer DAC point
     * \param std::size_t The low trimmer DAC point
     */
    using TrimPoints = std::tuple<std::size_t, std::size_t, std::size_t>;
  }  // namespace calib
}  // namespace nsw

#endif
