#ifndef GUARD_PLANNING_H
#define GUARD_PLANNING_H
#include "util.h"
#include <cstdio>
#include <curl/curl.h>
#include <iostream>
#include <string>
#include <vector>

// Structers
// API call
struct PlanningApplication {
  std::string address;
  std::string description;
  std::string size;
  std::string state;
  std::string date_received;
  std::string date_validated;
  std::string date_decision;
  std::string date_decisison_issued;
  float x;
  float y;

  std::string to_string() const;
};

std::vector<PlanningApplication> fetch_planning_apps(CURL *handle, double lat,
                                                     double lng, int radius);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlanningApplication, address, description,
                                   size, state, date_received, date_validated,
                                   date_decision, date_decisison_issued, x, y)
#endif
