#include "resample.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdlib.h>
#include <vector>

#define M_2PI 6.283185307179586476925286766559005

std::vector<float> sub_vector(const std::vector<float> &input, int begin,
                              int length) {
  std::vector<float> res(length);
  for (int i = 0; i < length; i++) {
    res[i] = input[i + begin];
  }
  return res;
}
std::vector<std::vector<float>>
sub_matrix(const std::vector<std::vector<float>> &input, int row_begin,
           int n_rows, int col_begin, int n_cols) {
  std::vector<std::vector<float>> res;
  res.resize(n_rows);
  for (int i = 0; i < n_rows; i++) {
    res[i].resize(n_cols);
    for (int j = 0; j < n_cols; j++)
      res[i][j] = input[row_begin + i][col_begin + j];
  }
  return res;
}

/*return gcd */
int Gcd(int m, int n) {
  if (m == 0 || n == 0) {
    if (m == 0 && n == 0) { // gcd not defined, as all integers are divisors.
      std::cerr << "Undefined GCD since m = 0, n = 0.";
    }
    return (m == 0 ? (n > 0 ? n : -n) : (m > 0 ? m : -m));
    // return absolute value of whichever is nonzero
  }
  while (1) {
    m %= n;
    if (m == 0)
      return (n > 0 ? n : -n);
    n %= m;
    if (n == 0)
      return (m > 0 ? m : -m);
  }
}
std::vector<float> add_mat_vec(float alpha,
                               const std::vector<std::vector<float>> &M,
                               const std::vector<float> &v, float beta) {
  int row, col, length;
  row = M.size();
  col = M[0].size();
  length = v.size();
  std::vector<float> res(row, 0);
  for (int i = 0; i < row; i++) {
    float tmp = 0.0;
    for (int j = 0; j < col; j++) {
      tmp += M[i][j] * v[j];
    }
    res[i] = alpha * tmp + beta * res[i];
  }
  return res;
}

LinearResample::LinearResample(int samp_rate_in_hz, int samp_rate_out_hz,
                               float filter_cutoff_hz, int num_zeros)
    : samp_rate_in_(samp_rate_in_hz), samp_rate_out_(samp_rate_out_hz),
      filter_cutoff_(filter_cutoff_hz), num_zeros_(num_zeros) {
  assert(samp_rate_in_hz > 0.0 && samp_rate_out_hz > 0.0 &&
         filter_cutoff_hz > 0.0 && filter_cutoff_hz * 2 <= samp_rate_out_hz &&
         num_zeros > 0);

  // base_freq is the frequency of the repeating unit, which is the gcd
  // of the input frequencies.
  int base_freq = Gcd(samp_rate_in_, samp_rate_out_);
  input_samples_in_unit_ = samp_rate_in_ / base_freq;
  output_samples_in_unit_ = samp_rate_out_ / base_freq;

  SetIndexesAndWeights();
  Reset();
}

int Lcm(int m, int n) {
  assert(m > 0 && n > 0);
  int gcd = Gcd(m, n);
  return gcd * (m / gcd) * (n / gcd);
}

int LinearResample::GetNumOutputSamples(int input_num_samp, bool flush) const {
  int tick_freq = Lcm(samp_rate_in_, samp_rate_out_);
  int ticks_per_input_period = tick_freq / samp_rate_in_;

  // work out the number of ticks in the time interval
  // [ 0, input_num_samp/samp_rate_in_ ).
  long long interval_length_in_ticks =
      (long long)input_num_samp * (long long)ticks_per_input_period;
  if (!flush) {
    float window_width = num_zeros_ / (2.0 * filter_cutoff_);
    int window_width_ticks = floor(window_width * tick_freq);
    interval_length_in_ticks -= window_width_ticks;
  }
  if (interval_length_in_ticks <= 0)
    return 0;
  int ticks_per_output_period = tick_freq / samp_rate_out_;
  int last_output_samp = interval_length_in_ticks / ticks_per_output_period;
  if (last_output_samp * ticks_per_output_period == interval_length_in_ticks)
    last_output_samp--;
  int num_output_samp = last_output_samp + 1;
  return num_output_samp;
}

void LinearResample::SetIndexesAndWeights() {
  first_index_.resize(output_samples_in_unit_);
  weights_.resize(output_samples_in_unit_);

  double window_width = num_zeros_ / (2.0 * filter_cutoff_);

  for (int i = 0; i < output_samples_in_unit_; i++) {
    double output_t = i / static_cast<double>(samp_rate_out_);
    double min_t = output_t - window_width, max_t = output_t + window_width;
    int min_input_index = ceil(min_t * samp_rate_in_),
        max_input_index = floor(max_t * samp_rate_in_),
        num_indices = max_input_index - min_input_index + 1;
    first_index_[i] = min_input_index;
    weights_[i].resize(num_indices);
    for (int j = 0; j < num_indices; j++) {
      int input_index = min_input_index + j;
      double input_t = input_index / static_cast<double>(samp_rate_in_),
             delta_t = input_t - output_t;
      // sign of delta_t doesn't matter.
      weights_[i][j] = FilterFunc(delta_t) / samp_rate_in_;
    }
  }
}

// inline
void LinearResample::GetIndexes(int samp_out, int *first_samp_in,
                                int *samp_out_wrapped) const {
  int unit_index = samp_out / output_samples_in_unit_;
  // samp_out_wrapped is equal to samp_out % output_samples_in_unit_
  *samp_out_wrapped =
      static_cast<int>(samp_out - unit_index * output_samples_in_unit_);
  *first_samp_in =
      first_index_[*samp_out_wrapped] + unit_index * input_samples_in_unit_;
}

void LinearResample::Resample(const std::vector<float> &input, bool flush,
                              std::vector<float> *output) {
  int input_dim = input.size();
  int tot_input_samp = input_sample_offset_ + input_dim,
      tot_output_samp = GetNumOutputSamples(tot_input_samp, flush);

  assert(tot_output_samp >= output_sample_offset_);

  output->resize(tot_output_samp - output_sample_offset_);

  // samp_out is the index into the total output signal, not just the part
  // of it we are producing here.
  for (int samp_out = output_sample_offset_; samp_out < tot_output_samp;
       samp_out++) {
    int first_samp_in;
    int samp_out_wrapped;
    GetIndexes(samp_out, &first_samp_in, &samp_out_wrapped);
    const std::vector<float> &weights = weights_[samp_out_wrapped];
    int first_input_index =
        static_cast<int>(first_samp_in - input_sample_offset_);
    float this_output;
    if (first_input_index >= 0 &&
        first_input_index + int(weights.size()) <= input_dim) {
      std::vector<float> input_part(input.begin() + first_input_index,
                                    input.begin() + first_input_index +
                                        int(weights.size()));
      this_output = std::inner_product(input_part.begin(), input_part.end(),
                                       weights.begin(), 0);
    } else { // Handle edge cases.
      this_output = 0.0;
      for (int i = 0; i < weights.size(); i++) {
        float weight = weights[i];
        int input_index = first_input_index + i;
        if ((input_index < 0) &&
            (int(input_remainder_.size()) + input_index >= 0)) {
          this_output +=
              weight *
              input_remainder_[int(input_remainder_.size()) + input_index];
        } else if (input_index >= 0 && input_index < input_dim) {
          this_output += weight * input[input_index];
        } else if (input_index >= input_dim) {
          assert(flush);
        }
      }
    }
    int output_index = static_cast<int>(samp_out - output_sample_offset_);
    (*output)[output_index] = this_output;
  }

  if (flush) {
    Reset(); // Reset the internal state.
  } else {
    SetRemainder(input);
    input_sample_offset_ = tot_input_samp;
    output_sample_offset_ = tot_output_samp;
  }
}

void LinearResample::SetRemainder(const std::vector<float> &input) {
  std::vector<float> old_remainder(input_remainder_);
  int max_remainder_needed = ceil(samp_rate_in_ * num_zeros_ / filter_cutoff_);
  input_remainder_.resize(max_remainder_needed);
  for (int index = -input_remainder_.size(); index < 0; index++) {
    int input_index = index + input.size();
    if (input_index >= 0)
      input_remainder_[index + input_remainder_.size()] = input[input_index];
    else if (input_index + old_remainder.size() >= 0)
      input_remainder_[index + input_remainder_.size()] =
          old_remainder[input_index + old_remainder.size()];
    // else leave it at zero.
  }
}

void LinearResample::Reset() {
  input_sample_offset_ = 0;
  output_sample_offset_ = 0;
  input_remainder_.resize(0);
}

float LinearResample::FilterFunc(float t) const {
  float window, // raised-cosine (Hanning) window of width
                // num_zeros_/2*filter_cutoff_
      filter;   // sinc filter function
  if (fabs(t) < num_zeros_ / (2.0 * filter_cutoff_))
    window = 0.5 * (1 + cos(M_2PI * filter_cutoff_ / num_zeros_ * t));
  else
    window = 0.0; // outside support of window function
  if (t != 0)
    filter = sin(M_2PI * filter_cutoff_ * t) / (M_PI * t);
  else
    filter = 2 * filter_cutoff_; // limit of the function at t = 0
  return filter * window;
}

ArbitraryResample::ArbitraryResample(int num_samples_in, float samp_rate_in,
                                     float filter_cutoff,
                                     const std::vector<float> &sample_points,
                                     int num_zeros)
    : num_samples_in_(num_samples_in), samp_rate_in_(samp_rate_in),
      filter_cutoff_(filter_cutoff), num_zeros_(num_zeros) {
  assert(num_samples_in > 0 && samp_rate_in > 0.0 && filter_cutoff > 0.0 &&
         filter_cutoff * 2.0 <= samp_rate_in && num_zeros > 0);
  // set up weights_ and indices_.  Please try to keep all functions short and
  SetIndexes(sample_points);
  SetWeights(sample_points);
}

void ArbitraryResample::Resample(
    const std::vector<std::vector<float>> &input,
    std::vector<std::vector<float>> *output) const {
  // each row of "input" corresponds to the data to resample;
  // the corresponding row of "output" is the resampled data.

  std::vector<float> output_col(output->size());
  int col_num = (*output)[0].size();
  for (int i = 0; i < NumSamplesOut(); i++) {
    std::vector<std::vector<float>> input_part =
        sub_matrix(input, 0, input.size(), first_index_[i], weights_[i].size());
    const std::vector<float> &weight_vec(weights_[i]);
    output_col = add_mat_vec(1.0, input_part, weight_vec, 0.0);
    for (int j = 0; j < output_col.size(); j++) {
      (*output)[j][i] = output_col[j];
    }
  }
}

void ArbitraryResample::Resample(const std::vector<float> &input,
                                 std::vector<float> *output) const {
  assert(input.size() == num_samples_in_ && output->size() == weights_.size());

  int output_dim = output->size();
  for (int i = 0; i < output_dim; i++) {
    std::vector<float> input_part(input.begin() + first_index_[i],
                                  input.begin() + first_index_[i] +
                                      weights_[i].size());
    (*output)[i] = std::inner_product(input_part.begin(), input_part.end(),
                                      weights_[i].begin(), 0);
  }
}

void ArbitraryResample::SetIndexes(const std::vector<float> &sample_points) {
  int num_samples = sample_points.size();
  first_index_.resize(num_samples);
  weights_.resize(num_samples);
  float filter_width = num_zeros_ / (2.0 * filter_cutoff_);
  for (int i = 0; i < num_samples; i++) {
    // the t values are in seconds.
    float t = sample_points[i], t_min = t - filter_width,
          t_max = t + filter_width;
    int index_min = ceil(samp_rate_in_ * t_min),
        index_max = floor(samp_rate_in_ * t_max);
    // the ceil on index min and the floor on index_max are because there
    // is no point using indices just outside the window (coeffs would be zero).
    if (index_min < 0)
      index_min = 0;
    if (index_max >= num_samples_in_)
      index_max = num_samples_in_ - 1;
    first_index_[i] = index_min;
    weights_[i].resize(index_max - index_min + 1);
  }
}

void ArbitraryResample::SetWeights(const std::vector<float> &sample_points) {
  int num_samples_out = NumSamplesOut();
  for (int i = 0; i < num_samples_out; i++) {
    for (int j = 0; j < weights_[i].size(); j++) {
      float delta_t = sample_points[i] - (first_index_[i] + j) / samp_rate_in_;
      // Include at this point the factor of 1.0 / samp_rate_in_ which
      // appears in the math.
      weights_[i][j] = FilterFunc(delta_t) / samp_rate_in_;
    }
  }
}

float ArbitraryResample::FilterFunc(float t) const {
  float window, // raised-cosine (Hanning) window of width
                // num_zeros_/2*filter_cutoff_
      filter;   // sinc filter function
  if (fabs(t) < num_zeros_ / (2.0 * filter_cutoff_))
    window = 0.5 * (1 + cos(M_2PI * filter_cutoff_ / num_zeros_ * t));
  else
    window = 0.0; // outside support of window function
  if (t != 0.0)
    filter = sin(M_2PI * filter_cutoff_ * t) / (M_PI * t);
  else
    filter = 2.0 * filter_cutoff_; // limit of the function at zero.
  return filter * window;
}

void ResampleWaveform(float orig_freq, const std::vector<float> &wave,
                      float new_freq, std::vector<float> *new_wave) {
  float min_freq = std::min(orig_freq, new_freq);
  float lowpass_cutoff = 0.99 * 0.5 * min_freq;
  int lowpass_filter_width = 6;
  LinearResample resampler(orig_freq, new_freq, lowpass_cutoff,
                           lowpass_filter_width);
  resampler.Resample(wave, true, new_wave);
}
