#ifndef CALIBRATIONMATH_H_
#define CALIBRATIONMATH_H_

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>
#include <stdexcept>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace nsw {

  /*!
   * Constant list used in the CalibrationSca, THRCalib, and PDOCalib
   */
  namespace ref {
    // The SCA has a 12 bit ADC, covering a 1000 mV max range
    constexpr float SAMPLES_PER_MV     = 1000.f/0xfff;  //!< Convert ADC samples to mV
    constexpr float MV_PER_SAMPLE      = 1.f/SAMPLES_PER_MV;  //!< Convert mV to ADC samples
    constexpr float MM_RESISTOR_FACTOR = 1.5f;  //!< Due to a resistor on MMs

    constexpr float SLOPE_CHECK        = 0.5f; //!< Combat the swoosh
    constexpr std::size_t MAX_NUM_BAD_SWOOSH   = 10;  //!< Maximum swooshes

    // FIXME TODO factor of 3 is really high
    constexpr float CHANNEL_HOT_FACTOR  = 3.f;  //!< Factor for baseline median to declare channel hot
    constexpr float CHANNEL_DEAD_FACTOR = 0.2f;  //!< Factor for baseline median to declare channel dead

    constexpr std::size_t BASELINE_CUTOFF= 7;  //!< Sample deviation cutoff on baseline ADC readings

    constexpr float RMS_CUTOFF           = 10.f;  //!< FIXME TODO RENAME THIS BAD NAME!!!
    constexpr float BROKEN_HIGH_BASELINE = 1000.f;  //!< Hot channel baseline [mV]
    constexpr float BROKEN_LOW_BASELINE  = 50.f;  //!< Dead channel baseline [mV]
    constexpr float HIGH_CH_BASELINE     = 190.f;  //!< Highest acceptable channel baseline [mV]
    constexpr float LOW_CH_BASELINE      = 140.f;  //</ Lowest acceptable channel baseline [mV]
    constexpr float THR_DEV_FACTOR       = 0.1f;  //!< Threshold sample deviation (min to max) margin fraction
    constexpr float EFF_THR_HIGH         = 60.f;  //!< Maximal acceptable threshold [mV]
    constexpr float EFF_THR_LOW          = 5.f;  //!< Minimal acceptable effective threshold [mV]

    constexpr std::size_t THR_DEAD_CUTOFF     = 150;  //!< Lowest acceptable threshold DAC value at nominal setting if 300 DAC [ADC]
    constexpr std::size_t THDAC_SAMPLE_MARGIN = 50;  //!< Trimmer DAC sample deviationmargin [ADC]

    constexpr std::size_t TRIM_LO              = 0;   //!< Lowest trimmer DAC position (physically highest)
    constexpr std::size_t TRIM_MID             = 15;  //!< Middle trimmer DAC position
    constexpr std::size_t TRIM_HI              = 31;  //!< Highest trimmer DAC position (physically lowest)
    constexpr std::size_t CHAN_THR_CUTOFF      = 1;   //!< Cutoff on the number of channels with baseline above threshold
    constexpr std::size_t NUM_VMM_SFEB6        = 6;   //!< Number of VMMs in an sFEB6 board
    constexpr std::size_t THDAC_SAMPLE_FACTOR  = 5;   //!< Multiplier in Threshold DAC calculations
    constexpr std::size_t BASELINE_SAMP_FACTOR = 10;  //!< Baseline reading multiplier
    constexpr std::size_t THR_SAMPLE_CUTOFF    = 50;  //!< ADC cutoff threshold for threshold DAC sampling

    constexpr std::size_t TRIM_CIRCUIT_MAX     = 31;  //!< Maximum range of the VMM channel trim setting
    const     std::size_t TRIM_CIRCUIT_MID     = std::ceil(TRIM_CIRCUIT_MAX/2.f);   //!< Midpoint of the VMM channel trim setting

    constexpr std::size_t AVG_THDAC_SLOPE = 2;  //!< Average slope of the threshhold DAC

    constexpr std::size_t VMM_THDAC_MAX = 1023;  //!< Maximum value of the VMM THDAC

    constexpr float PFEB_THDAC_TARGET_OFFSET = 50.f;  //!< Value (in mV) above the target for pFEBs
    constexpr float SFEB_THDAC_TARGET_OFFSET = 18.f;  //!< Value (in mV) above the target for sFEBs
    constexpr float TRIM_OFFSET = 15.f;  //!< Value (in mV ~ 1 mV -> 1 DAC) above the target for all vmms. Makes so target value of the global threshold is offset by the mid trimmer values.

    constexpr float TP_DELAY_STEP = 3.125f;  //!< Test pulse delay step [ns] in ROC
  }

  /*!
   * \brief Functions to perform mathematical operations on the calibration data
     *
     * \todo Adapt takeSum, takeMedian, takeMean, takeRms, takeMode to accept any Container<Numerical Type>
   */
  namespace CalibrationMath {
    /*!
     * \brief Takes sum of the vector of ADC samples
     *
     * \tparam T data type of sample vector, T must be a numerical type
     * \param v is a vector of samples
     *
     * \returns The sum of the elements in the input vector
     */
    template<typename T>
    float takeSum(const std::vector<T>& v)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "takeSum is only implemented for integral and floating point types.");

      return std::accumulate(std::cbegin(v), std::cend(v), 0.f);
    }

    /*!
     * \brief Takes mean of the vector of ADC samples
     *
     * \tparam T data type of sample vector, T must be a numerical type
     * \param v is a vector of samples
     *
     * \returns an std::pair holding the mean and sum of the elements in the input vector
     */
    template<typename T>
    float takeMean(const std::vector<T>& v)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "takeMean is only implemented for integral and floating point types.");

      const auto sum = takeSum(v);
      const auto mean = [&sum, &v]() {
        if (!v.empty()) {
          return sum / static_cast<float>(v.size());
        }
        return sum;
      }();
      return mean;
    }

    /*!
     * \brief Takes median of the vector of ADC samples
     *
     * N.B. In the case of an even sized vector, this will be the
     * arithmetic mean of the middle two elements.
     *
     * \tparam T data type of sample vector, T must be a numerical type
     * \param v is a vector of samples
     *
     * \returns The median of the elements in the input vector
     *
     * \throws std::logic_error in the case of an empty input vector
     */
    template<typename T>
    T takeMedian(std::vector<T> v)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "takeMedian is only implemented for integral and floating point types.");
      if (v.empty()) {
        throw std::logic_error("Cannot take the median of an empty vector");
      }

      const std::size_t n{v.size() / 2};
      std::nth_element(std::begin(v), std::begin(v)+n, std::end(v));

      if (n % 2 == 0) {
        // For an even sized vector, the median is the average of the middle two values
        const auto rmax = std::max_element(std::begin(v), std::begin(v)+n);
        return static_cast<T>((*rmax + v.at(n))/2);
      } else {
        return v.at(n);
      }
    }

    /*!
     * \brief Takes root-mean-square of the vector of samples.
     *
     * \tparam T data type of sample vector, T must be a numerical type
     * \param v is a vector of samples
     * \param mean mean of the collection of values to extract the RMS from
     *
     * \returns The RMS of the elements in the input vector
     *
     * \throws std::logic_error in the case of an empty input vector
     */
    template<typename T>
    float takeRms(const std::vector<T>& v, const float mean)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "takeRms is only implemented for integral and floating point types.");

      if (v.empty()) {
        throw std::logic_error("Cannot take the RMS of an empty vector");
      }

      // Create a vector holding the values x_i - mean for all x_i in v
      std::vector<float> d(v.size());
      std::transform(std::cbegin(v), std::cend(v), std::begin(d), [&mean](const auto sample) {
        return (static_cast<float>(sample) - mean);
      });

      // Calculate the sum of the (x_i - mean)^2
      const float sq_sum = std::inner_product(std::cbegin(d), std::cend(d), std::cbegin(d), 0.0f);
      const float stdev = std::sqrt(sq_sum / static_cast<float>(d.size()));
      return stdev;
    }

    /*!
     * \brief Takes mode of the vector of ADC samples
     *
     * \tparam T data type of sample vector, T must be a numerical type
     * \param v is a vector of samples
     *
     * \returns The largest value with the most entries
     */
    template<typename T>
    T takeMode(std::vector<T>& v)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "takeMode is only implemented for integral and floating point types.");

      std::sort(std::begin(v),std::end(v));

      std::size_t counter{0};
      std::size_t mode_count_max{0};
      T mode{0};

      for (std::size_t i{1}; i < v.size(); i++) {
        const std::size_t left{v.size() - i};
        if (mode_count_max > left) {
          break;
        }
        counter++;
        if (v.at(i) != v.at(i - 1)) {
          if (mode_count_max <= counter) {
            mode_count_max = counter;
            mode = v.at(i - 1);
            counter = 0;
          } else {
            counter = 0;
            continue;
          }
        } else {
          continue;
        }
      }
      return mode;
    }

    /*!
     * \brief Performs a linear fit to two input vectors of the same size
     *
     *   y = m*x + b
     *   y is the measurement in ADC counts
     *   x is the DAC value
     *
     * \tparam T1 data type of DAC sample vector, T1 must be a numerical type
     * \tparam T2 data type of ADC measurement sample vector, T2 must be a numerical type
     * \param xvals is a vector of input DAC values
     * \param yvals is a vector of corresponding ADC measurement values
     *
     * \returns The an std::pair holding the fitted slope and intercept
     *
     * \throws std::logic_error in the case the two vectors have different size
     * \throws std::logic_error in the case of an empty input vector
     */
    template<typename T1, typename T2>
    std::pair<float, float> fitLine(const std::vector<T1>& xvals, const std::vector<T2>& yvals)
    {
      static_assert(std::is_arithmetic_v<T1>,
                    "fitLine is only implemented for integral and floating point types.");
      static_assert(std::is_arithmetic_v<T2>,
                    "fitLine is only implemented for integral and floating point types.");

      if (xvals.size() != yvals.size()) {
           throw std::logic_error(fmt::format("Unable to fit a line with unequal data sets: x:{} y:{}", xvals, yvals));
      }

      const auto min_size = std::min(xvals.size(), yvals.size());
      if (min_size == 0) {
          throw std::logic_error(fmt::format("Unable to fit a line with no data points: x:{} y:{}", xvals, yvals));
      }

      const auto mean_x = takeMean(xvals);
      const auto mean_y = takeMean(yvals);

      auto num = 0.f;
      auto den = 0.f;
      for (std::size_t i{0}; i < min_size; ++i) {
        num += (static_cast<float>(xvals.at(i)) - mean_x) * (static_cast<float>(yvals.at(i)) - mean_y);
        den += std::pow((static_cast<float>(xvals.at(i)) - mean_x), 2);
      }

      const auto m = num / den;
      const auto b = mean_y - (m * mean_x);

      return {m, b};
    }

    /*!
     * \brief Converts SCA samples from ADC to mV
     *
     * \tparam T data type of sample vector, T must be a numerical type
     * \param sample ADC sample value
     * \param stgc sTGC front-end flag
     *
     * \returns the mV value of a sample ADC count
     */
    template<typename T>
    float sampleTomV(T sample, bool stgc)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "sample_to_mV is only implemented for integral and floating point types.");
      auto val = static_cast<float>(sample) * nsw::ref::SAMPLES_PER_MV;
      if (!stgc) {
        val *= nsw::ref::MM_RESISTOR_FACTOR;
      }
      return val;
    }

    /*!
     * \brief Converts mV units to DAC
     *
     * \param mV_read sample value
     * \param stgc sTGC front-end flag
     *
     * \returns the DAC unit equivalent of a sample ADC to mV conversion
     */
    std::size_t mVtoSample(float mV_read, bool stgc);

    /*!
     * \brief Checks if channel baseline is within RMS cutoff
     *
     * \param ch_baseline_rms channel sample RMS value
     * \param stgc sTGC front-end flag
     *
     * \returns true if the channel baseline is less than nsw::ref::RMS_CUTOFF
     */
    bool checkChannel(float ch_baseline_rms, bool stgc);

    /*!
     * \brief Checks if slopes of two trimmer operation regions are same
     *
     * \param m1 slope 1 value
     * \param m2 slope 2 value
     * \param slope_check_value reference value
     *
     * \returns true if the difference between two slope values is less than the reference
     */
    bool checkSlopes(float m1, float m2, float slope_check_val);

    /*!
     * \brief Calculates slopes of the full trimmer operational region
     *
     * The full trimmer operational range is split into low-mid and
     * mid-high regions, and each slope is calculated separately.
     *
     * The slope converts ADC counts to DAC units
     *
     * \param points_trim_lo channel low trimmer median ADC count and trimmer value
     * \param points_trim_mid channel mid trimmer median ADC count and trimmer value
     * \param points_trim_hi channel high trimmer median ADC count and trimmer value
     *
     *
     * \returns an std::pair containing the slope values for the two trimmer ranges
     */
  std::pair<float,float> getSlopes(const std::pair<float,int>& points_trim_low,
                                   const std::pair<float,int>& points_trim_mid,
                                   const std::pair<float,int>& points_trim_hi);
  }
}
#endif
