#ifndef CALIBRATIONMATH_H_
#define CALIBRATIONMATH_H_

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

namespace nsw {

  /*!
   * Constant list used in the CalibrationSca, THRCalib, and PDOCalib
   */
  namespace ref {
    constexpr float SAMPLES_PER_MV     = 1000./4095.;  //!< 12-bit ADC with 1 V max range
    constexpr float MV_PER_SAMPLE      = 1./SAMPLES_PER_MV;  //!< 12-bit ADC with 1 V max range
    constexpr float MM_RESISTOR_FACTOR = 1.5;  //!< Due to a resistor on MMs

    // NO REALLY, FIXME DOCUMENTATION
    // FIXME to be reevaluated
    constexpr float SLOPE_CHECK        = 0.5; //!< Combat the swoosh

    constexpr float CHANNEL_HOT_FACTOR  = 3;  //!< Factor for baseline median to declare channel hot
    constexpr float CHANNEL_DEAD_FACTOR = 0.2;  //!< Factor for baseline median to declare channel dead

    constexpr std::size_t BASELINE_CUTOFF= 7;  //!< Sample deviation cutoff on baseline ADC readings

    constexpr float RMS_CUTOFF           = 10.;  //!< FIXME TODO RENAME THIS BAD NAME!!!
    constexpr float BROKEN_HIGH_BASELINE = 1000.;  //!< Hot channel baseline [mV]
    constexpr float BROKEN_LOW_BASELINE  = 50.;  //!< Dead channel baseline [mV]
    constexpr float HIGH_CH_BASELINE     = 190.;  //!< Highest acceptable channel baseline [mV]
    constexpr float LOW_CH_BASELINE      = 140.;  //</ Lowest acceptable channel baseline [mV]
    // FIXME TODO value to be tuned
    constexpr float THR_DEV_FACTOR       = 0.1;  //!< Threshold sample deviation (min to max) margin fraction
    constexpr float EFF_THR_HIGH         = 60.;  //!< Maximal acceptable threshold [mV]
    constexpr float EFF_THR_LOW          = 5.;  //!< Minimal acceptable effective threshold [mV]

    constexpr std::size_t THR_DEAD_CUTOFF     = 150;  //!< Lowest acceptable threshold DAC value at nominal setting if 300 DAC [ADC]
    constexpr std::size_t THDAC_SAMPLE_MARGIN = 50;  //!< Trimmer DAC sample deviationmargin [ADC]

    constexpr std::size_t TRIM_MID             = 15;  //!< Trimmer DAC middle position
    constexpr std::size_t TRIM_LO              = 0;   //!< Lowest trimmer position (physicly highest)
    constexpr std::size_t TRIM_HI              = 31;  //!< Highest trimmer DAC position (physicly lowest)
    constexpr std::size_t CHAN_THR_CUTOFF      = 1;   //!< Cutoff on the number of channels with baseline above threshold
    constexpr std::size_t NUM_VMM_SFEB6        = 6;   //!< Number of VMMs in an sFEB6 board
    constexpr std::size_t THDAC_SAMPLE_FACTOR  = 5;   //!< Multiplier in Threshold DAC calculations
    constexpr std::size_t BASELINE_SAMP_FACTOR = 10;  //!< Baseline reading multiplier
    constexpr std::size_t THR_SAMPLE_CUTOFF    = 50;  //!< ADC cutoff threshold for threshold DAC sampling

    constexpr std::size_t TRIM_CIRCUIT_MAX     = 31;  //!< Maximum range of the VMM channel trim setting
    // FIXME TO BE REEVALUTATED
    constexpr std::size_t MAX_NUM_BAD_SWOOSH   = 10;  //!< Maximum swooshes
    const     std::size_t TRIM_CIRCUIT_MID     = std::ceil(TRIM_CIRCUIT_MAX/2.);   //!< Midpoint of the VMM channel trim setting

    constexpr std::size_t AVG_THDAC_SLOPE = 2;  //!< Average slope of the threshhold DAC

    constexpr std::size_t VMM_THDAC_MAX = 1023;  //!< Maximum value of the VMM THDAC

    constexpr std::size_t PFEB_THDAC_TARGET_OFFSET = 50;  //!< Value (in mV) above the target for pFEBs
    constexpr std::size_t SFEB_THDAC_TARGET_OFFSET = 18;  //!< Value (in mV) above the target for pFEBs

    constexpr float TP_DELAY_STEP = 3.125;  //!< Test pulse delay step [ns] in ROC
  }

  /*!
   * \brief Functions to perform mathematical operations on the calibration data
   */
  namespace CalibrationMath {
    /*!
     * \brief Takes median of the vector of ADC samples
     *
     * \param v vector of samples
     * \tparam T data type of sample vector, T must be a numerical type
     */
    template<typename T>
    T takeMedian(std::vector<T> v)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "takeMedian is only implemented for integral and floating point types.");
      const std::size_t n{v.size() / 2};
      std::nth_element(std::begin(v), std::begin(v)+n, std::end(v));
      return v.at(n);
    }

    /*!
     * \brief takes root-mean-square of the vector of samples
     *
     * \param v[in,out] vector of samples
     * \param mean FIXME documentation
     * \tparam T data type of sample vector, T must be a numerical type
     */
    template<typename T>
    float takeRms(const std::vector<T>& v, const float mean)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "takeRms is only implemented for integral and floating point types.");
      const float sq_sum = std::inner_product(v.begin(), v.end(), v.begin(), 0.0f);
      const float stdev = std::sqrt(sq_sum / v.size() - mean * mean);
      return stdev;
    }

    /*!
     * \brief Takes mode of the vector of ADC samples
     *
     * Will return the largest value that has the the most entries in
     * the vector.
     *
     * \param v vector of samples
     * \tparam T data type of sample vector, T must be a numerical type
     */
    template<typename T>
    float takeMode(std::vector<T>& v)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "takeMode is only implemented for integral and floating point types.");

      std::sort(v.begin(),v.end());

      std::size_t counter = 0;
      std::size_t mode_count_max = 0;
      T mode = 0;

      for (std::size_t i = 1; i < v.size(); i++) {
        counter++;
        if (v.at(i) != v.at(i - 1)) {
          if (mode_count_max <= counter) {
            mode_count_max = counter;
            mode           = v.at(i - 1);
            counter        = 0;
          } else {
            counter = 0;
            continue;
          }
        } else {
          continue;
        }
      }
      return static_cast<float>(mode);
    }

    /*!
     * \brief Converts SCA samples from ADC to mV
     *
     * \param sample ADC sample value
     * \param stgc sTGC front-end flag
     * \tparam T data type of sample vector, T must be a numerical type
     */
    template<typename T>
    float sampleTomV(T sample, bool stgc)
    {
      static_assert(std::is_arithmetic_v<T>,
                    "sample_to_mV is only implemented for integral and floating point types.");
      auto val = static_cast<float>(sample * nsw::ref::SAMPLES_PER_MV);
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
     */
    std::size_t mVtoSample(float mV_read, bool stgc);

    /*!
     * \brief Checks if channel baseline is within RMS cutoff
     *
     * \param ch_baseline_rms channel sample RMS value
     * \param stgc sTGC front-end flag
     */
    bool checkChannel(float ch_baseline_rms, bool stgc);

    /*!
     * \brief Checks if slopes of two trimmer operation regions are same
     *
     * \param m1 slope 1 value
     * \param m2 slope 2 value
     * \param slope_check_value reference value
     */
    bool checkSlopes(float m1, float m2, float slope_check_val);

    /*!
     * \brief Calculates slopes of trimmer operation regions
     *
     * \param ch_lo channel low trimmer DAC voltage
     * \param ch_mid channel mid trimmer DAC voltage
     * \param ch_hi channel high trimmer DAC voltage
     * \param trim_hi high trimmer DAC register value
     * \param trim_mid middle trimmer DAC register value
     * \param trim_lo low trimmer DAC register value
     */
    std::pair<float,float> getSlopes(float ch_lo,
                                     float ch_mid,
                                     float ch_hi,
                                     int trim_hi,
                                     int trim_mid,
                                     int trim_lo);

  }
}
#endif
