#include "../include/util.h"
#include <algorithm>
#include <curl/curl.h>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <proj.h>
#include <string>
#include <unordered_map>

static const std::string CONFIG_FILE = "config.txt";
static std::unordered_map<std::string, std::string> CONFIG;

int global_to_nat_grid(double lat, double lng, float &x, float &y) {
  // https://stackoverflow.com/questions/31426559/c-convert-lat-long-to-bng-with-proj-4
  // https://proj.org/en/stable/development/migration.html#code-example
  PJ_COORD c, c_out;
  PJ *P = proj_create_crs_to_crs(
      PJ_DEFAULT_CTX, "+proj=longlat +datum=WGS84",
      "+proj=tmerc +lat_0=49 +lon_0=-2 +k=0.9996012717 +x_0=400000 +y_0=-100000"
      " +ellps=airy +datum=OSGB36 +units=m +no_defs",
      NULL);
  if (P == 0)
    return 0;
  c.lpzt.z = 0.0;
  c.lpzt.t = HUGE_VAL;
  c.lpzt.lam = lng;
  c.lpzt.phi = lat;
  c_out = proj_trans(P, PJ_FWD, c);

  x = round(c_out.xy.x * 100.0) / 100.0; // rounded to 2 d.p.
  y = round(c_out.xy.y * 100.0) / 100.0;
  return 1;
}

void init_config() {
  std::string::iterator eq_sign;
  std::string line, conf_key, conf_val;
  std::ifstream confs_f(CONFIG_FILE);
  while (getline(confs_f, line)) {
    eq_sign = find(line.begin(), line.end(), '=');
    conf_key = std::string(line.begin(), eq_sign);
    conf_val = std::string(eq_sign + 1, line.end());
    CONFIG[conf_key] = conf_val;
  }
}

std::string config(const std::string &key) {
  if (CONFIG.empty()) {
    init_config();
  }
  return CONFIG[key];
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  std::string *datap = (std::string *)userdata;
  copy(ptr, ptr + nmemb, back_inserter(*datap));
  return nmemb;
}

void make_get_request(CURL *handle, char *url, std::string &data) {
  curl_easy_setopt(handle, CURLOPT_URL, url);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &data);
  curl_easy_setopt(handle, CURLOPT_HTTPGET, 1);
  curl_easy_perform(handle);
}

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
