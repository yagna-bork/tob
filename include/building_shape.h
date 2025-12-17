#ifndef GUARD_BUILDING_SHAPE
#define GUARD_BUILDING_SHAPE
#include "sqlitedb.h"
#include "util.h"
#include "vector_tile.pb.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

enum CommandType { MOVE = 0, LINE = 1, CLOSE = 2 };

enum EnclosureType { INSIDE = 0, EDGE = 1, OUTSIDE = 2 };

// Stores cell space coords
struct Point {
  int x;
  int y;
  std::string to_string() const;
};

// Stores BNG, grid and tile space coords
struct FPoint {
  float x;
  float y;
  std::string to_string() const;
};

bool operator==(const Point &p1, const Point &p2);

bool operator==(const FPoint &p1, const FPoint &p2);

template <> struct std::hash<Point> {
  size_t operator()(const Point &p) const {
    return std::hash<int>()(p.x) ^ std::hash<int>()(p.y);
  }
};

template <> struct std::hash<FPoint> {
  size_t operator()(const Point &p) const {
    return std::hash<float>()(p.x) ^ std::hash<float>()(p.y);
  }
};

struct PairHash {
  template <class U, class V>
  size_t operator()(const std::pair<U, V> &p) const {
    return std::hash<U>()(p.first) ^ std::hash<V>()(p.second);
  }
};

struct PairEq {
  template <class U, class V>
  bool operator()(const std::pair<U, V> &p1, const std::pair<U, V> &p2) const {
    return std::equal_to<U>()(p1.first, p2.first) &&
           std::equal_to<V>()(p1.second, p2.second);
  }
};

typedef std::pair<int, int> GridPos;
typedef std::unordered_set<GridPos, PairHash, PairEq> GridPosSet;
typedef std::unordered_map<std::pair<Point, Point>, int, PairHash, PairEq>
    EdgeToPenaltyMap;

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
  CoordConverter(const FPoint &centre);

  void bng_to_cell(const FPoint &bng, Point &res) const;

  void cell_to_bng(const Point &cell_coord, FPoint &res) const;

  GridPos get_tile_row_col(const Point &cell_coord) const;

  inline int get_centre_row() const { return tile_row; }

  inline int get_centre_col() const { return tile_col; }

private:
  FPoint grid_origin;
  int tile_row;
  int tile_col;
  FPoint tile_origin;
  float cell_size;
};

class BuildingShapesDB : public SQLiteDB {
public:
  BuildingShapesDB() : SQLiteDB() {}

  BuildingShapesDB(const BuildingShapesDB &other) = delete;
  BuildingShapesDB(BuildingShapesDB &other) = delete;
  BuildingShapesDB &operator=(const BuildingShapesDB &other) = delete;
  BuildingShapesDB &operator=(BuildingShapesDB &&other) = delete;

  std::vector<GridPos> missing_tiles(const std::vector<GridPos> &positions);

  void insert(const GridPos &pos, std::string &data);

  vector_tile::Tile tile(const GridPos &pos);

private:
  int make_missing_shapes_select(const std::vector<GridPos> &positions);

  int make_insert(const GridPos &pos);

  int make_tiles_grid_select(const GridPos &pos);
};

std::string value_to_string(const vector_tile::FullTile_Value &val);

inline bool is_building_shape_valid(const vector_tile::Tile_BuildingShape &b) {
  return b.approx_centre_size() == 2;
}

std::string edges_to_string(const vector_tile::Tile_BuildingShape &building);

inline int decode_param(unsigned int param) {
  // see 4.3.2 of the spec
  return (param >> 1) ^ (-(param & 1));
}

bool decode_command(unsigned int cmd, CommandType &type, unsigned int &count);

bool decode_feature(const vector_tile::FullTile_Feature &feat,
                    const vector_tile::FullTile_Layer &layer,
                    vector_tile::Tile_BuildingShape &res);

vector_tile::Tile parse_tile(std::string &tile_data);

inline int get_tiles_api_url(char url_buff[], size_t buff_sz, int grid_row,
                             int grid_col) {
  return snprintf(url_buff, buff_sz, "%s/%d/%d?key=%s",
                  config("TILES_API_URL").c_str(), grid_row, grid_col,
                  config("OS_PROJECT_API_KEY").c_str());
}

void fetch_missing_tiles(CURL *handle, char url_buff[], size_t buff_sz,
                         BuildingShapesDB &db, std::vector<GridPos> &missing);

float gradient(int x1, int y1, int x2, int y2);

Point midpoint(int x1, int y1, int x2, int y2);

/*
 * Modified version of the ray casting algorith from:
 * https://en.wikipedia.org/wiki/Point_in_polygon#Ray_casting_algorithm
 * which projects two vertical rays, above and below, to
 * determine if a point is inside, outside or on the edge of a shape.
 */
EnclosureType get_enclosure_type(const Point &p,
                                 const vector_tile::Tile_BuildingShape &shape,
                                 const EdgeToPenaltyMap &pen_mp);

inline int floor_mod(int x, int y) { return x - floor(x / (y * 1.0f)) * y; }

EdgeToPenaltyMap
edge_to_penalty_map(const vector_tile::Tile_BuildingShape &shape);

std::vector<EdgeToPenaltyMap>
edge_to_penalty_maps(const vector_tile::Tile &tile);

/*
 * Algorithm to prune any edges that
 * have midpoints inside of any of the given
 * `shapes`. These edges can't be boundry
 * edges by definition and don't belong in
 * the combined result shape `res`.
 */
void combine_building_shapes(
    const std::vector<const vector_tile::Tile_BuildingShape *> &shapes,
    vector_tile::Tile_BuildingShape &res);

/*
 * Get a single tile representing all of the
 * individual tiles in `positions`. Cell coordinates
 * in the result are relative to the centre tile at
 * (`centre_row`,`centre_col`).
 */
vector_tile::Tile get_combined_tile(CURL *handle,
                                    const std::vector<GridPos> &positions,
                                    int centre_row, int centre_col);

/*
 * Translate `Point p` to the centre of
 * a building shape if it's found to be
 * within one.
 */
void translate_point_to_building_centre(Point &p, const vector_tile::Tile &tile,
                                        std::vector<EdgeToPenaltyMap> &pen_mps);

/*
 * `centre` are the central bng coordinates
 * used in a OS radius call to obtain the
 * `bng_coords` to be translated.
 */
void translate_points_to_building_centres(CURL *handle,
                                          std::vector<FPoint *> &bng_coords,
                                          FPoint centre);
#endif
