#include <iterator>
#include <utility>
#include <iterator>
#include <cstddef>
#include <algorithm>
#include <fstream>
#include <curl/curl.h>
#include "../include/building_shape.h"
#include "../include/util.h"

static const std::string CLUSTERING_TEST_TILE_PATH="tiles/extra/tile.bin";
static const std::string CLUSTERING_TEST_INP_PATH="tiles/extra/test_input.csv";
static const std::string CLUSTERING_TEST_OUT_PATH="tiles/extra/test_output.csv";
static const std::string BNG_TEST_INP_PATH="tiles/extra/bng_test_input.csv";
static const std::string COMBINATION_TEST_TILE_PATH="tiles/extra/cmb-tile.bin";

static CURL *CURL_HANDLE = curl_easy_init(); 

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
	// centre
	building.add_approx_centre(1);
	building.add_approx_centre(0);
	std::cout << "test_edges_to_string(): " << edges_to_string(building) << std::endl;
}

void test_translate_multiple_points() {
	std::vector<FPoint> bng_points;
	{
		std::string line;
		std::ifstream input(BNG_TEST_INP_PATH, std::ios::in);
		std::string::const_iterator sep;
		float x, y;
		while (std::getline(input, line)) {
			sep = std::find(line.cbegin(), line.cend(), ',');
			x = std::stof(std::string(line.cbegin(), sep));
			y = std::stof(std::string(sep+1, line.cend()));
			bng_points.push_back({x, y});
		}
	}

	FPoint centre = bng_points[bng_points.size()-1];
	bng_points.pop_back();
	CoordConverter cc(centre);
	std::vector<Point> cell_points(bng_points.size());
	for (int i = 0; i != bng_points.size(); i++) {
		cc.bng_to_cell(bng_points[i], cell_points[i]);
	}
	{
		std::ofstream test_inp(CLUSTERING_TEST_INP_PATH, std::ios::trunc);
		for (const Point &p: cell_points) {
			test_inp << p.x << "," << p.y << std::endl;
		}
	}

	GridPosSet set;
	std::vector<GridPos> grid_positions;
	for (const Point &p: cell_points) {
		GridPos gp = cc.get_tile_row_col(p);
		if (!set.count(gp)) {
			grid_positions.push_back(gp);
			set.insert(gp);
		}
	}
	Tile tile = get_combined_tile(
		CURL_HANDLE, grid_positions, cc.get_centre_row(), cc.get_centre_col()
	);
	{
		std::ofstream output(
			CLUSTERING_TEST_TILE_PATH, std::ios::out | std::ios::binary
		);
		tile.SerializeToOstream(&output);
	}

	std::ofstream test_out(CLUSTERING_TEST_OUT_PATH, std::ios::out);
	std::vector<EdgeToPenaltyMap> pen_mps = edge_to_penalty_maps(tile);
	for (Point &p: cell_points) {
		translate_point_to_building_centre(p, tile, pen_mps);
		test_out << p.x << "," << p.y << std::endl;
	}
	std::cout << "test_translate_multiple_points(): Done" << std::endl;
}

void test_coord_converter() {
	std::vector<FPoint> bng_points {
		{580045.0,183233.0}, {580049.0,183276.0}, {580058.0,183279.0}, {580070.0,183236.0},
		{580018.0,183235.0}, {580012.0,183258.0}, {580012.0,183258.0}, {580018.0,183275.0},
		{580012.0,183268.0}, {580009.0,183262.0}, {580009.0,183262.0}, {580067.0,183282.0},
		{580019.0,183227.0}, {580008.75,18326.15}, {580032.0,183292.0}, {580032.0,183292.0},
		{580032.0,183292.0}, {580032.0,183292.0}, {580032.0,183292.0}, {580032.0,183292.0},
		{580083.0,183238.0}, {580023.0,183217.0}, {580007.0,183283.0}, {580007.0,183283.0},
		{580009.0,183287.0}, {580083.0,183283.0}, {580012.0,183292.0}
	};
	std::vector<FPoint> bng_points_cp(bng_points);
	FPoint centre = {580044,183254};
	std::vector<Point> cell_points(bng_points.size());
	CoordConverter cc(centre);
	// bng -> cell
	for (int i = 0; i != bng_points_cp.size(); i++) {
		cc.bng_to_cell(bng_points_cp[i], cell_points[i]);
	}
	// cell -> bng
	for (int i = 0; i != cell_points.size(); i++) {
		cc.cell_to_bng(cell_points[i], bng_points_cp[i]);
	}

	float diff_allowed = 0.125f, x_diff, y_diff;
	for (int i = 0; i != bng_points.size(); i++) {
		x_diff = fabs(bng_points[i].x - bng_points_cp[i].x);
		y_diff = fabs(bng_points[i].y - bng_points_cp[i].y);
		if (x_diff>diff_allowed || y_diff>diff_allowed) {
			std::cout << "test_coord_converter(): FAILED" << std::endl;
			std::cout << bng_points[i].to_string() << " != " 
					  << bng_points_cp[i].to_string() << std::endl;
			return;
		}
	}
	std::cout << "test_coord_converter(): PASSED" << std::endl;
}

void test_get_tile_row_col() {
	FPoint bng = {580008, 183261};
	CoordConverter cc(bng);
	Point cell; 
	cc.bng_to_cell(bng, cell);
	std::pair<int, int> rc = cc.get_tile_row_col(cell);
	if (rc.first == 21303 && rc.second == 14613) {
		std::cout << "test_get_row_col(): PASSED" << std::endl;
	} else {
		std::cout << "test_get_row_col(): FAILED" << std::endl;
		std::cout << "tile_row_col = " << rc.first << "," << rc.second 
				  << " EXPECTED = 21303,14613" << std::endl;
	}
}

void test_get_tile_rows_cols() {
	FPoint centre = {580008, 183261};
	CoordConverter cc(centre);
	std::vector<Point> cell_coords = {
		{502, -1014}, {-256, -256}, {256, -256}, {786, -256},
		{-256, 256}, {1014, 10}, {1524, 502}, {-256, 768},
		{10, 1014}
	};
	std::vector<std::pair<int, int>> expected = {
		{21301, 14613}, {21302, 14612}, {21302, 14613}, {21302, 14614},
		{21303, 14612}, {21303, 14614}, {21303, 14615}, {21304, 14612},
		{21304, 14613}
	};
	std::vector<std::pair<int, int>> res;
	std::transform(cell_coords.begin(), cell_coords.end(), std::back_inserter(res), 
		[&cc](const Point &p) {
			return cc.get_tile_row_col(p);
		});
	bool test_passed = std::equal(res.begin(), res.end(), expected.begin(), 
		[](const std::pair<int, int> &p1, const std::pair<int, int> &p2) {
			return p1.first == p2.first && p1.second == p2.second;
		});
	if (test_passed) {
		std::cout << "test_get_rows_cols(): PASSED" << std::endl;
	} else {
		std::cout << "test_get_rows_cols(): FAILED" << std::endl;
		auto diff = std::mismatch(res.begin(), res.end(), expected.begin());
		std::cout << diff.first->first << "," << diff.first->second << " " 
				  << diff.second->first << "," << diff.second->second 
				  << std::endl;
	}
}

void test_get_combined_tile() {
	std::vector<GridPos> grid_positions;
	grid_positions.push_back({21303, 14613});
	grid_positions.push_back({21302, 14613}); // above
	grid_positions.push_back({21303, 14614}); // right
	grid_positions.push_back({21302, 14614}); // right and above
	Tile tile = get_combined_tile(
		CURL_HANDLE, grid_positions, /*centre_row=*/21303, /*centre_col=*/14613
	);
	std::ofstream output(COMBINATION_TEST_TILE_PATH, std::ios::trunc | std::ios::binary);
	tile.SerializeToOstream(&output);
	std::cout << "test_get_combined_tile(): Done" << std::endl;
}

void test_get_enclosure_type() {
	BuildingShape shape;
	// 0,0 -> 0,2
	shape.add_edges(0);
	shape.add_edges(0);
	shape.add_edges(0);
	shape.add_edges(2);
	// 0,2->2,2
	shape.add_edges(0);
	shape.add_edges(2);
	shape.add_edges(2);
	shape.add_edges(2);
	// 2,2->2,0
	shape.add_edges(2);
	shape.add_edges(2);
	shape.add_edges(2);
	shape.add_edges(0);
	// 2,0->0,0
	shape.add_edges(2);
	shape.add_edges(0);
	shape.add_edges(0);
	shape.add_edges(0);
	EdgeToPenaltyMap pen_mp = edge_to_penalty_map(shape);
	if (get_enclosure_type({1,1}, shape, pen_mp) != EnclosureType::INSIDE) {
		std::cout << "test_get_enclosure_type(0): FAILED" << std::endl;
	} else if (get_enclosure_type({2,2}, shape, pen_mp) != EnclosureType::EDGE) {
		std::cout << "test_get_enclosure_type(1): FAILED" << std::endl;
	} else if (get_enclosure_type({3,3}, shape, pen_mp) != EnclosureType::OUTSIDE) {
		std::cout << "test_get_enclosure_type(2): FAILED" << std::endl;
	} else {
		std::cout << "test_get_enclosure_type(): PASSED" << std::endl;
	}
}

void test_edge_skimming() {
	BuildingShape shape;
	// 0,0 -> 4,0
	shape.add_edges(0);
	shape.add_edges(0);
	shape.add_edges(4);
	shape.add_edges(0);
	// 4,0 -> 4,2
	shape.add_edges(4);
	shape.add_edges(0);
	shape.add_edges(4);
	shape.add_edges(2);
	// 4,2 -> 2,2
	shape.add_edges(4);
	shape.add_edges(2);
	shape.add_edges(2);
	shape.add_edges(2);
	// 2,2 -> 2,4
	shape.add_edges(2);
	shape.add_edges(2);
	shape.add_edges(2);
	shape.add_edges(4);
	// 2,4 -> 0,4
	shape.add_edges(2);
	shape.add_edges(4);
	shape.add_edges(0);
	shape.add_edges(4);
	// 0,4 -> 0,0
	shape.add_edges(0);
	shape.add_edges(4);
	shape.add_edges(0);
	shape.add_edges(0);
	EdgeToPenaltyMap pen_mp = edge_to_penalty_map(shape);

	bool passed = true;
	EnclosureType t;
	t = get_enclosure_type({2, 1}, shape, pen_mp);
	if (t != EnclosureType::INSIDE) {
		std::cout << "test_edge_skimming(0): FAILED" << std::endl;
		std::cout << t << std::endl;
		passed = false;
	}
	t = get_enclosure_type({2, 2}, shape, pen_mp);
	if (t != EnclosureType::EDGE) {
		std::cout << "test_edge_skimming(1): FAILED" << std::endl;
		passed = false;
	}
	t = get_enclosure_type({2, 0}, shape, pen_mp);
	if (t != EnclosureType::EDGE) {
		std::cout << "test_edge_skimming(2): FAILED" << std::endl;
		passed = false;
	}
	t = get_enclosure_type({2, 3}, shape, pen_mp);
	if (t != EnclosureType::EDGE) {
		std::cout << "test_edge_skimming(3): FAILED" << std::endl;
		passed = false;
	}
	t = get_enclosure_type({2, 3}, shape, pen_mp);
	if (t != EnclosureType::EDGE) {
		std::cout << "test_edge_skimming(4): FAILED" << std::endl;
		passed = false;
	}
	t = get_enclosure_type({2, 5}, shape, pen_mp);
	if (t != EnclosureType::OUTSIDE) {
		std::cout << "test_edge_skimming(5): FAILED" << std::endl;
		passed = false;
	}
	t = get_enclosure_type({2, -1}, shape, pen_mp);
	if (t != EnclosureType::OUTSIDE) {
		std::cout << "test_edge_skimming(6): FAILED" << std::endl;
		std::cout << t << std::endl;
		passed = false;
	}
	t = get_enclosure_type({4, 3}, shape, pen_mp);
	if (t != EnclosureType::OUTSIDE) {
		std::cout << "test_edge_skimming(7): FAILED" << std::endl;
		passed = false;
	}
	if (passed) {
		std::cout << "test_edge_skimming(): PASSED" << std::endl;
	}
}

int main() {
	test_translate_multiple_points();
	test_edges_to_string();
	test_coord_converter();
	test_get_tile_row_col();
	test_get_tile_rows_cols();
	test_get_combined_tile();
	test_get_enclosure_type();
	test_edge_skimming();
	return 0;
}
