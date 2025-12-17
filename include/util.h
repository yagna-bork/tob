#ifndef UTIL_H
#define UTIL_H
#include <algorithm>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <proj.h>
#include <string>
#include <unordered_map>

/*
 * Converts latitude and longitude to British National Grid coords
 * as required by the Ordnance Survey API. Returns 0 on fail,
 * 1 on success.
 */
int global_to_nat_grid(double lat, double lng, float &x, float &y);

std::string config(const std::string &key);

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);

void make_get_request(CURL *handle, char *url, std::string &data);

template <class T> T get_json_field(nlohmann::json &obj, std::string key) {
  if (obj.contains(key) && !obj[key].is_null()) {
    return obj[key].get<T>();
  }
  return T();
};

template <class T>
T get_json_fields(nlohmann::json &obj, std::vector<std::string> attempt_keys) {
  for (const std::string &key : attempt_keys) {
    if (!obj.contains(key)) {
      continue;
    }
    if (obj[key].is_null()) {
      break;
    }
    return obj[key].get<T>();
  }
  return T();
};

inline std::string tabs(int tablevel) { return std::string(tablevel, '\t'); }

std::string longest_common_substr(const std::string &a, const std::string &b);
#endif
