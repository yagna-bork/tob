#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <iostream>
#include <iterator>
#include "include/building.h"
#include "include/valuation.h"
#include "include/longest_common_substr.h"
#include "include/planning.h"
#include "include/util.h"
#include <httplib.h>

using nlohmann::json;

bool get_search_params(const httplib::Request &req, httplib::Response &resp, double &lat, double &lng, int &rad) {
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

	std::vector<Building> buildings = fetch_buildings(handle, x, y, rad);
	curl_easy_cleanup(handle);
	ValuationDB db;
	if (!db.connected()) {
		resp.set_content("Failed to connect to db", "text/plain");
		resp.status = httplib::StatusCode::InternalServerError_500;
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

	json resp_json = {{"results", clustered}};
	resp.set_content(resp_json.dump(), "application/json");
}

void planning_endpoint(const httplib::Request &req, httplib::Response &resp) {
	double lat, lng;
	int rad;
	if (!get_search_params(req, resp, lat, lng, rad)) {
		return;
	}
	CURL *handle = curl_easy_init();
	if (!handle) {
		resp.set_content("Failed to setup easy curl", "text/plain");
		resp.status = httplib::StatusCode::InternalServerError_500;
		return;
	}

	std::vector<PlanningApplication> applications = fetch_planning_apps(handle, lat, lng, rad);
	curl_easy_cleanup(handle);

	json resp_json = {{"results", applications}};
	resp.set_content(resp_json.dump(), "application/json");
}

int main(int argc, char *argv[]) {
	init_config();
	httplib::Server server;
	std::string url = g_config["SERVER_URL"];
	int port = atoi(g_config["SERVER_PORT"].c_str());

	server.Get("/building", building_endpoint);
	server.Get("/planning", planning_endpoint);
	std::cout << "Starting server on " << url << ":" << port << std::endl;
	server.listen(url, port);
	return 0;
}
