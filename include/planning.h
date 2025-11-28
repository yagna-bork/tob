#ifndef GUARD_PLANNING_H
#define GUARD_PLANNING_H
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <curl/curl.h>
#include "util.h"

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

	std::string to_string() const {
		std::string res = address;
		res += "\n";
		res += std::string(address.size(), '-');
		res += "\n";
		res += description;
		return res;
	}
};

std::vector<PlanningApplication> fetch_planning_apps(CURL *handle, double lat, double lng, int radius) {
	std::vector<PlanningApplication> applications;
	float krad = radius / 1000.0;
	std::string data;
	char fields[] = "address,description,app_size,app_state,other_fields,start_date,location_x,location_y";
	char url[500];
	int index = 0;
	nlohmann::json jdata;
	double app_lat, app_lng;
	float x, y;

	do {
		data.clear();
		snprintf(url, 500, "%s?lat=%.9f&lng=%.9f&krad=%.3f&select=%s&sort=-start_date&index=%d", 
				 g_config["PLANIT_URL"].c_str(), lat, lng, krad, fields, index);
		make_get_request(handle, url, data);
		jdata = nlohmann::json::parse(data);

		for (nlohmann::json &app: jdata["records"]) {
			if (app["other_fields"].contains("northing") &&
				app["other_fields"].contains("easting")) 
			{
				x = app["other_fields"]["easting"];
				y = app["other_fields"]["northing"];
			} else {
				app_lat = app["location_y"];
				app_lng = app["location_x"];
				if(!global_to_nat_grid(app_lat, app_lng, x, y)) {
					std::cerr << "Failed to get BNG for (" 
						 << lat << ", " << lng << ")" << std::endl;
					continue;
				}
			}
			applications.push_back({
				std::move(app["address"]),
				std::move(app["description"]),
				get_json_field<std::string>(app, "app_size"),
				get_json_field<std::string>(app, "app_state"),
				get_json_field<std::string>(app["other_fields"], "date_received"),
				get_json_field<std::string>(app["other_fields"], "date_validated"),
				get_json_field<std::string>(app["other_fields"], "decision_date"),
				get_json_field<std::string>(app["other_fields"], "decision_issued_date"),
				x, y
			});
		}
		index = jdata["to"].get<int>()+1;
	} while (index < jdata["total"]);
	return applications;
}
#endif
