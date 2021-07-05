#include <array>
#include <iterator>
#include <functional>

#include <psac/psac.hpp>

// ----------------------------------------------------------------------------
//                               EXAMPLE
// ----------------------------------------------------------------------------

// Convert an iterator to a pointer to the value that the iterator points to
template<typename T>
auto to_ptr(T x) {
  return &(*x);
}

// Map implemented with a parallel for loop and no manual granularity control
template<typename It>
psac_function(map, It in_begin, It in_end, It out_begin, std::function<int(int)> f) {
  int n = std::distance(in_begin, in_end);
  psac_parallel_for(int i, 0, n, 1024, {
    auto in_mod = to_ptr(in_begin + i);
    auto out_mod = to_ptr(out_begin + i);
    psac_read((auto x), (in_mod), {
      psac_write(out_mod, f(x));
    });
  });
}

// Map implemented with divide and conquer and manual granularity control
template<typename It>
psac_function(map_granular_dc, It in_begin, It in_end, It out_begin,
    std::function<int(int)> f, int granularity = 1024) {
  
  if (std::distance(in_begin, in_end) <= granularity) {
    psac_read_array(const auto& a, std::make_pair(in_begin, in_end), {
      for (size_t i = 0; i < a.size(); i++) {
        psac_write(to_ptr(out_begin + i), f(a[i]));
      }
    });
  }
  else {
    auto in_mid = in_begin + std::distance(in_begin, in_end) / 2;
    auto out_mid = out_begin + std::distance(in_begin, in_end) / 2;
    psac_par(
      psac_call(map_granular_dc, in_begin, in_mid, out_begin, f, granularity),
      psac_call(map_granular_dc, in_mid, in_end, out_mid, f, granularity)
    );
  }
}

// Map implemented with manual granularity control with a parallel for loop
template<typename It>
psac_function(map_granular, It in_begin, It in_end, It out_begin,
    std::function<int(int)> f, int granularity = 1024) {
  int n_chunks = (std::distance(in_begin, in_end) + granularity - 1) / granularity;
  psac_parallel_for(int i, 0, n_chunks, 1, {
    auto in_from = in_begin + i * granularity;
    auto in_to = (i == n_chunks - 1) ? in_end : in_begin + (i + 1) * granularity;
    auto out_from = out_begin + i * granularity;
    psac_read_array(const auto& a, std::make_pair(in_from, in_to), {
      for (size_t j = 0; j < a.size(); j++) {
        psac_write(to_ptr(out_from + j), f(a[j]));
      }
    });
  });
}

constexpr size_t chunk_size = 12;
using int_chunk = std::array<int, chunk_size>;  // 12 ints in a mod fits in a cache line

template<typename It>
psac_function(map_chunks, It in_begin, It in_end, It out_begin, std::function<int(int)> f) {
  int n = std::distance(in_begin, in_end);
  psac_parallel_for(int i, 0, n, 1, {
    auto in_mod = to_ptr(in_begin + i);
    auto out_mod = to_ptr(out_begin + i);
    psac_read((auto a), (in_mod), {
      int_chunk out;
      for (size_t j = 0; j < chunk_size; j++) {
        out[j] = f(a[j]);
      }
      psac_write(out_mod, out);
    });
  });
}

template<typename It, typename It2, typename It3>
psac_function(shuffle_map, It in_begin, It in_end, It2 in_perm, It3 out_begin, std::function<int(int)> f) { 
  size_t n = std::distance(in_begin, in_end);
  size_t n_chunks = (n + chunk_size - 1) / chunk_size;
  psac_parallel_for(size_t i, size_t(0), n_chunks, size_t(1000), {
    psac_dynamic_context({ 
      int_chunk out;
      for (size_t j = 0; j < chunk_size && i * chunk_size + j < n; j++) {
        auto p = *(in_perm + i * chunk_size + j);
        out[j] = f(psac_dynamic_read(to_ptr(in_begin + p)));
      }
      psac_write(to_ptr(out_begin + i), out);
    });
  });
}

template<typename It>
psac_function(map_chunks_granular, It in_begin, It in_end, It out_begin,
    std::function<int(int)> f, int granularity = 64) {
  int n_chunks = (std::distance(in_begin, in_end) + granularity - 1) / granularity;
  psac_parallel_for(int i, 0, n_chunks, 1, {
    auto in_from = in_begin + i * granularity;
    auto in_to = (i == n_chunks - 1) ? in_end : in_begin + (i + 1) * granularity;
    auto out_from = out_begin + i * granularity;
    psac_read_array(const auto& a, std::make_pair(in_from, in_to), {
      for (size_t j = 0; j < a.size(); j++) {
        int_chunk out;
        for (size_t k = 0; k < chunk_size; k++) {
          out[k] = f(a[k][j]);
        }
        psac_write(to_ptr(out_from + j), out);
      }
    });
  });
}

template<typename It>
psac_function(sum, It in_begin, It in_end, psac::Mod<int>* result) {
  if (in_begin + 1 == in_end) {
    psac_read((auto x), (&(*in_begin)), { psac_write(result, x); });
  }
  else {
    auto in_mid = in_begin + std::distance(in_begin, in_end) / 2;
    psac::Mod<int>* left_result = psac_alloc(int);
    psac::Mod<int>* right_result = psac_alloc(int);
    psac_par(
      psac_call(sum, in_begin, in_mid, left_result),
      psac_call(sum, in_mid, in_end, right_result)
    );
    psac_read((auto x, auto y), (left_result, right_result), {
      psac_write(result, x + y);
    });
  }
}

template<typename It>
psac_function(map_reduce, It in_begin, It in_end, It out_begin, std::function<int(int)> f, psac::Mod<int>* result) {
  psac_call(map, in_begin, in_end, out_begin, f);
  psac_call(sum, out_begin, out_begin + std::distance(in_begin, in_end), result);
}

