#include <vector>
#include <iostream>
#include "include/building.h"
#include "include/valuation.h"
#include "include/longest_common_substr.h"
#include "include/planning.h"

void get_tobs(double lat, double lng, int radius) {
	float x, y;
	if (!global_to_nat_grid(lat, lng, x, y)) {
		std::cerr << "Failed to get BNG for (" 
			 << lat << ", " << lng << ")" << std::endl;
		exit(1);
	}
	CURL *handle = curl_easy_init();
	if (!handle) {
		std::cerr << "Failed to setup easy curl" << std::endl;
		exit(1);
	}
	std::vector<Building> buildings = fetch_buildings(handle, x, y, radius);
	std::vector<PlanningApplication> applications = fetch_planning_apps(handle, lat, lng, radius);
	ValuationDB db;
	if (!db.connected()) {
		return;
	}
	std::vector<ValuationDB::QueryParam> params;
	std::transform(buildings.begin(), buildings.end(), std::back_inserter(params), 
				   get_query_param);
	std::vector<ValuationDB::QueryResult> valuation_results = db.get_valuations(params);
	for (int i = 0; i != buildings.size(); i++) {
		buildings[i].valuations = std::move(valuation_results[i]);
	}
	std::vector<Building> clustered = cluster_buildings(buildings);
	for (const Building &b: clustered) {
		std::cout << b.to_string() << std::endl;
	}
	curl_easy_cleanup(handle);
}

int main(int argc, char *argv[]) {
	int radius = 30; // in meters
	if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--radius") == 0) {
		radius = atoi(argv[2]);
	}
	double lat = atof(argv[argc-2]);
	double lng = atof(argv[argc-1]);
	get_tobs(lat, lng, radius);
	return 0;
}
