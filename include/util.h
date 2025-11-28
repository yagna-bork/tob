#ifndef UTIL_H
#define UTIL_H
#include <algorithm>
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <proj.h>
#include <curl/curl.h>

// TODO what if script called from somewhere else???
static const std::string CONFIG_FILE="build/config.txt";
std::unordered_map<std::string, std::string> g_config;

/*
 * Converts latitude and longitude to British National Grid coords
 * as required by the Ordnance Survey API. Returns 0 on fail,
 * 1 on success.
 */
int global_to_nat_grid(double lat, double lng, float &x, float &y) {
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

	x = round(c_out.xy.x * 100.0) / 100.0; // rounded to 2 d.p.
	y = round(c_out.xy.y * 100.0) / 100.0;
	return 1;
}

void init_config() {
	if (!g_config.empty()) {
		return;
	}
	std::string::iterator eq_sign;
	std::string line, conf_key, conf_val;
	std::ifstream confs_f(CONFIG_FILE);
	while (getline(confs_f, line)) {
		eq_sign = find(line.begin(), line.end(), '=');
		conf_key = std::string(line.begin(), eq_sign);
		conf_val = std::string(eq_sign+1, line.end());
		g_config[conf_key] = conf_val;
	}
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
	std::string *datap = (std::string *)userdata;
	copy(ptr, ptr+nmemb, back_inserter(*datap));
	return nmemb;
}

void make_get_request(CURL *handle, char *url, std::string &data) {
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(handle, CURLOPT_HTTPGET, 1);
	curl_easy_perform(handle);
}


template <class T>
T get_json_field(nlohmann::json &obj, std::string key) {
	if (obj.contains(key) && !obj[key].is_null()) {
		return obj[key].get<T>();
	}
	return T();
}

template <class T>
T get_json_fields(nlohmann::json &obj, std::vector<std::string> attempt_keys) {
	for (const std::string &key: attempt_keys) {
		if (!obj.contains(key)) {
			continue;
		}
		if (obj[key].is_null()) {
			break;
		}
		return obj[key].get<T>();
	}
	return T();
}

std::string tabs(int tablevel) {
	return std::string(tablevel, '\t');
}
#endif
