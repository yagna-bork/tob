#include <cstddef>
#include <cmath>
#include <algorithm>
#include <limits.h>
#include <iostream>
#include <string>
#include <fstream>
#include <ios>
#include "../include/vector_tile.pb.h"
#include "../include/util.h"

/*
 * Conversion:
 * lat, long = 51.51955586617442, 0.5931578991618254
 * online conversion
 * easting, northing = 580009, 183258
 * dist from pointOfOrigin
 * dx, dy = 818384, -1192998
 * (magnitude of dist) / (512 * cell_size)
 * grid_dx, grid_dy = 913, 1331
 * swap
 * tile_row, tile_col = 1331, 913
 */

/*
 * 
 * lat, long = 51.544452858065306, 0.7125439099096778
 * easting, northing = 579911, 186026
 * dx, dy = 818286, -1190230
 * dx_grid, dy_grid = 913, 1328
 * tile_row, tile_col = 1328, 913
 */

using namespace vector_tile;
using BuildingShape = BuildingShapes::BuildingShape;

enum CommandType {
	MOVE = 0,
	LINE = 1,
	CLOSE = 2
};

struct Point {
	int x;
	int y;
};

bool get_local_buildings_layer(const Tile& tile, Tile_Layer &res) {
	for (int i = 0; i != tile.layers_size(); i++) {
		res = tile.layers(i);
		if (res.name() == "Local_buildings") {
			return true;
		}
	}
	res.Clear();
	return false;
}

std::string geomtype_to_string(Tile_GeomType gt) {
	switch (gt) {
		case Tile_GeomType::Tile_GeomType_UNKNOWN:
			return "UNKNOWN";
		case Tile_GeomType::Tile_GeomType_POINT:
			return "POINT";
		case Tile_GeomType::Tile_GeomType_LINESTRING:
			return "LINE_STRING";
		case Tile_GeomType::Tile_GeomType_POLYGON:
			return "PLOYGON";
		default:
			return "UNKNOWN";
	}
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

int decode_param(unsigned int param) {
	// see 4.3.2 of the spec
	return (param >> 1) ^ (-(param & 1));
}

bool is_building_shape_valid(const BuildingShape &b) {
	return b.approx_centre_size() == 2;
}

std::string edges_to_string(const BuildingShape &building) {
	if (!is_building_shape_valid(building)) {
		return "";
	}
	std::string res;
	size_t edges_sz = building.edges_size();
	Point beg, end;
	for (size_t i = 0; i != edges_sz; i += 4) {
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

bool decode_feature(const Tile_Feature &feat, BuildingShape &res) {
	size_t geometry_sz = feat.geometry_size();
	size_t i = 0;
	unsigned int cmd, count;
	CommandType type;
	Point cursor = {0, 0};
	Point start = {INT_MAX, INT_MAX};
	int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;

	while (i != geometry_sz) {
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

void test_get_tile() {
	init_config();
	Tile tile;
	{
		std::ifstream tile_f(g_config["TEST_TILE_PATH"], std::ios::in | std::ios::binary);
		tile.ParseFromIstream(&tile_f);
	}
	Tile_Layer layer;
	if (!get_local_buildings_layer(tile, layer)) {
		return;
	}

	BuildingShapes buildings;
	BuildingShape *buildingp = buildings.add_building_shapes();
	for (int i = 0; i != layer.features_size(); i++) {
		const Tile_Feature &feat = layer.features(i);
		if (decode_feature(feat, *buildingp)) {
			buildingp = buildings.add_building_shapes();
		}
	}
	buildings.mutable_building_shapes()->RemoveLast();

	std::ofstream output(g_config["BUILDING_SHAPES_PATH"], 
						 std::ios::out | std::ios::trunc | std::ios::binary);
	buildings.SerializeToOstream(&output);
	google::protobuf::ShutdownProtobufLibrary();
}

void test_edges_to_string() {
	BuildingShape building;
	// 0,0 -> 0,1
	building.add_edges(0);
	building.add_edges(0);
	building.add_edges(0);
	building.add_edges(1);
	// 0,1 -> 3,1
	building.add_edges(0);
	building.add_edges(1);
	building.add_edges(3);
	building.add_edges(1);
	// 3,1 -> 3,0
	building.add_edges(3);
	building.add_edges(1);
	building.add_edges(3);
	building.add_edges(0);
	// 3,0 -> 0,0
	building.add_edges(3);
	building.add_edges(0);
	building.add_edges(0);
	building.add_edges(0);
	
	building.add_approx_centre(1);
	building.add_approx_centre(0);
	std::cout << "test_edges_to_string(): " << edges_to_string(building) << std::endl;
}

int main() {
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	test_get_tile();
	test_edges_to_string();
	return 0;
}
