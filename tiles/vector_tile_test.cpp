#include <cstddef>
#include <cmath>
#include <algorithm>
#include <limits.h>
#include <iostream>
#include <string>
#include <fstream>
#include "../include/vector_tile.pb.h"

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
	size_t layer_sz = tile.layers_size();
	for (int i = 0; i != layer_sz; i++) {
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

bool is_building_feature_valid(const TileBuildings_BuildingFeature &bfeat) {
	return bfeat.approx_centre_size() == 2;
}

std::string edges_to_string(const TileBuildings_BuildingFeature &bfeat) {
	if (!is_building_feature_valid(bfeat)) {
		return "";
	}
	std::string res;
	size_t edges_sz = bfeat.edges_size();
	Point beg, end;
	for (size_t i = 0; i != edges_sz; i += 4) {
		if (i != 0) {
			res += ", ";
		}
		res += "(";
		res += std::to_string(bfeat.edges(i));
		res += ",";
		res += std::to_string(bfeat.edges(i+1));
		res += ") -> (";
		res += std::to_string(bfeat.edges(i+2));
		res += ",";
		res += std::to_string(bfeat.edges(i+3));
		res += ")";
	}
	return res;
}

TileBuildings_BuildingFeature decode_feature(const Tile_Feature &feat) {
	TileBuildings_BuildingFeature res;
	size_t geometry_sz = feat.geometry_size();
	size_t i = 0;
	unsigned int cmd, count;
	CommandType type;
	Point cursor = {0, 0};
	Point start = {INT_MAX, INT_MAX};
	int min_x = INT_MAX, min_y = INT_MAX, 
		max_x = INT_MIN, max_y = INT_MIN;

	while (i != geometry_sz) {
		cmd = feat.geometry(i++);
		if(!decode_command(cmd, type, count)) {
			res.Clear();
			break;
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
				min_x = std::min(min_x, cursor.x);
				max_x = std::max(max_x, cursor.x);
				min_y = std::min(min_y, cursor.y);
				max_y = std::max(max_y, cursor.y);
				cursor.x += decode_param(feat.geometry(i));
				cursor.y += decode_param(feat.geometry(i+1));
				res.add_edges(cursor.x);
				res.add_edges(cursor.y);
				i += 2;
			} else {
				res.add_edges(cursor.x);
				res.add_edges(cursor.y);
				res.add_edges(start.x);
				res.add_edges(start.y);
				min_x = std::min(min_x, cursor.x);
				max_x = std::max(max_x, cursor.x);
				min_y = std::min(min_y, cursor.y);
				max_y = std::max(max_y, cursor.y);
			}
		}
	}
	res.add_approx_centre((min_x + max_x) / 2.0);
	res.add_approx_centre((min_y + max_y) / 2.0);
	return res;
}

void test_get_tile() {
	std::string tile_f_path = "tiles/extra/test_tile.mvt";
	Tile tile;
	{
		std::ifstream tile_f(tile_f_path, std::ios::in | std::ios::binary);
		tile.ParseFromIstream(&tile_f);
	}
	Tile_Layer layer;
	if (!get_local_buildings_layer(tile, layer)) {
		return;
	}
	Tile_Feature feat;
	size_t feat_sz = layer.features_size();
	TileBuildings_BuildingFeature bfeat;
	std::vector<TileBuildings_BuildingFeature> bfeats;
	for (int i = 0; i != feat_sz; i++) {
		feat = layer.features(i);
		bfeat = decode_feature(feat);
		if (is_building_feature_valid(bfeat)) {
			bfeats.push_back(std::move(bfeat));
		}
	}
	for (const TileBuildings_BuildingFeature &bfeat: bfeats) {
		std::cout << "approx_centre = " << bfeat.approx_centre(0) 
				  << ", " << bfeat.approx_centre(1) << std::endl;
		std::cout << edges_to_string(bfeat) << std::endl;
	}
}

void test_edges_to_string() {
	TileBuildings_BuildingFeature bfeat;
	bfeat.add_approx_centre(2);
	bfeat.add_approx_centre(0);
	// 0,0 -> 0,1
	bfeat.add_edges(0);
	bfeat.add_edges(0);
	bfeat.add_edges(0);
	bfeat.add_edges(1);
	// 0,1 -> 3,1
	bfeat.add_edges(0);
	bfeat.add_edges(1);
	bfeat.add_edges(3);
	bfeat.add_edges(1);
	// 3,1 -> 3,0
	bfeat.add_edges(3);
	bfeat.add_edges(1);
	bfeat.add_edges(3);
	bfeat.add_edges(0);
	// 3,0 -> 0,0
	bfeat.add_edges(3);
	bfeat.add_edges(0);
	bfeat.add_edges(0);
	bfeat.add_edges(0);
	std::cout << "test_edges_to_string(): " << edges_to_string(bfeat) << std::endl;
}

int main() {
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	test_get_tile();
	test_edges_to_string();
	return 0;
}
