#include "../include/building_shape.h"
#include "../include/sqlitedb.h"
#include "../include/vector_tile.pb.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace vector_tile;
using BuildingShape = Tile_BuildingShape;

std::string Point::to_string() const {
  std::string res = "(";
  res += std::to_string(x);
  res += ",";
  res += std::to_string(y);
  res += ")";
  return res;
}

std::string FPoint::to_string() const {
  std::string res = "(";
  res += std::to_string(x);
  res += ",";
  res += std::to_string(y);
  res += ")";
  return res;
}

bool operator==(const Point &p1, const Point &p2) {
  return p1.x == p2.x && p1.y == p2.y;
}

bool operator==(const FPoint &p1, const FPoint &p2) {
  return p1.x == p2.x && p1.y == p2.y;
}

/*
 * CoordConverter code
 */
CoordConverter::CoordConverter(const FPoint &centre) {
  grid_origin.x = std::stof(config("NGD_TILES_API_BNG_ORIGIN_X"));
  grid_origin.y = std::stof(config("NGD_TILES_API_BNG_ORIGIN_Y"));
  cell_size = std::stof(config("NGD_TILES_API_CELL_SIZE"));
  // grid is always 512x512
  tile_col = static_cast<int>((centre.x - grid_origin.x) / (512 * cell_size));
  tile_row = static_cast<int>((grid_origin.y - centre.y) / (512 * cell_size));
  tile_origin.x = tile_col * 512 * cell_size;
  tile_origin.y = tile_row * 512 * cell_size;
}

void CoordConverter::bng_to_cell(const FPoint &bng, Point &res) const {
  float tile_space_x = bng.x - grid_origin.x - tile_origin.x;
  float tile_space_y = grid_origin.y - bng.y - tile_origin.y;
  res.x = static_cast<int>(tile_space_x / cell_size);
  res.y = static_cast<int>(tile_space_y / cell_size);
}

void CoordConverter::cell_to_bng(const Point &cell_coord, FPoint &res) const {
  float grid_space_x = cell_coord.x * cell_size + tile_origin.x;
  float grid_space_y = cell_coord.y * cell_size + tile_origin.y;
  res.x = grid_space_x + grid_origin.x;
  res.y = grid_origin.y - grid_space_y;
}

GridPos CoordConverter::get_tile_row_col(const Point &cell_coord) const {
  return {tile_row + floorf(cell_coord.y / 512.0f),
          tile_col + floorf(cell_coord.x / 512.0f)};
}

/*
 * BuildingShapesDB code
 */
std::vector<GridPos>
BuildingShapesDB::missing_tiles(const std::vector<GridPos> &positions) {
  std::vector<GridPos> res;
  int query_sz = make_missing_shapes_select(positions);
  sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
  if (stmt == nullptr) {
    std::cerr << "Failed to prepare missing building shapes statement"
              << std::endl;
    return res;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    res.push_back({/*row*/ sqlite3_column_int(stmt, 0),
                   /*col*/ sqlite3_column_int(stmt, 1)});
  }
  sqlite3_finalize(stmt);
  return res;
}

void BuildingShapesDB::insert(const GridPos &pos, std::string &data) {
  int query_sz = make_insert(pos);
  sqlite3_prepare_v2(db, query_buff, query_sz, &stmt, NULL);
  if (stmt == nullptr) {
    std::cerr << "Failed to prepare tiles_grid insert" << std::endl;
    return;
  }
  sqlite3_bind_blob(stmt, /*idx*/ 1, data.data(), data.size(), SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

Tile BuildingShapesDB::tile(const GridPos &pos) {
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

int BuildingShapesDB::make_missing_shapes_select(
    const std::vector<GridPos> &positions) {
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
                  inputs_tbl.c_str());
}

int BuildingShapesDB::make_insert(const GridPos &pos) {
  return snprintf(query_buff, query_buff_sz,
                  "INSERT INTO tiles_grid VALUES(%d, %d, ?);", pos.first,
                  pos.second);
}

int BuildingShapesDB::make_tiles_grid_select(const GridPos &pos) {
  return snprintf(query_buff, query_buff_sz,
                  "SELECT tile "
                  "FROM tiles_grid "
                  "WHERE row = %d AND col = %d;",
                  pos.first, pos.second);
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

/*
 * Debug code
 */
void debug_print_tags(const FullTile_Feature &feat,
                      const FullTile_Layer &layer) {
  for (int i = 0; i != feat.tags_size(); i += 2) {
    std::cout << "DEBUG: " << layer.keys(feat.tags(i)) << ": "
              << value_to_string(layer.values(feat.tags(i + 1))) << std::endl;
  }
}

void debug_save_single_shape(const BuildingShape &shape, int idx) {
  if (shape.osid() == "2b9b9a8b-5469-4932-a107-6e2ab7e85e54") {
    std::filesystem::path output_path =
        std::filesystem::current_path() / "tiles/extra" /
        (shape.osid() + "-" + std::to_string(idx) + ".bin");
    std::ofstream output(output_path, std::ios::out | std::ios::binary);
    Tile tile;
    *tile.add_shapes() = shape;
    std::cout << "DEBUG: saving tile to " << output_path << std::endl;
    tile.SerializeToOstream(&output);
  }
}

void debug_save_combined_shape(const BuildingShape &shape) {
  if (shape.osid() == "2b9b9a8b-5469-4932-a107-6e2ab7e85e54") {
    std::filesystem::path output_path = std::filesystem::current_path() /
                                        "tiles/extra" /
                                        (shape.osid() + "-cmb.bin");
    std::ofstream output(output_path, std::ios::out | std::ios::binary);
    Tile tile;
    *tile.add_shapes() = shape;
    std::cout << "DEBUG: saving tile to " << output_path << std::endl;
    tile.SerializeToOstream(&output);
  }
}

/*
 * Combination algorithm code
 */
std::string edges_to_string(const vector_tile::Tile_BuildingShape &building) {
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
    res += std::to_string(building.edges(i + 1));
    res += ") -> (";
    res += std::to_string(building.edges(i + 2));
    res += ",";
    res += std::to_string(building.edges(i + 3));
    res += ")";
  }
  return res;
}

bool decode_command(unsigned int cmd, CommandType &type, unsigned int &count) {
  unsigned int type_val =
      cmd & 0x7; // least significant 3 digits represent type
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

bool decode_feature(const FullTile_Feature &feat, const FullTile_Layer &layer,
                    BuildingShape &res) {
  for (int i = 0; i + 1 < feat.tags_size(); i += 2) {
    if (layer.keys(feat.tags(i)) == "osid") {
      FullTile_Value val = layer.values(feat.tags(i + 1));
      res.set_osid(value_to_string(val));
    }
  }
  if (!res.has_osid()) {
    std::cerr << "Couldn't find osid" << std::endl;
    res.Clear();
    return false;
  }

  int geometry_sz = feat.geometry_size();
  int i = 0;
  unsigned int cmd, count;
  CommandType type;
  Point cursor = {0, 0};
  Point start = {INT_MAX, INT_MAX};
  int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;

  while (i < geometry_sz) {
    cmd = feat.geometry(i++);
    if (!decode_command(cmd, type, count)) {
      std::cerr << "Failed to decode command: " << cmd << std::endl;
      res.Clear();
      return false;
    }
    for (; count > 0; count--) {
      if (type == CommandType::MOVE) {
        cursor.x += decode_param(feat.geometry(i));
        cursor.y += decode_param(feat.geometry(i + 1));
        i += 2;
      } else if (type == CommandType::LINE) {
        if (start.x == INT_MAX && start.y == INT_MAX) {
          start = cursor;
        }
        res.add_edges(cursor.x);
        res.add_edges(cursor.y);
        cursor.x += decode_param(feat.geometry(i));
        cursor.y += decode_param(feat.geometry(i + 1));
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

Tile parse_tile(std::string &tile_data) {
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
  const FullTile_Layer &buildings_layer = *it;

  BuildingShape *added = res.add_shapes();
  for (const FullTile_Feature &feat : buildings_layer.features()) {
    if (decode_feature(feat, buildings_layer, *added)) {
      added = res.add_shapes();
    }
  }
  res.mutable_shapes()->RemoveLast();
  return res;
}

void fetch_missing_tiles(CURL *handle, char url_buff[], size_t buff_sz,
                         BuildingShapesDB &db, std::vector<GridPos> &missing) {
  std::string full_tile_data; // full tile data directly from api
  std::string tile_data;      // tile data filtered with only the info we need
  for (const GridPos &pos : missing) {
    full_tile_data.clear();
    tile_data.clear();
    get_tiles_api_url(url_buff, buff_sz, pos.first, pos.second);
    std::cout << "Fetching tile from " << url_buff << std::endl;
    make_get_request(handle, url_buff, full_tile_data);
    Tile tile = parse_tile(full_tile_data);
    tile.SerializeToString(&tile_data);
    db.insert(pos, tile_data);
  }
}

float gradient(int x1, int y1, int x2, int y2) {
  if (x1 != x2) {
    return (y2 - y1) / (x2 - x1 * 1.0f);
  } else { // perfectly vertical edge
    return std::numeric_limits<float>::infinity();
  }
}

Point midpoint(int x1, int y1, int x2, int y2) {
  float grad = gradient(x1, y1, x2, y2);
  int mid_x = (x1 + x2) / 2;
  int mid_y;
  if (!isinf(grad)) {
    mid_y = (mid_x - x1) * grad + y1;
  } else {
    mid_y = (y1 + y2) / 2;
  }
  return {mid_x, mid_y};
}

EnclosureType get_enclosure_type(const Point &p, const BuildingShape &shape,
                                 const EdgeToPenaltyMap &pen_mp) {
  std::unordered_set<int> above_contacts, below_contacts;
  int above_penalty = 0, below_penalty = 0, penalty;
  int x1, y1, x2, y2, contact_y;
  float grad;
  Point from, to, before_from, after_to;
  bool is_before_from_left, is_after_to_left;
  for (int i = 0; i != shape.edges_size(); i += 4) {
    x1 = shape.edges(i);
    y1 = shape.edges(i + 1);
    x2 = shape.edges(i + 2);
    y2 = shape.edges(i + 3);
    if (std::min(x1, x2) > p.x || std::max(x1, x2) < p.x) {
      continue;
    }
    grad = gradient(x1, y1, x2, y2);
    if (!isinf(grad)) {
      contact_y = (p.x - x1) * grad + y1;
      if (p.y < contact_y) {
        above_contacts.insert(contact_y);
      }
      if (p.y > contact_y) {
        below_contacts.insert(contact_y);
      }
    } else {
      if (std::max(y1, y2) >= p.y && std::min(y1, y2) <= p.y) {
        return EnclosureType::EDGE;
      } else {
        from.x = x1;
        from.y = y1;
        to.x = x2;
        to.y = y2;
        if (std::min(y1, y2) > p.y) {
          above_contacts.insert(y1);
          above_contacts.insert(y2);
          above_penalty += pen_mp.at({from, to});
        } else {
          below_contacts.insert(y1);
          below_contacts.insert(y2);
          below_penalty += pen_mp.at({from, to});
        }
      }
    }
  }
  int nabove_contact = above_contacts.size() - above_penalty;
  int nbelow_contact = below_contacts.size() - below_penalty;
  if ((nabove_contact % 2) != (nbelow_contact % 2)) {
    return EnclosureType::EDGE;
  } else if (nabove_contact % 2 == 0) {
    return EnclosureType::OUTSIDE;
  } else {
    return EnclosureType::INSIDE;
  }
}

EdgeToPenaltyMap edge_to_penalty_map(const BuildingShape &shape) {
  EdgeToPenaltyMap res;
  Point from, to, before, after;
  bool is_before_left, is_after_left;
  int penalty;
  int nedge = shape.edges_size() / 4;
  if (nedge < 3) {
    return res;
  }
  for (int i = 0; i != nedge; i++) {
    from.x = shape.edges(i * 4);
    from.y = shape.edges(i * 4 + 1);
    to.x = shape.edges(i * 4 + 2);
    to.y = shape.edges(i * 4 + 3);
    // check the i-1th edge using mod to wrap around
    before.x = shape.edges(floor_mod(i - 1, nedge) * 4);
    before.y = shape.edges(floor_mod(i - 1, nedge) * 4 + 1);
    // check the i+1th edge using mod to wrap around
    after.x = shape.edges(floor_mod(i + 1, nedge) * 4 + 2);
    after.y = shape.edges(floor_mod(i - 1, nedge) * 4 + 3);
    if (from.x != to.x) {
      continue;
    }
    is_before_left = (before.x - from.x) < 0;
    is_after_left = (after.x - to.x) < 0;
    penalty = is_before_left == is_after_left ? 2 : 1;
    res[{from, to}] = penalty;
  }
  return res;
}

std::vector<EdgeToPenaltyMap>
edge_to_penalty_maps(const vector_tile::Tile &tile) {
  std::vector<EdgeToPenaltyMap> res;
  std::transform(
      tile.shapes().begin(), tile.shapes().end(), std::back_inserter(res),
      [](const BuildingShape &sh) { return edge_to_penalty_map(sh); });
  return res;
}

void combine_building_shapes(const std::vector<const BuildingShape *> &shapes,
                             BuildingShape &res) {
  res.set_osid(shapes[0]->osid());
  int x1, y1, x2, y2;
  int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
  Point mid;
  EnclosureType enc_type;
  bool is_boundry_edge;
  std::vector<EdgeToPenaltyMap> pen_mps;
  std::transform(
      shapes.begin(), shapes.end(), std::back_inserter(pen_mps),
      [](const BuildingShape *shape) { return edge_to_penalty_map(*shape); });
  for (const BuildingShape *shape : shapes) {
    for (int n = 0; n != shape->edges_size(); n += 4) {
      x1 = shape->edges(n);
      y1 = shape->edges(n + 1);
      x2 = shape->edges(n + 2);
      y2 = shape->edges(n + 3);
      mid = midpoint(x1, y1, x2, y2);
      is_boundry_edge = true;
      for (int j = 0; j != shapes.size(); j++) {
        enc_type = get_enclosure_type(mid, *shapes[j], pen_mps[j]);
        if (enc_type == EnclosureType::INSIDE) {
          is_boundry_edge = false;
        }
      }
      if (is_boundry_edge) {
        res.add_edges(x1);
        res.add_edges(y1);
        res.add_edges(x2);
        res.add_edges(y2);
        min_x = std::min(min_x, std::min(x1, x2));
        min_y = std::min(min_y, std::min(y1, y2));
        max_x = std::max(max_x, std::max(x1, x2));
        max_y = std::max(max_y, std::max(y1, y2));
      }
    }
  }
  res.add_approx_centre((min_x + max_x) / 2);
  res.add_approx_centre((min_y + max_y) / 2);
}

Tile get_combined_tile(CURL *handle, const std::vector<GridPos> &positions,
                       int centre_row, int centre_col) {
  Tile res, intermediate_res;
  BuildingShape *added;
  char url[500];
  BuildingShapesDB db;
  int x_shift, y_shift, idx;
  // TODO multimap
  std::unordered_map<std::string, std::vector<int>> osid_to_idxs;

  std::vector<GridPos> missing = db.missing_tiles(positions);
  fetch_missing_tiles(handle, url, 500, db, missing);
  for (const GridPos &pos : positions) {
    Tile tile = db.tile(pos);
    y_shift = (pos.first - centre_row) * 512;
    x_shift = (pos.second - centre_col) * 512;
    for (const BuildingShape &building : tile.shapes()) {
      added = intermediate_res.add_shapes();
      added->set_osid(building.osid());
      added->add_approx_centre(building.approx_centre(0) + x_shift);
      added->add_approx_centre(building.approx_centre(1) + y_shift);
      for (int i = 0; i + 1 < building.edges_size(); i += 2) {
        added->add_edges(building.edges(i) + x_shift);
        added->add_edges(building.edges(i + 1) + y_shift);
      }
      idx = intermediate_res.shapes_size() - 1;
      osid_to_idxs[added->osid()].push_back(idx);
    }
  }

  std::vector<const BuildingShape *> buildings;
  for (auto &p : osid_to_idxs) {
    std::vector<int> &idxs = p.second;
    added = res.add_shapes();
    if (idxs.size() == 1) {
      idx = idxs[0];
      *added = std::move(*intermediate_res.mutable_shapes(idx));
    } else {
      buildings.clear();
      std::transform(
          idxs.begin(), idxs.end(), std::back_inserter(buildings),
          [&intermediate_res](int i) { return &intermediate_res.shapes(i); });
      combine_building_shapes(buildings, *added);
    }
  }
  return res;
}

void translate_point_to_building_centre(
    Point &p, const Tile &tile, std::vector<EdgeToPenaltyMap> &pen_mps) {
  EnclosureType enc_type;
  for (int i = 0; i != tile.shapes_size(); i++) {
    const BuildingShape &shape = tile.shapes(i);
    enc_type = get_enclosure_type(p, shape, pen_mps[i]);
    if (enc_type == EnclosureType::OUTSIDE) {
      continue;
    }
    p.x = shape.approx_centre(0);
    p.y = shape.approx_centre(1);
    return;
  }
}

void translate_points_to_building_centres(CURL *handle,
                                          std::vector<FPoint *> &bng_coords,
                                          FPoint centre) {
  int n = bng_coords.size();
  CoordConverter conv(centre);
  std::vector<Point> cell_coords(n);
  for (int i = 0; i != n; i++) {
    conv.bng_to_cell(*bng_coords[i], cell_coords[i]);
  }
  GridPosSet set;
  std::vector<GridPos> grid_positions;
  for (const Point &p : cell_coords) {
    GridPos gp = conv.get_tile_row_col(p);
    if (!set.count(gp)) {
      grid_positions.push_back(gp);
      set.insert(gp);
    }
  }
  Tile tile = get_combined_tile(handle, grid_positions, conv.get_centre_row(),
                                conv.get_centre_col());
  std::vector<EdgeToPenaltyMap> pen_mps = edge_to_penalty_maps(tile);
  for (Point &p : cell_coords) {
    translate_point_to_building_centre(p, tile, pen_mps);
  }
  for (int i = 0; i != n; i++) {
    conv.cell_to_bng(cell_coords[i], *bng_coords[i]);
  }
}
