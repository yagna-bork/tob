#include "../include/building.h"
#include "../include/valuation.h"
#include "util.h"
#include <cstdio>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

static std::string TOB_TO_STR[] = {"Commercial", "Residential", "Mixed",
                                   "Development"};

std::string tob_to_string(TypeOfBuilding t) { return TOB_TO_STR[t]; }

/*
 * Subunit code
 */
std::string SubUnit::to_string(int tablevel) const {
  std::string res = tabs(tablevel);
  if (!sub_building_name.empty()) {
    res += sub_building_name;
    res += ", ";
  }
  res += building_name;
  res += ", ";
  res += code;
  res += ", ";
  res += description;
  res += ", ";
  res += is_commercial ? "Commercial" : "Residential";
  return res;
}

/*
 * Building code
 */
std::string Building::to_string(int tablevel) const {
  std::string res = tabs(tablevel);
  res += name;
  res += ", ";
  res += street;
  res += ", ";
  res += town;
  res += ", ";
  res += postcode;
  res += ", ";
  res += tob_to_string(tob);
  for (const SubUnit &unit : subunits) {
    res += "\n";
    res += unit.to_string(tablevel + 1);
  }
  for (const Valuation &val : valuations) {
    res += "\n";
    res += val.to_string(tablevel + 1);
  }
  return res;
}

void Building::set_tob() {
  tob = subunits[0].is_commercial ? TypeOfBuilding::COMMERCIAL
                                  : TypeOfBuilding::RESIDENTIAL;
  bool is_commercial = tob == TypeOfBuilding::COMMERCIAL;
  for (int i = 1; i != subunits.size(); i++) {
    if (is_commercial != subunits[i].is_commercial) {
      tob = TypeOfBuilding::MIXED;
      return;
    }
  }
}

/*
 * Fetching and combination code
 */
std::vector<Building> fetch_buildings(CURL *handle, float x, float y,
                                      int radius) {
  std::vector<Building> buildings;
  std::unordered_map<std::string, size_t> idxs;
  std::string data;
  char url[500];
  nlohmann::json jdata;
  bool is_commercial;
  size_t building_idx;
  int offset = 0;

  do {
    data.clear();
    get_os_radius_url(url, 500, x, y, radius, offset);
    make_get_request(handle, url, data);
    jdata = nlohmann::json::parse(data);

    for (nlohmann::json &jb : jdata["results"]) {
      std::string building_name = get_json_fields<std::string>(
          jb["DPA"],
          {"BUILDING_NUMBER", "BUILDING_NAME", "DEPENDENT_THOROUGHFARE_NAME"});
      std::string street =
          get_json_field<std::string>(jb["DPA"], "THOROUGHFARE_NAME");
      std::string key = building_name + street;
      if (!idxs.count(key)) {
        buildings.push_back(
            {building_name,
             street,
             get_json_field<std::string>(jb["DPA"], "POST_TOWN"),
             get_json_field<std::string>(jb["DPA"], "POSTCODE"),
             {get_json_field<float>(jb["DPA"], "X_COORDINATE"),
              get_json_field<float>(jb["DPA"], "Y_COORDINATE")}});
        idxs[key] = buildings.size() - 1;
      }
      std::string classification_code =
          get_json_field<std::string>(jb["DPA"], "CLASSIFICATION_CODE");
      is_commercial = classification_code[0] == 'C';
      buildings[idxs[key]].subunits.push_back(
          {get_json_fields<std::string>(
               jb["DPA"], std::vector<std::string>(
                              {"ORGANISATION_NAME", "SUB_BUILDING_NAME"})),
           std::move(building_name), classification_code,
           get_json_field<std::string>(jb["DPA"],
                                       "CLASSIFICATION_CODE_DESCRIPTION"),
           is_commercial});
    }
    offset += get_json_field<int>(jdata["header"], "maxresults");
  } while (offset < jdata["header"]["totalresults"]);
  for (Building &b : buildings) {
    b.set_tob();
  }
  return buildings;
}

void combine_buildings(Building &x, Building &y) {
  std::move(y.subunits.begin(), y.subunits.end(),
            std::back_inserter(x.subunits));
  std::move(y.valuations.begin(), y.valuations.end(),
            std::back_inserter(x.valuations));
  if ((x.tob != TypeOfBuilding::MIXED) && (x.tob != y.tob)) {
    x.tob = TypeOfBuilding::MIXED;
  }
  x.name = longest_common_substr(x.name, y.name);
  if (x.name.empty()) {
    x.name = "Building Shell";
  }
}

Building make_development(PlanningApplication &&plan_app) {
  Building development;
  development.name = plan_app.address;
  development.location = plan_app.location;
  development.plan_apps.push_back(plan_app);
  development.tob = TypeOfBuilding::DEVELOPMENT;
  return development;
}
