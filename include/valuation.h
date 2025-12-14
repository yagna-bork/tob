#ifndef GUARD_VALUATION_H
#define GUARD_VALUATION_H
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <utility>
#include <sqlite3.h>
#include "util.h"
#include "sqlitedb.h"

struct LineItem {
	std::string floor;
	std::string description;
	double area;
	long value;

	std::string to_string(int tablevel = 0) const {
		std::string res = tabs(tablevel);
		res += floor;
		res += ", ";
		res += description;
		res += ", ";
		res += std::to_string(area);
		res += ", ";
		res += std::to_string(value);
		return res;
	}
};

struct Parking {
	int spaces = 0;
	long value = 0;
};

struct Valuation {
	std::string building_name;
	std::string primary_description;
	std::string secondary_description;
	bool is_composite;
	long rateable_value;
	long plants_machinery_value = 0;
	std::vector<LineItem> line_items;
	Parking parking;

	std::string to_string(int tablevel = 0) const {
		std::string res;
		char buff[500];
		size_t chars_written;
		chars_written = snprintf(buff, 500, "%s%s, %s, %s, pm_value=%ld, cp_spaces=%d, cp_value=%ld", 
				 tabs(tablevel).c_str(), building_name.c_str(), primary_description.c_str(), 
				 secondary_description.c_str(), 
				 plants_machinery_value, parking.spaces, parking.value);
		copy(buff, buff+chars_written, std::back_inserter(res));
		for (const LineItem &item: line_items) {
			res += '\n';
			res += item.to_string(tablevel+1);
		}
		return res;
	}
};

class ValuationDB : public SQLiteDB {
public:
	struct QueryParam {
		const char *building_name;
		const char *street;
		const char *postcode;
		bool ignore;
	};
	typedef std::vector<Valuation> QueryResult;
private:
	typedef std::unordered_map<long, Valuation *> PkToValuationMap;

public:
	ValuationDB() : SQLiteDB() { }
	ValuationDB(const ValuationDB &other) = delete;
	ValuationDB(ValuationDB &&other) = delete;
	ValuationDB& operator=(const ValuationDB &other) = delete;
	ValuationDB& operator=(ValuationDB &&other) = delete;

	std::vector<QueryResult> get_valuations(std::vector<QueryParam> &params) {
		std::vector<QueryResult> results(params.size());
		PkToValuationMap pk_to_valuation;
		for (int i = 0; i != params.size(); i++) {
			if (!params[i].ignore) {
				get_valuations(params[i], results[i], pk_to_valuation);
			}
		}

		std::string pks_str;
		int i = 0;
		for (const std::pair<long, Valuation *> &p: pk_to_valuation) {
			if (i++ > 0) {
				pks_str += ",";
			}
			pks_str += std::to_string(p.first);
		};

		get_line_items(pks_str, pk_to_valuation);
		get_plants_machinery_value(pks_str, pk_to_valuation);
		get_car_parking(pks_str, pk_to_valuation);
		return results;
	}
private:
	void get_valuations(const QueryParam &param, QueryResult &result, PkToValuationMap &pk_to_valuation) {
		long primary_key;
		const char *primary_desc_col, *secondary_desc_col, *composite_col, *building_name_col;
		int query_sz = make_valuations_select(param);
		sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
		if (stmt == nullptr) {
			std::cerr << "Failed to prepare valuations statement" << std::endl;
			return;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			result.push_back({});
			Valuation &valuation = result.back();
			primary_key = sqlite3_column_int64(stmt, 0);
			primary_desc_col = (const char *)sqlite3_column_text(stmt, 1);
			secondary_desc_col = (const char *)sqlite3_column_text(stmt, 2);
			composite_col = (const char *)sqlite3_column_text(stmt, 3);
			building_name_col = (const char *)sqlite3_column_text(stmt, 5);

			valuation.building_name = building_name_col;
			valuation.primary_description = primary_desc_col;
			valuation.secondary_description = secondary_desc_col;
			valuation.is_composite = strcmp(composite_col, "C")==0;
			valuation.rateable_value = sqlite3_column_int64(stmt, 4);
			pk_to_valuation[primary_key] = &valuation;
		}
		sqlite3_finalize(stmt);
	}

	int make_valuations_select(const QueryParam &param) {
		return snprintf(query_buff, query_buff_sz,
			"SELECT rle.assessment_reference, le.primary_description_text, sc.description_text, "
				  "le.composite_indicator, le.rateable_value, le.number_or_name "
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
			param.postcode, param.street, param.building_name, param.building_name, 
			param.building_name, param.building_name
		);
	}

	void get_line_items(const std::string &references, PkToValuationMap &ref_to_valuation) {
		int query_sz = make_line_items_select(references);
		sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
		if (stmt == nullptr) {
			std::cerr << "Failed to prepare line_items statement" << std::endl;
			return;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Valuation *valuation = ref_to_valuation[sqlite3_column_int64(stmt, 0)];
			valuation->line_items.push_back({
				(const char *)sqlite3_column_text(stmt, 1),
				(const char *)sqlite3_column_text(stmt, 2),
				sqlite3_column_double(stmt, 3),
				sqlite3_column_int64(stmt, 4)
			});
		}
		sqlite3_finalize(stmt);
	}

	int make_line_items_select(const std::string &references) {
		return snprintf(query_buff, query_buff_sz, 
			"SELECT assessment_reference, floor, description, area, value "
			"FROM line_items WHERE assessment_reference IN (%s) "
			"UNION "
			"SELECT assessment_reference, 'Addtional' floor, oa_description description, "
			"oa_size area, oa_value value "
			"FROM additional_items "
			"WHERE assessment_reference IN (%s); ", references.c_str(), references.c_str()
		);
	}

	void get_plants_machinery_value(const std::string &references, PkToValuationMap &ref_to_valuation) {
		int query_sz = make_plants_machinery_select(references);
		sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
		if (stmt == nullptr) {
			std::cerr << "Failed to prepare plants_and_machinery statement" << std::endl;
			return;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Valuation &valuation = *ref_to_valuation[sqlite3_column_int64(stmt, 0)];
			valuation.plants_machinery_value = sqlite3_column_int64(stmt, 1);
		}
		sqlite3_finalize(stmt);
	}

	int make_plants_machinery_select(const std::string &references) {
		return snprintf(query_buff, query_buff_sz, 
			"SELECT assessment_reference, pm_value "
			"FROM plant_and_machinery "
			"WHERE assessment_reference IN (%s); ", 
			references.c_str()
		);
	}

	void get_car_parking(const std::string &references, PkToValuationMap &ref_to_valuation) {
		int query_sz = make_car_parks_select(references);
		sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
		if (stmt == nullptr) {
			std::cerr << "Failed to prepare car_parks statement" << std::endl;
			return;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Valuation &valuation = *ref_to_valuation[sqlite3_column_int64(stmt, 0)];
			valuation.parking.spaces = sqlite3_column_int(stmt, 1);
			valuation.parking.value = sqlite3_column_int64(stmt, 2);
		}
		sqlite3_finalize(stmt);
	}

	int make_car_parks_select(const std::string &references) {
		return snprintf(query_buff, query_buff_sz, 
			"SELECT assessment_reference, cp_spaces, cp_total "
			"FROM car_parks "
			"WHERE assessment_reference IN (%s);",
			references.c_str()
		);
	}
};

// serialisation code
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LineItem, floor, description, area, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Parking, spaces, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Valuation, building_name, primary_description, 
								   secondary_description, is_composite, rateable_value, 
								   plants_machinery_value, line_items, parking)
#endif
