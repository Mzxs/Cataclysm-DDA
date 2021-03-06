
#include "veh_type.h"

#if (defined SDLTILES)
#include <algorithm>
#include "cata_tiles.h"
#include "debug.h"
#include "json.h"
#include <fstream>

// SDL headers end up in different places depending on the OS, sadly
#if (defined OSX_SDL_FW)
#include "SDL_image/SDL_image.h" // Make sure to add this to the other OS inclusions
#else
#include "SDL2/SDL_image.h" // Make sure to add this to the other OS inclusions
#endif

#define ITEM_HIGHLIGHT "highlight_item"

extern game *g;
//extern SDL_Surface *screen;
extern int WindowHeight, WindowWidth;
extern int fontwidth, fontheight;

cata_tiles::cata_tiles(SDL_Renderer *render)
{
    //ctor
    tile_atlas = NULL;
    renderer = NULL;
    tile_values = NULL;
    tile_ids = NULL;
    screen_tiles = NULL;
    renderer = render;

    tile_height = 0;
    tile_width = 0;
    screentile_height = 0;
    screentile_width = 0;
    tile_ratiox = 0;
    tile_ratioy = 0;

    in_animation = false;
    do_draw_explosion = false;
    do_draw_bullet = false;
    do_draw_hit = false;
    do_draw_line = false;
    do_draw_weather = false;
    do_draw_footsteps = false;

    boomered = false;
    sight_impaired = false;
    bionight_bionic_active = false;

    last_pos_x = 0;
    last_pos_y = 0;
}

cata_tiles::~cata_tiles()
{
    //dtor
    // free surfaces
    if (tile_atlas) {
        SDL_FreeSurface(tile_atlas);
    }
    // release maps
    if (tile_values) {
        for (tile_iterator it = tile_values->begin(); it != tile_values->end(); ++it) {
            SDL_DestroyTexture(it->second);
            it->second = NULL;
        }
        tile_values->clear();
        tile_values = NULL;
    }
    if (tile_ids) {
        for (tile_id_iterator it = tile_ids->begin(); it != tile_ids->end(); ++it) {
            it->second = NULL;
        }
        tile_ids->clear();
        tile_ids = NULL;
    }
    if (screen_tiles) {
        for (int i = 0; i < num_tiles; ++i) {
            delete &(screen_tiles[i]);
        }
        screen_tiles = NULL;
    }
}

void cata_tiles::init(std::string json_path, std::string tileset_path)
{
    // load files
    DebugLog() << "Attempting to Load JSON file\n";
    load_tilejson(json_path);
    DebugLog() << "Attempting to Load Tileset file\n";
    load_tileset(tileset_path);
}
void cata_tiles::init(std::string load_file_path)
{
    std::string json_path, tileset_path;
    // get path information from load_file_path
    get_tile_information(load_file_path, json_path, tileset_path);
    // send this information to old init to avoid redundant code
    init(json_path, tileset_path);
}
void cata_tiles::reinit(std::string load_file_path)
{
    std::string json_path, tileset_path;
    get_tile_information(load_file_path, json_path, tileset_path);
    init(json_path, tileset_path);
}

void cata_tiles::get_tile_information(std::string dir_path, std::string &json_path, std::string &tileset_path)
{
    DebugLog() << "Attempting to Initialize JSON and TILESET path information from [" << dir_path << "]\n";
    const std::string filename = "tileset.txt";                 // tileset-information-file
    const std::string default_json = "gfx/tile_config.json";    // defaults
    const std::string default_tileset = "gfx/tinytile.png";

    std::vector<std::string> files;
    files = file_finder::get_files_from_path(filename, dir_path, true);     // search for the files (tileset.txt)

    for(std::vector<std::string>::iterator it = files.begin(); it != files.end(); ++it) {   // iterate through every file found
        std::ifstream fin;
        fin.open(it->c_str());
        if(!fin.is_open()) {
            fin.close();
            DebugLog() << "\tCould not read ." << *it << " -- Setting to default values!\n";
            json_path = default_json;
            tileset_path = default_tileset;
            return;
        }
        // should only have 2 values inside it, otherwise is going to only load the last 2 values
        while(!fin.eof()) {
            std::string sOption;
            fin >> sOption;

            if(sOption == "") {
                getline(fin, sOption);    // Empty line, chomp it
            } else if(sOption[0] == '#') { // # indicates a comment
                getline(fin, sOption);
            } else {

                if (sOption.find("NAME") != std::string::npos) {
                    std::string tileset_name;
                    tileset_name = "";
                    fin >> tileset_name;
                    if(tileset_name != OPTIONS["TILES"].getValue()) {   // if the current tileset name isn't the same
                        // as the current one in the options break
                        break;                                          // out of the while loop, close the file and
                    }                                                   // continue with the next file
                } else if (sOption.find("VIEW") != std::string::npos) { // we don't need the view name here
                    getline(fin, sOption);                              // so we just skip it
                } else if (sOption.find("JSON") != std::string::npos) {
                    fin >> json_path;
                    DebugLog() << "\tJSON path set to [" << json_path << "].\n";
                } else if (sOption.find("TILESET") != std::string::npos) {
                    fin >> tileset_path;
                    DebugLog() << "\tTILESET path set to [" << tileset_path << "].\n";
                }
            }
        }

        fin.close();
    }
    if (json_path == "") {
        json_path = default_json;
        DebugLog() << "\tJSON set to default [" << json_path << "].\n";
    }
    if (tileset_path == "") {
        tileset_path = default_tileset;
        DebugLog() << "\tTILESET set to default [" << tileset_path << "].\n";
    }
}

void cata_tiles::reload_tileset() {
    /* release stored tiles */
    if (tile_values) {
        for (tile_iterator it = tile_values->begin(); it != tile_values->end(); ++it) {
            SDL_DestroyTexture(it->second);
            it->second = NULL;
        }
        tile_values->clear();
    }

    /** Check to make sure the tile_atlas loaded correctly, will be NULL if didn't load */
    if (tile_atlas) {
        /** get dimensions of the atlas image */
        int w = tile_atlas->w;
        int h = tile_atlas->h;
        /** sx and sy will take care of any extraneous pixels that do not add up to a full tile */
        int sx = w / tile_width;
        int sy = h / tile_height;

        sx *= tile_width;
        sy *= tile_height;

        // Set up initial source and destination information. Destination is going to be unchanging
        SDL_Rect source_rect = {0,0,tile_width,tile_height};
        SDL_Rect dest_rect = {0,0,tile_width,tile_height};

        /** split the atlas into tiles using SDL_Rect structs instead of slicing the atlas into individual surfaces */
        int tilecount = 0;
        for (int y = 0; y < sy; y += tile_height) {
            for (int x = 0; x < sx; x += tile_width) {
                source_rect.x = x;
                source_rect.y = y;

                SDL_Surface *tile_surf = create_tile_surface();
                SDL_BlitSurface(tile_atlas, &source_rect, tile_surf, &dest_rect);

                SDL_Texture *tile_tex = SDL_CreateTextureFromSurface(renderer,tile_surf);

                SDL_FreeSurface(tile_surf);

                if (!tile_values) {
                    tile_values = new tile_map;
                }

                (*tile_values)[tilecount++] = tile_tex;
            }
        }

        DebugLog() << "Tiles Created: " << tilecount << "\n";
    }
}

void cata_tiles::set_draw_scale(int scale) {
    /* release tile_atlas from memory if it has already been initialized */

    tile_width = default_tile_width * scale / 16;
    tile_height = default_tile_height * scale / 16;

    tile_ratiox = ((float)tile_width/(float)fontwidth);
    tile_ratioy = ((float)tile_height/(float)fontheight);

    screentile_width =  (int)(terrain_term_x / tile_ratiox) + 1;
    screentile_height = (int)(terrain_term_y / tile_ratioy) + 1;
}

void cata_tiles::load_tileset(std::string path)
{
    /* release tile_atlas from memory if it has already been initialized */
    if (tile_atlas) {
        SDL_FreeSurface(tile_atlas);
    }

    terrain_term_x = OPTIONS["TERMINAL_X"] - ((OPTIONS["SIDEBAR_STYLE"] == "narrow") ? 45 : 55);
    terrain_term_y = OPTIONS["TERMINAL_Y"];
	
    set_draw_scale(16);

    /** reinit tile_atlas */
    tile_atlas = IMG_Load(path.c_str());

    if(!tile_atlas) {
        DebugLog() << "Could not load tileset file " << path << "\nError: " << IMG_GetError() << "\n";
        throw std::string("failed to open tileset image");
    }

    reload_tileset();
}

void cata_tiles::load_tilejson(std::string path)
{
    std::ifstream config_file(path.c_str(), std::ifstream::in | std::ifstream::binary);

    if (!config_file.good()) {
        //throw (std::string)"ERROR: " + path + (std::string)" could not be read.";
        DebugLog() << "ERROR: " << path << " could not be read.\n";
        throw std::string("failed to open tile info json");
    }

    load_tilejson_from_file( config_file );
}

void cata_tiles::load_tilejson_from_file(std::ifstream &f)
{
    JsonIn config_json(f);
    // it's all one json object
    JsonObject config = config_json.get_object();

    if (tile_ids) {
        for (tile_id_iterator it = tile_ids->begin(); it != tile_ids->end(); ++it) {
            delete it->second;
        }
        tile_ids->clear();
    }

    /** 1) Make sure that the loaded file has the "tile_info" section */
    if (!config.has_member("tile_info")) {
        throw (std::string)"ERROR: \"tile_info\" missing\n";
    }

    JsonArray info = config.get_array("tile_info");
    while (info.has_more()) {
        JsonObject curr_info = info.next_object();
        tile_height = curr_info.get_int("height");
        tile_width = curr_info.get_int("width");

        default_tile_width = tile_width;
        default_tile_height = tile_height;
    }

    /** 2) Load tile information if available */
    if (!config.has_member("tiles")) {
        throw (std::string)"ERROR: \"tiles\" section missing\n";
    }

    JsonArray tiles = config.get_array("tiles");
    while (tiles.has_more()) {
        JsonObject entry = tiles.next_object();

        tile_type *curr_tile = new tile_type();
        std::string t_id = entry.get_string("id");
        int t_fg = entry.get_int("fg", -1);
        int t_bg = entry.get_int("bg", -1);
        bool t_multi = entry.get_bool("multitile", false);
        bool t_rota = entry.get_bool("rotates", t_multi);
        if (t_multi) {
            // fetch additional tiles
            JsonArray subentries = entry.get_array("additional_tiles");
            while (subentries.has_more()) {
                JsonObject subentry = subentries.next_object();

                tile_type *curr_subtile = new tile_type();
                std::string s_id = subentry.get_string("id");
                int s_fg = subentry.get_int("fg", 0);
                int s_bg = subentry.get_int("bg", 0);

                curr_subtile->fg = s_fg;
                curr_subtile->bg = s_bg;
                curr_subtile->rotates = true;
                std::string m_id = t_id + "_" + s_id;

                if (!tile_ids) {
                    tile_ids = new tile_id_map;
                }
                (*tile_ids)[m_id] = curr_subtile;
                curr_tile->available_subtiles.push_back(s_id);
            }
        }

        // write the information of the base tile to curr_tile
        curr_tile->bg = t_bg;
        curr_tile->fg = t_fg;
        curr_tile->multitile = t_multi;
        curr_tile->rotates = t_rota;

        if (!tile_ids) {
            tile_ids = new tile_id_map;
        }
        (*tile_ids)[t_id] = curr_tile;
    }
    DebugLog() << "Tile Width: " << tile_width << " Tile Height: " << tile_height << " Tile Definitions: " << tile_ids->size() << "\n";
}

void cata_tiles::draw(int destx, int desty, int centerx, int centery, int width, int height)
{
    if (!g) {
        return;
    }

    {
        //set clipping to prevent drawing over stuff we shouldn't
        SDL_Rect clipRect = {destx,desty,width,height};
        SDL_RenderSetClipRect(renderer,&clipRect);
    }

    int posx = centerx;
    int posy = centery;

    int sx, sy;
    get_window_tile_counts(width, height, sx, sy);

    init_light();

    int x, y;
    LIGHTING l;

    o_x = posx - sx / 2;
    o_y = posy - sy / 2;

    for (int my = 0; my < sy; ++my) {
        for (int mx = 0; mx < sx; ++mx) {
            x = mx + o_x;
            y = my + o_y;
            l = light_at(x, y);

            if (l != CLEAR) {
                // Draw lighting
                draw_lighting(x, y, l);
                // continue on to next part of loop
                continue;
            }
            // light is no longer being considered, for now.
            // Draw Terrain if possible. If not possible then we need to continue on to the next part of loop
            if (!draw_terrain(x, y)) {
                continue;
            }
            draw_furniture(x, y);
            draw_trap(x, y);
            draw_field_or_item(x, y);
            draw_vpart(x, y);
            draw_entity(x, y);
        }
    }
    in_animation = do_draw_explosion || do_draw_bullet || do_draw_hit || do_draw_line || do_draw_weather || do_draw_footsteps;
    if (in_animation) {
        if (do_draw_explosion) {
            draw_explosion_frame();
        }
        if (do_draw_bullet) {
            draw_bullet_frame();
        }
        if (do_draw_hit) {
            draw_hit_frame();
            void_hit();
        }
        if (do_draw_line) {
            draw_line();
            void_line();
        }
        if (do_draw_weather) {
            draw_weather_frame();
            void_weather();
        }
        if (do_draw_footsteps) {
            draw_footsteps_frame();
            void_footsteps();
        }
    }
    // check to see if player is located at ter
    else if (g->u.posx + g->u.view_offset_x != g->ter_view_x ||
             g->u.posy + g->u.view_offset_y != g->ter_view_y) {
        draw_from_id_string("cursor", "misc", "", g->ter_view_x, g->ter_view_y, 0, 0);
    }

    SDL_RenderSetClipRect(renderer,NULL);
}

void cata_tiles::clear_buffer()
{
    //TODO convert this to use sdltiles ClearScreen() function
    SDL_RenderClear(renderer);
}

void cata_tiles::get_window_tile_counts(const int width, const int height, int &columns, int &rows) const
{
    columns = ceil((double) width / tile_width);
    rows = ceil((double) height / tile_height);
}

int cata_tiles::get_tile_width() const
{
    return tile_width;
}


bool cata_tiles::draw_from_id_string(std::string id, int x, int y, int subtile, int rota, bool is_at_screen_position)
{
    return cata_tiles::draw_from_id_string(id, "", "", x, y, subtile, rota, is_at_screen_position);
}

bool cata_tiles::draw_from_id_string(std::string id, std::string category, std::string subcategory, int x, int y, int subtile, int rota, bool is_at_screen_position)
{
    // For the moment, if the ID string does not produce a drawable tile it will revert to the "unknown" tile.
    // The "unknown" tile is one that is highly visible so you kinda can't miss it :D

    // check to make sure that we are drawing within a valid area [0->width|height / tile_width|height]
    if (is_at_screen_position && (x < 0 || x > screentile_width || y < 0 || y > screentile_height)) {
        return false;
    } else if (!is_at_screen_position && ((x - o_x) < 0 || (x - o_x) > screentile_width || (y - o_y) < 0 || (y - o_y) > screentile_height)) {
        return false;
    }

    tile_id_iterator it = tile_ids->find(id);

    // if id is not found, try to find a tile for the category+subcategory combination
    if (it == tile_ids->end()) {
        if(category != "" && subcategory != "") {
            it = tile_ids->find("unknown_"+category+"_"+subcategory);
        }
    }

    // if at this point we have no tile, try just the category
    if (it == tile_ids->end()) {
        if(category != "") {
            it = tile_ids->find("unknown_"+category);
        }
    }

    // if we still have no tile, we're out of luck, fall back to unknown
    if (it == tile_ids->end()) {
        it = tile_ids->find("unknown");
    }

    //  this really shouldn't happen, but the tileset creator might have forgotten to define an unknown tile
    if (it == tile_ids->end()) {
        debugmsg("The tileset you're using has no 'unknown' tile defined!");
    }

    tile_type *display_tile = it->second;
    // if found id does not have a valid tile_type then return unknown tile
    if (!display_tile) {
        return draw_from_id_string("unknown", x, y, subtile, rota, is_at_screen_position);
    }

    // if both bg and fg are -1 then return unknown tile
    if (display_tile->bg == -1 && display_tile->fg == -1) {
        return draw_from_id_string("unknown", x, y, subtile, rota, is_at_screen_position);
    }

    // check to see if the display_tile is multitile, and if so if it has the key related to subtile
    if (subtile != -1 && display_tile->multitile) {
        std::vector<std::string> display_subtiles = display_tile->available_subtiles;
        if (std::find(display_subtiles.begin(), display_subtiles.end(), multitile_keys[subtile]) != display_subtiles.end()) {
            // append subtile name to tile and re-find display_tile
            id += "_" + multitile_keys[subtile];
            //DebugLog() << "<"<< id << ">\n";
            return draw_from_id_string(id, x, y, -1, rota);
        }
    }

    // make sure we aren't going to rotate the tile if it shouldn't be rotated
    if (!display_tile->rotates) {
        rota = 0;
    }

    // translate from player-relative to screen relative tile position
    int screen_x;// = (x - o_x) * tile_width;
    int screen_y;// = (y - o_y) * tile_height;
    if (!is_at_screen_position) {
        screen_x = (x - o_x) * tile_width;
        screen_y = (y - o_y) * tile_height;
    } else {
        screen_x = x * tile_width;
        screen_y = y * tile_width;
    }

    //draw it!
    draw_tile_at(display_tile,screen_x,screen_y,rota);

    return true;
}

bool cata_tiles::draw_tile_at(tile_type *tile, int x, int y, int rota)
{
    // don't need to check for tile existance, should always exist if it gets this far
    const int fg = tile->fg;
    const int bg = tile->bg;

    SDL_Rect destination;
    destination.x = x;
    destination.y = y;
    destination.w = tile_width;
    destination.h = tile_height;

    // blit background first : always non-rotated
    if (bg >= 0 && bg < tile_values->size()) {
        SDL_Texture *bg_tex = (*tile_values)[bg];
        SDL_RenderCopy(renderer, bg_tex, NULL, &destination);
    }

    // blit foreground based on rotation
    if (rota == 0) {
        if (fg >= 0 && fg < tile_values->size()) {
            SDL_Texture *fg_tex = (*tile_values)[fg];
            SDL_RenderCopy(renderer, fg_tex, NULL, &destination);
        }
    } else {
        if (fg >= 0 && fg < tile_values->size()) {
            SDL_Texture *fg_tex = (*tile_values)[fg];

            if(rota == 1) {
#if (defined _WIN32 || defined WINDOWS)
                destination.y -= 1;
#endif
                SDL_RenderCopyEx(renderer, fg_tex, NULL, &destination,
                    -90, NULL, SDL_FLIP_NONE );
            } else if(rota == 2) {
                //flip rather then rotate here
                SDL_RenderCopyEx(renderer, fg_tex, NULL, &destination,
                    0, NULL, (SDL_RendererFlip)(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL) );
            } else { //rota == 3
#if (defined _WIN32 || defined WINDOWS)
                destination.x -= 1;
#endif
                SDL_RenderCopyEx(renderer, fg_tex, NULL, &destination,
                    90, NULL, SDL_FLIP_NONE );
            }
        }
    }

    return true;
}

bool cata_tiles::draw_lighting(int x, int y, LIGHTING l)
{
    std::string light_name;
    switch(l) {
        case HIDDEN:
            light_name = "lighting_hidden";
            break;
        case LIGHT_NORMAL:
            light_name = "lighting_lowlight_light";
            break;
        case LIGHT_DARK:
            light_name = "lighting_lowlight_dark";
            break;
        case BOOMER_NORMAL:
            light_name = "lighting_boomered_light";
            break;
        case BOOMER_DARK:
            light_name = "lighting_boomered_dark";
            break;
    }

    // lighting is never rotated, though, could possibly add in random rotation?
    draw_from_id_string(light_name, "lighting", "", x, y, 0, 0);

    return false;
}

bool cata_tiles::draw_terrain(int x, int y)
{
    int t = g->m.ter(x, y); // get the ter_id value at this point
    // check for null, if null return false
    if (t == t_null) {
        return false;
    }

    // need to check for walls, and then deal with wallfication details!
    int s = terlist[t].sym;

    //char alteration = 0;
    int subtile = 0, rotation = 0;

    // check walls
    if (s == LINE_XOXO /*vertical*/ || s == LINE_OXOX /*horizontal*/) {
        get_wall_values(x, y, LINE_XOXO, LINE_OXOX, subtile, rotation);
    }
    // check windows and doors for wall connections, may or may not have a subtile available, but should be able to rotate to some extent
    else if (s == '"' || s == '+' || s == '\'') {
        get_wall_values(x, y, LINE_XOXO, LINE_OXOX, subtile, rotation);
    } else {
        get_terrain_orientation(x, y, rotation, subtile);
        // do something to get other terrain orientation values
    }

    std::string tname;

    tname = terlist[t].id;

    return draw_from_id_string(tname, "terrain", "", x, y, subtile, rotation);
}

bool cata_tiles::draw_furniture(int x, int y)
{
    // get furniture ID at x,y
    bool has_furn = g->m.has_furn(x, y);
    if (!has_furn) {
        return false;
    }

    int f_id = g->m.furn(x, y);

    // for rotation inforomation
    const int neighborhood[4] = {
        static_cast<int> (g->m.furn(x, y + 1)), // south
        static_cast<int> (g->m.furn(x + 1, y)), // east
        static_cast<int> (g->m.furn(x - 1, y)), // west
        static_cast<int> (g->m.furn(x, y - 1)) // north
    };

    int subtile = 0, rotation = 0;
    get_tile_values(f_id, neighborhood, subtile, rotation);

    // get the name of this furniture piece
    std::string f_name = furnlist[f_id].id; // replace with furniture names array access
    bool ret = draw_from_id_string(f_name, "furniture", "", x, y, subtile, rotation);
    if (ret && g->m.sees_some_items(x, y, g->u)) {
        draw_item_highlight(x, y);
    }
    return ret;
}

bool cata_tiles::draw_trap(int x, int y)
{
    int tr_id = g->m.tr_at(x, y);
    if (tr_id == tr_null) {
        return false;
    }

    const std::string tr_name = g->traps[tr_id]->id;

    const int neighborhood[4] = {
        static_cast<int> (g->m.tr_at(x, y + 1)), // south
        static_cast<int> (g->m.tr_at(x + 1, y)), // east
        static_cast<int> (g->m.tr_at(x - 1, y)), // west
        static_cast<int> (g->m.tr_at(x, y - 1)) // north
    };

    int subtile = 0, rotation = 0;
    get_tile_values(tr_id, neighborhood, subtile, rotation);

    return draw_from_id_string(tr_name, "trap", "", x, y, subtile, rotation);
}

bool cata_tiles::draw_field_or_item(int x, int y)
{
    // check for field
    field &f = g->m.field_at(x, y);
    // check for items
    const std::vector<item> &items = g->m.i_at(x, y);
    field_id f_id = f.fieldSymbol();
    bool is_draw_field;
    bool do_item;
    switch(f_id) {
        case fd_null:
            //only draw items
            is_draw_field = false;
            do_item = true;
            break;
        case fd_blood:
        case fd_blood_veggy:
        case fd_blood_insect:
        case fd_blood_invertebrate:
        case fd_gibs_flesh:
        case fd_gibs_veggy:
        case fd_gibs_insect:
        case fd_gibs_invertebrate:
        case fd_bile:
        case fd_slime:
        case fd_acid:
        case fd_sap:
        case fd_sludge:
            //need to draw fields and items both
            is_draw_field = true;
            do_item = true;
            break;
        default:
            //only draw fields
            do_item = false;
            is_draw_field = true;
            break;
    }
    bool ret_draw_field = true;
    bool ret_draw_item = true;
    if (is_draw_field) {
        std::string fd_name = field_names[f.fieldSymbol()];

        // for rotation inforomation
        const int neighborhood[4] = {
            static_cast<int> (g->m.field_at(x, y + 1).fieldSymbol()), // south
            static_cast<int> (g->m.field_at(x + 1, y).fieldSymbol()), // east
            static_cast<int> (g->m.field_at(x - 1, y).fieldSymbol()), // west
            static_cast<int> (g->m.field_at(x, y - 1).fieldSymbol()) // north
        };

        int subtile = 0, rotation = 0;
        get_tile_values(f.fieldSymbol(), neighborhood, subtile, rotation);

        ret_draw_field = draw_from_id_string(fd_name, "field", "", x, y, subtile, rotation);
    }
    if(do_item) {
        if (!g->m.sees_some_items(x, y, g->u)) {
            return false;
        }
        // get the last item in the stack, it will be used for display
        const item &display_item = items[items.size() - 1];
        // get the item's name, as that is the key used to find it in the map
        const std::string &it_name = display_item.type->id;
        const std::string it_category = display_item.type->get_item_type_string();
        ret_draw_item = draw_from_id_string(it_name, "item", it_category, x, y, 0, 0);
        if (ret_draw_item && items.size() > 1) {
            draw_item_highlight(x, y);
        }
    }
    return ret_draw_field && ret_draw_item;
}

bool cata_tiles::draw_vpart(int x, int y)
{
    int veh_part = 0;
    vehicle *veh = g->m.veh_at(x, y, veh_part);

    if (!veh) {
        return false;
    }
    // veh_part is the index of the part
    // get a north-east-south-west value instead of east-south-west-north value to use with rotation
    int veh_dir = (veh->face.dir4() + 1) % 4;
    if (veh_dir == 1 || veh_dir == 3) {
        veh_dir = (veh_dir + 2) % 4;
    }

    // Gets the visible part, should work fine once tileset vp_ids are updated to work with the vehicle part json ids
    // get the vpart_id
    char part_mod = 0;
    std::string vpid = veh->part_id_string(veh_part, part_mod);

    // prefix with vp_ ident
    vpid = "vp_" + vpid;
    int subtile = 0;
    if (part_mod > 0) {
        switch (part_mod) {
            case 1:
                subtile = open_;
                break;
            case 2:
                subtile = broken;
                break;
        }
    }
    int cargopart = veh->part_with_feature(veh_part, "CARGO");
    bool draw_highlight = (cargopart > 0) && (!veh->parts[cargopart].items.empty());
    bool ret = draw_from_id_string(vpid, "vehicle_part", "", x, y, subtile, veh_dir);
    if (ret && draw_highlight) {
        draw_item_highlight(x, y);
    }
    return ret;
}

bool cata_tiles::draw_entity(int x, int y)
{
    // figure out what entity exists at x,y
    std::string ent_name;
    std::string ent_category = "";
    std::string ent_subcategory = "";
    bool entity_here = false;
    // check for monster (most common)
    if (!entity_here && g->mon_at(x, y) >= 0) {
        monster m = g->zombie(g->mon_at(x, y));
        if (!m.dead) {
            ent_name = m.type->id;
            ent_category = "monster";
            if(m.type->species.size() >= 1) {
                ent_subcategory = *(m.type->species.begin());
            }
            entity_here = true;
        }
    }
    // check for NPC (next most common)
    if (!entity_here && g->npc_at(x, y) >= 0) {
        npc &m = *g->active_npc[g->npc_at(x, y)];
        if (!m.dead) {
            ent_name = m.male ? "npc_male" : "npc_female";
            entity_here = true;
        }
    }
    // check for PC (least common, only ever 1)
    if (!entity_here && g->u.posx == x && g->u.posy == y) {
        ent_name = g->u.male ? "player_male" : "player_female";
        entity_here = true;
    }
    if (entity_here) {
        int subtile = corner;
        return draw_from_id_string(ent_name, ent_category, ent_subcategory, x, y, subtile, 0);
    }
    return false;
}

bool cata_tiles::draw_item_highlight(int x, int y)
{
    bool item_highlight_available = tile_ids->find(ITEM_HIGHLIGHT) != tile_ids->end();

    if (!item_highlight_available) {
        create_default_item_highlight();
        item_highlight_available = true;
    }
    return draw_from_id_string(ITEM_HIGHLIGHT, "", "", x, y, 0, 0);
}

SDL_Surface *cata_tiles::create_tile_surface() {
    SDL_Surface *surface;
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        surface = SDL_CreateRGBSurface(0, tile_width, tile_height, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    #else
        surface = SDL_CreateRGBSurface(0, tile_width, tile_height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    #endif
    return surface;
}

void cata_tiles::create_default_item_highlight()
{
    const Uint8 highlight_alpha = 127;

    std::string key = ITEM_HIGHLIGHT;
    int index = tile_values->size();

    SDL_Surface *surface = create_tile_surface();
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 127, highlight_alpha));
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer,surface);
    SDL_FreeSurface(surface);

    (*tile_values)[index] = texture;
    tile_type *type = new tile_type;
    type->fg = index;
    type->bg = -1;
    (*tile_ids)[key] = type;
}

/* Animation Functions */
/* -- Inits */
void cata_tiles::init_explosion(int x, int y, int radius)
{
    do_draw_explosion = true;
    exp_pos_x = x;
    exp_pos_y = y;
    exp_rad = radius;
}
void cata_tiles::init_draw_bullet(int x, int y, std::string name)
{
    do_draw_bullet = true;
    bul_pos_x = x;
    bul_pos_y = y;
    bul_id = name;
}
void cata_tiles::init_draw_hit(int x, int y, std::string name)
{
    do_draw_hit = true;
    hit_pos_x = x;
    hit_pos_y = y;
    hit_entity_id = name;
}
void cata_tiles::init_draw_line(int x, int y, std::vector<point> trajectory, std::string name, bool target_line)
{
    do_draw_line = true;
    is_target_line = target_line;
    line_pos_x = x;
    line_pos_y = y;
    line_endpoint_id = name;
    line_trajectory = trajectory;
}
void cata_tiles::init_draw_weather(weather_printable weather, std::string name)
{
    do_draw_weather = true;
    weather_name = name;
    anim_weather = weather;
}
void cata_tiles::init_draw_footsteps(std::queue<point> steps)
{
    do_draw_footsteps = true;
    footsteps = steps;
}
/* -- Void Animators */
void cata_tiles::void_explosion()
{
    do_draw_explosion = false;
    exp_pos_x = -1;
    exp_pos_y = -1;
    exp_rad = -1;
}
void cata_tiles::void_bullet()
{
    do_draw_bullet = false;
    bul_pos_x = -1;
    bul_pos_y = -1;
    bul_id = "";
}
void cata_tiles::void_hit()
{
    do_draw_hit = false;
    hit_pos_x = -1;
    hit_pos_y = -1;
    hit_entity_id = "";
}
void cata_tiles::void_line()
{
    do_draw_line = false;
    is_target_line = false;
    line_pos_x = -1;
    line_pos_y = -1;
    line_endpoint_id = "";
    line_trajectory.clear();
}
void cata_tiles::void_weather()
{
    do_draw_weather = false;
    weather_name = "";
    anim_weather.vdrops.clear();
}
void cata_tiles::void_footsteps()
{
    do_draw_footsteps = false;
}
/* -- Animation Renders */
void cata_tiles::draw_explosion_frame()
{
    std::string exp_name = "explosion";
    int subtile, rotation;
    const int mx = exp_pos_x, my = exp_pos_y;

    for (int i = 1; i < exp_rad; ++i) {
        subtile = corner;
        rotation = 0;

        draw_from_id_string(exp_name, mx - i, my - i, subtile, rotation++);
        draw_from_id_string(exp_name, mx - i, my + i, subtile, rotation++);
        draw_from_id_string(exp_name, mx + i, my + i, subtile, rotation++);
        draw_from_id_string(exp_name, mx + i, my - i, subtile, rotation++);

        subtile = edge;
        for (int j = 1 - i; j < 0 + i; j++) {
            rotation = 0;
            draw_from_id_string(exp_name, mx + j, my - i, subtile, rotation);
            draw_from_id_string(exp_name, mx + j, my + i, subtile, rotation);

            rotation = 1;
            draw_from_id_string(exp_name, mx - i, my + j, subtile, rotation);
            draw_from_id_string(exp_name, mx + i, my + j, subtile, rotation);
        }
    }
}
void cata_tiles::draw_bullet_frame()
{
    const int mx = bul_pos_x, my = bul_pos_y;

    draw_from_id_string(bul_id, "bullet", "", mx, my, 0, 0);
}
void cata_tiles::draw_hit_frame()
{
    const int mx = hit_pos_x, my = hit_pos_y;
    std::string hit_overlay = "animation_hit";

    draw_from_id_string(hit_entity_id, "hit_entity", "", mx, my, 0, 0);
    draw_from_id_string(hit_overlay, mx, my, 0, 0);
}
void cata_tiles::draw_line()
{
    int mx = line_pos_x, my = line_pos_y;
    std::string line_overlay = "animation_line";
    if (!is_target_line || g->u_see(mx, my)) {
        for (int i = 0; i < line_trajectory.size() - 1; i++) {
            mx = line_trajectory[i].x;
            my = line_trajectory[i].y;

            draw_from_id_string(line_overlay, mx, my, 0, 0);
        }
    }
    mx = line_trajectory[line_trajectory.size() - 1].x;
    my = line_trajectory[line_trajectory.size() - 1].y;

    draw_from_id_string(line_endpoint_id, mx, my, 0, 0);
}
void cata_tiles::draw_weather_frame()
{
    for (std::vector<std::pair<int, int> >::iterator weather_iterator = anim_weather.vdrops.begin();
            weather_iterator != anim_weather.vdrops.end();
            ++weather_iterator) {
        // currently in ascii screen coordinates
        int x = weather_iterator->first;
        int y = weather_iterator->second;

        x = x + g->ter_view_x - getmaxx(g->w_terrain) / 2;
        y = y + g->ter_view_y - getmaxy(g->w_terrain) / 2;

        draw_from_id_string(weather_name, "weather", "", x, y, 0, 0, false);
    }
}
void cata_tiles::draw_footsteps_frame()
{
    const std::string footstep_tilestring = "footstep";
    while (!footsteps.empty()) {
        point p = footsteps.front();
        footsteps.pop();

        int x = p.x;
        int y = p.y;

        draw_from_id_string(footstep_tilestring, x, y, 0, 0, false);
    }
}
/* END OF ANIMATION FUNCTIONS */

void cata_tiles::init_light()
{
    g->m.build_map_cache();
    g->reset_light_level();

    sightrange_natural = g->u.sight_range(1);
    g_lightlevel = (int)g->light_level();
    sightrange_light = g->u.sight_range(g_lightlevel);
    sightrange_lowlight = std::max(g_lightlevel / 2, sightrange_natural);
    sightrange_max = g->u.unimpaired_range();
    u_clairvoyance = g->u.clairvoyance();

    boomered = g->u.has_disease("boomered");
    sight_impaired = g->u.sight_impaired();
    bionight_bionic_active = g->u.has_active_bionic("bio_night");
}

LIGHTING cata_tiles::light_at(int x, int y)
{
    /** Logic */
    const int dist = rl_dist(g->u.posx, g->u.posy, x, y);

    int real_max_sight_range = sightrange_light > sightrange_max ? sightrange_light : sightrange_max;
    int distance_to_look = real_max_sight_range;

    distance_to_look = DAYLIGHT_LEVEL;

    bool can_see = g->m.pl_sees(g->u.posx, g->u.posy, x, y, distance_to_look);
    lit_level lit = g->m.light_at(x, y);

    if (lit != LL_BRIGHT && dist > real_max_sight_range) {
        int intlit = (int)lit - (dist - real_max_sight_range) / 2;
        if (intlit < 0) {
            intlit = LL_DARK;
        }
        lit = (lit_level)intlit;
    }

    if (lit > LL_DARK && real_max_sight_range > 1) {
        real_max_sight_range = distance_to_look;
    }

    /** Conditional Returns */
    if ((bionight_bionic_active && dist < 15 && dist > sightrange_natural) ||
            dist > real_max_sight_range ||
            (dist > sightrange_light &&
             (lit == LL_DARK ||
              (sight_impaired && lit != LL_BRIGHT) ||
              !can_see))) {
        if (boomered) {
            // exit w/ dark boomerfication
            return BOOMER_DARK;
        } else {
            // exit w/ dark normal
            return LIGHT_DARK;
        }
    } else if (dist > sightrange_light && sight_impaired && lit == LL_BRIGHT) {
        if (boomered) {
            // exit w/ light boomerfication
            return BOOMER_NORMAL;
        } else {
            // exit w/ light normal
            return LIGHT_NORMAL;
        }
    } else if (dist <= u_clairvoyance || can_see) {
        // check for rain

        // return with okay to draw the square = 0
        return CLEAR;
    }

    return HIDDEN;
}

void cata_tiles::get_terrain_orientation(int x, int y, int &rota, int &subtile)
{
    // get terrain at x,y
    ter_id tid = g->m.ter(x, y);
    if (tid == t_null) {
        subtile = 0;
        rota = 0;
        return;
    }

    // get terrain neighborhood
    const ter_id neighborhood[4] = {
        g->m.ter(x, y + 1), // south
        g->m.ter(x + 1, y), // east
        g->m.ter(x - 1, y), // west
        g->m.ter(x, y - 1) // north
    };

    bool connects[4];
    char val = 0;
    int num_connects = 0;

    // populate connection information
    for (int i = 0; i < 4; ++i) {
        connects[i] = (neighborhood[i] == tid);

        if (connects[i]) {
            ++num_connects;
            val += 1 << i;
        }
    }

    get_rotation_and_subtile(val, num_connects, rota, subtile);
}
void cata_tiles::get_rotation_and_subtile(const char val, const int num_connects, int &rotation, int &subtile)
{
    switch(num_connects) {
        case 0:
            rotation = 0;
            subtile = unconnected;
            break;
        case 4:
            rotation = 0;
            subtile = center;
            break;
        case 1: // all end pieces
            subtile = end_piece;
            switch(val) {
                case 8:
                    rotation = 2;
                    break;
                case 4:
                    rotation = 3;
                    break;
                case 2:
                    rotation = 1;
                    break;
                case 1:
                    rotation = 0;
                    break;
            }
            break;
        case 2:
            switch(val) {
                    // edges
                case 9:
                    subtile = edge;
                    rotation = 0;
                    break;
                case 6:
                    subtile = edge;
                    rotation = 1;
                    break;
                    // corners
                case 12:
                    subtile = corner;
                    rotation = 2;
                    break;
                case 10:
                    subtile = corner;
                    rotation = 1;
                    break;
                case 3:
                    subtile = corner;
                    rotation = 0;
                    break;
                case 5:
                    subtile = corner;
                    rotation = 3;
                    break;
            }
            break;
        case 3: // all t_connections
            subtile = t_connection;
            switch(val) {
                case 14:
                    rotation = 2;
                    break;
                case 11:
                    rotation = 1;
                    break;
                case 7:
                    rotation = 0;
                    break;
                case 13:
                    rotation = 3;
                    break;
            }
            break;
    }
}
void cata_tiles::get_wall_values(const int x, const int y, const long vertical_wall_symbol, const long horizontal_wall_symbol, int &subtile, int &rotation)
{
    // makes the assumption that x,y is a wall | window | door of some sort
    const long neighborhood[4] = {
        terlist[g->m.ter(x, y + 1)].sym, // south
        terlist[g->m.ter(x + 1, y)].sym, // east
        terlist[g->m.ter(x - 1, y)].sym, // west
        terlist[g->m.ter(x, y - 1)].sym // north
    };

    bool connects[4];

    char val = 0;
    int num_connects = 0;

    // populate connection information
    for (int i = 0; i < 4; ++i) {
        connects[i] = (neighborhood[i] == vertical_wall_symbol || neighborhood[i] == horizontal_wall_symbol) || (neighborhood[i] == '"' || neighborhood[i] == '+' || neighborhood[i] == '\'');

        if (connects[i]) {
            ++num_connects;
            val += 1 << i;
        }
    }
    get_rotation_and_subtile(val, num_connects, rotation, subtile);
}

void cata_tiles::get_tile_values(const int t, const int *tn, int &subtile, int &rotation)
{
    bool connects[4];
    int num_connects = 0;
    char val = 0;
    for (int i = 0; i < 4; ++i) {
        connects[i] = (tn[i] == t);
        if (connects[i]) {
            ++num_connects;
            val += 1 << i;
        }
    }
    get_rotation_and_subtile(val, num_connects, rotation, subtile);
}

#endif // SDL_TILES
