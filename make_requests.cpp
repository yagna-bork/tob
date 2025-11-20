#include <cstdlib>
#include <cmath>
#include <proj.h>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <string>
#include <algorithm>
#include <curl/curl.h>
#include <vector>
#include <iterator>
#include <cstdio>
#include <nlohmann/json.hpp>

using std::cout; using std::endl; using std::cerr;
using std::string; using std::ifstream; using std::find;
using std::unordered_map; using std::copy; using std::back_inserter;
using std::vector;
using json = nlohmann::json;

const string API_CONFIG_FILE="api_keys.txt";
unordered_map<string, string> configs;

struct PlanningApplication {
	string description;
	string size;
	string state;
	string date_received;
	string date_validated;
	string date_decision;
	string date_decisison_issued;
	float x;
	float y;
};

struct SubUnit {
	string address;
	float x;
	float y;
	string code;
	string description;
	vector<PlanningApplication> plan_apps;
};
typedef unordered_map<string, SubUnit> AddressRegister;

/*
 * Converts latitude and longitude to British National Grid coords
 * as required by the Ordnance Survey API. Returns 0 on fail,
 * 1 on success.
 */
int global_to_nat_grid(double lat, double lng, int &x, int &y) {
	// https://stackoverflow.com/questions/31426559/c-convert-lat-long-to-bng-with-proj-4
	// https://proj.org/en/stable/development/migration.html#code-example
	PJ_COORD c, c_out;
	PJ *P = proj_create_crs_to_crs(PJ_DEFAULT_CTX, "+proj=longlat +datum=WGS84", 
			"+proj=tmerc +lat_0=49 +lon_0=-2 +k=0.9996012717 +x_0=400000 +y_0=-100000"
			" +ellps=airy +datum=OSGB36 +units=m +no_defs", NULL);
	if (P == 0)
		return 0;
	c.lpzt.z = 0.0;
	c.lpzt.t = HUGE_VAL;
	c.lpzt.lam = lng;
	c.lpzt.phi = lat;
	c_out = proj_trans(P, PJ_FWD, c);

	x = round(c_out.xy.x);
	y = round(c_out.xy.y);
	return 1;
}

void load_configs() {
	string::iterator eq_sign;
	string line, conf_key, conf_val;
	ifstream confs_f = ifstream(API_CONFIG_FILE);
	while (getline(confs_f, line)) {
		eq_sign = find(line.begin(), line.end(), '=');
		conf_key = string(line.begin(), eq_sign);
		conf_val = string(eq_sign+1, line.end());
		configs[conf_key] = conf_val;
	}
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
	string *datap = (string *)userdata;
	copy(ptr, ptr+nmemb, back_inserter(*datap));
	return nmemb;
}

AddressRegister get_subunits(int x, int y, int radius, CURL *handle) {
	string data;
	char url[500];
	AddressRegister addr2unit;
	snprintf(url, 500, "%s?key=%s&point=%d,%d&radius=%d", 
			 configs["PLACES_RADIUS_URL"].c_str(), configs["OS_PROJECT_API_KEY"].c_str(), 
			 x, y, radius);
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(handle, CURLOPT_HTTPGET, 1);

	curl_easy_perform(handle);
	json jdata = json::parse(data);
	for (json &jb: jdata["results"]) {
		addr2unit[jb["DPA"]["ADDRESS"]] = {
			std::move(jb["DPA"]["ADDRESS"]),
			jb["DPA"]["X_COORDINATE"],
			jb["DPA"]["Y_COORDINATE"],
			std::move(jb["DPA"]["CLASSIFICATION_CODE"]),
			std::move(jb["DPA"]["CLASSIFICATION_CODE_DESCRIPTION"]),
		};
	}
	return std::move(addr2unit);
}

vector<PlanningApplication> get_planning_apps(double lat, double lng, int radius) {
	float krad = radius / 1000.0;
	vector<PlanningApplication> apps;
	string data;
	char fields[] = "address,description,app_size,app_state,other_fields,start_date";
	char url[500];
	snprintf(url, 500, "%s?lat=%.9f&lng=%.9f&krad=%.3f&select=%s&sort=-start_date", 
			 configs["PLANIT_URL"].c_str(), lat, lng, krad, fields);
	cout << url << endl;
	return apps;
}

void get_tobs(double lat, double lng) {
	int x, y;
	int search_radius = 20; // in meters
	if (!global_to_nat_grid(lat, lng, x, y)) {
		cerr << "Failed to get BNG for (" 
			 << lat << ", " << lng << ")" << endl;
		exit(1);
	}
	load_configs();
	CURL *handle = curl_easy_init();
	if (!handle) {
		cerr << "Failed to setup easy curl" << endl;
		exit(1);
	}
	AddressRegister addr2unit = get_subunits(x, y, search_radius, handle);
	vector<PlanningApplication> plan_apps = get_planning_apps(lat, lng, search_radius);
	curl_easy_cleanup(handle);
}

int main(int argc, char *argv[]) {
	double lat = atof(argv[1]);
	double lng = atof(argv[2]);
	get_tobs(lat, lng);
	return 0;
}
