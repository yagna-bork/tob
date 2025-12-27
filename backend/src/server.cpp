#include "../include/building.h"
#include "../include/httplib.h"
#include "../include/planning.h"
#include "../include/util.h"
#include "../include/valuation.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <float.h>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <vector>

using nlohmann::json;

// Buildings grouped by location
typedef std::unordered_map<FPoint, std::vector<Building *>> BuildingGroups;

// PlanningApplications grouped by address value
typedef std::unordered_map<std::string, std::vector<PlanningApplication *>>
    PlanAppGroups;

bool get_search_params(const httplib::Request &req, httplib::Response &resp,
                       double &lat, double &lng, int &rad) {
  if (!req.has_param("lat") || !req.has_param("lng")) {
    resp.set_content("lat and long required", "text/plain");
    resp.status = httplib::StatusCode::BadRequest_400;
    return false;
  }
  lat = atof(req.get_param_value("lat").c_str());
  lng = atof(req.get_param_value("lng").c_str());
  rad = 30; // in meters
  if (req.has_param("rad")) {
    rad = atoi(req.get_param_value("rad").c_str());
  }
  return true;
}

template <class T>
void translate_locations(CURL *handle, std::vector<T> &objs,
                         const FPoint &centre) {
  std::vector<FPoint *> locations;
  std::transform(objs.begin(), objs.end(), std::back_inserter(locations),
                 [](T &o) { return &o.location; });
  translate_points_to_building_centres(handle, locations, centre);
}

/*
 * Combines both `Building`s and `PlanningApplication`s streams
 * into a single result stream of `Building`s.
 * It also groups `Building`s together by location e.g. a block
 * of flats.
 */
std::vector<Building>
cluster_buildings(CURL *handle, std::vector<Building> &buildings,
                  std::vector<PlanningApplication> &plan_apps,
                  const FPoint &centre) {
  // translate building locations
  translate_locations(handle, buildings, centre);
  BuildingGroups building_groups;
  for (Building &b : buildings) {
    building_groups[b.location].push_back(&b);
  }
  std::unordered_map<FPoint, Building *> loc_to_building;
  for (const auto &pr : building_groups) {
    std::vector<Building *> group = pr.second;
    if (group.size() > 1) {
      for (int i = 1; i != group.size(); i++) {
        combine_buildings(*group[0], *group[i]);
      }
    }
    loc_to_building[group.front()->location] = group.front();
  }

  // translate planning application locations
  translate_locations(handle, plan_apps, centre);

  // Group planning applications by address
  PlanAppGroups planapp_groups;
  for (PlanningApplication &plan_app : plan_apps) {
    planapp_groups[plan_app.address].push_back(&plan_app);
  }

  // Try moving PlanningApplication into matching building
  // otherwise consider them `Development`s
  FPoint building_location;
  std::vector<PlanningApplication *> developments;
  for (const auto &pr : planapp_groups) {
    const std::vector<PlanningApplication *> &group = pr.second;
    building_location.x = FLT_MAX;
    building_location.y = FLT_MAX;
    for (PlanningApplication *plan_app : group) {
      if (loc_to_building.count(plan_app->location)) {
        building_location.x = plan_app->location.x;
        building_location.y = plan_app->location.y;
        break;
      }
    }
    if (building_location.x != FLT_MAX && building_location.y != FLT_MAX) {
      // Move the entire group into building
      for (PlanningApplication *plan_app : group) {
        loc_to_building[building_location]->plan_apps.push_back(
            std::move(*plan_app));
      }
    } else {
      // Settle for developments with same address
      // having same location aswell
      building_location = group.front()->location;
      for (PlanningApplication *plan_app : group) {
        plan_app->location = building_location;
        developments.push_back(plan_app);
      }
    }
  }

  // Combine Buildings and Developments into the same stream
  std::vector<Building> res;
  for (const auto &pr : loc_to_building) {
    res.push_back(std::move(*pr.second));
  }
  std::transform(developments.begin(), developments.end(),
                 std::back_inserter(res), [](PlanningApplication *plan_app) {
                   return make_development(std::move(*plan_app));
                 });
  return res;
}

void building_endpoint(const httplib::Request &req, httplib::Response &resp) {
  double lat, lng;
  int rad;
  if (!get_search_params(req, resp, lat, lng, rad)) {
    return;
  }
  float x, y;
  if (!global_to_nat_grid(lat, lng, x, y)) {
    char msg[100];
    snprintf(msg, 100, "Failed to get BND for (%f, %f)\n", lat, lng);
    resp.set_content(msg, "text/plain");
    resp.status = httplib::StatusCode::InternalServerError_500;
    return;
  }
  CURL *handle = curl_easy_init();
  if (!handle) {
    resp.set_content("Failed to setup easy curl", "text/plain");
    resp.status = httplib::StatusCode::InternalServerError_500;
    return;
  }

  // Get Buildings
  std::vector<Building> buildings = fetch_buildings(handle, x, y, rad);
  ValuationDB db;
  if (!db.connected()) {
    resp.set_content("Failed to connect to db", "text/plain");
    resp.status = httplib::StatusCode::InternalServerError_500;
    return;
  }
  std::vector<ValuationDB::QueryParam> params;
  std::transform(buildings.begin(), buildings.end(), std::back_inserter(params),
                 get_query_param);
  std::vector<ValuationDB::QueryResult> valuation_results =
      db.get_valuations(params);
  for (int i = 0; i != buildings.size(); i++) {
    buildings[i].valuations = std::move(valuation_results[i]);
  }

  // Get PlanningApplications
  std::vector<PlanningApplication> plan_apps =
      fetch_planning_apps(handle, lat, lng, rad);
  curl_easy_cleanup(handle);

  // Combine both streams into result
  FPoint centre = {x, y};
  std::vector<Building> res =
      cluster_buildings(handle, buildings, plan_apps, centre);

  json resp_json = {{"results_size", res.size()}, {"results", res}};
  resp.set_content(resp_json.dump(), "application/json");
}

int main(int argc, char *argv[]) {
  httplib::Server server;
  std::string url = config("SERVER_URL");
  int port = atoi(config("SERVER_PORT").c_str());

  server.Get("/buildings", building_endpoint);
  std::cout << "Starting server on " << url << ":" << port << std::endl;
  server.listen(url, port);
  return 0;
}
