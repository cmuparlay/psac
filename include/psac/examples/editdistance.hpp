
#include <string>
#include <vector>

#include <psac/psac.hpp>

constexpr int granularity = 1;

// Compute the edit distance between a and b in O(nm) time
// where |a| = n, and |b| = m. Computation is sequential.
int edit_distance(const std::string& a, const std::string& b) {
  size_t n = a.size(), m = b.size();
  std::vector<std::vector<int>> DP(n + 1, std::vector<int>(m + 1));
  DP[0][0] = 0;
  for (size_t i = 0; i <= n; i++) DP[i][0] = i;
  for (size_t j = 0; j <= m; j++) DP[0][j] = j;
  for (size_t i = 1; i <= n; i++) {
    for (size_t j = 1; j <= m; j++) {
      if (a[i-1] == b[j-1]) {
        DP[i][j] = DP[i-1][j-1];
      }
      else {
        DP[i][j] = std::min(std::min(DP[i-1][j-1], DP[i-1][j]), DP[i][j-1]) + 1;
      }
    }
  }
  return DP[n][m];
}

// Compute the minimum edit distance to s in the range [in_begin, in_end)
template<typename It>
psac_function(reduce_edit_distance, It in_begin, It in_end, std::string* s, psac::Mod<int>* res) {
  if (std::distance(in_begin, in_end) <= granularity) {
    psac_read_array(const auto& a, std::make_pair(in_begin, in_end), {
      int min_result = edit_distance(a[0], *s);
      for (size_t i = 1; i < a.size(); i++) {
        min_result = std::min(min_result, edit_distance(a[i], *s));
      }
      psac_write(res, min_result);
    });
  }
  else {
    auto in_mid = in_begin + std::distance(in_begin, in_end) / 2;
    psac::Mod<int>* left = psac_alloc(int);
    psac::Mod<int>* right = psac_alloc(int);
    psac_par(
      psac_call(reduce_edit_distance, in_begin, in_mid, s, left),
      psac_call(reduce_edit_distance, in_mid, in_end, s, right)
    );
    psac_read((auto x, auto y), (left, right), {
      psac_write(res, std::min(x, y));
    });
  }
}

