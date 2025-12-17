#ifndef GUARD_LCS_H
#define GUARD_LCS_H
#include <algorithm>
#include <string>
#include <vector>

std::string longest_common_substr(const std::string &a, const std::string &b) {
  int m = a.size(), n = b.size();
  if (n == 0 || m == 0) {
    return "";
  }
  int lcs_beg, lcs_size = 0;
  std::vector<std::vector<int>> dp(m, std::vector<int>(n, 0));
  for (int i = 0; i != m; i++) {
    dp[i][0] = a[i] == b[0];
  }
  for (int i = 0; i != m; i++) {
    dp[0][i] = a[0] == b[i];
  }
  for (int i = 1; i != m; i++) {
    for (int j = 1; j != n; j++) {
      if (a[i] == b[j]) {
        dp[i][j] = 1 + dp[i - 1][j - 1];
      } else {
        dp[i][j] = 0;
      }
      if (dp[i][j] > lcs_size) {
        lcs_size = dp[i][j];
        lcs_beg = i - (lcs_size - 1);
      }
    }
  }
  return std::string(a.begin() + lcs_beg, a.begin() + lcs_beg + lcs_size);
}
#endif
