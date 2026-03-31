#include "gui.h"
#include "../vga/vga.h"
#include "../mouse/mouse.h"
#include "../keyboard/keyboard.h"
#include "../../cpu/timer/timer.h"
#include "../../kernal/libc/string.h"
#include "../../kernal/kfs/vfs.h"
#include "../../kernal/kfs/kfs.h"

int gui_active = 0;

#define BG_COLOR        COL_DGRAY
#define BG_ALT          COL_LBLUE
#define PANEL_BG        COL_BLACK
#define PANEL_HEAD      COL_BLUE
#define PANEL_LINE      COL_DGRAY
#define TEXT_NORMAL     COL_LGRAY
#define TEXT_HILITE     COL_WHITE
#define TEXT_ACCENT     COL_YELLOW
#define SEL_BG          COL_GREEN
#define TASKBAR_BG      COL_BLUE
#define TASKBAR_FG      COL_WHITE

#define PANEL_X         8
#define PANEL_Y         8
#define PANEL_W         (VGA_WIDTH - 16)
#define PANEL_H         172
#define LIST_TOP        30
#define LIST_ROW_H      10
#define LIST_X          12

#define DOUBLE_CLICK_TICKS 18

enum { BROWSER, EDITOR, PROMPT };
static int state = BROWSER;

static char current_path[128] = "/";
static char fnames[64][28];
static u8   ftypes[64];
static int  fcount = 0;
static int  sel_idx = 0;

static char edit_buf[4096];
static char edit_path[128];
static int  edit_len = 0;

static char prompt_msg[64];
static char prompt_buf[32];
static int  prompt_len = 0;
static int  prompt_mode = 0; /* 0=file, 1=dir */

static u8  prev_mouse_buttons = 0;
static u32 last_click_tick    = 0;
static int last_click_index   = -1;

#define CUR_W 12
#define CUR_H 12
static const u8 cursor_mask[CUR_H][CUR_W] = {
    { 1,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,0,0,0,0,0,0,0,0,0,0 },
    { 1,2,1,0,0,0,0,0,0,0,0,0 },
    { 1,2,2,1,0,0,0,0,0,0,0,0 },
    { 1,2,2,2,1,0,0,0,0,0,0,0 },
    { 1,2,2,2,2,1,0,0,0,0,0,0 },
    { 1,2,2,2,2,2,1,0,0,0,0,0 },
    { 1,2,2,2,1,1,0,0,0,0,0,0 },
    { 1,2,1,2,1,0,0,0,0,0,0,0 },
    { 1,1,0,1,2,1,0,0,0,0,0,0 },
    { 0,0,0,0,1,2,1,0,0,0,0,0 },
    { 0,0,0,0,0,1,1,0,0,0,0,0 },
};
static u8 cursor_save[CUR_H * CUR_W];
static int cursor_saved_x = -1;
static int cursor_saved_y = -1;

static void gui_redraw(void);

void gui_update_cursor(void) {
    if (!gui_active) return;

    int nx = mouse_x, ny = mouse_y, r, c;
    if (cursor_saved_x >= 0) {
        for (r = 0; r < CUR_H; r++) {
            for (c = 0; c < CUR_W; c++) {
                int px = cursor_saved_x + c;
                int py = cursor_saved_y + r;
                if (px < VGA_WIDTH && py < VGA_HEIGHT)
                    vga_put_pixel(px, py, cursor_save[r * CUR_W + c]);
            }
        }
    }

    cursor_saved_x = nx;
    cursor_saved_y = ny;
    for (r = 0; r < CUR_H; r++) {
        for (c = 0; c < CUR_W; c++) {
            int px = nx + c;
            int py = ny + r;
            if (px >= VGA_WIDTH || py >= VGA_HEIGHT) {
                cursor_save[r * CUR_W + c] = 0;
                continue;
            }
            cursor_save[r * CUR_W + c] = vga_get_pixel(px, py);
            if (cursor_mask[r][c] == 1) vga_put_pixel(px, py, COL_WHITE);
            else if (cursor_mask[r][c] == 2) vga_put_pixel(px, py, COL_BLACK);
        }
    }
}

static int join_path(const char *base, const char *name, char *out, int out_sz) {
    int bi = 0;
    int ni = 0;

    while (base[bi]) {
        if (bi >= out_sz - 1) return -1;
        out[bi] = base[bi];
        bi++;
    }

    if (bi == 0) {
        if (out_sz < 2) return -1;
        out[bi++] = '/';
    }

    if (out[bi - 1] != '/') {
        if (bi >= out_sz - 1) return -1;
        out[bi++] = '/';
    }

    while (name[ni]) {
        if (bi >= out_sz - 1) return -1;
        out[bi++] = name[ni++];
    }

    out[bi] = '\0';
    return 0;
}

static void refresh_dir(void) {
    fcount = vfs_list_dir(current_path, fnames, ftypes, 64);
    if (fcount < 0) fcount = 0;
    if (sel_idx >= fcount) sel_idx = (fcount > 0) ? (fcount - 1) : 0;
}

static void draw_background(void) {
    int y;
    for (y = 0; y < 188; y++) {
        u8 c = (y & 1) ? BG_COLOR : BG_ALT;
        vga_hline(0, y, VGA_WIDTH, c);
    }
}

static void draw_taskbar(void) {
    vga_fill_rect(0, 188, VGA_WIDTH, 12, TASKBAR_BG);
    vga_draw_string(6, 190, "Kernal Manager", TASKBAR_FG, TASKBAR_BG);
    vga_draw_string(130, 190, "ENTER/DblClick Open", TEXT_ACCENT, TASKBAR_BG);
}

static void draw_browser(void) {
    draw_background();
    draw_taskbar();

    vga_fill_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_BG);
    vga_draw_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_LINE);

    vga_fill_rect(PANEL_X + 1, PANEL_Y + 1, PANEL_W - 2, 14, PANEL_HEAD);
    vga_draw_string(PANEL_X + 4, PANEL_Y + 4, "FILES", TEXT_HILITE, PANEL_HEAD);
    vga_draw_string(PANEL_X + 52, PANEL_Y + 4, current_path, TEXT_ACCENT, PANEL_HEAD);
    vga_hline(PANEL_X, PANEL_Y + 16, PANEL_W, PANEL_LINE);

    int y = LIST_TOP;
    int i;
    for (i = 0; i < fcount && y < 170; i++, y += LIST_ROW_H) {
        u8 bg = (i == sel_idx) ? SEL_BG : PANEL_BG;
        u8 fg = (i == sel_idx) ? TEXT_HILITE : TEXT_NORMAL;
        if (i == sel_idx) vga_fill_rect(PANEL_X + 2, y - 1, PANEL_W - 4, LIST_ROW_H, SEL_BG);

        if (ftypes[i] == KFS_TYPE_DIR)
            vga_draw_string(LIST_X, y, "[DIR]", fg, bg);
        else
            vga_draw_string(LIST_X, y, "[FILE]", fg, bg);

        vga_draw_string(LIST_X + 48, y, fnames[i], fg, bg);
    }

    vga_hline(PANEL_X, 168, PANEL_W, PANEL_LINE);
    vga_draw_string(12, 172, "N:new file  M:new dir  DEL:remove  ESC:up", TEXT_NORMAL, PANEL_BG);
}

static void draw_editor(void) {
    draw_background();
    draw_taskbar();

    vga_fill_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_BG);
    vga_draw_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_LINE);

    vga_fill_rect(PANEL_X + 1, PANEL_Y + 1, PANEL_W - 2, 14, PANEL_HEAD);
    vga_draw_string(12, 12, "EDIT", TEXT_HILITE, PANEL_HEAD);
    vga_draw_string(52, 12, edit_path, TEXT_ACCENT, PANEL_HEAD);
    vga_hline(PANEL_X, PANEL_Y + 16, PANEL_W, PANEL_LINE);

    int cx = 12;
    int cy = 26;
    int i;
    for (i = 0; i < edit_len; i++) {
        if (edit_buf[i] == '\n' || cx > VGA_WIDTH - 24) {
            cx = 12;
            cy += 10;
        }
        if (edit_buf[i] != '\n' && cy < 168) {
            vga_draw_char(cx, cy, edit_buf[i], TEXT_NORMAL, PANEL_BG);
            cx += 8;
        }
    }

    if (cy < 168) vga_fill_rect(cx, cy, 8, 8, COL_WHITE);
    vga_hline(PANEL_X, 168, PANEL_W, PANEL_LINE);
    vga_draw_string(12, 172, "ESC:save and return", TEXT_NORMAL, PANEL_BG);
}

static void draw_prompt(void) {
    vga_fill_rect(48, 76, 224, 48, PANEL_HEAD);
    vga_draw_rect(48, 76, 224, 48, COL_WHITE);
    vga_draw_string(54, 82, prompt_msg, COL_WHITE, PANEL_HEAD);
    vga_fill_rect(54, 96, 212, 16, PANEL_BG);
    vga_draw_rect(54, 96, 212, 16, PANEL_LINE);
    vga_draw_string(58, 100, prompt_buf, TEXT_HILITE, PANEL_BG);
    vga_fill_rect(58 + prompt_len * 8, 100, 8, 8, COL_WHITE);
}

static void gui_redraw(void) {
    cursor_saved_x = -1;
    cursor_saved_y = -1;

    if (state == BROWSER) draw_browser();
    else if (state == EDITOR) draw_editor();

    if (state == PROMPT) draw_prompt();

    gui_update_cursor();
}

static void open_selected(void) {
    if (fcount <= 0 || sel_idx < 0 || sel_idx >= fcount) return;

    if (ftypes[sel_idx] == KFS_TYPE_DIR) {
        if (join_path(current_path, fnames[sel_idx], current_path, sizeof(current_path)) == 0) {
            refresh_dir();
            sel_idx = 0;
        }
        gui_redraw();
        return;
    }

    if (join_path(current_path, fnames[sel_idx], edit_path, sizeof(edit_path)) != 0) return;

    edit_len = vfs_read_into(edit_path, edit_buf, 4095);
    if (edit_len < 0) edit_len = 0;
    edit_buf[edit_len] = '\0';

    state = EDITOR;
    gui_redraw();
}

static int browser_index_from_mouse(int x, int y) {
    if (x < PANEL_X + 2 || x >= PANEL_X + PANEL_W - 2) return -1;
    if (y < LIST_TOP - 1 || y >= 168) return -1;

    int idx = (y - (LIST_TOP - 1)) / LIST_ROW_H;
    if (idx < 0 || idx >= fcount) return -1;
    return idx;
}

static void gui_mouse_packet(void) {
    if (!gui_active) return;

    if (state == BROWSER) {
        u8 left_now = mouse_buttons & 0x01;
        u8 left_prev = prev_mouse_buttons & 0x01;

        if (left_now && !left_prev) {
            int idx = browser_index_from_mouse(mouse_x, mouse_y);
            if (idx >= 0) {
                u32 now = timer_get_ticks();
                int is_double = (last_click_index == idx) && ((now - last_click_tick) <= DOUBLE_CLICK_TICKS);

                sel_idx = idx;
                if (is_double) {
                    last_click_index = -1;
                    last_click_tick = 0;
                    open_selected();
                } else {
                    last_click_index = idx;
                    last_click_tick = now;
                    gui_redraw();
                }
            }
        }
    }

    prev_mouse_buttons = mouse_buttons;
    gui_update_cursor();
}

void gui_init(void) {
    vga_init();
    gui_active = 1;

    vga_set_palette(COL_BLACK,    2,  3,  6);
    vga_set_palette(COL_BLUE,     7, 16, 29);
    vga_set_palette(COL_GREEN,   10, 30, 22);
    vga_set_palette(COL_LGRAY,   42, 46, 52);
    vga_set_palette(COL_DGRAY,    8, 10, 16);
    vga_set_palette(COL_LBLUE,   12, 18, 28);
    vga_set_palette(COL_YELLOW,  50, 45, 18);
    vga_set_palette(COL_WHITE,   62, 63, 63);

    prev_mouse_buttons = mouse_buttons;
    last_click_index = -1;
    last_click_tick = timer_get_ticks();
    mouse_set_move_callback(gui_mouse_packet);

    refresh_dir();
    gui_redraw();
}

static void go_up_dir(void) {
    int len = strlen(current_path);
    if (len <= 1) return;

    if (current_path[len - 1] == '/') {
        current_path[len - 1] = '\0';
        len--;
    }

    while (len > 1 && current_path[len - 1] != '/') len--;
    current_path[len] = '\0';

    refresh_dir();
    sel_idx = 0;
    gui_redraw();
}

static void run_prompt_action(void) {
    char full[128];
    if (join_path(current_path, prompt_buf, full, sizeof(full)) != 0) {
        state = BROWSER;
        gui_redraw();
        return;
    }

    if (prompt_mode == 0) vfs_touch(full);
    else vfs_mkdir(full);

    state = BROWSER;
    refresh_dir();
    gui_redraw();
}

void gui_handle_keyboard(u8 scancode, char ascii) {
    if (state == BROWSER) {
        if (scancode == SC_UP && sel_idx > 0) {
            sel_idx--;
            gui_redraw();
        } else if (scancode == SC_DOWN && sel_idx < fcount - 1) {
            sel_idx++;
            gui_redraw();
        } else if (scancode == SC_ESC) {
            go_up_dir();
        } else if (scancode == SC_DEL && fcount > 0) {
            char full[128];
            if (join_path(current_path, fnames[sel_idx], full, sizeof(full)) == 0) vfs_rm(full);
            refresh_dir();
            gui_redraw();
        } else if (ascii == 'n' || ascii == 'N') {
            k_strcpy(prompt_msg, "Enter new file name:");
            prompt_buf[0] = '\0';
            prompt_len = 0;
            prompt_mode = 0;
            state = PROMPT;
            gui_redraw();
        } else if (ascii == 'm' || ascii == 'M') {
            k_strcpy(prompt_msg, "Enter new directory name:");
            prompt_buf[0] = '\0';
            prompt_len = 0;
            prompt_mode = 1;
            state = PROMPT;
            gui_redraw();
        } else if (scancode == 0x1C) {
            open_selected();
        }
    } else if (state == EDITOR) {
        if (scancode == SC_ESC) {
            vfs_write(edit_path, edit_buf);
            state = BROWSER;
            refresh_dir();
            gui_redraw();
        } else if (scancode == 0x0E) {
            if (edit_len > 0) {
                edit_len--;
                edit_buf[edit_len] = '\0';
                gui_redraw();
            }
        } else if (scancode == 0x1C) {
            if (edit_len < 4095) {
                edit_buf[edit_len++] = '\n';
                edit_buf[edit_len] = '\0';
                gui_redraw();
            }
        } else if (ascii >= 32 && ascii <= 126) {
            if (edit_len < 4095) {
                edit_buf[edit_len++] = ascii;
                edit_buf[edit_len] = '\0';
                gui_redraw();
            }
        }
    } else if (state == PROMPT) {
        if (scancode == SC_ESC) {
            state = BROWSER;
            gui_redraw();
        } else if (scancode == 0x0E) {
            if (prompt_len > 0) {
                prompt_len--;
                prompt_buf[prompt_len] = '\0';
                gui_redraw();
            }
        } else if (scancode == 0x1C) {
            run_prompt_action();
        } else if (ascii >= 32 && ascii <= 126) {
            if (prompt_len < 26) {
                prompt_buf[prompt_len++] = ascii;
                prompt_buf[prompt_len] = '\0';
                gui_redraw();
            }
        }
    }
}

void gui_draw_window(int x, int y, int w, int h, const char *title) {
    (void)x; (void)y; (void)w; (void)h; (void)title;
}

void gui_print(const char *s) {
    (void)s;
}
