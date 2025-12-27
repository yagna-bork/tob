#include "../../include/vector_tile.pb.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>

using namespace vector_tile;
using BuildingShape = Tile_BuildingShape;

static const std::string FONT_PATH = "src/visualiser/fonts/Swansea-q3pd.ttf";

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Event event;
static TTF_TextEngine *engine;
static TTF_Font *font;

static const int WIN_HEIGHT = 2160;
static const int WIN_WIDTH = 3840;

bool before_opt, show_id_opt, show_centre_opt, show_edge_opt;

// convert from grid coordinate space to the pixel coordinate space
float grid_x2pxl(int grid_coord) { return (grid_coord + 600); }

float grid_y2pxl(int grid_coord) { return (grid_coord + 300); }

void display_building_outlines(const Tile &tile) {
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
  float x1, y1, x2, y2;
  std::string text;
  TTF_Text *ttf_text;
  for (const BuildingShape &building : tile.shapes()) {
    for (size_t i = 0; i != building.edges_size(); i += 4) {
      x1 = grid_x2pxl(building.edges(i));
      y1 = grid_y2pxl(building.edges(i + 1));
      x2 = grid_x2pxl(building.edges(i + 2));
      y2 = grid_y2pxl(building.edges(i + 3));
      SDL_RenderLine(renderer, x1, y1, x2, y2);
      if (show_edge_opt) {
        text = "(" + std::to_string(building.edges(i)) + "," +
               std::to_string(building.edges(i + 1)) + ")";
        ttf_text = TTF_CreateText(engine, font, text.c_str(), text.size());
        TTF_DrawRendererText(ttf_text, x1 + 2, y1);
        TTF_DestroyText(ttf_text);
        text = "(" + std::to_string(building.edges(i + 2)) + "," +
               std::to_string(building.edges(i + 3)) + ")";
        ttf_text = TTF_CreateText(engine, font, text.c_str(), text.size());
        TTF_DrawRendererText(ttf_text, x2 + 2, y2);
        TTF_DestroyText(ttf_text);
      }
    }
  }
}

void display_building_centres(const Tile &tile) {
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, SDL_ALPHA_OPAQUE);
  SDL_FRect pxl;
  pxl.h = pxl.w = 5;
  TTF_Text *text;
  std::string centre_str;
  for (const BuildingShape &building : tile.shapes()) {
    pxl.x = grid_x2pxl(building.approx_centre(0));
    pxl.y = grid_y2pxl(building.approx_centre(1));
    SDL_RenderFillRect(renderer, &pxl);
    if (show_id_opt) {
      text = TTF_CreateText(engine, font, building.osid().c_str(),
                            building.osid().size());
      TTF_DrawRendererText(text, pxl.x + 2 * pxl.w, pxl.y + 15);
      TTF_DestroyText(text);
    }
    if (show_centre_opt) {
      centre_str = "(" + std::to_string(building.approx_centre(0)) + "," +
                   std::to_string(building.approx_centre(1)) + ")";
      text =
          TTF_CreateText(engine, font, centre_str.c_str(), centre_str.size());
      TTF_DrawRendererText(text, pxl.x + 2 * pxl.w, pxl.y);
      TTF_DestroyText(text);
    }
  }
}

void display_test_points(const std::string &file_path, int rgb_red,
                         int rgb_blue) {
  SDL_SetRenderDrawColor(renderer, rgb_red, 0, rgb_blue, SDL_ALPHA_OPAQUE);
  SDL_FRect pxl;
  pxl.h = pxl.w = 5;
  std::ifstream file(file_path, std::ios::in);
  std::string line;
  std::string::const_iterator sep;
  int x, y;
  while (std::getline(file, line)) {
    sep = std::find(line.cbegin(), line.cend(), ',');
    x = atoi(std::string(line.cbegin(), sep).c_str());
    y = atoi(std::string(sep + 1, line.cend()).c_str());
    pxl.x = grid_x2pxl(x);
    pxl.y = grid_y2pxl(y);
    SDL_RenderFillRect(renderer, &pxl);
  }
}

void display_axis() {
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderLine(renderer, grid_x2pxl(0), 0, grid_x2pxl(0), WIN_HEIGHT);
  SDL_RenderLine(renderer, 0, grid_y2pxl(0), WIN_WIDTH, grid_y2pxl(0));
}

int main(int argc, char *argv[]) {
  if (argc < 4 ||
      (strcmp(argv[1], "-a") != 0 && strcmp(argv[1], "--after") != 0 &&
       strcmp(argv[1], "-b") != 0 && strcmp(argv[1], "--before") != 0)) {
    std::cerr << "Usage: visualiser.exe [options] tile_file coords_file"
              << std::endl
              << "option -b, --before" << std::endl
              << "option -a, --after" << std::endl
              << "option -i, display each building's osid" << std::endl
              << "option -e, display edge edge's start and end coords"
              << std::endl
              << "option -c, display each building's centre coords"
              << std::endl;
    return 1;
  }
  before_opt = strcmp(argv[1], "-b") == 0 || strcmp(argv[1], "--before") == 0;
  show_id_opt = show_centre_opt = show_edge_opt = false;
  for (int i = 2; i != argc - 2; i++) {
    if (strcmp(argv[i], "-i") == 0) {
      show_id_opt = true;
    } else if (strcmp(argv[i], "-c") == 0) {
      show_centre_opt = true;
    } else if (strcmp(argv[i], "-e") == 0) {
      show_edge_opt = true;
    } else {
      std::cout << argv[i] << std::endl;
      std::cerr << "Usage: visualiser.exe [options] tile_file coords_file"
                << std::endl
                << "option -b, --before" << std::endl
                << "option -a, --after" << std::endl
                << "option -i, display each building's osid" << std::endl
                << "option -e, display edge edge's start and end coords"
                << std::endl
                << "option -c, display each building's centre coords"
                << std::endl;
      return 1;
    }
  }
  const char *tile_path = argv[argc - 2];
  const char *coords_path = argv[argc - 1];

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s",
                 SDL_GetError());
    return 3;
  }
  if (!SDL_CreateWindowAndRenderer("Hello SDL", WIN_WIDTH, WIN_HEIGHT,
                                   SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Couldn't create window and renderer: %s", SDL_GetError());
    return 3;
  }

  // TTF
  if (!TTF_Init()) {
    SDL_Log("Couldn't initialize SDL_ttf: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  font = TTF_OpenFont(FONT_PATH.c_str(), 12);
  if (font == nullptr) {
    SDL_Log("Couldn't load font: %s\n", SDL_GetError());
    return 3;
  }
  engine = TTF_CreateRendererTextEngine(renderer);

  // get all building shapes
  Tile tile;
  {
    std::ifstream inp(tile_path, std::ios::binary | std::ios::in);
    tile.ParseFromIstream(&inp);
  }

  while (1) {
    SDL_PollEvent(&event);
    if (event.type == SDL_EVENT_QUIT) {
      break;
    }
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    display_axis();
    display_building_outlines(tile);
    if (before_opt) {
      display_building_centres(tile);
      display_test_points(coords_path, 255, 0);
    } else {
      display_test_points(coords_path, 0, 255);
    }
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
