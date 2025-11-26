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
#include <utility>
#include <locale>
#include <cstddef>
#include <functional>
#include <sqlite3.h>
#include <cstring>
#include <optional>
#include <format>

using std::cout; using std::endl; using std::cerr;
using std::string; using std::ifstream; using std::find;
using std::unordered_map; using std::copy; using std::back_inserter;
using std::vector; using std::pair; using std::ostream_iterator;
using std::transform; using std::all_of; using std::remove_copy_if;
using json = nlohmann::json;

const string API_CONFIG_FILE="api_keys.txt";
unordered_map<string, string> configs;

struct PlanningApplication {
	string address;
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
// unordered_map: Address -> Appliction object
typedef unordered_map<string, vector<PlanningApplication>> ApplicationRegister;

struct LineItem {
	string floor;
	string description;
	double area;
	long value;
};

struct Parking {
	int spaces = 0;
	long value = 0;
};

struct Valuation {
	string primary_description;
	string secondary_description;
	bool is_composite;
	int rateable_value;
	long uarn;
	long plants_machinery_value = 0;
	vector<LineItem> line_items;
	Parking parking;

	std::string to_string() const {
		string res;
		char buff[500];
		size_t chars_written;
		chars_written = snprintf(buff, 500, "%s, %s, pm_value=%ld, cp_spaces=%d, cp_value=%ld", 
				 primary_description.c_str(), secondary_description.c_str(), 
				 plants_machinery_value, parking.spaces, parking.value);
		copy(buff, buff+chars_written, std::back_inserter(res));
		for (const LineItem &item: line_items) {
			chars_written = snprintf(buff, 500, "\n\t%s, %s, %.2f, %ld", 
									 item.floor.c_str(), item.description.c_str(), 
									 item.area, item.value);
			copy(buff, buff+chars_written, std::back_inserter(res));
		}
		return res;
	}
};

typedef unordered_map<long, Valuation> ValuationRegister;

struct SubUnit {
	string organisation;
	string number;
	string street;
	string town;
	string postcode;
	float x;
	float y;
	string code;
	string description;
	bool is_commercial;
	std::optional<Valuation> valuation;

	string address() const {
		string addr = !organisation.empty() ? organisation + ", " : "";
		char buff[300];
		size_t buff_size = snprintf(buff, 300, "%s, %s, %s, %s",
				 number.c_str(), street.c_str(), town.c_str(), 
				 postcode.c_str());
		copy(buff, buff+buff_size, back_inserter(addr));
		return addr;
	}
};

struct Building {
	std::vector<SubUnit> subunits;
};

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

void make_get_request(CURL *handle, char *url, string &data) {
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(handle, CURLOPT_HTTPGET, 1);
	curl_easy_perform(handle);
}

template <class T>
T get_json_field(json &obj, string key, string alt_key = "", const T &default_val = T()) {
	if (obj.contains(key)) {
		return obj[key].get<T>();
	}
	if (alt_key != "" && obj.contains(alt_key)) {
		return obj[alt_key].get<T>();
	}
	return default_val;
}

void get_os_radius_url(char *url_buff, size_t url_buff_sz, float x, float y, 
					   int radius, int offset) 
{
	snprintf(url_buff, url_buff_sz, "%s?key=%s&point=%.2f,%.2f&radius=%d&offset=%d", 
			 configs["PLACES_RADIUS_URL"].c_str(), 
			 configs["OS_PROJECT_API_KEY"].c_str(), 
			 x, y, radius, offset);
}

vector<SubUnit> get_subunits(CURL *handle, float x, float y, int radius) {
	int offset = 0;
	string data, classification_code;
	char url[500];
	vector<SubUnit> subunits;
	json jdata, header;
	bool is_commercial;
	do {
		data.clear();
		get_os_radius_url(url, 500, x, y, radius, offset);
		cout << url << endl;
		make_get_request(handle, url, data);
		jdata = json::parse(data);

		for (json &jb: jdata["results"]) {
			classification_code = std::move(jb["DPA"]["CLASSIFICATION_CODE"]);
			is_commercial = classification_code[0] == 'C';
			subunits.push_back({
				get_json_field<string>(jb["DPA"], "ORGANISATION_NAME"),
				get_json_field<string>(jb["DPA"], "BUILDING_NUMBER", 
					/*alt_key=*/"BUILDING_NAME",
					/*default_val=*/get_json_field<string>(jb["DPA"], "DEPENDENT_THOROUGHFARE_NAME")),
				std::move(jb["DPA"]["THOROUGHFARE_NAME"]),
				std::move(jb["DPA"]["POST_TOWN"]),
				std::move(jb["DPA"]["POSTCODE"]),
				jb["DPA"]["X_COORDINATE"],
				jb["DPA"]["Y_COORDINATE"],
				classification_code,
				std::move(jb["DPA"]["CLASSIFICATION_CODE_DESCRIPTION"]),
				is_commercial
			});
		}
		header = jdata["header"];
		offset += header["maxresults"].get<int>();
	} while (offset < header["totalresults"]);
	return subunits;
}

ApplicationRegister get_planning_apps(CURL *handle, double lat, double lng, int radius) {
	float krad = radius / 1000.0;
	ApplicationRegister addr2apps;
	string data;
	char fields[] = "address,description,app_size,app_state,other_fields,start_date,location_x,location_y";
	char url[500];
	snprintf(url, 500, "%s?lat=%.9f&lng=%.9f&krad=%.3f&select=%s&sort=-start_date", 
			 configs["PLANIT_URL"].c_str(), lat, lng, krad, fields);
	cout << url << endl;
	make_get_request(handle, url, data);
	json jdata = json::parse(data);

	double app_lat, app_lng;
	float x, y;
	for (json &app: jdata["records"]) {
		if (app["other_fields"].contains("northing") &&
			app["other_fields"].contains("easting")) 
		{
			x = app["other_fields"]["easting"];
			y = app["other_fields"]["northing"];
		} else {
			app_lat = app["location_y"];
			app_lng = app["location_x"];
			if(!global_to_nat_grid(app_lat, app_lng, x, y)) {
				cerr << "Failed to get BNG for (" 
					 << lat << ", " << lng << ")" << endl;
				continue;
			}
		}
		addr2apps[app["address"]].push_back({
			std::move(app["address"]),
			std::move(app["description"]),
			get_json_field<string>(app, "app_size"),
			get_json_field<string>(app, "app_state"),
			get_json_field<string>(app["other_fields"], "date_received"),
			get_json_field<string>(app["other_fields"], "date_validated"),
			get_json_field<string>(app["other_fields"], "decision_date"),
			get_json_field<string>(app["other_fields"], "decision_issued_date"),
			x, y
		});
	}
	return std::move(addr2apps);
}

int get_list_entries_select(SubUnit &subunit, char *buff, size_t buff_size) {
	return snprintf(buff, buff_size,
	   "SELECT rle.assessment_reference, le.primary_description_text, sc.description_text, "
	   		  "le.composite_indicator, le.rateable_value, le.uarn "
		"FROM list_entries le "
		  "INNER JOIN related_list_entries rle "
		  "ON le.uarn = rle.uarn AND rle.from_date = ( "
			"SELECT MAX(from_date) FROM related_list_entries WHERE uarn = le.uarn) "
		  "INNER JOIN scat_codes sc "
		  "ON le.scat_code_and_suffix = sc.scat_code_and_suffix "
		"WHERE le.postcode = '%s' AND le.street = '%s' AND ( "
		  "le.number_or_name = '%s' OR le.number_or_name LIKE '%% %s'  "
		  "OR le.number_or_name LIKE '%s %%' OR le.number_or_name LIKE '%% %s %%' "
		"); ", 
		subunit.postcode.c_str(), subunit.street.c_str(), subunit.number.c_str(), 
		subunit.number.c_str(), subunit.number.c_str(), 
		subunit.number.c_str()
	);
}

void init_valuations(ValuationRegister &ref2val, unordered_map<long, SubUnit *> &uarn2unit, 
					 vector<SubUnit> &subunits, char *stmt_str, size_t stmt_str_size, sqlite3 *db) 
{
	sqlite3_stmt *stmt;
	long ref;
	const char *primary_desc_col, *secondary_desc_col, *composite_col;
	for (SubUnit &unit: subunits) {
		if (!unit.is_commercial) {
			continue;
		}
		get_list_entries_select(unit, stmt_str, stmt_str_size);
		sqlite3_prepare_v2(db, stmt_str, stmt_str_size, &stmt, NULL);
		if (stmt == nullptr) {
			cerr << "Failed to prepare list entries select statement" << endl;
			continue;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			ref = sqlite3_column_int64(stmt, 0);
			Valuation &val = ref2val[ref];
			primary_desc_col = (const char *)sqlite3_column_text(stmt, 1);
			secondary_desc_col = (const char *)sqlite3_column_text(stmt, 2);
			composite_col = (const char *)sqlite3_column_text(stmt, 3);

			val.primary_description = primary_desc_col;
			val.secondary_description = secondary_desc_col;
			val.is_composite = strcmp(composite_col, "C") == 0;
			val.rateable_value = sqlite3_column_int(stmt, 4);
			val.uarn = sqlite3_column_int64(stmt, 5);
			uarn2unit[val.uarn] = &unit;
		}
		sqlite3_finalize(stmt);
	}
}

string get_assessment_references_str(ValuationRegister &ref2val) {
	string res;
	int i = 0;
	for (const pair<long, Valuation> &p: ref2val) {
		if (i++ > 0) {
			res += ",";
		}
		res += std::to_string(p.first);
	}
	return res;
}

int get_line_items_select(ValuationRegister &ref2val, char *buff, size_t buff_size) {
	string in_list = get_assessment_references_str(ref2val);
	return snprintf(buff, buff_size, 
		"SELECT assessment_reference, floor, description, area, value "
		"FROM line_items WHERE assessment_reference IN (%s) "
		"UNION "
		"SELECT assessment_reference, 'Addtional' floor, oa_description description, "
        "oa_size area, oa_value value "
		"FROM additional_items "
		"WHERE assessment_reference IN (%s); ", in_list.c_str(), in_list.c_str()
	);
}

void get_line_items(ValuationRegister &ref2val, char *stmt_buff, size_t buff_sz,
				    sqlite3 *db) 
{
	sqlite3_stmt *stmt;
	int stmt_sz = get_line_items_select(ref2val, stmt_buff, buff_sz);
	sqlite3_prepare_v2(db, stmt_buff, stmt_sz, &stmt, NULL);
	long ref;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ref = sqlite3_column_int64(stmt, 0);
		Valuation &valuation = ref2val[ref];
		valuation.line_items.push_back({
			(const char *)sqlite3_column_text(stmt, 1),
			(const char *)sqlite3_column_text(stmt, 2),
			sqlite3_column_double(stmt, 3),
			sqlite3_column_int64(stmt, 4)
		});
	}
	sqlite3_finalize(stmt);
}

int get_plant_machinery_select(ValuationRegister &ref2val, char *buff, size_t buff_size) {
	string in_list = get_assessment_references_str(ref2val);
	return snprintf(buff, buff_size, 
			 "SELECT assessment_reference, pm_value "
			 "FROM plant_and_machinery "
			 "WHERE assessment_reference IN (%s); ", 
			 in_list.c_str());
}

// TODO make class for all of these queries with same method param list
void get_plant_machinery_value(ValuationRegister &ref2val, char *stmt_buff, 
							   size_t buff_sz, sqlite3 *db) 
{
	sqlite3_stmt *stmt;
	size_t stmt_buff_sz = get_plant_machinery_select(ref2val, stmt_buff, buff_sz);
	sqlite3_prepare_v2(db, stmt_buff, stmt_buff_sz, &stmt, NULL);
	long ref;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ref = sqlite3_column_int64(stmt, 0);
		ref2val[ref].plants_machinery_value = sqlite3_column_int64(stmt, 1);
	}
	sqlite3_finalize(stmt);
}

int get_car_parks_select(ValuationRegister &ref2val, char *buff, size_t buff_size) {
	string in_list = get_assessment_references_str(ref2val);
	return snprintf(buff, buff_size, 
			 "SELECT assessment_reference, cp_spaces, cp_total "
			 "FROM car_parks "
			 "WHERE assessment_reference IN (%s);",
			 in_list.c_str());
}

void get_parking_spaces(ValuationRegister &ref2val, char *stmt_buff, 
					    size_t buff_sz, sqlite3 *db) 
{
	sqlite3_stmt *stmt;
	size_t stmt_buff_sz = get_car_parks_select(ref2val, stmt_buff, buff_sz);
	cout << stmt_buff << endl;
	sqlite3_prepare_v2(db, stmt_buff, stmt_buff_sz, &stmt, NULL);
	long ref;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ref = sqlite3_column_int64(stmt, 0);
		ref2val[ref].parking.spaces = sqlite3_column_int(stmt, 1);
		ref2val[ref].parking.value = sqlite3_column_int64(stmt, 2);
	}
	sqlite3_finalize(stmt);
}

// TODO return type
void get_valuations(vector<SubUnit> &subunits) {
	size_t buff_sz = 1000;
	char stmt[buff_sz];
	sqlite3 *db;
	if (sqlite3_open(configs["DB_PATH"].c_str(), &db) != SQLITE_OK) {
		cerr << "Couldn't open database" << endl;
		return;
	}
	ValuationRegister assessment_ref2valuation;
	unordered_map<long, SubUnit *> uarn2subunit;
	init_valuations(assessment_ref2valuation, uarn2subunit, subunits, 
					stmt, buff_sz, db);
	// no commerical buildings in current search
	if (assessment_ref2valuation.empty()) {
		sqlite3_close(db);
		return;
	}
	get_line_items(assessment_ref2valuation, stmt, buff_sz, db);
	get_plant_machinery_value(assessment_ref2valuation, stmt, buff_sz, db);
	get_parking_spaces(assessment_ref2valuation, stmt, buff_sz, db);
	SubUnit *unitp;
	for (const pair<long, Valuation> &p: assessment_ref2valuation) {
		const Valuation &val = p.second;
		unitp = uarn2subunit[val.uarn];
		unitp->valuation = std::move(val);
	}
	for (const SubUnit &unit: subunits) {
		cout << unit.address();
		if (unit.valuation) {
			cout << ", " << unit.valuation->to_string();
		}
		cout << endl;
	}
	sqlite3_close(db);
}

vector<Building> get_buildings(vector<SubUnit> &subunit) {
	vector<Building> buildings;
	return buildings;
}

void get_tobs(double lat, double lng, int radius) {
	float x, y;
	load_configs();
	if (!global_to_nat_grid(lat, lng, x, y)) {
		cerr << "Failed to get BNG for (" 
			 << lat << ", " << lng << ")" << endl;
		exit(1);
	}
	CURL *handle = curl_easy_init();
	if (!handle) {
		cerr << "Failed to setup easy curl" << endl;
		exit(1);
	}
	vector<SubUnit> subunits = get_subunits(handle, x, y, radius);
	ApplicationRegister plan_apps = get_planning_apps(handle, lat, lng, radius);
	ostream_iterator<string> out(cout, "\n");
	cout << "Sub units" << endl;
	transform(subunits.begin(), subunits.end(), out, [](const SubUnit &su) {
		return su.address() + " X, Y = " + std::to_string(su.x) + "," + std::to_string(su.y);
	});
	/*cout << endl << endl << endl << endl;
	cout << "Planning apps" << endl;
	for (auto &p: plan_apps) {
		cout << p.first << ":" << endl;
		cout << string(p.first.size()+1, '-') << endl;
		for (PlanningApplication &app: p.second) {
			cout << app.description << endl;
		}
		cout << endl;
	}*/
	get_valuations(subunits);
	get_buildings(subunits);
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
