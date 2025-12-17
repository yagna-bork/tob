#ifndef GUARD_VALUATION_H
#define GUARD_VALUATION_H
#include "sqlitedb.h"
#include "util.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <sqlite3.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct LineItem {
  std::string floor;
  std::string description;
  double area;
  long value;

  std::string to_string(int tablevel = 0) const;
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

  std::string to_string(int tablevel = 0) const;
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
  ValuationDB() : SQLiteDB() {}
  ValuationDB(const ValuationDB &other) = delete;
  ValuationDB(ValuationDB &&other) = delete;
  ValuationDB &operator=(const ValuationDB &other) = delete;
  ValuationDB &operator=(ValuationDB &&other) = delete;

  std::vector<QueryResult> get_valuations(std::vector<QueryParam> &params);

private:
  void get_valuations(const QueryParam &param, QueryResult &result,
                      PkToValuationMap &pk_to_valuation);

  int make_valuations_select(const QueryParam &param);

  void get_line_items(const std::string &references,
                      PkToValuationMap &ref_to_valuation);

  int make_line_items_select(const std::string &references);

  void get_plants_machinery_value(const std::string &references,
                                  PkToValuationMap &ref_to_valuation);

  int make_plants_machinery_select(const std::string &references);

  void get_car_parking(const std::string &references,
                       PkToValuationMap &ref_to_valuation);

  int make_car_parks_select(const std::string &references);
};

// serialisation code
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LineItem, floor, description, area, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Parking, spaces, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Valuation, building_name,
                                   primary_description, secondary_description,
                                   is_composite, rateable_value,
                                   plants_machinery_value, line_items, parking)
#endif
