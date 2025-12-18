#ifndef BUILDING_H
#define BUILDING_H
#include "building_shape.h"
#include "planning.h"
#include "util.h"
#include "valuation.h"
#include <cstdio>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct SubUnit {
  std::string sub_building_name;
  // Keeping this around so old name info isn't lost
  // when two buildings are combined
  std::string building_name;
  std::string code;
  std::string description;
  bool is_commercial;

  std::string to_string(int tablevel = 0) const;
};

enum TypeOfBuilding {
  COMMERCIAL = 0,
  RESIDENTIAL = 1,
  MIXED = 2,
  DEVELOPMENT = 3
};

struct Building {
  std::string name;
  std::string street;
  std::string town;
  std::string postcode;
  FPoint location;
  std::vector<SubUnit> subunits;
  std::vector<Valuation> valuations;
  TypeOfBuilding tob;
  std::vector<PlanningApplication> plan_apps;

  std::string to_string(int tablevel = 0) const;

  void set_tob();
};

inline void get_os_radius_url(char *url_buff, size_t url_buff_sz, float x,
                              float y, int radius, int offset) {
  snprintf(url_buff, url_buff_sz,
           "%s?key=%s&point=%.2f,%.2f&radius=%d&offset=%d",
           config("PLACES_RADIUS_URL").c_str(),
           config("OS_PROJECT_API_KEY").c_str(), x, y, radius, offset);
}

std::vector<Building> fetch_buildings(CURL *handle, float x, float y,
                                      int radius);

inline std::string get_location_key(float x, float y) {
  return std::to_string(static_cast<int>(x * 100)) +
         std::to_string(static_cast<int>(y * 100));
}

void combine_buildings(Building &x, Building &y);

std::vector<Building> cluster_buildings(std::vector<Building> &buildings);

inline ValuationDB::QueryParam get_query_param(const Building &b) {
  return {b.name.c_str(), b.street.c_str(), b.postcode.c_str(),
          b.tob == TypeOfBuilding::RESIDENTIAL};
}

/*
 * Create a special type of building
 * with only `PlanningApplication`
 * information. Moves `PlanningApplication`
 * into result.
 */
Building make_development(PlanningApplication &&plan_app);

// serialisation
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SubUnit, sub_building_name, building_name,
                                   code, description, is_commercial)
NLOHMANN_JSON_SERIALIZE_ENUM(TypeOfBuilding, {{MIXED, "MIXED"},
                                              {RESIDENTIAL, "RESIDENTIAL"},
                                              {COMMERCIAL, "COMMERCIAL"},
                                              {DEVELOPMENT, "DEVELOPMENT"}})
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Building, name, street, town, postcode,
                                   location, subunits, valuations, tob,
                                   plan_apps)
#endif
