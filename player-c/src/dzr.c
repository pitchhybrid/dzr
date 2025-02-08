#include "dzr.h"
#include "requests.h"

#include <string.h>
#include <wchar.h>


enum Commands {
    CTRL_D = 4,
    COMMAND = ':',
    UP = KEY_UP,
    DOWN = KEY_DOWN,
    LEFT = KEY_LEFT,
    RIGHT = KEY_RIGHT,
    ENTER = 10,
    PAGE_UP = KEY_NPAGE,
    PAGE_DOWN = KEY_PPAGE,
    SELECT = ' ',

    TRACK = 't',
    ALBUM = 'b',
    ARTIST = 'a',
    PLAYLIST = 'p',
    USER = 'u',
    GENRE = 'g',
    RADIO = 'r'
};

typedef struct {
    Menu_Options on;
    Menu_Options off;
} Menu_Options_Seeting;

void create_menu(window_t *w, Menu_Options_Seeting options);

void destroy_menu(window_t *w);

void drive_menu(window_t *w, int key);

void addLabel(WINDOW *win, char *str);

char *search_input();

char *int_to_string(char *format, int i);

int search_api(const char *path, window_t *w, Menu_Options_Seeting options);

void create_win(window_t *w);
void free_window(window_t *w);
void clear_and_write(window_t *w, char *str);


// https://pubs.opengroup.org/onlinepubs/7908799/xcurses/intovix.html
// http://graysoftinc.com/terminal-tricks/curses-windows-pads-and-panels

int main(void) { // int argc, char **argv

    init_curses();

    // Menu_Options_Seeting playlist_options = {
    //     .on = O_ONEVALUE,
    //     .off = O_SHOWDESC
    // };

    Menu_Options_Seeting painel_options = {.on = O_ONEVALUE, .off = O_SHOWDESC};

    window_t *playlist_w = calloc(1, sizeof(window_t));
    strcpy(playlist_w->label, "Playlist");
    playlist_w->y = yMax() - 1;
    playlist_w->x = xDiv();
    playlist_w->starty = 0;
    playlist_w->startx = 0;
    
    window_t *painel_w = calloc(1, sizeof(window_t));
    strcpy(painel_w->label, "Painel");
    painel_w->y = yMax() - 1;
    painel_w->x = xMax() - xDiv();
    painel_w->starty = 0;
    painel_w->startx = xDiv();

    create_win(playlist_w);

    create_win(painel_w);

    int ch;
    while ((ch = getch()) != CTRL_D) {
        DEBUG("key: %d char: %c", ch, ch);
        switch (ch) {
        case UP: {
            DEBUG("up");
            drive_menu(painel_w, REQ_UP_ITEM);
            break;
        }
        case DOWN: {
            DEBUG("down");
            drive_menu(painel_w, REQ_DOWN_ITEM);
            break;
        }
        case PAGE_UP: {
            DEBUG("previous page");
            drive_menu(painel_w, REQ_SCR_DPAGE);
            break;
        }
        case PAGE_DOWN: {
            DEBUG("next page");
            drive_menu(painel_w, REQ_SCR_UPAGE);
            break;
        }
        case LEFT: {
            DEBUG("left");
            break;
        }
        case RIGHT: {
            DEBUG("right");
            break;
        }
        case SELECT: {
            DEBUG("backspace");
            ITEM *it = current_item(painel_w->menu);
            int index = item_index(it);
            DEBUG("selecting item %d", index);
            menu_driver(painel_w->menu, REQ_TOGGLE_ITEM);
            break;
        }
        case KEY_F(1): {
            DEBUG("f1");

            break;
        }
        case COMMAND: {
            DEBUG("command");
            ch = getch();
            switch (ch) {
            case TRACK: {
                DEBUG("track");
                if (!search_api("track", painel_w, painel_options)) {
                    DEBUG("Error searching track");
                }
                break;
            }
            case ARTIST: {
                DEBUG("artist");
                if (!search_api("artist", painel_w, painel_options)) {
                    DEBUG("Error searching track");
                }
                break;
            }
            case ALBUM: {
                DEBUG("album");
                if (!search_api("album", painel_w, painel_options)) {
                    DEBUG("Error searching track");
                }
                break;
            }
            case PLAYLIST: {
                DEBUG("playlist");
                if (!search_api("playlist", painel_w, painel_options)) {
                    DEBUG("Error searching track");
                }
                break;
            }
            case USER: {
                DEBUG("user");
                DEBUG("Not implemented");
                break;
            }
            case RADIO: {
                DEBUG("radio");
                DEBUG("Not implemented");
                break;
            }
            default: {
                DEBUG("invalid");
                break;
            }
            }
            break;
        }
        default:
            break;
        }
    }

    DEBUG("exiting ...");

    free_window(playlist_w);
    free_window(painel_w);

    endwin();
    exit(0);
    return 0;
}

int search_api(const char *path, window_t *w, Menu_Options_Seeting options) {

    char *input = search_input();

    if (!input || strlen(input) == 0) {
        return 1;
    }

    if (w->items != NULL || w->menu != NULL) {
        destroy_menu(w);
    }

    buffer_t *response_data = api_url_search(path, input);
    if (!response_data) {
        free(input);
        return 0;
    }

    cJSON *json = cJSON_ParseWithLength(response_data->data, response_data->size);
    if (!json) {
        free(input);
        free(response_data->data);
        free(response_data);
        return 0;
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (!data) {
        free(input);
        cJSON_Delete(json);
        free(response_data->data);
        free(response_data);
        return 0;
    }

    int size = cJSON_GetArraySize(data);
    w->items = malloc(sizeof(ITEM *) * (size + 1));
    if (!w->items) {
        free(input);
        cJSON_Delete(json);
        free(response_data->data);
        free(response_data);
        return 0;
    }
    for (int i = 0; i < size; i++) {

        char *name_item = malloc(1);

        cJSON *c_item = cJSON_GetArrayItem(data, i);
        if (!c_item) {
            free(input);
            cJSON_Delete(json);
            free(response_data->data);
            free(response_data);
            return 0;
        }
        if (cJSON_HasObjectItem(c_item, "artist")) {
            cJSON *c_artist = cJSON_GetObjectItem(c_item, "artist");
            if (!c_artist) {
                free(input);
                cJSON_Delete(json);
                free(response_data->data);
                free(response_data);
                return 0;
            }
            if (cJSON_HasObjectItem(c_artist, "name")) {
                cJSON *c_name = cJSON_GetObjectItem(c_artist, "name");
                if (!c_name) {
                    free(input);
                    cJSON_Delete(json);
                    free(response_data->data);
                    free(response_data);
                    return 0;
                }
                char *artist_name = cJSON_GetStringValue(c_name);
                if (!artist_name) {
                    free(input);
                    cJSON_Delete(json);
                    free(response_data->data);
                    free(response_data);
                    return 0;
                }
                void *tmp = realloc(name_item, (strlen(artist_name) * sizeof(char)) + (3 * sizeof(char)));
                if (!tmp) {
                    free(input);
                    cJSON_Delete(json);
                    free(response_data->data);
                    free(response_data);
                    return 0;
                }
                name_item = tmp;
                strcat(name_item, artist_name);
                strcat(name_item, " - ");
            }
        }
        if (cJSON_HasObjectItem(c_item, "title")) {
            cJSON *c_title = cJSON_GetObjectItem(c_item, "title");
            if (!c_title) {
                free(input);
                cJSON_Delete(json);
                free(response_data->data);
                free(response_data);
                return 0;
            }
            char *title = cJSON_GetStringValue(c_title);
            if (!title) {
                free(input);
                cJSON_Delete(json);
                free(response_data->data);
                free(response_data);
                return 0;
            }
            void *tmp = realloc(name_item,  (strlen(title) * sizeof(char)) + (3 * sizeof(char)));
            if (!tmp) {
                free(input);
                cJSON_Delete(json);
                free(response_data->data);
                free(response_data);
                return 0;
            }
            name_item = tmp;
            strcat(name_item, title);
            strcat(name_item, " - ");
        }
        if (cJSON_HasObjectItem(c_item, "nb_tracks")) {
            cJSON *c_nb_tracks = cJSON_GetObjectItem(c_item, "nb_tracks");
            if (!c_nb_tracks) {
                free(input);
                cJSON_Delete(json);
                free(response_data->data);
                free(response_data);
                return 0;
            }
            int nb_tracks = (int)cJSON_GetNumberValue(c_nb_tracks);
            char *nb_tracks_str = int_to_string(" (%d)", nb_tracks);
            if (!nb_tracks_str) {
                free(input);
                cJSON_Delete(json);
                free(response_data->data);
                free(response_data);
                return 0;
            }
            void *tmp = realloc(name_item, strlen(nb_tracks_str) * sizeof(char));
            if (!tmp) {
                free(input);
                cJSON_Delete(json);
                free(response_data->data);
                free(response_data);
                return 0;
            }
            name_item = tmp;
            strcat(name_item, nb_tracks_str);
        }

        cJSON *c_id = cJSON_GetObjectItem(c_item, "id");
        if (!c_id) {
            free(input);
            cJSON_Delete(json);
            free(response_data->data);
            free(response_data);
            return 0;
        }

        char *id_item = int_to_string(NULL, (int)cJSON_GetNumberValue(c_id));
        if (!id_item) {
            free(input);
            cJSON_Delete(json);
            free(response_data->data);
            free(response_data);
            return 0;
        }
        w->items[i] = new_item(name_item, id_item);
    }

    w->items[size] = NULL;

    create_menu(w, options);

    free(input);
    cJSON_Delete(json);
    free(response_data->data);
    free(response_data);
    return 1;
}

char *int_to_string(char *format, int i) {
    char *str = calloc(10, sizeof(char));
    sprintf(str, format ? format : "%d", i);
    return (char *)str;
}

void clear_and_write(window_t *w, char *str) {
    if (w == NULL || str == NULL || w->window == NULL) {
        DEBUG("Invalid parameters in clear_and_write");
        return;
    }

    wclear(w->window);
    if (box(w->window, 0, 0) == ERR) {
        DEBUG("Error drawing box in clear_and_write");
        return;
    }

    addLabel(w->window, w->label);

    if (mvwaddstr(w->window, 1, 3, str) == ERR) {
        DEBUG("Error adding string in clear_and_write");
        return;
    }

    if (wrefresh(w->window) == ERR) {
        DEBUG("Error refreshing window in clear_and_write");
        return;
    }
}

char *search_input() {
    window_t *w = calloc(1, sizeof(window_t));
    if (!w) {
        DEBUG("Error allocating memory for search window");
        return NULL;
    }

    strcpy(w->label, "Search");
    w->y = 3;
    w->x = getmaxx(stdscr) * 0.80;
    w->starty = (getmaxy(stdscr) / 2) - (w->y / 2);
    w->startx = (getmaxx(stdscr) / 2) - (w->x / 2);

    create_win(w);

    char *track = malloc(1);
    if (!track) {
        DEBUG("Error allocating memory for track");
        free_window(w);
        return NULL;
    }
    track[0] = '\0';

    int i = 0;
    int c;
    while ((c = getch()) != '\n') {
        if (c == KEY_BACKSPACE) {
            if (i > 0) {
                i--;
                track[i] = '\0';
                char *tmp = realloc(track, i + 1);
                if (tmp) {
                    track = tmp;
                }
                clear_and_write(w, track);
            }
            continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == ',' || c == '!' || c == '?') {
            char *tmp = realloc(track, i + 2);
            if (tmp) {
                track = tmp;
                track[i++] = c;
                track[i] = '\0';
                clear_and_write(w, track);
            } else {
                DEBUG("Error reallocating memory for track");
                free(track);
                free_window(w);
                return NULL;
            }
        }
    }

    wclear(w->window);
    if (wrefresh(w->window) == ERR) {
        DEBUG("Error refreshing search window");
    }
    delwin(w->window);
    free(w);
    refresh();

    return track;
}

void create_win(window_t *w) {
    w->window = newwin(w->y, w->x, w->starty, w->startx);
    CHECK_WINDOW(w->window);
    box(w->window, 0, 0);
    addLabel(w->window, w->label);
    wrefresh(w->window);
    refresh();
}

void addLabel(WINDOW *win, char *str) {
    if (win == NULL || str == NULL) {
        DEBUG("Invalid parameters in addLabel");
        return;
    }

    size_t len = strlen(str);
    char *nstr = malloc(len + 3);
    if (nstr == NULL) {
        DEBUG("Error allocating memory for nstr in addLabel");
        return;
    }

    sprintf(nstr, " %.*s ", (int)len, str);
    int x = getmaxx(win) * 0.10;
    if (mvwaddstr(win, 0, x, nstr) == ERR) {
        DEBUG("Error adding string in addLabel");
        free(nstr);
        return;
    }
    if (wrefresh(win) == ERR) {
        DEBUG("Error refreshing window in addLabel");
        free(nstr);
        return;
    }
    free(nstr);
}

void destroy_menu(window_t *w) {
    if (w == NULL || w->menu == NULL) {
        return;
    }

    if (unpost_menu(w->menu) != E_OK) {
        DEBUG("Error unposting menu");
    }
    if (free_menu(w->menu) != E_OK) {
        DEBUG("Error freeing menu");
    }
    w->menu = NULL;

    if (w->items == NULL) {
        return;
    }

    for (int i = 0; w->items[i] != NULL; i++) {
        if (free_item(w->items[i]) != E_OK) {
            DEBUG("Error freeing item %d", i);
        }
        w->items[i] = NULL;
    }
}

void create_menu(window_t *w, Menu_Options_Seeting options) {
    if (w == NULL || w->items == NULL) {
        DEBUG("Invalid parameters in create_menu");
        return;
    }
    DEBUG("%p", (void *)*((&w->items + 1) - 1));
    w->menu = new_menu((ITEM **)w->items);
    if (w->menu == NULL) {
        DEBUG("Error creating new menu");
        return;
    }

    set_menu_win(w->menu, w->window);
    set_menu_sub(w->menu, derwin(w->window, getmaxy(w->window) - 1, getmaxx(w->window) - 1, 1, 1));
    set_menu_format(w->menu, getmaxy(w->window) - 3, 0);
    set_menu_mark(w->menu, " * ");

    menu_opts_off(w->menu, options.off);
    menu_opts_on(w->menu, options.on);

    if (post_menu(w->menu) != E_OK) {
        DEBUG("Error posting menu");
        free_menu(w->menu);
        w->menu = NULL;
        return;
    }

    if (wrefresh(w->window) == ERR) {
        DEBUG("Error refreshing window in create_menu");
    }
    if (refresh() == ERR) {
        DEBUG("Error refreshing screen in create_menu");
    }
}

void drive_menu(window_t *w, int key) {
    menu_driver(w->menu, key);
    wrefresh(w->window);
    refresh();
}

void free_window(window_t *w) {
    if (w != NULL) {
        if (w->window != NULL) {
            delwin(w->window);
        }
        if (w->menu != NULL) {
            destroy_menu(w);
        }
        free(w);
    }
}
