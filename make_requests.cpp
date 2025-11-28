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
#include "include/building.h"
#include "include/valuation.h"
#include "include/longest_common_substr.h"
#include "include/planning.h"
// TODO remove most of these

using std::cout; using std::endl; using std::cerr;
using std::string; using std::ifstream; using std::find;
using std::unordered_map; using std::copy; using std::back_inserter;
using std::vector; using std::pair; using std::ostream_iterator;
using std::transform; using std::all_of; using std::remove_copy_if;
using json = nlohmann::json;

typedef unordered_map<long, Valuation> ValuationRegister;

int get_list_entries_select(Building &building, char *buff, size_t buff_size) {
	return snprintf(buff, buff_size,
	   "SELECT rle.assessment_reference, le.primary_description_text, sc.description_text, "
	   		  "le.composite_indicator, le.rateable_value, le.uarn, le.number_or_name "
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
		building.postcode.c_str(), building.street.c_str(), building.name.c_str(), 
		building.name.c_str(), building.name.c_str(), 
		building.name.c_str()
	);
}

void init_valuations(ValuationRegister &ref2val, unordered_map<long, Building *> &uarn_to_building, 
					 vector<Building> &buildings, char *stmt_str, size_t stmt_str_size, sqlite3 *db) 
{
	sqlite3_stmt *stmt;
	long ref;
	const char *primary_desc_col, *secondary_desc_col, *composite_col, *building_name_col;
	for (Building &building: buildings) {
		if (building.tob == TypeOfBuilding::RESIDENTIAL) {
			continue;
		}
		get_list_entries_select(building, stmt_str, stmt_str_size);
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
			building_name_col = (const char *)sqlite3_column_text(stmt, 6);

			val.primary_description = primary_desc_col;
			val.secondary_description = secondary_desc_col;
			val.is_composite = strcmp(composite_col, "C") == 0;
			val.rateable_value = sqlite3_column_int(stmt, 4);
			val.uarn = sqlite3_column_int64(stmt, 5);
			val.building_name = building_name_col;
			uarn_to_building[val.uarn] = &building;
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

void get_valuations(vector<Building> &buildings) {
	size_t buff_sz = 1000;
	char stmt[buff_sz];
	sqlite3 *db;
	if (sqlite3_open(g_config["DB_PATH"].c_str(), &db) != SQLITE_OK) {
		cerr << "Couldn't open database" << endl;
		return;
	}
	ValuationRegister assessment_ref2valuation;
	unordered_map<long, Building *> uarn_to_building;
	init_valuations(assessment_ref2valuation, uarn_to_building, buildings, 
					stmt, buff_sz, db);
	// no commerical buildings in current search
	if (assessment_ref2valuation.empty()) {
		sqlite3_close(db);
		return;
	}
	get_line_items(assessment_ref2valuation, stmt, buff_sz, db);
	get_plant_machinery_value(assessment_ref2valuation, stmt, buff_sz, db);
	get_parking_spaces(assessment_ref2valuation, stmt, buff_sz, db);
	Building *buildingp;
	for (const pair<long, Valuation> &p: assessment_ref2valuation) {
		const Valuation &val = p.second;
		buildingp = uarn_to_building[val.uarn];
		buildingp->valuations.push_back(std::move(val));
	}
	for (const Building &building: buildings) {
		cout << building.to_string() << endl;
		for (const Valuation &valuation: building.valuations) {
			cout << valuation.to_string() << endl;
		}
	}
	sqlite3_close(db);
}

void get_tobs(double lat, double lng, int radius) {
	float x, y;
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
	vector<Building> buildings = fetch_buildings(handle, x, y, radius);
	std::vector<PlanningApplication> applications = 
		get_planning_apps(handle, lat, lng, radius);
	get_valuations(buildings);

	// testing
	ValuationDB db;
	std::vector<ValuationDB::QueryResult> alt_valuations;
	if (db.connected()) {
		std::vector<ValuationDB::QueryParam> params;
		for (const Building &b: buildings) {
			if (b.tob != TypeOfBuilding::RESIDENTIAL) {
				params.push_back(get_query_param(b));
			}
		}
		alt_valuations = std::move(db.get_valuations(params));
		cout << "params.size = " << params.size() << " results.size = " << alt_valuations.size() << endl;
		cout << "Alt valuations = " << endl;
		for (int i = 0; i != params.size(); i++) {
			cout << params[i].building_name << ", " << params[i].street << ", " 
				 << params[i].postcode << endl;
			for (const Valuation &val: alt_valuations[i]) {
				cout << val.to_string(1) << endl;
			}
		}
	}

	vector<Building> clustered = cluster_buildings(buildings);
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
