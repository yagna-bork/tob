#ifndef BUILDING_H
#define BUILDING_H
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "valuation.h"
#include "util.h"
#include "longest_common_substr.h"

// TODO: a helper function to combine planning application data into Building objs

struct SubUnit {
	std::string sub_building_name;
	// Keeping this around so old name info isn't lost 
	// when two buildings are combined
	std::string building_name; 
	std::string code;
	std::string description;
	bool is_commercial;

	std::string to_string(int tablevel = 0) const {
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
};

enum TypeOfBuilding {
	COMMERCIAL = 0,
	RESIDENTIAL = 1,
	MIXED = 2
};

std::vector<std::string> TOB_TO_STR = {"Commercial", "Residential", "Mixed"};

std::string tob_to_string(TypeOfBuilding t) {
	return TOB_TO_STR[t];
}

struct Building {
	std::string name;
	std::string street;
	std::string town;
	std::string postcode;
	float x;
	float y;
	std::vector<SubUnit> subunits;
	std::vector<Valuation> valuations;
	TypeOfBuilding tob;

	std::string to_string(int tablevel = 0) const {
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
		for (const SubUnit& unit: subunits) {
			res += "\n";
			res += unit.to_string(tablevel+1);
		}
		for (const Valuation& val: valuations) {
			res += "\n";
			res += val.to_string(tablevel+1);
		}
		return res;
	}

	void set_tob() {
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
};

void get_os_radius_url(char *url_buff, size_t url_buff_sz, float x, float y, 
					   int radius, int offset) 
{
	snprintf(url_buff, url_buff_sz, "%s?key=%s&point=%.2f,%.2f&radius=%d&offset=%d", 
			 g_config["PLACES_RADIUS_URL"].c_str(), 
			 g_config["OS_PROJECT_API_KEY"].c_str(), 
			 x, y, radius, offset);
}

std::vector<Building> fetch_buildings(CURL *handle, float x, float y, int radius) {
	init_config();
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

		for (nlohmann::json &jb: jdata["results"]) {
			std::string building_name = get_json_fields<std::string>(jb["DPA"], {
				"BUILDING_NUMBER", "BUILDING_NAME", "DEPENDENT_THOROUGHFARE_NAME"
			});
			std::string street = get_json_field<std::string>(jb["DPA"], "THOROUGHFARE_NAME");
			std::string key = building_name+street;
			if (!idxs.count(key)) { 
				buildings.push_back({
					building_name,
					street,
					get_json_field<std::string>(jb["DPA"], "POST_TOWN"),
					get_json_field<std::string>(jb["DPA"], "POSTCODE"),
					get_json_field<float>(jb["DPA"], "X_COORDINATE"),
					get_json_field<float>(jb["DPA"], "Y_COORDINATE"),
				});
				idxs[key] = buildings.size()-1;
			}
			std::string classification_code = 
				get_json_field<std::string>(jb["DPA"], "CLASSIFICATION_CODE");
			is_commercial = classification_code[0] == 'C';
			buildings[idxs[key]].subunits.push_back({
				get_json_fields<std::string>(jb["DPA"], std::vector<std::string>({
					"ORGANISATION_NAME", "SUB_BUILDING_NAME"
				})),
				std::move(building_name),
				classification_code,
				get_json_field<std::string>(jb["DPA"], "CLASSIFICATION_CODE_DESCRIPTION"),
				is_commercial
			});
		}
		offset += get_json_field<int>(jdata["header"], "maxresults");
	} while (offset < jdata["header"]["totalresults"]);
	for (Building &b: buildings) {
		b.set_tob();
	}
	return buildings;
}

std::string get_location_key(float x, float y) {
	x *= 100;
	y *= 100;
	return std::to_string(static_cast<int>(x)) 
		 + std::to_string(static_cast<int>(y));
}

void combine_buildings(Building &x, Building &y) {
	std::move(y.subunits.begin(), y.subunits.end(), std::back_inserter(x.subunits));
	std::move(y.valuations.begin(), y.valuations.end(), std::back_inserter(x.valuations));
	if (x.tob != y.tob) {
		x.tob = TypeOfBuilding::MIXED;
	}
	x.name = longest_common_substr(x.name, y.name);
	if (x.name.empty()) {
		x.name = "Building Shell";
	}
}

std::vector<Building> cluster_buildings(std::vector<Building> &buildings) {
	std::vector<Building> combined_buildings;
	std::unordered_map<std::string, std::vector<Building *>> clusters;
	for (Building &b: buildings) {
		clusters[get_location_key(b.x, b.y)].push_back(&b);
	}
	for (const auto &p: clusters) {
		const std::vector<Building *> &cluster = p.second;
		Building *combined_building = cluster[0];
		for (int i = 1; i != cluster.size(); i++) {
			combine_buildings(*combined_building, *cluster[i]);
		}
		combined_buildings.push_back(std::move(*combined_building));
	}
	return combined_buildings;
}

ValuationDB::QueryParam get_query_param(const Building &b) {
	return {b.name.c_str(), b.street.c_str(), b.postcode.c_str(), 
			b.tob==TypeOfBuilding::RESIDENTIAL};
}
#endif
