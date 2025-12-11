#ifndef GUARD_BUILDING_SHAPE
#define GUARD_BUILDING_SHAPE
#include <cstring>
#include <cstddef>
#include <cstdio>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <unordered_set>
#include <utility>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
#include <iostream>
#include <ios>
#include <filesystem>
#include <curl/curl.h>
#include "vector_tile.pb.h"
#include "util.h"
#include "sqlitedb.h"

using namespace vector_tile;
using BuildingShape = BuildingShapes::BuildingShape;

enum CommandType {
	MOVE = 0,
	LINE = 1,
	CLOSE = 2
};

// Stores cell space coords
struct Point {
	int x;
	int y;

	std::string to_string() const {
		std::string res = "(";
		res += std::to_string(x);
		res += ",";
		res += std::to_string(y);
		res += ")";
		return res;
	}
};

// Stores BNG, grid and tile space coords
struct FPoint {
	float x;
	float y;

	std::string to_string() const {
		std::string res = "(";
		res += std::to_string(x);
		res += ",";
		res += std::to_string(y);
		res += ")";
		return res;
	}
};

struct PairHash {
	template <class T, class U>
	size_t operator()(const std::pair<T, U> &p) const {
		return std::hash<T>()(p.first) ^ std::hash<U>()(p.second);
	}
};

struct PairEq {
	template <class X, class Y>
	bool operator()(const std::pair<X, Y> &p1, const std::pair<X, Y> &p2) const 
	{
		return p1.first == p2.first && p1.second == p2.second;
	}
};

typedef std::pair<int, int> GridPos;
typedef std::unordered_set<GridPos, PairHash, PairEq> GridPosSet;

/* 
 * class to convert between the BNG and
 * cell coordinate spaces
 */
class CoordConverter {
public:
	/*
	 * `FPoint centre` represents the centre 
	 * of the search that yielded the coordinates
	 * you wish to translate.
	 * Technically any centre would yield correct results
	 * but a true centre keeps cell space coords small 
	 * and is more space efficient.
	 */
	CoordConverter(const FPoint &centre) {
		init_config();
		grid_origin.x = std::stof(g_config["NGD_TILES_API_BNG_ORIGIN_X"]);
		grid_origin.y = std::stof(g_config["NGD_TILES_API_BNG_ORIGIN_Y"]);
		cell_size = std::stof(g_config["NGD_TILES_API_CELL_SIZE"]);
		// grid is always 512x512
		tile_col = static_cast<int>((centre.x - grid_origin.x) / (512 * cell_size)); 
		tile_row = static_cast<int>((grid_origin.y - centre.y) / (512 * cell_size)); 
		tile_origin.x = tile_col * 512 * cell_size;
		tile_origin.y = tile_row * 512 * cell_size;
	}

	void bng_to_cell(const FPoint &bng, Point &res) const {
		float tile_space_x = bng.x - grid_origin.x - tile_origin.x;
		float tile_space_y = grid_origin.y - bng.y - tile_origin.y;
		res.x = static_cast<int>(tile_space_x / cell_size);
		res.y = static_cast<int>(tile_space_y / cell_size);
	}

	void cell_to_bng(const Point &cell_coord, FPoint &res) const {
		float grid_space_x = cell_coord.x * cell_size + tile_origin.x;
		float grid_space_y = cell_coord.y * cell_size + tile_origin.y;
		res.x = grid_space_x + grid_origin.x;
		res.y = grid_origin.y - grid_space_y;
	}

	GridPos get_tile_row_col(const Point &cell_coord) const {
		return {
			tile_row + floorf(cell_coord.y / 512.0f),
			tile_col + floorf(cell_coord.x / 512.0f)
		};
	}

	int get_centre_row() const {
		return tile_row;
	}

	int get_centre_col() const {
		return tile_col;
	}
private:
	FPoint grid_origin;
	int tile_row;
	int tile_col;
	FPoint tile_origin;
	float cell_size;
};

class BuildingShapesDB : public SQLiteDB {
public:
	BuildingShapesDB() : SQLiteDB() { }
	BuildingShapesDB(const BuildingShapesDB &other) = delete;
	BuildingShapesDB(BuildingShapesDB &other) = delete;
	BuildingShapesDB& operator=(const BuildingShapesDB &other) = delete;
	BuildingShapesDB& operator=(BuildingShapesDB &&other) = delete;

	std::vector<GridPos> missing_building_shapes(const std::vector<GridPos> &positions) {
		std::vector<GridPos> res;
		int query_sz = make_missing_shapes_select(positions);
		sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
		if (stmt == nullptr) {
			std::cerr << "Failed to prepare missing building shapes statement" 
					  << std::endl;
			return res;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			res.push_back({
				/*row*/sqlite3_column_int(stmt, 0),
				/*col*/sqlite3_column_int(stmt, 1)
			});
		}
		sqlite3_finalize(stmt);
		return res;
	}

	void insert(const GridPos &pos, std::string &data) {
		int query_sz = make_insert(pos);
		sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
		if (stmt == nullptr) {
			std::cerr << "Failed to prepare building_shapes insert" << std::endl;
			return;
		}
		sqlite3_bind_blob(stmt, /*idx*/1, data.data(), data.size(), SQLITE_STATIC);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	BuildingShapes building_shapes(const GridPos &pos) {
		BuildingShapes res;
		int query_sz = make_building_shapes_select(pos);
		sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
		if (stmt == nullptr) {
			std::cerr << "Failed to prepare building shapes select" << std::endl;
			return res;
		}
		if (sqlite3_step(stmt) != SQLITE_ROW) {
			std::cerr << "Failed to execute building shapes select" << std::endl;
		} else {
			const char *data = static_cast<const char *>(sqlite3_column_blob(stmt, 0));
			res.ParseFromString(data);
		}
		sqlite3_finalize(stmt);
		return res;
	}
private:
	int make_missing_shapes_select(const std::vector<GridPos> &positions) {
		std::string inputs_tbl;
		for (int i = 0; i != positions.size(); i++) {
			if (i > 0) {
				inputs_tbl += " UNION";
			}
			inputs_tbl += " SELECT ";
			inputs_tbl += std::to_string(positions[i].first);
			inputs_tbl += " grid_row, ";
			inputs_tbl += std::to_string(positions[i].second);
			inputs_tbl += " grid_col";
		}
		return snprintf(query_buff, query_buff_sz, 
			"SELECT inp.grid_row, inp.grid_col "
			"FROM (%s) inp"
			"  LEFT JOIN building_shapes bs "
			"  ON inp.grid_row = bs.grid_row AND inp.grid_col = bs.grid_col "
			"WHERE bs.grid_row IS NULL AND bs.grid_col IS NULL; ",
			inputs_tbl.c_str()
		);
	}

	int make_insert(const GridPos &pos) {
		return snprintf(query_buff, query_buff_sz, 
			"INSERT INTO building_shapes VALUES(%d, %d, ?);",
			pos.first, pos.second);
	}

	int make_building_shapes_select(const GridPos &pos) {
		return snprintf(query_buff, query_buff_sz, 
			"SELECT data "
			"FROM building_shapes "
			"WHERE grid_row = %d AND grid_col = %d;",
			pos.first, pos.second);
	}
};

bool is_building_shape_valid(const BuildingShape &b) {
	return b.approx_centre_size() == 2;
}

std::string edges_to_string(const BuildingShape &building) {
	if (!is_building_shape_valid(building)) {
		return "";
	}
	std::string res;
	int edges_sz = building.edges_size();
	Point beg, end;
	for (int i = 0; i != edges_sz; i += 4) {
		if (i != 0) {
			res += ", ";
		}
		res += "(";
		res += std::to_string(building.edges(i));
		res += ",";
		res += std::to_string(building.edges(i+1));
		res += ") -> (";
		res += std::to_string(building.edges(i+2));
		res += ",";
		res += std::to_string(building.edges(i+3));
		res += ")";
	}
	return res;
}

int decode_param(unsigned int param) {
	// see 4.3.2 of the spec
	return (param >> 1) ^ (-(param & 1));
}

bool decode_command(unsigned int cmd, CommandType &type, unsigned int &count) {
	unsigned int type_val = cmd & 0x7; // least significant 3 digits represent type
	switch (type_val) {
		case 1:
			type = CommandType::MOVE;
			break;
		case 2:
			type = CommandType::LINE;
			break;
		case 7:
			type = CommandType::CLOSE;
			break;
		default:
			return false;
	}
	count = cmd >> 3; // rest are the param count
	return true;
}

bool decode_feature(const Tile_Feature &feat, BuildingShape &res) {
	int geometry_sz = feat.geometry_size();
	int i = 0;
	unsigned int cmd, count;
	CommandType type;
	Point cursor = {0, 0};
	Point start = {INT_MAX, INT_MAX};
	int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;

	while (i < geometry_sz) {
		cmd = feat.geometry(i++);
		if(!decode_command(cmd, type, count)) {
			res.Clear();
			return false;
		}
		for (; count > 0; count--) {
			if (type == CommandType::MOVE) {
				cursor.x += decode_param(feat.geometry(i));
				cursor.y += decode_param(feat.geometry(i+1));
				i += 2;
			} else if (type == CommandType::LINE) {
				if (start.x == INT_MAX && start.y == INT_MAX) {
					start = cursor;
				}
				res.add_edges(cursor.x);
				res.add_edges(cursor.y);
				cursor.x += decode_param(feat.geometry(i));
				cursor.y += decode_param(feat.geometry(i+1));
				res.add_edges(cursor.x);
				res.add_edges(cursor.y);
				min_x = std::min(min_x, cursor.x);
				min_y = std::min(min_y, cursor.y);
				max_x = std::max(max_x, cursor.x);
				max_y = std::max(max_y, cursor.y);
				i += 2;
			} else {
				res.add_edges(cursor.x);
				res.add_edges(cursor.y);
				res.add_edges(start.x);
				res.add_edges(start.y);
			}
		}
	}
	min_x = std::min(min_x, start.x);
	min_y = std::min(min_y, start.y);
	max_x = std::max(max_x, start.x);
	max_y = std::max(max_y, start.y);
	res.add_approx_centre((min_x + max_x) / 2);
	res.add_approx_centre((min_y + max_y) / 2);
	return true;
}

BuildingShapes parse_building_shapes(std::string &tile_data) 
{
	BuildingShapes res;
	Tile tile;
	tile.ParseFromString(tile_data);
	Tile_Layer *buildings_layer_p;
	for (Tile_Layer &layer: *tile.mutable_layers()) {
		if (layer.name() == "bld_fts_buildingpart") {
			buildings_layer_p = &layer;
		}
	}
	if (!buildings_layer_p) {
		return res;
	}
	const Tile_Layer& buildings_layer = *buildings_layer_p;

	BuildingShape *buildingp = res.add_building_shapes();
	for (const Tile_Feature &feat: buildings_layer.features()) {
		if (decode_feature(feat, *buildingp)) {
			buildingp = res.add_building_shapes();
		}
	}
	res.mutable_building_shapes()->RemoveLast();
	return res;
}

int get_tiles_api_url(char url_buff[], size_t buff_sz, int grid_row, int grid_col) {
	init_config();
	return snprintf(url_buff, buff_sz, "%s/%d/%d?key=%s", 
					g_config["TILES_API_URL"].c_str(), grid_row, grid_col, 
					g_config["OS_PROJECT_API_KEY"].c_str());
}

void fetch_missing_buildings(CURL *handle, char url_buff[], size_t buff_sz,
							 BuildingShapesDB &db,
							 std::vector<GridPos> &missing) 
{
	std::string tile_data;
	std::string building_shapes_data;
	for (const GridPos &pos: missing) {
		tile_data.clear();
		building_shapes_data.clear();
		get_tiles_api_url(url_buff, buff_sz, pos.first, pos.second);
		make_get_request(handle, url_buff, tile_data);
		BuildingShapes bs = parse_building_shapes(tile_data);
		bs.SerializeToString(&building_shapes_data);
		db.insert(pos, building_shapes_data);
	}
}

BuildingShapes get_building_shapes(CURL *handle, const std::vector<GridPos> &positions, 
								   int centre_row, int centre_col) 
{
	BuildingShapes res;
	BuildingShape *added;
	char url[500];
	BuildingShapesDB db;
	int x_shift, y_shift;

	std::vector<GridPos> missing = db.missing_building_shapes(positions);
	fetch_missing_buildings(handle, url, 500, db, missing);
	for (const GridPos &pos: positions) {
		BuildingShapes buildings = db.building_shapes(pos);
		y_shift = (pos.first - centre_row) * 512;
		x_shift = (pos.second - centre_col) * 512;
		for (const BuildingShape &building: buildings.building_shapes()) {
			added = res.add_building_shapes();
			added->add_approx_centre(building.approx_centre(0) + x_shift);
			added->add_approx_centre(building.approx_centre(1) + y_shift);
			for (int i = 0; i+1 < building.edges_size(); i += 2) {
				added->add_edges(building.edges(i) + x_shift);
				added->add_edges(building.edges(i+1) + y_shift);
			}
		}
	}
	return res;
}

/*
 * Determine if Point p is in a BuildingShape b by checking if 
 * a ray projected straight up from p penetrates an odd number of
 * edges of b
 * https://en.wikipedia.org/wiki/Point_in_polygon#Ray_casting_algorithm
 */
// TODO?: make into conversion table class?
void translate_point_to_building_centre(Point &p, const BuildingShapes &buildings) {
	int x1, y1, x2, y2, edges_passed_thru;
	float gradient, contact_y;
	for (const BuildingShape &building: buildings.building_shapes()) {
		edges_passed_thru = 0;
		for (int i = 0; i != building.edges_size(); i += 4) {
			x1 = building.edges(i);
			y1 = building.edges(i+1);
			x2 = building.edges(i+2);
			y2 = building.edges(i+3);
			if (std::min(x1, x2) > p.x || std::max(x1, x2) < p.x) {
				continue;
			}
			gradient = (y2-y1) / (x2-x1*1.0f);
			contact_y = (p.x - x1)*gradient + y1;
			if (contact_y < p.y) {
				continue;
			}
			edges_passed_thru++;
		}
		if (edges_passed_thru % 2 == 1) {
			p.x = building.approx_centre(0);
			p.y = building.approx_centre(1);
		}
	}
}
#endif
