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

using std::cout; using std::endl; using std::cerr;
using std::string; using std::ifstream; using std::find;
using std::unordered_map; using std::copy; using std::back_inserter;

const string API_CONFIG_FILE="api_keys.txt";
const char *PLACES_RADIUS_URL = "https://api.os.uk/search/places/v1/radius";
string OS_PROJECT_API_KEY;

struct Building {
	string address;
	float x_coordinate;
	float y_coordinate;
	string classification_code;
	string classification_code_description;
};

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
	unordered_map<string, string> confs_m;
	string::iterator eq_sign;
	string line, conf_key, conf_val;
	ifstream confs_f = ifstream(API_CONFIG_FILE);
	while (getline(confs_f, line)) {
		eq_sign = find(line.begin(), line.end(), '=');
		conf_key = string(line.begin(), eq_sign);
		conf_val = string(eq_sign+1, line.end());
		confs_m[conf_key] = conf_val;
	}
	OS_PROJECT_API_KEY = std::move(confs_m["OS_PROJECT_API_KEY"]);
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
	string *datap = (string *)userdata;
	copy(ptr, ptr+nmemb, back_inserter(*datap));
	return nmemb;
}

void get_buildings(int x, int y, int radius, CURL *handle) {
	string data;
	char url[500];
	snprintf(url, 500, "%s?key=%s&point=%d,%d&radius=%d", 
			 PLACES_RADIUS_URL, OS_PROJECT_API_KEY.c_str(), x, y, radius);
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(handle, CURLOPT_HTTPGET, 1);
	curl_easy_perform(handle);
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
	get_buildings(x, y, search_radius, handle);
	curl_easy_cleanup(handle);
}

int main(int argc, char *argv[]) {
	double lat = atof(argv[1]);
	double lng = atof(argv[2]);
	get_tobs(lat, lng);
	return 0;
}
