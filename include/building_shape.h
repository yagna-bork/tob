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
using BuildingShape = Tile_BuildingShape;

enum CommandType {
	MOVE = 0,
	LINE = 1,
	CLOSE = 2
};

enum EnclosureType {
	INSIDE = 0,
	EDGE = 1,
	OUTSIDE = 2
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

struct PointHash {
	size_t operator()(const Point &p) const {
		return std::hash<int>()(p.x) ^ std::hash<int>()(p.y);
	}
};

struct PointEq {
	bool operator()(const Point &p1, const Point &p2) const 
	{
		return p1.x == p2.x && p1.y == p2.y;
	}
};

typedef std::pair<int, int> GridPos;
typedef std::unordered_set<GridPos, PairHash, PairEq> GridPosSet;
typedef std::unordered_map<
	Point, std::vector<std::pair<Point, unsigned long long>>, PointHash, PointEq
> ShapeGraph;

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

	std::vector<GridPos> missing_tiles(const std::vector<GridPos> &positions) {
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
			std::cerr << "Failed to prepare tiles_grid insert" << std::endl;
			return;
		}
		sqlite3_bind_blob(stmt, /*idx*/1, data.data(), data.size(), SQLITE_STATIC);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	Tile tile(const GridPos &pos) {
		Tile res;
		int query_sz = make_tiles_grid_select(pos);
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
			"FROM (%s) inp "
			"  LEFT JOIN tiles_grid gd "
			"  ON inp.grid_row = gd.row AND inp.grid_col = gd.col "
			"WHERE gd.row IS NULL AND gd.col IS NULL; ",
			inputs_tbl.c_str()
		);
	}

	int make_insert(const GridPos &pos) {
		return snprintf(query_buff, query_buff_sz, 
			"INSERT INTO tiles_grid VALUES(%d, %d, ?);",
			pos.first, pos.second);
	}

	int make_tiles_grid_select(const GridPos &pos) {
		return snprintf(query_buff, query_buff_sz, 
			"SELECT tile "
			"FROM tiles_grid "
			"WHERE row = %d AND col = %d;",
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

std::string value_to_string(const FullTile_Value &val) {
	if (val.has_string_value()) {
		return val.string_value();
	} else if (val.has_float_value()) {
		return std::to_string(val.float_value());
	} else if (val.has_double_value()) {
		return std::to_string(val.double_value());
	} else if (val.has_int_value()) {
		return std::to_string(val.int_value());
	} else if (val.has_uint_value()) {
		return std::to_string(val.has_uint_value());
	} else if (val.has_sint_value()) {
		return std::to_string(val.sint_value());
	} else {
		return std::to_string(val.bool_value());
	}
}

bool decode_feature(const FullTile_Feature &feat, const FullTile_Layer &layer, BuildingShape &res) {
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
			std::cerr << "Failed to decode command: " << cmd << std::endl;
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

	for (int i = 0; i+1 < feat.tags_size(); i += 2) {
		if (layer.keys(feat.tags(i)) == "osid") {
			FullTile_Value val = layer.values(feat.tags(i+1));
			res.set_osid(value_to_string(val));
		}
	}
	if (!res.has_osid()) {
		std::cerr << "Couldn't find osid" << std::endl;
		res.Clear();
		return false;
	}
	return true;
}

/*
 * Only for debugging. Ignore.
 */
void print_tags(const FullTile_Feature &feat, const FullTile_Layer &layer) {
	for (int i = 0; i != feat.tags_size(); i += 2) {
		std::cout << layer.keys(feat.tags(i)) << ": "
				  << value_to_string(layer.values(feat.tags(i+1))) 
				  << std::endl;
	}
}

Tile parse_tile(std::string &tile_data) 
{
	Tile res;
	FullTile ftile;
	ftile.ParseFromString(tile_data);
	auto it = std::find_if(ftile.layers().begin(), ftile.layers().end(), 
		[](const FullTile_Layer &layer) {
			return layer.name() == "bld_fts_buildingpart";
		});
	if (it == ftile.layers().end()) {
		std::cerr << "Couldn't find bld_fts_buildingpart layer" << std::endl;
		return res;
	}
	const FullTile_Layer& buildings_layer = *it;

	BuildingShape *buildingp = res.add_shapes();
	for (const FullTile_Feature &feat: buildings_layer.features()) {
		if (decode_feature(feat, buildings_layer, *buildingp)) {
			buildingp = res.add_shapes();
		}
	}
	res.mutable_shapes()->RemoveLast();
	return res;
}

int get_tiles_api_url(char url_buff[], size_t buff_sz, int grid_row, int grid_col) {
	init_config();
	return snprintf(url_buff, buff_sz, "%s/%d/%d?key=%s", 
					g_config["TILES_API_URL"].c_str(), grid_row, grid_col, 
					g_config["OS_PROJECT_API_KEY"].c_str());
}

void fetch_missing_tiles(CURL *handle, char url_buff[], size_t buff_sz,
							 BuildingShapesDB &db,
							 std::vector<GridPos> &missing) 
{
	std::string full_tile_data; // full tile data directly from api
	std::string tile_data; // tile data filtered with only the info we need
	for (const GridPos &pos: missing) {
		full_tile_data.clear();
		tile_data.clear();
		get_tiles_api_url(url_buff, buff_sz, pos.first, pos.second);
		make_get_request(handle, url_buff, full_tile_data);
		Tile tile = parse_tile(full_tile_data);
		tile.SerializeToString(&tile_data);
		db.insert(pos, tile_data);
	}
}

/*
 * TODO write me
 */
void combine_building_shapes(const std::vector<int>& idxs, const Tile &tile, BuildingShape &res) {
	// TODO delete debugging
	std::filesystem::path output_dir = std::filesystem::relative("tiles/extra");
	std::filesystem::path output_path;
	if (tile.shapes(idxs[0]).osid() == "7162adaf-b4d8-44de-82fd-c79830dc6a5d") {
		for (int idx: idxs) {
			Tile tile_container;
			*tile_container.add_shapes() = tile.shapes(idx);
			output_path = output_dir / (tile.shapes(idx).osid()+"-"+std::to_string(idx)+".bin");
			std::cout << "Writing to " << output_path << std::endl;
			std::ofstream output(output_path, std::ios::out | std::ios::binary);
			tile_container.SerializeToOstream(&output);
		}
	}

	// build graph
	ShapeGraph graph;
	Point from, to;
	int dx, dy;
	unsigned long long square_dist;

	for (int idx: idxs) {
		const BuildingShape &shape = tile.shapes(idx);
		for (int i = 0; i != shape.edges_size(); i += 4) {
			from = {shape.edges(i), shape.edges(i+1)};
			to = {shape.edges(i+2), shape.edges(i+3)};
			dx = to.x-from.x;
			dy = to.y-from.y;
			square_dist = dx*dx + dy*dy;
			graph[from].push_back({to, square_dist});
			graph[to].push_back({from, square_dist});
		}
	}
	// TODO delete
	for (const auto &pr1: graph) {
		std::cout << pr1.first.to_string() << ": ";
		for (auto &pr2: pr1.second) {
			std::cout << "{" <<pr2.first.to_string() << ", " << pr2.second << "}, ";
		}
		std::cout << std::endl;
	}

	// first cycle to find valid start
	std::unordered_set<Point, PointHash, PointEq> visited;
	Point prev = {INT_MAX, INT_MAX};
	Point curr = {tile.shapes(idxs[0]).edges(0), tile.shapes(idxs[0]).edges(1)};
	unsigned long long max_dist = 0;
	Point next;
	std::cout << "First cycle" << std::endl;
	while (!visited.count(curr)) {
		std::cout << curr.to_string() << " -> ";
		visited.insert(curr);
		for (const auto &gph_edge: graph[curr]) {
			if (PointEq{}(gph_edge.first, prev)) {
				continue; // don't backtrack
			}
			if (gph_edge.second > max_dist) {
				next = gph_edge.first;
				max_dist = gph_edge.second;
			}
		}
		std::swap(curr, next);
		std::swap(next, prev);
		max_dist = 0;
	}
	std::cout << curr.to_string() << " -> ";
	std::cout << std::endl;

	std::cout << "Second cycle" << std::endl;
	// save second cycle as combined shape
	visited.clear();
	prev = {INT_MAX, INT_MAX};
	// curr stays as end of first cycle
	max_dist = 0;
	int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
	while (!visited.count(curr)) {
		std::cout << curr.to_string() << " -> ";
		visited.insert(curr);
		for (const auto &gph_edge: graph[curr]) {
			if (PointEq{}(gph_edge.first, prev)) { // don't backtrack
				continue; 
			}
			if (gph_edge.second > max_dist) {
				next = gph_edge.first;
				max_dist = gph_edge.second;
			}
		}
		min_x = std::min(min_x, curr.x);
		min_y = std::min(min_y, curr.y);
		max_x = std::max(max_x, curr.x);
		max_y = std::max(max_y, curr.y);
		std::swap(curr, next);
		std::swap(next, prev);
		res.add_edges(prev.x);
		res.add_edges(prev.y);
		res.add_edges(curr.x);
		res.add_edges(curr.y);
		max_dist = 0;
	}
	std::cout << curr.to_string() << " -> ";
	std::cout << std::endl;
	res.add_approx_centre((min_x + max_x) / 2);
	res.add_approx_centre((min_y + max_y) / 2);
	res.set_osid(tile.shapes(idxs[0]).osid());
	std::cout << "approx_centre = " << res.approx_centre(0) << "," << res.approx_centre(1) << std::endl;
	std::cout << "osid = " << res.osid() << std::endl;

	// TODO remove
	if (tile.shapes(idxs[0]).osid() == "7162adaf-b4d8-44de-82fd-c79830dc6a5d") {
		Tile tile_container;
		output_path = output_dir / (res.osid()+"-cmb.bin");
		*tile_container.add_shapes() = res;
		std::ofstream output(output_path, std::ios::out | std::ios::binary);
		std::cout << "Writing to " << output_path << std::endl;
		tile_container.SerializeToOstream(&output);
	}
}

// Change in .proto, change db table name, change in this file, change DB class, change in test file
/*
 * Get a single tile representing all of the 
 * individual tiles in `positions`. Cell coordinates
 * in the result are relative to the centre tile at
 * (`centre_row`,`centre_col`).
 */
Tile get_combined_tile(CURL *handle, const std::vector<GridPos> &positions, 
					   int centre_row, int centre_col) 
{
	Tile res, intermediate_res;
	BuildingShape *added;
	char url[500];
	BuildingShapesDB db;
	int x_shift, y_shift, idx;
	// TODO multimap
	std::unordered_map<std::string, std::vector<int>> osid_to_idxs;

	std::vector<GridPos> missing = db.missing_tiles(positions);
	fetch_missing_tiles(handle, url, 500, db, missing);
	for (const GridPos &pos: positions) {
		Tile tile = db.tile(pos);
		y_shift = (pos.first - centre_row) * 512;
		x_shift = (pos.second - centre_col) * 512;
		for (const BuildingShape &building: tile.shapes()) {
			added = intermediate_res.add_shapes();
			added->set_osid(building.osid());
			added->add_approx_centre(building.approx_centre(0) + x_shift);
			added->add_approx_centre(building.approx_centre(1) + y_shift);
			for (int i = 0; i+1 < building.edges_size(); i += 2) {
				added->add_edges(building.edges(i) + x_shift);
				added->add_edges(building.edges(i+1) + y_shift);
			}
			idx = intermediate_res.shapes_size() - 1;
			osid_to_idxs[added->osid()].push_back(idx);
		}
	}

	for (auto &p: osid_to_idxs) {
		std::cout << "Looking at group id = " << p.first << std::endl;
		std::vector<int> &idxs = p.second;
		added = res.add_shapes();
		if (idxs.size() == 1) {
			idx = idxs[0];
			*added = std::move(*intermediate_res.mutable_shapes(idx));
		} else {
			combine_building_shapes(idxs, intermediate_res, *added);
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
void translate_point_to_building_centre(Point &p, const Tile &tile) {
	int x1, y1, x2, y2, edges_passed_thru;
	float gradient, contact_y;
	for (const BuildingShape &building: tile.shapes()) {
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
