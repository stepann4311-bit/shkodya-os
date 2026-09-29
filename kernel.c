/*
 * Shkodya OS: ATA PIO + ShkodyaFS integrated into this single source file.
 * Persistent files: Notepad, ShkWord, terminal touch, Store games.app.
 * Use a dedicated blank PRIMARY MASTER IDE disk, boot the OS from ISO.
 * LBA 100 holds the directory; LBA 101 onward is reserved for file data.
 * No partitions, AHCI, DMA, deletion or power-failure recovery are provided.
 * Files are committed on Save; unsaved edits remain in RAM.
 * No disk / invalid filesystem => file operations fail, never fake success.
 * 32-bit freestanding kernel. Compiles with:
 * gcc -m32 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -c kernel.c -o kernel.o
 *
 * Boot: Multiboot 1 via GRUB. Entry is kernel_main(magic, mbi).
 */

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;

/* Pointer-width integer. Addresses that have to live in an integer (descriptor
   table bases, handler entry points, the IDT pointer) use this so the same
   source builds for both i386 and x86_64. */
#if defined(__x86_64__)
typedef unsigned long long uintptr_t;
#else
typedef unsigned int       uintptr_t;
#endif

#define NULL ((void *)0)

#define IRQ_TIMER_VECTOR    0x20
#define IRQ_KEYBOARD_VECTOR 0x21
#define IRQ_MOUSE_VECTOR    0x2C


/* ATA/ShkodyaFS is embedded below the port I/O helpers. */



#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_HZ       1193182
#define PIT_FREQ     100

#define KBD_DATA     0x60
#define KBD_STATUS   0x64
#define KBD_COMMAND  0x64

#define MOUSE_CURSOR_W 12
#define MOUSE_CURSOR_H 16

/* Cat / game colors — compile-time constants (used in const arrays) */
#define COLOR_AMBER        0xFFE08A4C
#define COLOR_GOLD         0xFFFFC107
#define COLOR_DANGER       0xFFC45C5C
#define COLOR_TERM_BG      0xFF0A120C
#define COLOR_TERM_FG      0xFF7DCE7A
#define COLOR_GINGER       0xFFC46A2A
#define COLOR_GINGER2      0xFFD47832
#define COLOR_GINGER3      0xFFB45E24
#define COLOR_EYE          0xFFF4F0E8
#define COLOR_INK          0xFF1A1510

/* Themeable UI colors — runtime-modifiable globals.
   COLOR_* macros expand to variable refs so all existing code works
   without changes; theme_apply() updates these at runtime. */
static uint32_t C_BLACK        = 0xFF070B14;
static uint32_t C_WHITE        = 0xFFECE8E0;
static uint32_t C_SURFACE      = 0xFF1C2233;
static uint32_t C_SURFACE2     = 0xFF252C40;
static uint32_t C_TITLE_HI     = 0xFF3D4866;
static uint32_t C_TITLE_LO     = 0xFF232A3B;
static uint32_t C_BORDER       = 0xFF343C55;
static uint32_t C_ACCENT       = 0xFF8FA4C4;
static uint32_t C_MUTED        = 0xFF9AA3B8;
static uint32_t C_TASKBAR      = 0xD00C101C;
static uint32_t C_TOAST_BG     = 0xE01C2233;
static uint32_t C_TOAST_BORDER = 0xFF4A5A7A;
static uint32_t C_SHADOW       = 0x10000000;
/* Extended dark-UI tokens — same surface language for every app window */
static uint32_t C_PAPER        = 0xFF12161F;   /* document / canvas surface */
static uint32_t C_PAPER_FG     = 0xFFE2E8F2;   /* text on document surface  */
static uint32_t C_FIELD        = 0xFF0E1219;   /* inset fields / displays   */
static uint32_t C_SUCCESS      = 0xFF1F5F3A;   /* positive action button    */
static uint32_t C_DANGER_BG    = 0xFF4A1D22;   /* destructive action button */
static uint32_t C_CHROME_HI    = 0xFF2B3446;   /* raised control surface    */
static uint32_t C_VIEW_BG      = 0xFF0A0E14;   /* terminal / media viewport */

#define COLOR_BLACK        C_BLACK
#define COLOR_WHITE        C_WHITE
#define COLOR_SURFACE      C_SURFACE
#define COLOR_SURFACE2     C_SURFACE2
#define COLOR_TITLE_HI     C_TITLE_HI
#define COLOR_TITLE_LO     C_TITLE_LO
#define COLOR_BORDER       C_BORDER
#define COLOR_ACCENT       C_ACCENT
#define COLOR_MUTED        C_MUTED
#define COLOR_TASKBAR      C_TASKBAR
#define COLOR_TOAST_BG     C_TOAST_BG
#define COLOR_TOAST_BORDER C_TOAST_BORDER
#define COLOR_SHADOW       C_SHADOW
#define COLOR_PAPER        C_PAPER
#define COLOR_PAPER_FG     C_PAPER_FG
#define COLOR_FIELD        C_FIELD
#define COLOR_SUCCESS      C_SUCCESS
#define COLOR_DANGER_BG    C_DANGER_BG
#define COLOR_CHROME_HI    C_CHROME_HI
#define COLOR_VIEW_BG      C_VIEW_BG

/* Cursor overlay: real cursor bitmap is 16x20 px (see draw_cursor) */
#define CURSOR_W 16
#define CURSOR_H 20

#define MAX_WINDOWS 8
#define TITLEBAR_H  28
/* Floating taskbar: TASKBAR_H is the complete reserved work-area inset. */
#define TASKBAR_PANEL_H  58
#define TASKBAR_MARGIN   14
#define TASKBAR_H       (TASKBAR_PANEL_H + TASKBAR_MARGIN + 10)
#define TASKBAR_RADIUS   18
#define TASKBAR_BUTTON   40
#define TASKBAR_STEP     46
#define TASKBAR_TRAY_W  172
#define TASKBAR_MAX_W  1120

#define KBD_BUF_SIZE 256
#define NOTEPAD_SIZE 4096
#define TERM_COLS    78
#define TERM_ROWS    22
#define TERM_LINE    80

/* Synthetic key codes for keys that produce no printable character. Both sit
   outside the character ranges the keyboard maps can emit (0, 8, 9, 10, 27,
   32..126 and the single-byte Cyrillic codes 0x80..0xED), so forwarding an
   arrow can never collide with ordinary typing. */
#define TERM_KEY_UP      0x11
#define TERM_KEY_DOWN    0x12
#define TERM_HISTORY_MAX 16
#define PAINT_W      600
#define PAINT_H      380

#define APP_NONE     0
#define APP_PAINT    1
#define APP_NOTEPAD  2
#define APP_RIKKI    3
#define APP_CALC     4
#define APP_TERM     5
#define APP_EXPLORER 6
#define APP_STORE    7
#define APP_MEDIA    8
#define APP_GAMES   9
#define APP_SETTINGS 10
#define APP_SHEET    11
#define APP_WORD     12
#define APP_BROWSER  13
#define APP_AICHAT   14
#define APP_TASKMGR  15

/* Shared with guest programs: one header so field offsets cannot drift. */
#include "guest/shk_exe.h"
/* Theme IDs */
#define THEME_MIDNIGHT 0
#define THEME_SUNSET   1
#define THEME_FOREST   2
#define THEME_OCEAN    3
#define THEME_ROSE     4
#define THEME_COUNT    5

/* Settings view tabs */
#define SETTINGS_VIEW_THEMES  0
#define SETTINGS_VIEW_SOUND   1
#define SETTINGS_VIEW_SYSTEM  2
#define SETTINGS_VIEW_UPDATE  3
#define SETTINGS_TAB_COUNT    4

/* Sheet dimensions */
#define SHEET_COLS 8
#define SHEET_ROWS 16
#define SHEET_CELL_W 72
#define SHEET_CELL_H 24
#define SHEET_TEXT_LEN 32


#define STORE_VIEW_2048   6
#define STORE_VIEW_PONG   7
#define STORE_VIEW_MEMORY 8

#define WP_SRC_W 1024
#define WP_SRC_H 768

/* VFS constants */
#define VFS_MAX_FILES    16
#define VFS_FILENAME_LEN 16
#define VFS_FILE_SIZE    8192 /* Text staging buffer, not a disk-file limit. */

/* ShkodyaFS v1 itself has a flat directory (and deliberately rejects '/' in
 * on-disk filenames). The Explorer keeps a normalized UI path separately. */
#define EXPLORER_PATH_MAX 64

/* Toast notification */
#define TOAST_DISPLAY_TICKS (PIT_FREQ * 3)

#define MAX_USERS    8
#define USERNAME_LEN 24

/* Forward-declare window_t so forward declarations can reference it */
struct window;
typedef struct window window_t;

/* Forward Declarations */
void putpixel(int x, int y, uint32_t color);
void putpixel_alpha(int x, int y, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void fill_rect(int x, int y, int w, int h, uint32_t color);
void fill_rect_alpha(int x, int y, int w, int h, uint32_t color);
void fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color);
void fill_rounded_rect_aa(int x, int y, int w, int h, int r, uint32_t color);
void draw_window_shadow(int x, int y, int w, int h);
void draw_char(int x, int y, char c, uint32_t color);
void draw_string(int x, int y, const char *s, uint32_t color);
void toast_show(const char *text);
static void sound_stop(void);
static void sound_raw_on(uint32_t freq);
static void sound_beep(uint32_t freq, uint32_t ticks);
static void sound_update(void);
static void ttt_init(void);
static void snake_init(void);
static void snake_update(void);
static void clicker_update(void);
static void g2048_init(void);
static void g2048_move(int dir);
static void g2048_update(void);
static void pong_init(void);
static void pong_update(void);
static void mem_init(void);
static void mem_update(void);
static void mem_click(int idx);
static void draw_settings_client(window_t *win);
static void draw_sheet_client(window_t *win);
static void draw_word_client(window_t *win);
static void handle_settings_click(window_t *win, int mx, int my);
/* COM1 bridge output, used by the shell and by Shkodya Web */
static void serial_send_str(const char *s);
static void draw_browser_client(window_t *win);
static void handle_browser_click(window_t *win, int px, int py);
static void web_key(char ch);
static void web_handle_packet(const char *line);
/* Shkodya package manager (defined with the COM1 bridge; called by the shell) */
static void pkg_cmd_search(const char *query);
static void pkg_cmd_install(const char *pkg);
static void pkg_list_installed(void);
static void handle_sheet_click(window_t *win, int mx, int my);
static void handle_word_toolbar_click(window_t *win, int mx, int my);
static void sheet_init(void);
static void word_reset(void);
static void theme_apply(int theme_id);

/* External symbols provided by boot.S */
extern uint32_t backbuffer[];
/* Demo guest executable, packed by tools/pack_exe.py and embedded by testexe.S. */
extern const uint8_t  test_exe_blob[];
extern const uint32_t test_exe_size;
extern const uint32_t wallpaper_data[1024 * 768];
extern void keyboard_handler_asm(void);
extern void mouse_handler_asm(void);
extern void timer_handler_asm(void);
extern void load_idt(uintptr_t idt_ptr_addr);

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  reserved_fb;
    uint8_t  color_info[6];
} __attribute__((packed)) multiboot_info_t;

/* Globals */
uint32_t fb_width;
uint32_t fb_height;
uint32_t fb_pitch;
uint32_t *framebuffer_addr;
volatile uint32_t system_ticks;
static multiboot_info_t *g_mbi;

static uint8_t  gdt_table[24];
static uint16_t gdt_limit;
static uintptr_t gdt_base;

#if defined(__x86_64__)
/* Long mode gates are 16 bytes: the handler address is split into three pieces
   and the byte after the selector carries the IST index. */
typedef struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;
#else
typedef struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;
#endif

typedef struct idt_ptr {
    uint16_t  limit;
    uintptr_t base;              /* 4 bytes on i386, 8 on x86_64 */
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t   idtp;

static int mouse_x;
static int mouse_y;
static int mouse_left;
static int mouse_left_prev;
static int mouse_right;
static uint8_t mouse_cycle;
static int8_t mouse_bytes[3];

static uint8_t  kbd_buf[KBD_BUF_SIZE];
static uint32_t kbd_head;
static uint32_t kbd_tail;
static int kbd_shift;
static int kbd_extended;
static int kbd_alt = 0;
static int kbd_caps = 0;

#define LANG_EN 0
#define LANG_RU 1
static int current_lang = LANG_EN;

/* Sound management */
static int sound_muted = 0;
static int sound_volume = 70;
static int sound_popup_open = 0;
static int sound_effects_enabled = 1;  /* UI click sounds toggle */

/* Theme management */
static int current_theme = THEME_MIDNIGHT;

/* Cat purr sound state */
static int purr_active = 0;
static uint32_t purr_start_tick = 0;
#define PURR_DURATION_TICKS 60  /* ~0.6s */

/* Settings app state */
static int settings_view = SETTINGS_VIEW_THEMES;

/* Spreadsheet (ShkSheet) state */
static char sheet_cells[SHEET_ROWS][SHEET_COLS][SHEET_TEXT_LEN];
static int sheet_sel_row = 0;
static int sheet_sel_col = 0;
static int sheet_editing = 0;
static int sheet_cursor = 0;

/* Word processor (ShkWord) state */
#define WORD_SIZE 8192
static char word_text[WORD_SIZE];
static uint32_t word_len;
static int word_cursor;
static int word_bold = 0;
static int word_italic = 0;
static int word_font_size = 2;  /* 1=small 2=normal 3=large */
static char word_current_file[VFS_FILENAME_LEN] = "doc1.txt";


static int start_open;
static int dragging;
static int drag_index;
static int drag_off_x;
static int drag_off_y;
static int focused;
static volatile int need_redraw;
static volatile int mouse_moved;   /* set by the mouse IRQ, serviced by the loop */

static uint32_t paint_canvas[PAINT_W * PAINT_H];
static int paint_last_x = -1;
static int paint_last_y = -1;
static int paint_drawing = 0;

static char notepad_text[NOTEPAD_SIZE];
static uint32_t notepad_len;
static int notepad_cursor;
static char notepad_current_file[VFS_FILENAME_LEN] = "readme.txt";

static char calc_display[16];
static int32_t calc_acc;
static int calc_op;
static int calc_fresh;

/* Redesign state: the secondary expression line above the main digits, a
   divide-by-zero latch, and hover/press feedback for the keypad. */
static char calc_expr[28];
static int calc_error = 0;
static int calc_hover = -1;
static int calc_active = -1;
static uint32_t calc_hover_tick = 0;

static char term_lines[TERM_ROWS][TERM_LINE];
static int term_row;
static int term_col;
static char term_input[TERM_LINE];
static int term_in_len;

/* Command history ring: term_hist_next is the write cursor, so the newest
   entry lives at (term_hist_next - 1) and term_history_at() walks backwards. */
static char term_history[TERM_HISTORY_MAX][TERM_LINE];
static int term_hist_next = 0;    /* ring write cursor                        */
static int term_hist_count = 0;   /* stored entries, saturating at the cap    */
static int term_hist_view = -1;   /* -1 = editing a fresh line, else N back   */

/* The COM1 bridge answers asynchronously, so status lines can land while the
   shell sits at a prompt. term_prompt_live tracks whether a '>' is on screen
   (so it can be stepped off) and term_prompt_held suppresses the prompt while
   a reply is outstanding, so the reply ends on a clean prompt instead of a
   half-drawn line. */
static int term_prompt_live = 0;
static int term_prompt_held = 0;

#define PAINT_PALETTE_SIZE 16
static uint32_t paint_palette[PAINT_PALETTE_SIZE] = {
    0xFF000000, 0xFFFFFFFF, 0xFF7F7F7F, 0xFFC3C3C3,
    0xFFFF0000, 0xFFFF7F27, 0xFFFFF200, 0xFF22B14C,
    0xFF00A2E8, 0xFF3F48CC, 0xFFA349A4, 0xFFFFAEC9,
    0xFFB5E61D, 0xFF99D9EA, 0xFF880015, 0xFFB97A57
};

static int paint_color_index = 0;
static int paint_brush_size = 2;
static int paint_is_eraser = 0;

#define PAINT_BAR_H     72        /* two-row toolbar: swatches + actions        */
#define PAINT_STATUS_H  22        /* footer readout                             */
#define PAINT_SAVE_FILE "art.ppm"

/* Live pointer position in canvas space, -1 when the cursor is elsewhere. */
static int paint_cursor_x = -1;
static int paint_cursor_y = -1;
static uint32_t paint_cursor_tick = 0;

/* Binary PPM (P6) staging buffer for the Save action: header + 600x380x3.
   Static so a 684 KB image never lands on the kernel stack. */
#define PAINT_PPM_HDR_MAX 32
static uint8_t paint_ppm[PAINT_PPM_HDR_MAX + PAINT_W * PAINT_H * 3];

/* VFS directory cache for the UI; file contents live on disk. */
typedef struct vfs_file {
    uint8_t  used;
    char     name[VFS_FILENAME_LEN];
    uint32_t size;
} vfs_file_t;

static vfs_file_t vfs_files[VFS_MAX_FILES];
/* Shared main-loop staging area preserves editor contents on read errors. */
static uint8_t vfs_text_buffer[VFS_FILE_SIZE];
static int vfs_last_error;
static int vfs_boot_error;
static char current_path[EXPLORER_PATH_MAX] = "/";

/* Toast notification */
typedef struct {
    uint8_t  active;
    char     text[64];
    uint32_t display_start;
} toast_t;

static toast_t toast;

/* Window manager */
typedef struct window {
    int used;
    int app;
    int x;
    int y;
    int w;
    int h;
    int z;
    char title[28];
} window_t;

static window_t windows[MAX_WINDOWS];
static int z_top;
static int z_order[MAX_WINDOWS];

/* App Store (Account System & Mini-games) */
typedef struct user {
    char     username[USERNAME_LEN];
    uint32_t password_hash;
    uint8_t  is_dev;
} user_t;

static user_t store_users[MAX_USERS];
static int store_user_count = 0;
static int store_current_user = 0;

#define STORE_VIEW_CATALOG 0
#define STORE_VIEW_LOGIN   1
#define STORE_VIEW_REG     2
#define STORE_VIEW_TTT     3
#define STORE_VIEW_SNAKE   4
#define STORE_VIEW_CLICKER 5

static int store_view = STORE_VIEW_CATALOG;
static char store_in_user[USERNAME_LEN];
static char store_in_pass[USERNAME_LEN];
static int store_field_focus = 0;

/* Mini-game 1: Tic-Tac-Toe */
static char ttt_board[9];
static char ttt_turn = 'X';
static int  ttt_winner = 0;
static int  ttt_score_x = 0;
static int  ttt_score_o = 0;

/* Mini-game 2: Snake */
#define SNAKE_MAX_LEN 128
static int snake_x[SNAKE_MAX_LEN];
static int snake_y[SNAKE_MAX_LEN];
static int snake_len = 3;
static int snake_dir = 0;
static int snake_food_x = 10;
static int snake_food_y = 7;
static int snake_score = 0;
static int snake_alive = 1;
static uint32_t snake_last_tick = 0;

/* Mini-game 3: Clicker */
static uint32_t clicker_coins = 0;
static uint32_t clicker_power = 1;
static uint32_t clicker_cps = 0;
static uint32_t clicker_last_tick = 0;

/* Mini-game 4: 2048 */
#define G2048_N 4
static int g2048_grid[G2048_N * G2048_N];
static int g2048_score;
static int g2048_won;

/* Mini-game 5: Pong */
static int pong_bx, pong_by, pong_bdx, pong_bdy;
static int pong_py, pong_ay;
static int pong_sp, pong_sa;
static int pong_paused;
static uint32_t pong_tick;

/* Mini-game 6: Memory Match */
#define MEM_N 16
static int mem_vals[MEM_N];
static int mem_shown[MEM_N];
static int mem_sel1, mem_sel2;
static int mem_pairs;
static uint32_t mem_timer;
static int mem_lock;

/* Media Player */
#define MEDIA_MODE_VIDEO 0
#define MEDIA_MODE_AUDIO 1
#define MEDIA_STATE_STOP 0
#define MEDIA_STATE_PLAY 1
#define MEDIA_STATE_PAUSE 2

static int media_mode = MEDIA_MODE_VIDEO;
static int media_state = MEDIA_STATE_STOP;
static uint32_t media_progress = 0;
static uint32_t media_total_ticks = 1200;
static uint32_t media_curr_tick = 0;

/* Chiptune Song (Korobeiniki) */
typedef struct {
    uint16_t freq;
    uint8_t  ticks;
} note_t;

static const note_t chiptune_track[] = {
    {659, 24}, {494, 12}, {523, 12}, {587, 24}, {523, 12}, {494, 12},
    {440, 24}, {440, 12}, {523, 12}, {659, 24}, {587, 12}, {523, 12},
    {494, 36}, {523, 12}, {587, 24}, {659, 24}, {523, 24}, {440, 24},
    {440, 36}, {0,   12},
    {587, 28}, {698, 14}, {880, 24}, {784, 12}, {698, 12},
    {659, 36}, {523, 12}, {659, 24}, {587, 12}, {523, 12},
    {494, 24}, {494, 12}, {523, 12}, {587, 24}, {659, 24}, {523, 24},
    {440, 24}, {440, 36}, {0,   16}
};
#define CHIPTUNE_LEN (sizeof(chiptune_track) / sizeof(chiptune_track[0]))

static uint32_t media_note_idx = 0;
static uint32_t media_note_tick = 0;
static uint8_t  eq_heights[20];

/* Hardware I/O */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void) {
    outb(0x80, 0x00);
}

static inline void sti(void) { __asm__ volatile ("sti"); }
static inline void cli(void) { __asm__ volatile ("cli"); }
static inline void hlt(void) { __asm__ volatile ("hlt"); }

/* ===== Embedded ATA PIO / ShkodyaFS: no extra source files required ===== */
enum {
    SHK_OK = 0, SHK_EINVAL = -1, SHK_ENODEV = -2, SHK_ETIMEOUT = -3,
    SHK_EIO = -4, SHK_ERANGE = -5, SHK_EUNSUPPORTED = -6,
    SHK_ENOTREADY = -7, SHK_ECORRUPT = -8, SHK_ENOENT = -9,
    SHK_ENOSPC = -10, SHK_EBUFFER = -11
};
#define SHK_SECTOR_SIZE     512u
#define SHK_FS_ROOT_LBA     100u
#define SHK_FS_DATA_LBA     101u
#define SHK_FS_MAX_FILES    16u
#define SHK_FS_NAME_BYTES   16u
#define SHK_FS_MAX_BYTES    0x7FFFFFFFu
#define SHK_ATA_LBA28_COUNT 0x10000000u
#ifndef SHK_ATA_POLL_LIMIT
#define SHK_ATA_POLL_LIMIT  10000000u
#endif

/* Public API: sector/write functions return 0 or a negative error.
 * fs_read_file returns the full byte count (no trailing NUL); it rejects
 * undersized buffers. fs_list requires char names[16][16].
 * Calls must be serialized and must NOT run inside interrupt handlers.
 */
int ata_init(void);
int ata_read_sector(uint32_t lba, uint8_t *buf);
int ata_write_sector(uint32_t lba, const uint8_t *buf);
void fs_init(void);
int fs_status(void);
int fs_list(char out_names[][16]);
int fs_write_file(const char *name, const uint8_t *data, uint32_t size);
int fs_read_file(const char *name, uint8_t *buffer, uint32_t max_size);

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

#define SHK_ATA_DATA    0x1F0u
#define SHK_ATA_ERROR   0x1F1u
#define SHK_ATA_COUNT   0x1F2u
#define SHK_ATA_LBA0    0x1F3u
#define SHK_ATA_LBA1    0x1F4u
#define SHK_ATA_LBA2    0x1F5u
#define SHK_ATA_DRIVE   0x1F6u
#define SHK_ATA_STATUS  0x1F7u
#define SHK_ATA_COMMAND 0x1F7u
#define SHK_ATA_CONTROL 0x3F6u
#define SHK_ATA_BSY     0x80u
#define SHK_ATA_DRDY    0x40u
#define SHK_ATA_DF      0x20u
#define SHK_ATA_DRQ     0x08u
#define SHK_ATA_ERR     0x01u

/* Read-only diagnostics for callers. Capacity is the usable LBA28 count. */
int ata_present;
uint32_t ata_total_sectors;
uint8_t ata_last_status;
uint8_t ata_last_error;

static void shk_ata_delay(void) {
    unsigned int i;
    for (i = 0; i < 15; ++i) (void)inb(SHK_ATA_CONTROL);
}

/* Poll budgets are iterations, not calibrated wall-clock timeouts.
 * BSY must clear before other status bits can be interpreted.
 * Ignore old ERR only before a new command or during signature detection.
 */
static int shk_ata_wait(uint8_t required, uint8_t forbidden, int check_error) {
    uint32_t i;
    for (i = 0; i < SHK_ATA_POLL_LIMIT; ++i) {
        uint8_t s = inb(SHK_ATA_CONTROL);
        ata_last_status = s;
        if (s == 0xFFu) return SHK_ENODEV;
        if (s & SHK_ATA_BSY) continue;
        if (check_error) {
            if (s == 0) return SHK_ENODEV;
            if (s & (SHK_ATA_ERR | SHK_ATA_DF)) {
                ata_last_error = (s & SHK_ATA_ERR) ? inb(SHK_ATA_ERROR) : 0;
                return SHK_EIO;
            }
        }
        if ((s & required) == required && !(s & forbidden)) return SHK_OK;
    }
    return SHK_ETIMEOUT;
}

int ata_init(void) {
    uint16_t id[256];
    uint32_t count;
    unsigned int i;
    int r;
    ata_present = 0;
    ata_total_sectors = 0;
    ata_last_error = 0;
    ata_last_status = inb(SHK_ATA_STATUS);
    if (ata_last_status == 0xFFu) return SHK_ENODEV;
    outb(SHK_ATA_CONTROL, 0x02); /* nIEN: no disk IRQ handler required. */
    r = shk_ata_wait(0, SHK_ATA_DRQ, 0);
    if (r < 0) return r;
    outb(SHK_ATA_DRIVE, 0xA0); /* Primary master only. */
    shk_ata_delay();
    r = shk_ata_wait(0, SHK_ATA_DRQ, 0);
    if (r < 0) return r;
    outb(SHK_ATA_COUNT, 0);
    outb(SHK_ATA_LBA0, 0);
    outb(SHK_ATA_LBA1, 0);
    outb(SHK_ATA_LBA2, 0);
    outb(SHK_ATA_COMMAND, 0xEC);
    shk_ata_delay();
    ata_last_status = inb(SHK_ATA_STATUS);
    if (ata_last_status == 0 || ata_last_status == 0xFFu) return SHK_ENODEV;
    r = shk_ata_wait(0, 0, 0);
    if (r < 0) return r;
    if (inb(SHK_ATA_LBA1) != 0 || inb(SHK_ATA_LBA2) != 0)
        return SHK_EUNSUPPORTED; /* ATAPI / non-ATA signature. */
    r = shk_ata_wait(SHK_ATA_DRQ, 0, 1);
    if (r < 0) return r;
    for (i = 0; i < 256; ++i) id[i] = inw(SHK_ATA_DATA);
    shk_ata_delay();
    r = shk_ata_wait(0, SHK_ATA_DRQ, 1);
    if (r < 0) return r;
    if ((id[0] & 0x8000u) || !(id[49] & 0x0200u)) return SHK_EUNSUPPORTED;
    if ((id[106] & 0xC000u) == 0x4000u && (id[106] & 0x1000u)) {
        uint32_t words = (uint32_t)id[117] | ((uint32_t)id[118] << 16);
        if (words != 256u) return SHK_EUNSUPPORTED;
    }
    count = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    if (count == 0) return SHK_EUNSUPPORTED;
    if (count > SHK_ATA_LBA28_COUNT) count = SHK_ATA_LBA28_COUNT;
    ata_total_sectors = count;
    ata_present = 1;
    return SHK_OK;
}

static int shk_ata_begin(uint32_t lba, uint8_t command) {
    int r;
    if (!ata_present) return SHK_ENODEV;
    if (lba >= ata_total_sectors || lba >= SHK_ATA_LBA28_COUNT) return SHK_ERANGE;
    ata_last_error = 0;
    r = shk_ata_wait(0, SHK_ATA_DRQ, 0);
    if (r < 0) return r;
    outb(SHK_ATA_DRIVE, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    shk_ata_delay();
    r = shk_ata_wait(SHK_ATA_DRDY, SHK_ATA_DRQ, 0);
    if (r < 0) return r;
    outb(SHK_ATA_ERROR, 0); /* Features register on writes. */
    outb(SHK_ATA_COUNT, 1);
    outb(SHK_ATA_LBA0, (uint8_t)lba);
    outb(SHK_ATA_LBA1, (uint8_t)(lba >> 8));
    outb(SHK_ATA_LBA2, (uint8_t)(lba >> 16));
    outb(SHK_ATA_COMMAND, command);
    shk_ata_delay();
    return shk_ata_wait(SHK_ATA_DRQ, 0, 1);
}

int ata_read_sector(uint32_t lba, uint8_t *buf) {
    unsigned int i;
    int r;
    if (!buf) return SHK_EINVAL;
    r = shk_ata_begin(lba, 0x20);
    if (r < 0) return r;
    for (i = 0; i < 256; ++i) {
        uint16_t word = inw(SHK_ATA_DATA);
        buf[i * 2] = (uint8_t)word;
        buf[i * 2 + 1] = (uint8_t)(word >> 8);
    }
    shk_ata_delay();
    return shk_ata_wait(0, SHK_ATA_DRQ, 1);
}

int ata_write_sector(uint32_t lba, const uint8_t *buf) {
    unsigned int i;
    int r;
    if (!buf) return SHK_EINVAL;
    r = shk_ata_begin(lba, 0x30);
    if (r < 0) return r;
    for (i = 0; i < 256; ++i) {
        uint16_t word = (uint16_t)((uint16_t)buf[i * 2] |
                                 ((uint16_t)buf[i * 2 + 1] << 8));
        outw(SHK_ATA_DATA, word);
        (void)inb(SHK_ATA_CONTROL);
    }
    shk_ata_delay();
    r = shk_ata_wait(0, SHK_ATA_DRQ, 1);
    if (r < 0) return r;
    outb(SHK_ATA_COMMAND, 0xE7); /* Commit data before directory metadata. */
    shk_ata_delay();
    return shk_ata_wait(0, SHK_ATA_DRQ, 1);
}

/* Packed little-endian disk format; compatible with standalone shkodyafs.c. */
typedef struct __attribute__((packed)) {
    char filename[SHK_FS_NAME_BYTES];
    uint32_t start_lba;
    uint32_t size_bytes;
    uint8_t is_used;
} shk_fs_entry_t;

typedef struct __attribute__((packed)) {
    uint8_t magic[8];
    uint32_t version;
    uint32_t total_sectors;
    uint32_t data_start_lba;
    shk_fs_entry_t entries[SHK_FS_MAX_FILES];
    uint8_t reserved[88];
    uint32_t checksum;
} shk_fs_root_t;

typedef char shk_entry_size_check[(sizeof(shk_fs_entry_t) == 25) ? 1 : -1];
typedef char shk_root_size_check[(sizeof(shk_fs_root_t) == 512) ? 1 : -1];
static const uint8_t shk_fs_magic[8] = {'S','H','K','O','D','Y','A','F'};
static shk_fs_root_t shk_fs_root;
static int shk_fs_state = SHK_ENOTREADY;
static int shk_fs_formatted; /* Seed demo documents only after first format. */

static void shk_copy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
}
static void shk_zero(void *dst, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = 0;
}
static uint32_t shk_fs_checksum(const shk_fs_root_t *root) {
    const uint8_t *p = (const uint8_t *)root;
    uint32_t hash = 2166136261u, i;
    for (i = 0; i < SHK_SECTOR_SIZE - 4u; ++i)
        hash = (hash ^ p[i]) * 16777619u;
    return hash;
}
static uint32_t shk_fs_sectors(uint32_t bytes) {
    return bytes / SHK_SECTOR_SIZE + (bytes % SHK_SECTOR_SIZE != 0);
}
static uint32_t shk_fs_name_length(const char *name) {
    uint32_t i;
    if (!name) return 0;
    for (i = 0; i < SHK_FS_NAME_BYTES; ++i) {
        if (name[i] == '\0') return i;
        if (name[i] == '/' || name[i] == '\\') return 0;
    }
    return 0;
}
static int shk_fs_same_name(const char *a, const char *b) {
    uint32_t i;
    for (i = 0; i < SHK_FS_NAME_BYTES; ++i) {
        if (a[i] != b[i]) return 0;
        if (a[i] == '\0') return 1;
    }
    return 0;
}
static int shk_fs_validate(void) {
    uint32_t i, j;
    for (i = 0; i < 8; ++i)
        if (shk_fs_root.magic[i] != shk_fs_magic[i]) return SHK_ECORRUPT;
    if (shk_fs_root.version != 1 ||
        shk_fs_root.data_start_lba != SHK_FS_DATA_LBA ||
        shk_fs_root.total_sectors <= SHK_FS_DATA_LBA ||
        shk_fs_root.total_sectors > ata_total_sectors ||
        shk_fs_root.checksum != shk_fs_checksum(&shk_fs_root)) return SHK_ECORRUPT;
    for (i = 0; i < SHK_FS_MAX_FILES; ++i) {
        const shk_fs_entry_t *e = &shk_fs_root.entries[i];
        uint32_t n;
        if (e->is_used > 1) return SHK_ECORRUPT;
        if (!e->is_used) continue;
        if (!shk_fs_name_length(e->filename) || e->size_bytes > SHK_FS_MAX_BYTES)
            return SHK_ECORRUPT;
        n = shk_fs_sectors(e->size_bytes);
        if (!n) {
            if (e->start_lba != 0) return SHK_ECORRUPT;
        } else if (e->start_lba < SHK_FS_DATA_LBA ||
                   e->start_lba >= shk_fs_root.total_sectors ||
                   n > shk_fs_root.total_sectors - e->start_lba) return SHK_ECORRUPT;
        for (j = 0; j < i; ++j) {
            const shk_fs_entry_t *p = &shk_fs_root.entries[j];
            uint32_t pn;
            if (!p->is_used) continue;
            if (shk_fs_same_name(e->filename, p->filename)) return SHK_ECORRUPT;
            pn = shk_fs_sectors(p->size_bytes);
            if (n && pn && e->start_lba < p->start_lba + pn &&
                p->start_lba < e->start_lba + n) return SHK_ECORRUPT;
        }
    }
    return SHK_OK;
}
int fs_status(void) { return shk_fs_state; }

void fs_init(void) {
    uint32_t i;
    int r;
    uint8_t boot_sector[SHK_SECTOR_SIZE];
    const uint8_t *raw = (const uint8_t *)&shk_fs_root;
    shk_fs_state = SHK_ENOTREADY;
    shk_fs_formatted = 0;
    r = ata_init();
    if (r < 0) { shk_fs_state = r; return; }
    if (ata_total_sectors <= SHK_FS_DATA_LBA) {
        shk_fs_state = SHK_ENOSPC; return;
    }
    r = ata_read_sector(SHK_FS_ROOT_LBA, (uint8_t *)&shk_fs_root);
    if (r < 0) { shk_fs_state = r; return; }
    for (i = 0; i < SHK_SECTOR_SIZE && raw[i] == 0; ++i) ;
    if (i != SHK_SECTOR_SIZE) { shk_fs_state = shk_fs_validate(); return; }
    /* Extra guard: never auto-format a disk with MBR/boot-sector contents.
     * This does not prove that the rest of the disk is unused: the caller
     * must still provide a dedicated blank disk, not a system disk.
     */
    r = ata_read_sector(0, boot_sector);
    if (r < 0) { shk_fs_state = r; return; }
    for (i = 0; i < SHK_SECTOR_SIZE; ++i)
        if (boot_sector[i] != 0) { shk_fs_state = SHK_EUNSUPPORTED; return; }
    shk_copy(shk_fs_root.magic, shk_fs_magic, 8);
    shk_fs_root.version = 1;
    shk_fs_root.total_sectors = ata_total_sectors;
    shk_fs_root.data_start_lba = SHK_FS_DATA_LBA;
    shk_fs_root.checksum = shk_fs_checksum(&shk_fs_root);
    shk_fs_state = ata_write_sector(SHK_FS_ROOT_LBA, (const uint8_t *)&shk_fs_root);
    if (shk_fs_state == SHK_OK) shk_fs_formatted = 1;
}
static int shk_fs_find(const char *name) {
    uint32_t i;
    for (i = 0; i < SHK_FS_MAX_FILES; ++i)
        if (shk_fs_root.entries[i].is_used &&
            shk_fs_same_name(shk_fs_root.entries[i].filename, name)) return (int)i;
    return SHK_ENOENT;
}
int fs_list(char out_names[][16]) {
    uint32_t i;
    int count = 0;
    if (shk_fs_state < 0) return shk_fs_state;
    if (!out_names) return SHK_EINVAL;
    shk_zero(out_names, SHK_FS_MAX_FILES * SHK_FS_NAME_BYTES);
    for (i = 0; i < SHK_FS_MAX_FILES; ++i)
        if (shk_fs_root.entries[i].is_used)
            shk_copy(out_names[count++], shk_fs_root.entries[i].filename, 16);
    return count;
}
/* First-fit; reserve the old extent until the new version is committed. */
static int shk_fs_allocate(uint32_t count, uint32_t *start) {
    uint32_t pos = SHK_FS_DATA_LBA, i;
    if (!count) { *start = 0; return SHK_OK; }
    for (;;) {
        if (pos >= shk_fs_root.total_sectors ||
            count > shk_fs_root.total_sectors - pos) return SHK_ENOSPC;
        for (i = 0; i < SHK_FS_MAX_FILES; ++i) {
            const shk_fs_entry_t *e = &shk_fs_root.entries[i];
            uint32_t n = shk_fs_sectors(e->size_bytes);
            if (e->is_used && n && pos < e->start_lba + n && e->start_lba < pos + count) {
                pos = e->start_lba + n;
                break;
            }
        }
        if (i == SHK_FS_MAX_FILES) { *start = pos; return SHK_OK; }
    }
}
/* Not a journal: a torn root-sector write is detectable, not recoverable.
 * A metadata-write error makes the mount unusable until fs_init() succeeds.
 */
int fs_write_file(const char *name, const uint8_t *data, uint32_t size) {
    shk_fs_root_t next;
    uint8_t sector[SHK_SECTOR_SIZE];
    shk_fs_entry_t *e;
    uint32_t name_len, start, count, i, offset = 0;
    int index, r;
    if (shk_fs_state < 0) return shk_fs_state;
    name_len = shk_fs_name_length(name);
    if (!name_len || (!data && size)) return SHK_EINVAL;
    if (size > SHK_FS_MAX_BYTES) return SHK_ERANGE;
    index = shk_fs_find(name);
    if (index < 0) {
        for (i = 0; i < SHK_FS_MAX_FILES; ++i)
            if (!shk_fs_root.entries[i].is_used) break;
        if (i == SHK_FS_MAX_FILES) return SHK_ENOSPC;
        index = (int)i;
    }
    count = shk_fs_sectors(size);
    r = shk_fs_allocate(count, &start);
    if (r < 0) return r;
    for (i = 0; i < count; ++i) {
        uint32_t n = size - offset;
        if (n > SHK_SECTOR_SIZE) n = SHK_SECTOR_SIZE;
        shk_zero(sector, SHK_SECTOR_SIZE);
        shk_copy(sector, data + offset, n);
        r = ata_write_sector(start + i, sector);
        if (r < 0) return r;
        offset += n;
    }
    shk_copy(&next, &shk_fs_root, sizeof(next));
    e = &next.entries[index];
    shk_zero(e, sizeof(*e));
    shk_copy(e->filename, name, name_len);
    e->start_lba = start;
    e->size_bytes = size;
    e->is_used = 1;
    next.checksum = shk_fs_checksum(&next);
    r = ata_write_sector(SHK_FS_ROOT_LBA, (const uint8_t *)&next);
    if (r < 0) { shk_fs_state = r; return r; }
    shk_copy(&shk_fs_root, &next, sizeof(next));
    return SHK_OK;
}
int fs_read_file(const char *name, uint8_t *buffer, uint32_t max_size) {
    uint8_t sector[SHK_SECTOR_SIZE];
    const shk_fs_entry_t *e;
    uint32_t offset = 0, lba;
    int index, r;
    if (shk_fs_state < 0) return shk_fs_state;
    if (!shk_fs_name_length(name)) return SHK_EINVAL;
    index = shk_fs_find(name);
    if (index < 0) return index;
    e = &shk_fs_root.entries[index];
    if (max_size < e->size_bytes) return SHK_EBUFFER;
    if (!buffer && e->size_bytes) return SHK_EINVAL;
    lba = e->start_lba;
    while (offset < e->size_bytes) {
        uint32_t n = e->size_bytes - offset;
        if (n > SHK_SECTOR_SIZE) n = SHK_SECTOR_SIZE;
        r = ata_read_sector(lba++, sector);
        if (r < 0) return r;
        shk_copy(buffer + offset, sector, n);
        offset += n;
    }
    return (int)e->size_bytes;
}
/* ===== End of embedded ATA / ShkodyaFS ===== */

/* Minimal libc */
void memcpy(void *dest, const void *src, uint32_t n) {
    uint32_t *d32 = (uint32_t *)dest;
    const uint32_t *s32 = (const uint32_t *)src;
    uint32_t words = n >> 2;
    while (words >= 4) {
        d32[0] = s32[0]; d32[1] = s32[1]; d32[2] = s32[2]; d32[3] = s32[3];
        d32 += 4; s32 += 4; words -= 4;
    }
    while (words--) *d32++ = *s32++;
    uint8_t *d8 = (uint8_t *)d32;
    const uint8_t *s8 = (const uint8_t *)s32;
    uint32_t rem = n & 3;
    while (rem--) *d8++ = *s8++;
}

void memset(void *dest, int value, uint32_t n) {
    uint8_t b = (uint8_t)value;
    uint32_t val32 = (b << 24) | (b << 16) | (b << 8) | b;
    uint32_t *d32 = (uint32_t *)dest;
    uint32_t words = n >> 2;
    while (words >= 4) {
        d32[0] = val32; d32[1] = val32; d32[2] = val32; d32[3] = val32;
        d32 += 4; words -= 4;
    }
    while (words--) *d32++ = val32;
    uint8_t *d8 = (uint8_t *)d32;
    uint32_t rem = n & 3;
    while (rem--) *d8++ = b;
}

uint32_t strlen(const char *s) {
    uint32_t n = 0;
    if (s == NULL) return 0;
    while (s[n] != '\0') n++;
    return n;
}

void itoa(int32_t value, char *buf, int base) {
    char tmp[16];
    uint32_t u;
    int i, j, neg;
    if (buf == NULL) return;
    if (base < 2 || base > 16) { buf[0] = '0'; buf[1] = '\0'; return; }
    if (value == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    neg = 0;
    if (value < 0 && base == 10) { neg = 1; u = (uint32_t)(-value); } else u = (uint32_t)value;
    i = 0;
    while (u > 0) {
        uint32_t digit = u % (uint32_t)base;
        tmp[i] = (digit < 10) ? (char)('0' + digit) : (char)('A' + digit - 10);
        i++;
        u /= (uint32_t)base;
    }
    j = 0;
    if (neg) { buf[j] = '-'; j++; }
    while (i > 0) { i--; buf[j] = tmp[i]; j++; }
    buf[j] = '\0';
}

static int strcmp_c(const char *a, const char *b) {
    uint32_t i = 0;
    if (a == NULL || b == NULL) return 1;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return (int)((uint8_t)a[i] - (uint8_t)b[i]);
        i++;
    }
    return (int)((uint8_t)a[i] - (uint8_t)b[i]);
}

static int strncmp_c(const char *a, const char *b, uint32_t n) {
    uint32_t i = 0;
    while (i < n) {
        if (a[i] != b[i]) return (int)((uint8_t)a[i] - (uint8_t)b[i]);
        if (a[i] == '\0') return 0;
        i++;
    }
    return 0;
}

static void strcpy_c(char *dest, const char *src) {
    uint32_t i = 0;
    while (src[i] != '\0') { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}

static void strcat_c(char *dest, const char *src) {
    uint32_t i = strlen(dest);
    uint32_t j = 0;
    while (src[j] != '\0') { dest[i + j] = src[j]; j++; }
    dest[i + j] = '\0';
}

/* Decimal append helper for the on-screen status/metric readouts. */
static void append_int(char *dest, int32_t value) {
    char tmp[16];
    itoa(value, tmp, 10);
    strcat_c(dest, tmp);
}

/* Fast DJB2 Password Hash */
static uint32_t hash_password(const char *pwd) {
    uint32_t hash = 5381;
    while (*pwd) {
        hash = ((hash << 5) + hash) ^ (uint8_t)(*pwd++);
    }
    return hash;
}

/* Surface that putpixel()/draw_char() paint into. NULL means the real back
   buffer; the Shkodya EXE loader points it at the guest overlay so an exec'd
   program draws on its own layer instead of racing the compositor, which
   repaints the wallpaper (and would otherwise erase the guest's output). */
static uint32_t *draw_target = (uint32_t *)0;

static inline uint32_t *draw_surface(void) {
    return draw_target ? draw_target : backbuffer;
}

static inline uint32_t *bb_ptr(int x, int y) {
    return draw_surface() + (uint32_t)y * fb_width + (uint32_t)x;
}

static inline uint32_t alpha_blend(uint32_t dst, uint32_t src) {
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa == 0) return dst;
    if (sa == 255) return src;
    uint32_t s_rb = src & 0x00FF00FF;
    uint32_t d_rb = dst & 0x00FF00FF;
    uint32_t rb = (d_rb + ((((s_rb - d_rb) * sa) + 0x00800080) >> 8)) & 0x00FF00FF;
    uint32_t s_g = src & 0x0000FF00;
    uint32_t d_g = dst & 0x0000FF00;
    uint32_t g = (d_g + ((((s_g - d_g) * sa) + 0x00008000) >> 8)) & 0x0000FF00;
    return 0xFF000000 | rb | g;
}

static inline uint32_t blend_color(uint32_t bg, uint32_t fg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;
    uint32_t rb_bg = bg & 0x00FF00FF;
    uint32_t g_bg  = bg & 0x0000FF00;
    uint32_t rb_fg = fg & 0x00FF00FF;
    uint32_t g_fg  = fg & 0x0000FF00;
    uint32_t rb = rb_bg + ((((rb_fg - rb_bg) * alpha) >> 8) & 0x00FF00FF);
    uint32_t g  = g_bg  + ((((g_fg - g_bg) * alpha) >> 8) & 0x0000FF00);
    return (rb & 0x00FF00FF) | (g & 0x0000FF00) | 0xFF000000;
}

static inline uint32_t lerp_color(uint32_t c1, uint32_t c2, int step, int total) {
    if (total <= 0 || step <= 0) return c1;
    if (step >= total) return c2;
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    int r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    int r = r1 + ((r2 - r1) * step) / total;
    int g = g1 + ((g2 - g1) * step) / total;
    int b = b1 + ((b2 - b1) * step) / total;
    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Graphics Drawing Primitives */
void putpixel(int x, int y, uint32_t color) {
    if ((uint32_t)x >= fb_width || (uint32_t)y >= fb_height) return;
    draw_surface()[(uint32_t)y * fb_width + (uint32_t)x] = color;
}

void putpixel_alpha(int x, int y, uint32_t color) {
    if ((uint32_t)x >= fb_width || (uint32_t)y >= fb_height) return;
    uint32_t *p = bb_ptr(x, y);
    *p = alpha_blend(*p, color);
}

void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy;
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (1) {
        putpixel(x, y, color);
        if (x == x1 && y == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}

void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_width)  w = (int)fb_width - x;
    if (y + h > (int)fb_height) h = (int)fb_height - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        uint32_t *row = backbuffer + yy * fb_width + x;
        int i = 0;
        while (i <= w - 4) {
            row[i] = color; row[i+1] = color; row[i+2] = color; row[i+3] = color;
            i += 4;
        }
        while (i < w) { row[i] = color; i++; }
    }
}

void fill_rect_alpha(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_width)  w = (int)fb_width - x;
    if (y + h > (int)fb_height) h = (int)fb_height - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        uint32_t *row = backbuffer + yy * fb_width + x;
        for (int xx = 0; xx < w; xx++) {
            row[xx] = alpha_blend(row[xx], color);
        }
    }
}

/* Clipped horizontal span writers. Every rounded-rect span goes through
   these, so a rectangle with negative or oversized x can never scribble
   outside the back buffer (the old code only checked y). */
static inline void span_opaque(int x, int y, int w, uint32_t color) {
    if (w <= 0 || (uint32_t)y >= fb_height) return;
    if (x < 0) { w += x; x = 0; }
    if (x >= (int)fb_width) return;
    if (x + w > (int)fb_width) w = (int)fb_width - x;
    if (w <= 0) return;
    uint32_t *row = backbuffer + (uint32_t)y * fb_width + (uint32_t)x;
    for (int i = 0; i < w; i++) row[i] = color;
}

static inline void span_alpha(int x, int y, int w, uint32_t color) {
    if (w <= 0 || (uint32_t)y >= fb_height) return;
    if (x < 0) { w += x; x = 0; }
    if (x >= (int)fb_width) return;
    if (x + w > (int)fb_width) w = (int)fb_width - x;
    if (w <= 0) return;
    uint32_t *row = backbuffer + (uint32_t)y * fb_width + (uint32_t)x;
    for (int i = 0; i < w; i++) row[i] = alpha_blend(row[i], color);
}

static void draw_rounded_spans(int x, int y, int w, int h, int r, uint32_t color, int is_alpha) {
    if (r <= 0) {
        if (is_alpha) fill_rect_alpha(x, y, w, h, color);
        else fill_rect(x, y, w, h, color);
        return;
    }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    if (is_alpha) fill_rect_alpha(x, y + r, w, h - 2 * r, color);
    else fill_rect(x, y + r, w, h - 2 * r, color);

    int r2 = r * r;
    for (int dy = 0; dy < r; dy++) {
        int py_top = y + (r - 1 - dy);
        int py_bot = y + h - r + dy;
        int dx = 0;
        while ((r - dx) * (r - dx) + dy * dy > r2 && dx < r) dx++;
        int seg_x = x + dx, seg_w = w - 2 * dx;
        if (seg_w <= 0) continue;
        if (is_alpha) {
            span_alpha(seg_x, py_top, seg_w, color);
            span_alpha(seg_x, py_bot, seg_w, color);
        } else {
            span_opaque(seg_x, py_top, seg_w, color);
            span_opaque(seg_x, py_bot, seg_w, color);
        }
    }
}

/* ---- subpixel coverage for rounded corners -------------------------------- */

/* Fraction of pixel (px,py) that lies inside the disc of radius r centred on
 * pixel (cx,cy), as 0..255.
 *
 * The previous corner code ramped alpha linearly in d*d, which is not distance:
 * a pixel just inside the arc and one just outside got a gradient that did not
 * match where the curve actually crosses that pixel, and the eye reads the
 * result as a staircase. This supersamples instead - 4x4 points per pixel - so
 * the value really is the covered fraction.
 *
 * Integer only. Subsamples sit at odd eighths of a pixel from the pixel centre,
 * so the whole test is dx*dx + dy*dy <= (r*8)^2: no sqrt, no float, nothing that
 * wants an FPU. A corner is r*r pixels and there are four of them per window, so
 * sixteen samples each is affordable.
 */
static int corner_coverage(int px, int py, int cx, int cy, int r) {
    const int S = 4;                     /* 4x4 subsamples -> 17 alpha levels */
    int r8 = r * 8;
    int lim = r8 * r8;
    int bx = (px - cx) * 8;
    int by = (py - cy) * 8;
    int inside = 0;
    for (int sy = 0; sy < S; sy++) {
        int dy = by + 2 * sy - 3;        /* -3, -1, +1, +3 eighths */
        int dy2 = dy * dy;
        for (int sx = 0; sx < S; sx++) {
            int dx = bx + 2 * sx - 3;
            if (dx * dx + dy2 <= lim) inside++;
        }
    }
    return inside * 255 / (S * S);
}

/* Blend `cov`/255 of `color` into (x,y). cov == 255 writes opaquely, which also
 * keeps the interior of a fill from being pointlessly blended. */
static inline void blend_cov(int x, int y, uint32_t color, int cov) {
    if (cov <= 0) return;
    if ((uint32_t)x >= fb_width || (uint32_t)y >= fb_height) return;
    uint32_t *p = draw_surface() + (uint32_t)y * fb_width + (uint32_t)x;
    if (cov >= 255) { *p = color | 0xFF000000u; return; }
    *p = alpha_blend(*p, ((uint32_t)cov << 24) | (color & 0x00FFFFFFu));
}

void fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    draw_rounded_spans(x, y, w, h, r, color, 0);
}

void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color) {
    draw_rounded_spans(x, y, w, h, r, color, 1);
}

void fill_rounded_rect_aa(int x, int y, int w, int h, int r, uint32_t color) {
    if (r <= 0) { fill_rect(x, y, w, h, color); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r <= 0) { fill_rect(x, y, w, h, color); return; }

    /* The middle w-2r columns are solid for the full height; only the two r-wide
     * corner bands need per-pixel coverage. */
    fill_rect(x + r, y, w - 2 * r, h, color);

    for (int py = y; py < y + h; py++) {
        int is_top = (py < y + r);
        int is_bot = (py >= y + h - r);
        if (!is_top && !is_bot) {
            fill_rect(x, py, r, 1, color);
            fill_rect(x + w - r, py, r, 1, color);
            continue;
        }
        int cy = is_top ? (y + r) : (y + h - 1 - r);
        for (int k = 0; k < r; k++) {
            blend_cov(x + k, py, color,
                      corner_coverage(x + k, py, x + r, cy, r));
            blend_cov(x + w - 1 - k, py, color,
                      corner_coverage(x + w - 1 - k, py, x + w - 1 - r, cy, r));
        }
    }
}

/* One row of a box whose top corners are rounded and whose bottom edge is square
 * because it meets something below - a title bar meeting the window body.
 *
 * The title bar previously wrote opaque rows using a binary per-row inset, which
 * is what produced the staircase: a row either started at the inset or it did
 * not, with nothing in between. Here the corner pixels get the same coverage the
 * window body computes, so the two arcs are identical and line up exactly.
 */
static void fill_row_rounded_top(int x, int py, int w, int r, int y_top, uint32_t color) {
    if (w <= 0) return;
    if (r * 2 > w) r = w / 2;
    int dy = py - y_top;
    if (r <= 0 || dy >= r) {                /* below the arc: full width */
        fill_rect(x, py, w, 1, color);
        return;
    }
    int cy = y_top + r;
    for (int k = 0; k < r; k++) {
        blend_cov(x + k, py, color,
                  corner_coverage(x + k, py, x + r, cy, r));
        blend_cov(x + w - 1 - k, py, color,
                  corner_coverage(x + w - 1 - k, py, x + w - 1 - r, cy, r));
    }
    fill_rect(x + r, py, w - 2 * r, 1, color);
}

/* One-pixel antialiased outline of a rounded rectangle.
 *
 * The coverage of a 1px stroke is the difference between two nested discs that
 * share a centre: the outer of radius r and the inner of radius r-1. Taking the
 * difference of their covered fractions gives the stroke a smooth edge, instead
 * of the corners simply being left out as they were before.
 */
static void stroke_rounded_rect_aa(int x, int y, int w, int h, int r, uint32_t color) {
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r <= 1) return;

    for (int dx = r; dx < w - r; dx++) {
        blend_cov(x + dx, y, color, 255);
        blend_cov(x + dx, y + h - 1, color, 255);
    }
    for (int dy = r; dy < h - r; dy++) {
        blend_cov(x, y + dy, color, 255);
        blend_cov(x + w - 1, y + dy, color, 255);
    }

    int cxl = x + r, cxr = x + w - 1 - r;
    int cyt = y + r, cyb = y + h - 1 - r;
    for (int k = 0; k < r; k++) {
        for (int j = 0; j < r; j++) {
            int cov;
            cov = corner_coverage(x + k, y + j, cxl, cyt, r)
                - corner_coverage(x + k, y + j, cxl, cyt, r - 1);
            blend_cov(x + k, y + j, color, cov);

            cov = corner_coverage(x + w - 1 - k, y + j, cxr, cyt, r)
                - corner_coverage(x + w - 1 - k, y + j, cxr, cyt, r - 1);
            blend_cov(x + w - 1 - k, y + j, color, cov);

            cov = corner_coverage(x + k, y + h - 1 - j, cxl, cyb, r)
                - corner_coverage(x + k, y + h - 1 - j, cxl, cyb, r - 1);
            blend_cov(x + k, y + h - 1 - j, color, cov);

            cov = corner_coverage(x + w - 1 - k, y + h - 1 - j, cxr, cyb, r)
                - corner_coverage(x + w - 1 - k, y + h - 1 - j, cxr, cyb, r - 1);
            blend_cov(x + w - 1 - k, y + h - 1 - j, color, cov);
        }
    }
}

/* 1px rounded outline (alpha-blended). Only the perimeter is touched:
   the interior of the ring is covered by the opaque window body that is
   painted right after, so blending it was pure wasted work (the old
   version alpha-filled the whole window area on every frame). */
static void rounded_ring_alpha(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r < 1) r = 1;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 1) return;
    int x1 = x + w - 1, y1 = y + h - 1;
    /* straight edges */
    for (int px = x + r; px <= x1 - r; px++) {
        putpixel_alpha(px, y, color);
        putpixel_alpha(px, y1, color);
    }
    for (int py = y + r; py <= y1 - r; py++) {
        putpixel_alpha(x, py, color);
        putpixel_alpha(x1, py, color);
    }
    /* corner rows: same span insets as the original rounded fill, but only
       the sliver beside the window body (the middle is covered by it) */
    int r2 = r * r;
    for (int i = 0; i < r; i++) {
        int dy = (r - 1) - i;
        int d = 0;
        while ((r - d) * (r - d) + dy * dy > r2 && d < r) d++;
        int py_t = y + i, py_b = y1 - i;
        for (int px = x + d; px <= x + r - 1; px++) {
            putpixel_alpha(px, py_t, color);
            putpixel_alpha(px, py_b, color);
        }
        for (int px = x1 - r + 1; px <= x1 - d; px++) {
            putpixel_alpha(px, py_t, color);
            putpixel_alpha(px, py_b, color);
        }
    }
}

/* Horizontal inset a rounded corner needs at row `y`, so a span drawn with it
 * cannot spill past the arc.
 *
 * These exist because both gradient fills used to hand-roll this and got it
 * wrong in ways that showed up as small square blocks in the corners:
 *
 *   - the window title bar tested `indent*indent + dy*dy > radius*radius`. At
 *     y == 0 that is `144 > 144`, i.e. false, so the loop never advanced and row
 *     0 was painted the full width of the window, straight across both top
 *     rounded corners. Row 1 then jumped to inset 5, leaving a stepped square.
 *   - tb_surface jumped its inset from `radius` to `2` and back, so each end of
 *     the taskbar carried a flat rectangular band at the top and at the bottom.
 *
 * Returning an inset that follows the arc fixes both without changing the
 * intended look: the gradient simply gets narrower where the shape curves. */
static int corner_inset_top(int radius, int y) {
    if (radius < 1 || y >= radius) return 0;
    int dy = radius - y, dx = radius;
    while (dx > 0 && dx * dx + dy * dy > radius * radius) dx--;
    return radius - dx;
}

static int corner_inset_bottom(int radius, int y, int box_h) {
    if (radius < 1 || y < box_h - radius) return 0;
    int dy = y - (box_h - radius) + 1;
    if (dy > radius) dy = radius;
    int dx = radius;
    while (dx > 0 && dx * dx + dy * dy > radius * radius) dx--;
    return radius - dx;
}

/* Window shadow removed for performance — replaced by thin outline border */
void draw_window_shadow(int x, int y, int w, int h) {
    /* Subtle 1px outer glow ring instead of heavy multi-layer shadows */
    rounded_ring_alpha(x - 1, y - 1, w + 2, h + 2, 11, C_SHADOW);
}

void blit_backbuffer(void) {
    uint32_t pitch_pixels = fb_pitch >> 2;
    if (pitch_pixels == fb_width) {
        memcpy(framebuffer_addr, backbuffer, (fb_width * fb_height) << 2);
    } else {
        for (uint32_t y = 0; y < fb_height; y++) {
            uint8_t  *dst_row = (uint8_t *)framebuffer_addr + y * fb_pitch;
            uint32_t *src_row = backbuffer + y * fb_width;
            memcpy(dst_row, src_row, fb_width << 2);
        }
    }
}

/* GDT & IDT & PIC */
static void gdt_encode(uint8_t *target, uintptr_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    target[0] = (uint8_t)(limit & 0xFF);
    target[1] = (uint8_t)((limit >> 8) & 0xFF);
    target[2] = (uint8_t)(base & 0xFF);
    target[3] = (uint8_t)((base >> 8) & 0xFF);
    target[4] = (uint8_t)((base >> 16) & 0xFF);
    target[5] = access;
    target[6] = (uint8_t)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    target[7] = (uint8_t)((base >> 24) & 0xFF);
}

static void gdt_load(void) {
    struct {
        uint16_t  limit;
        uintptr_t base;
    } __attribute__((packed)) gdtr;
    gdtr.limit = gdt_limit;
    gdtr.base = gdt_base;
#if defined(__x86_64__)
    /* In long mode CS cannot be reloaded with a far jump to an immediate
       selector; push the selector and the return address, then lretq. */
    __asm__ volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        : : "m"(gdtr) : "rax", "memory"
    );
#else
    __asm__ volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:\n\t"
        : : "m"(gdtr) : "eax", "memory"
    );
#endif
}

static void gdt_install(void) {
    memset(gdt_table, 0, 24);
#if defined(__x86_64__)
    /* Code descriptor needs L=1 (64-bit) with D/B clear; the data descriptor
       is unchanged from the 32-bit build. */
    gdt_encode(gdt_table + 8,  0x00000000, 0x000FFFFF, 0x9A, 0xA0);
#else
    gdt_encode(gdt_table + 8,  0x00000000, 0x000FFFFF, 0x9A, 0xC0);
#endif
    gdt_encode(gdt_table + 16, 0x00000000, 0x000FFFFF, 0x92, 0xC0);
    gdt_limit = 23;
    gdt_base = (uintptr_t)gdt_table;
    gdt_load();
}

static void idt_set_gate(uint8_t num, uintptr_t handler, uint16_t selector, uint8_t flags) {
#if defined(__x86_64__)
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFFu);
    idt[num].reserved    = 0;
#else
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
#endif
}

static void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    outb(PIC1_COMMAND, 0x11); io_wait();
    outb(PIC2_COMMAND, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

static void pic_unmask_irqs(void) {
    uint8_t mask1 = inb(PIC1_DATA) & ~0x07;
    uint8_t mask2 = inb(PIC2_DATA) & ~0x10;
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

static void idt_install(void) {
    memset(idt, 0, sizeof(idt));
    for (uint32_t i = 0; i < 256; i++) idt_set_gate((uint8_t)i, 0, 0x08, 0x8E);
    idt_set_gate(IRQ_TIMER_VECTOR,    (uintptr_t)timer_handler_asm,    0x08, 0x8E);
    idt_set_gate(IRQ_KEYBOARD_VECTOR, (uintptr_t)keyboard_handler_asm, 0x08, 0x8E);
    idt_set_gate(IRQ_MOUSE_VECTOR,    (uintptr_t)mouse_handler_asm,    0x08, 0x8E);
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base = (uintptr_t)&idt[0];
    load_idt((uintptr_t)&idtp);
}

/* PIT Timer */
void handle_timer(void) {
    system_ticks++;
    if (system_ticks % 100 == 0) need_redraw = 1;
    sound_update();   /* switch off an expired beep without busy-waiting */
    pic_send_eoi(0);
}

static void pit_install(void) {
    uint32_t divisor = PIT_HZ / PIT_FREQ;
    uint8_t lo = (uint8_t)(divisor & 0xFF);
    uint8_t hi = (uint8_t)((divisor >> 8) & 0xFF);
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, lo);
    outb(PIT_CHANNEL0, hi);
    system_ticks = 0;
}

/* PC Speaker Sound Driver */
static uint32_t sound_off_tick = 0;
static int sound_tone_pending = 0;  /* a sound_beep() tone awaiting its cutoff */

static void sound_stop(void) {
    sound_tone_pending = 0;
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

static void sound_raw_on(uint32_t freq) {
    /* a direct raw tone (purr, chiptune, media) overrides a pending cutoff */
    sound_tone_pending = 0;
    if (freq == 0 || sound_muted || sound_volume <= 0) { sound_stop(); return; }
    uint32_t div = PIT_HZ / freq;
    if (div == 0) div = 1;
    if (div > 65535) div = 65535;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp | 0x03);
}

/* Non-blocking beep: start the tone now, let sound_update() (main loop,
   timer tick) switch it off. The old version busy-waited up to ~1.2M NOPs
   with interrupts live — that stall was hit on every UI click and on the
   CapsLock key (which is handled straight from the keyboard IRQ). */
static void sound_beep(uint32_t freq, uint32_t ticks) {
    if (sound_muted || sound_volume <= 0 || freq == 0) { sound_stop(); return; }
    sound_raw_on(freq);
    sound_off_tick = system_ticks + (ticks ? ticks : 1);
    sound_tone_pending = 1;
}

static void sound_update(void) {
    if (sound_tone_pending && (int32_t)(system_ticks - sound_off_tick) >= 0) {
        sound_stop();
    }
}

/* UI click sound — short high tick */
static void sound_click(void) {
    if (!sound_effects_enabled || sound_muted) return;
    sound_beep(1200, 1);
}

/* Cat purr — low-frequency rumble that oscillates, played for PURR_DURATION_TICKS */
/* `delay` lets a short chirp land before the purr overrides the speaker. */
static void sound_purr_start(uint32_t delay) {
    purr_active = 1;
    purr_start_tick = system_ticks + delay;
}

static void purr_update(void) {
    if (!purr_active) return;
    /* Signed compare: a delayed start makes the raw difference underflow. */
    if ((int32_t)(system_ticks - purr_start_tick) > PURR_DURATION_TICKS) {
        purr_active = 0;
        sound_stop();
        need_redraw = 1;
        return;
    }
    /* Oscillate between ~25Hz and ~40Hz rumble */
    uint32_t elapsed = (int32_t)(system_ticks - purr_start_tick) > 0
                     ? system_ticks - purr_start_tick : 0;
    uint32_t phase = elapsed % 8;
    uint32_t freq = 25 + (phase < 4 ? phase : 7 - phase) * 4;
    sound_raw_on(freq);
}

/* ===== Theme System ===== */
static void theme_apply(int theme_id) {
    current_theme = theme_id;
    switch (theme_id) {
        case THEME_MIDNIGHT:
            C_BLACK = 0xFF070B14; C_WHITE = 0xFFECE8E0;
            C_SURFACE = 0xFF161B28; C_SURFACE2 = 0xFF1E2534;
            C_TITLE_HI = 0xFF2C3A57; C_TITLE_LO = 0xFF1A2130;
            C_BORDER = 0xFF2E374B; C_ACCENT = 0xFF8FA4C4;
            C_MUTED = 0xFF97A1B6; C_TASKBAR = 0xDC0A0E18;
            C_TOAST_BG = 0xE01A2130; C_TOAST_BORDER = 0xFF445068;
            C_PAPER = 0xFF10141D; C_PAPER_FG = 0xFFE2E8F2;
            C_FIELD = 0xFF0B0F16; C_SUCCESS = 0xFF1F5F3A;
            C_DANGER_BG = 0xFF4A1D22; C_CHROME_HI = 0xFF2A3446;
            C_VIEW_BG = 0xFF080C12;
            break;
        case THEME_SUNSET:
            C_BLACK = 0xFF1A0A0A; C_WHITE = 0xFFFFF5E6;
            C_SURFACE = 0xFF2D1810; C_SURFACE2 = 0xFF3D2018;
            C_TITLE_HI = 0xFF6B3522; C_TITLE_LO = 0xFF3D2018;
            C_BORDER = 0xFF5C3A2A; C_ACCENT = 0xFFE8985C;
            C_MUTED = 0xFFC4A88A; C_TASKBAR = 0xD01A0A0A;
            C_TOAST_BG = 0xE02D1810; C_TOAST_BORDER = 0xFF8B5A3C;
            C_PAPER = 0xFF1E1008; C_PAPER_FG = 0xFFF6E9DA;
            C_FIELD = 0xFF180C06; C_SUCCESS = 0xFF2F6B33;
            C_DANGER_BG = 0xFF5A2118; C_CHROME_HI = 0xFF4A2818;
            C_VIEW_BG = 0xFF140A06;
            break;
        case THEME_FOREST:
            C_BLACK = 0xFF0A1410; C_WHITE = 0xFFE8F5E9;
            C_SURFACE = 0xFF1B2E26; C_SURFACE2 = 0xFF243A30;
            C_TITLE_HI = 0xFF2E5D44; C_TITLE_LO = 0xFF1E3329;
            C_BORDER = 0xFF2A4A3A; C_ACCENT = 0xFF6BBF8A;
            C_MUTED = 0xFF8AAB9A; C_TASKBAR = 0xD00A1410;
            C_TOAST_BG = 0xE01B2E26; C_TOAST_BORDER = 0xFF4A7A5A;
            C_PAPER = 0xFF0F1B15; C_PAPER_FG = 0xFFE4F2E6;
            C_FIELD = 0xFF0A130F; C_SUCCESS = 0xFF276B3E;
            C_DANGER_BG = 0xFF3E2222; C_CHROME_HI = 0xFF2E4A3C;
            C_VIEW_BG = 0xFF08120D;
            break;
        case THEME_OCEAN:
            C_BLACK = 0xFF050E1A; C_WHITE = 0xFFE0F2F7;
            C_SURFACE = 0xFF0D2E44; C_SURFACE2 = 0xFF143A52;
            C_TITLE_HI = 0xFF1E5A7E; C_TITLE_LO = 0xFF0D2E44;
            C_BORDER = 0xFF1E4A66; C_ACCENT = 0xFF4FC3F7;
            C_MUTED = 0xFF7AAAB8; C_TASKBAR = 0xD0050E1A;
            C_TOAST_BG = 0xE00D2E44; C_TOAST_BORDER = 0xFF2A6A88;
            C_PAPER = 0xFF0A2233; C_PAPER_FG = 0xFFE0F2F7;
            C_FIELD = 0xFF061523; C_SUCCESS = 0xFF1B6B5A;
            C_DANGER_BG = 0xFF46202A; C_CHROME_HI = 0xFF1B4258;
            C_VIEW_BG = 0xFF050C14;
            break;
        case THEME_ROSE:
            C_BLACK = 0xFF1A0A14; C_WHITE = 0xFFFCE4EC;
            C_SURFACE = 0xFF2D1424; C_SURFACE2 = 0xFF3D1E30;
            C_TITLE_HI = 0xFF6B224E; C_TITLE_LO = 0xFF3D1E30;
            C_BORDER = 0xFF5C2A44; C_ACCENT = 0xFFF06292;
            C_MUTED = 0xFFC48AAA; C_TASKBAR = 0xD01A0A14;
            C_TOAST_BG = 0xE02D1424; C_TOAST_BORDER = 0xFF8B3A6A;
            C_PAPER = 0xFF20101C; C_PAPER_FG = 0xFFFBE7EF;
            C_FIELD = 0xFF170A13; C_SUCCESS = 0xFF3A5F3A;
            C_DANGER_BG = 0xFF5A1F35; C_CHROME_HI = 0xFF45243A;
            C_VIEW_BG = 0xFF120810;
            break;
    }
    need_redraw = 1;
}

static const char *theme_names[THEME_COUNT] = {
    "Midnight", "Sunset", "Forest", "Ocean", "Rose"
};

/* Keyboard Scan-Code Tables */
static const char kbd_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char kbd_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char kbd_map_ru[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 0xA9, 0xE6, 0xE3, 0xAA, 0xA5, 0xAD, 0xA3, 0xE8, 0xE9, 0xA7, 0xE5, 0xEA, '\n',
    0, 0xE4, 0xEB, 0xA2, 0xA0, 0xAF, 0xE0, 0xAE, 0xAB, 0xA4, 0xA6, 0xED, 0xEC,
    0, '\\', 0xEF, 0xE7, 0xE1, 0xAC, 0xA8, 0xE2, 0xEC, 0xA1, 0xEE, '.', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char kbd_map_ru_shift[128] = {
    0,  27, '!', '"', '#', ';', '%', ':', '?', '*', '(', ')', '_', '+', '\b',
    '\t', 0x89, 0x96, 0x93, 0x8A, 0x85, 0x8D, 0x83, 0x98, 0x99, 0x87, 0x95, 0x9A, '\n',
    0, 0x94, 0x9B, 0x82, 0x80, 0x8F, 0x90, 0x8E, 0x8B, 0x84, 0x86, 0x9D, 0x9C,
    0, '/', 0x9F, 0x97, 0x91, 0x8C, 0x88, 0x92, 0x9C, 0x81, 0x9E, ',', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void kbd_push(uint8_t ch) {
    uint32_t next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_tail) return;
    kbd_buf[kbd_head] = ch;
    kbd_head = next;
}

static int kbd_pop(uint8_t *out) {
    if (kbd_head == kbd_tail) return 0;
    *out = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return 1;
}

void handle_keyboard(void) {
    uint8_t scancode = inb(KBD_DATA);
    if (scancode == 0xE0) {
        kbd_extended = 1;
        pic_send_eoi(1);
        return;
    }
    uint8_t released = (uint8_t)(scancode & 0x80);
    uint8_t code = (uint8_t)(scancode & 0x7F);
    kbd_extended = 0;

    if (code == 0x2A || code == 0x36) {
        kbd_shift = !released;
        if (!released && kbd_alt) {
            current_lang = (current_lang == LANG_EN) ? LANG_RU : LANG_EN;
            need_redraw = 1;
        }
        pic_send_eoi(1);
        return;
    }

    if (code == 0x38) {
        kbd_alt = !released;
        if (!released && kbd_shift) {
            current_lang = (current_lang == LANG_EN) ? LANG_RU : LANG_EN;
            need_redraw = 1;
        }
        pic_send_eoi(1);
        return;
    }

    if (code == 0x3A) {
        if (!released) {
            kbd_caps = !kbd_caps;
            sound_beep(kbd_caps ? 880 : 440, 2);
            need_redraw = 1;
        }
        pic_send_eoi(1);
        return;
    }

    if (released) {
        pic_send_eoi(1);
        return;
    }

    /* Arrow keys: scan-code 0x48 = Up, 0x50 = Down. Forwarded as synthetic
       codes so the Terminal can walk its command history. QEMU and most PS/2
       boards send these E0-prefixed while the numeric keypad sends them bare;
       both are accepted here since the E0 flag is cleared before decoding.
       Neither scan code is mapped in kbd_map/kbd_map_shift, so nothing else
       loses a key. */
    if (code == 0x48 || code == 0x50) {
        kbd_push((uint8_t)(code == 0x48 ? TERM_KEY_UP : TERM_KEY_DOWN));
        need_redraw = 1;
        pic_send_eoi(1);
        return;
    }

    int is_letter = ((code >= 0x10 && code <= 0x19) || (code >= 0x1E && code <= 0x26) || (code >= 0x2C && code <= 0x32));
    int shift_effective = is_letter ? (kbd_shift ^ kbd_caps) : kbd_shift;
    char ch = 0;
    if (current_lang == LANG_RU) {
        ch = (char)(shift_effective ? kbd_map_ru_shift[code] : kbd_map_ru[code]);
    } else {
        ch = (char)(shift_effective ? kbd_map_shift[code] : kbd_map[code]);
    }

    if (ch != 0) {
        kbd_push((uint8_t)ch);
        need_redraw = 1;
    }
    pic_send_eoi(1);
}

/* PS/2 Mouse Controller */
static void ps2_wait_write(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        if ((inb(0x64) & 0x02) == 0) return;
    }
}

static void ps2_wait_read(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        if ((inb(0x64) & 0x01) == 1) return;
    }
}

static void mouse_write(uint8_t val) {
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, val);
}

static uint8_t mouse_read(void) {
    ps2_wait_read();
    return inb(0x60);
}

static void mouse_clamp(void) {
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= (int)fb_width)  mouse_x = (int)fb_width - 1;
    if (mouse_y >= (int)fb_height) mouse_y = (int)fb_height - 1;
}

void handle_mouse(void) {
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }
    uint8_t b = inb(0x60);
    outb(0xA0, 0x20);
    outb(0x20, 0x20);

    if (mouse_cycle == 0) {
        if ((b & 0x08) == 0) return;
        mouse_bytes[0] = (int8_t)b;
        mouse_cycle = 1;
    } else if (mouse_cycle == 1) {
        mouse_bytes[1] = (int8_t)b;
        mouse_cycle = 2;
    } else if (mouse_cycle == 2) {
        mouse_bytes[2] = (int8_t)b;
        mouse_cycle = 0;
        int8_t state = mouse_bytes[0];
        int8_t rel_x = mouse_bytes[1];
        int8_t rel_y = mouse_bytes[2];
        if (state & 0x80 || state & 0x40) return;
        mouse_left_prev = mouse_left;
        mouse_left = (state & 0x01) ? 1 : 0;
        mouse_right = (state & 0x02) ? 1 : 0;
        mouse_x += rel_x;
        mouse_y -= rel_y;
        mouse_clamp();
        /* Motion alone no longer forces a full recomposite: the main loop
           only re-presents the small cursor patch (see frame_present_cursor). */
        mouse_moved = 1;
    }
}

static void mouse_install(void) {
    uint8_t status;
    ps2_wait_write();
    outb(0x64, 0xA8);
    ps2_wait_write();
    outb(0x64, 0x20);
    status = mouse_read();
    status |= 0x02;
    status &= ~0x20;
    ps2_wait_write();
    outb(0x64, 0x60);
    ps2_wait_write();
    outb(0x60, status);
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
    mouse_cycle = 0;
    mouse_x = fb_width / 2;
    mouse_y = fb_height / 2;
    mouse_left = 0;
    mouse_left_prev = 0;
}

/* Antialiased 8x16 glyph coverage, generated by tools/gen_font_aa.py.
 *
 * The previous font here was a 1-bit bitmap: one bit per pixel, so every
 * diagonal and curve came out as a staircase and there was nothing to blend
 * with. These tables carry 0..255 per pixel instead, rendered from an outline
 * font at 8x and downsampled, so draw_char can antialias edges.
 *
 * Kept as a separate include rather than pasted in: the tables are large, and
 * this way kernel.c stays openable. The file is committed, so a build needs no
 * Pillow - only regenerating the font does. */
#include "font_aa.inc"


/* Antialiased text: every pixel of a glyph carries its own coverage, so edges
 * are alpha-blended into whatever is already in the framebuffer instead of being
 * painted as hard pixels. Fully covered and fully empty pixels - the majority of
 * any glyph - skip the blend, so this stays cheap. */
void draw_char(int x, int y, char c, uint32_t color) {
    unsigned char uc = (unsigned char)c;
    const uint8_t (*glyph)[8] = NULL;

    if (uc < 128) {
        if (uc < 32 || uc == 127) return;      /* control codes draw nothing */
        glyph = font_aa_ascii[uc];
    } else if (uc >= 0x80 && uc <= 0xAF) {
        glyph = font_aa_ru[uc - 0x80];
    } else if (uc >= 0xE0 && uc <= 0xEF) {
        glyph = font_aa_ru[uc - 0xE0 + 48];
    } else if (uc == 0xF0 || uc == 0xF1) {
        glyph = font_aa_ascii['E'];            /* no Yo glyph; falls back to E */
    } else {
        return;                                /* outside the renderable set */
    }

    uint32_t rgb = color & 0x00FFFFFFu;
    for (int row = 0; row < 16; row++) {
        int py = y + row;
        if (py < 0 || py >= (int)fb_height) continue;
        uint32_t *dst = draw_surface() + (uint32_t)py * fb_width;
        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)fb_width) continue;
            int cov = glyph[row][col];
            if (cov == 0) continue;
            if (cov == 255) { dst[px] = rgb | 0xFF000000u; continue; }
            dst[px] = alpha_blend(dst[px], ((uint32_t)cov << 24) | rgb);
        }
    }
}

void draw_string(int x, int y, const char *s, uint32_t color) {
    while (*s) {
        draw_char(x, y, *s, color);
        x += 8;
        s++;
    }
}

void draw_wallpaper(void) {
    memcpy(backbuffer, wallpaper_data, fb_width * fb_height * 4);
}

/* Ginger cat Ricky, 16x16, white eyes */
static const uint32_t rikki_pal[8] = {
    0x00000000,
    COLOR_GINGER3,
    COLOR_GINGER,
    COLOR_GINGER2,
    COLOR_EYE,
    COLOR_INK,
    COLOR_AMBER,
    0xFF3A2218
};

static const uint8_t rikki_map[16][16] = {
    {0,0,0,2,2,0,0,0,0,0,0,2,2,0,0,0},
    {0,0,0,2,7,0,0,0,0,0,0,7,2,0,0,0},
    {0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0},
    {0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0},
    {0,0,3,3,3,3,1,1,1,1,3,3,3,3,0,0},
    {0,0,3,4,4,4,3,3,3,3,4,4,4,3,0,0},
    {0,0,3,4,5,4,3,3,3,3,4,5,4,3,0,0},
    {0,0,3,4,5,4,3,6,6,3,4,5,4,3,0,0},
    {0,3,3,3,3,3,3,6,6,3,3,3,3,3,3,0},
    {0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0},
    {0,0,3,3,3,3,7,7,7,7,3,3,3,3,0,0},
    {0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
    {0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0},
    {0,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0},
    {0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

void draw_rikki(int x, int y, int scale) {
    if (scale < 1) scale = 1;
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            uint8_t idx = rikki_map[row][col];
            uint32_t color = rikki_pal[idx];
            if ((color >> 24) != 0) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        putpixel(x + col * scale + sx, y + row * scale + sy, color);
                    }
                }
            }
        }
    }
}

static void draw_cursor(int x, int y) {
    static const uint8_t cursor_shape[20][16] = {
        {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {3,4,3,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {3,4,4,3,0,0,0,0,0,0,0,0,0,0,0,0},
        {3,4,4,4,3,0,0,0,0,0,0,0,0,0,0,0},
        {3,4,4,4,4,3,0,0,0,0,0,0,0,0,0,0},
        {3,4,4,4,4,4,3,0,0,0,0,0,0,0,0,0},
        {3,4,4,4,4,4,4,3,0,0,0,0,0,0,0,0},
        {3,4,4,4,4,4,4,4,3,0,0,0,0,0,0,0},
        {3,4,4,4,4,4,4,4,4,3,0,0,0,0,0,0},
        {3,4,4,4,4,4,4,4,4,4,3,0,0,0,0,0},
        {3,4,4,4,4,4,3,3,3,3,3,2,0,0,0,0},
        {3,4,4,3,4,4,3,0,0,0,0,1,0,0,0,0},
        {3,4,3,2,3,4,4,3,0,0,0,0,0,0,0,0},
        {3,3,0,0,3,4,4,3,0,0,0,0,0,0,0,0},
        {3,0,0,0,0,3,4,4,3,0,0,0,0,0,0,0},
        {0,0,0,0,0,3,4,4,3,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,3,4,4,3,0,0,0,0,0,0},
        {0,0,0,0,0,0,3,4,4,3,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0}
    };
    for (int cy = 0; cy < 20; cy++) {
        int py = y + cy;
        if (py < 0 || py >= (int)fb_height) continue;
        for (int cx = 0; cx < 16; cx++) {
            int px = x + cx;
            if (px < 0 || px >= (int)fb_width) continue;
            uint8_t code = cursor_shape[cy][cx];
            if (code == 0) continue;
            uint32_t bg = backbuffer[py * fb_width + px];
            uint32_t final_col;
            if (code == 4) final_col = 0xFFFFFFFF;
            else if (code == 3) final_col = 0xFF181A20;
            else if (code == 2) final_col = blend_color(bg, 0xFF000000, 160);
            else final_col = blend_color(bg, 0xFF000000, 70);
            backbuffer[py * fb_width + px] = final_col;
        }
    }
}

/* ===== Cursor overlay: small saved/restored region, no full repaint =====
   The pixels under the cursor are kept in a tiny 16x20 snapshot. Moving the
   mouse restores those pixels and only presents two small rectangles instead
   of re-compositing wallpaper + every window (3 MB memcpy + full blit). */
static uint32_t cursor_save[CURSOR_H][CURSOR_W];
static int cursor_saved = 0;
static int cursor_sx = 0, cursor_sy = 0;

/* Copy a rectangle of the back buffer to the real framebuffer (pitch aware) */
static void fb_blit_rect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_width)  w = (int)fb_width - x;
    if (y + h > (int)fb_height) h = (int)fb_height - y;
    if (w <= 0 || h <= 0) return;
    uint32_t pitch_pixels = fb_pitch >> 2;
    for (int yy = y; yy < y + h; yy++) {
        uint32_t *dst = framebuffer_addr + (uint32_t)yy * pitch_pixels + (uint32_t)x;
        const uint32_t *src = backbuffer + (uint32_t)yy * fb_width + (uint32_t)x;
        memcpy(dst, src, (uint32_t)w * 4);
    }
}

/* Store the clean pixels currently under the cursor position */
static void cursor_capture(void) {
    cursor_sx = mouse_x;
    cursor_sy = mouse_y;
    for (int cy = 0; cy < CURSOR_H; cy++) {
        int py = cursor_sy + cy;
        for (int cx = 0; cx < CURSOR_W; cx++) {
            int px = cursor_sx + cx;
            cursor_save[cy][cx] = ((uint32_t)px < fb_width && (uint32_t)py < fb_height)
                ? backbuffer[(uint32_t)py * fb_width + (uint32_t)px] : 0;
        }
    }
    cursor_saved = 1;
}

/* Put the saved pixels back (erases the old cursor from the back buffer) */
static void cursor_restore(void) {
    if (!cursor_saved) return;
    for (int cy = 0; cy < CURSOR_H; cy++) {
        int py = cursor_sy + cy;
        if ((uint32_t)py >= fb_height) continue;
        for (int cx = 0; cx < CURSOR_W; cx++) {
            int px = cursor_sx + cx;
            if ((uint32_t)px >= fb_width) continue;
            backbuffer[(uint32_t)py * fb_width + (uint32_t)px] = cursor_save[cy][cx];
        }
    }
    cursor_saved = 0;
}

/* VFS adapter: synchronous disk commits; never silently fall back to RAM. */
static const char *vfs_error_text(int error) {
    switch (error) {
        case SHK_OK: return "ShkodyaFS ready";
        case SHK_EINVAL: return "Invalid name (1-15 bytes) or buffer";
        case SHK_ENODEV: return "No primary-master ATA disk";
        case SHK_ETIMEOUT: return "ATA timeout";
        case SHK_EIO: return "Disk I/O failed; retry or reboot";
        case SHK_ERANGE: return "File/address exceeds supported range";
        case SHK_EUNSUPPORTED: return "Unsupported disk or nonblank boot sector";
        case SHK_ENOTREADY: return "Storage not initialized";
        case SHK_ECORRUPT: return "Invalid ShkodyaFS; disk left unchanged";
        case SHK_ENOENT: return "File not found";
        case SHK_ENOSPC: return "Directory full or no contiguous disk space";
        case SHK_EBUFFER: return "File too large for this text editor";
        default: return "Storage error";
    }
}

/* Preserve directory slot numbers; do not read file data during painting. */
static void vfs_refresh(void) {
    shk_zero(vfs_files, sizeof(vfs_files));
    if (fs_status() < 0) return;
    for (uint32_t i = 0; i < SHK_FS_MAX_FILES; ++i) {
        const shk_fs_entry_t *e = &shk_fs_root.entries[i];
        if (!e->is_used) continue;
        vfs_files[i].used = 1;
        shk_copy(vfs_files[i].name, e->filename, VFS_FILENAME_LEN);
        vfs_files[i].size = e->size_bytes;
    }
}

static void vfs_init(void) {
    const char *readme = "Shkodya OS\nShkodyaFS disk storage active.\n"
                         "Save commits files to disk.\nNames: 1-15 bytes; up to 16 files.\n";
    const char *todo = "- Play 2048\n- Beat Pong AI\n- Find Memory pairs\n- Listen to Chiptune\n";
    fs_init();
    vfs_boot_error = fs_status();
    /* Never recreate or overwrite user files on a later boot. */
    if (vfs_boot_error == SHK_OK && shk_fs_formatted) {
        vfs_boot_error = fs_write_file("readme.txt", (const uint8_t *)readme, strlen(readme));
        if (vfs_boot_error == SHK_OK)
            vfs_boot_error = fs_write_file("todo.txt", (const uint8_t *)todo, strlen(todo));
        /* Drop the demo executable in too, so `exec test.exe` works on a
           freshly formatted disk without any upload step. */
        if (vfs_boot_error == SHK_OK)
            vfs_boot_error = fs_write_file("test.exe", test_exe_blob, test_exe_size);
    }
    vfs_last_error = vfs_boot_error;
    vfs_refresh();
}

static int vfs_find(const char *name) {
    if (fs_status() < 0) return vfs_last_error = fs_status();
    if (!shk_fs_name_length(name)) return vfs_last_error = SHK_EINVAL;
    vfs_refresh();
    for (int i = 0; i < VFS_MAX_FILES; ++i) {
        if (vfs_files[i].used && strcmp_c(vfs_files[i].name, name) == 0) {
            vfs_last_error = SHK_OK;
            return i;
        }
    }
    return vfs_last_error = SHK_ENOENT;
}

/* Save by name in ONE transaction: no intermediate empty file on failure. */
static int vfs_write(const char *name, const uint8_t *data, uint32_t size) {
    int r = fs_write_file(name, data, size);
    vfs_last_error = r;
    vfs_refresh();
    need_redraw = 1;
    return r;
}

static int vfs_create(const char *name) {
    int index = vfs_find(name);
    if (index >= 0) return index; /* touch never truncates an existing file. */
    if (index != SHK_ENOENT) return index;
    int r = vfs_write(name, NULL, 0);
    if (r < 0) return r;
    return vfs_find(name);
}

/* Load text through staging so a partial disk read cannot destroy edits. */
static int vfs_read_text(const char *name, uint32_t capacity) {
    if (capacity == 0 || capacity > sizeof(vfs_text_buffer))
        return vfs_last_error = SHK_EINVAL;
    int n = fs_read_file(name, vfs_text_buffer, capacity - 1);
    if (n < 0) return vfs_last_error = n;
    for (int i = 0; i < n; ++i)
        if (vfs_text_buffer[i] == 0)
            return vfs_last_error = SHK_EUNSUPPORTED; /* Binary, not text. */
    vfs_text_buffer[n] = 0;
    vfs_last_error = SHK_OK;
    return n;
}

/* Toast Notifications */
void toast_show(const char *text) {
    uint32_t i = 0;
    toast.active = 1;
    if (!text) text = "";
    while (text[i] && i < sizeof(toast.text) - 1) {
        toast.text[i] = text[i];
        i++;
    }
    toast.text[i] = '\0';
    toast.display_start = system_ticks;
}

static void toast_update(void) {
    if (!toast.active) return;
    if (system_ticks - toast.display_start > TOAST_DISPLAY_TICKS) {
        toast.active = 0;
        need_redraw = 1;
    }
}

static void draw_toast(void) {
    if (!toast.active) return;
    int w = 280, h = 54;
    int x = (int)fb_width - w - 16;
    int y = (int)fb_height - TASKBAR_H - h - 16;
    fill_rounded_rect_alpha(x + 2, y + 4, w, h, 8, 0x40000000);
    fill_rounded_rect_alpha(x,     y,     w, h, 8, COLOR_TOAST_BG);
    draw_line(x, y, x + w, y, COLOR_TOAST_BORDER);
    fill_rounded_rect(x + 8, y + 10, 16, 16, 4, COLOR_ACCENT);
    draw_string(x + 13, y + 10, "i", COLOR_WHITE);
    draw_string(x + 32, y + 14, toast.text, COLOR_WHITE);
}

/* Window Manager Core */
static int point_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static int window_at(int px, int py) {
    int best = -1, best_z = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].used && point_in_rect(px, py, windows[i].x, windows[i].y, windows[i].w, windows[i].h)) {
            if (windows[i].z >= best_z) { best_z = windows[i].z; best = i; }
        }
    }
    return best;
}

static void focus_window(int index) {
    if (index < 0 || index >= MAX_WINDOWS || !windows[index].used) return;
    z_top++;
    windows[index].z = z_top;
    focused = index;
}

static int open_window(int app, const char *title, int x, int y, int w, int h) {
    int found = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].used && windows[i].app == app) { focus_window(i); return i; }
        if (!windows[i].used && found < 0) found = i;
    }
    if (found < 0) return -1;
    /* Keep every window inside the desktop work area. This used to be a
       STORE-only guard, but the 860px browser hung off the right edge. */
    if (w > (int)fb_width - 24) w = (int)fb_width - 24;
    if (h > (int)fb_height - TASKBAR_H - 16) h = (int)fb_height - TASKBAR_H - 16;
    if (x + w > (int)fb_width - 12) x = (int)fb_width - 12 - w;
    if (y + h > (int)fb_height - TASKBAR_H - 8) y = (int)fb_height - TASKBAR_H - 8 - h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    windows[found].used = 1;
    windows[found].app = app;
    windows[found].x = x;
    windows[found].y = y;
    windows[found].w = w;
    windows[found].h = h;
    strcpy_c(windows[found].title, title);
    focus_window(found);
    return found;
}

static void close_window(int index) {
    if (index < 0 || index >= MAX_WINDOWS) return;
    windows[index].used = 0;
    windows[index].app = APP_NONE;
    if (focused == index) {
        int best = -1, best_z = -1;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].used && windows[i].z >= best_z) { best_z = windows[i].z; best = i; }
        }
        focused = best;
    }
}

static void clamp_window(window_t *win) {
    if (win->w < 200) win->w = 200;
    if (win->h < 140) win->h = 140;
    int max_x = (int)fb_width - 48;
    int max_y = (int)fb_height - TASKBAR_H - TITLEBAR_H;
    if (win->x < 0) win->x = 0;
    if (win->y < 0) win->y = 0;
    if (win->x > max_x) win->x = max_x;
    if (win->y > max_y) win->y = max_y;
}

/* Standard Applications */
static void paint_clear(void) {
    for (uint32_t i = 0; i < PAINT_W * PAINT_H; i++) paint_canvas[i] = COLOR_WHITE;
    paint_last_x = -1; paint_last_y = -1; paint_drawing = 0;
}

static void paint_draw_brush_point(int x, int y, uint32_t color) {
    int radius = paint_brush_size;
    for (int dy = -radius + 1; dy < radius; dy++) {
        for (int dx = -radius + 1; dx < radius; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < PAINT_W && py >= 0 && py < PAINT_H)
                paint_canvas[py * PAINT_W + px] = color;
        }
    }
}

static void paint_stroke(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 - x0, dy = y1 - y0;
    int sx = (dx < 0) ? -1 : 1;
    int sy = (dy < 0) ? -1 : 1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = (dx > dy ? dx : -dy) / 2;
    int max_steps = 1000;
    while (max_steps-- > 0) {
        paint_draw_brush_point(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

static void notepad_reset(void) {
    const char *seed = "Shkodya OS Core v1.9\nUser: Stepa\n\nType here. Backspace deletes.\n";
    uint32_t i = 0;
    while (seed[i] != '\0' && i < NOTEPAD_SIZE - 1) { notepad_text[i] = seed[i]; i++; }
    notepad_text[i] = '\0';
    notepad_len = i;
    notepad_cursor = (int)i;
    strcpy_c(notepad_current_file, "readme.txt");
}

static void notepad_insert(char ch) {
    if (notepad_len + 1 >= NOTEPAD_SIZE) return;
    for (uint32_t i = notepad_len; (int)i > notepad_cursor; i--) notepad_text[i] = notepad_text[i - 1];
    notepad_text[notepad_cursor] = ch;
    notepad_len++;
    notepad_cursor++;
    notepad_text[notepad_len] = '\0';
}

static void notepad_backspace(void) {
    if (notepad_cursor <= 0) return;
    for (uint32_t i = (uint32_t)notepad_cursor; i < notepad_len; i++) notepad_text[i - 1] = notepad_text[i];
    notepad_len--;
    notepad_cursor--;
    notepad_text[notepad_len] = '\0';
}

static void notepad_new(void) {
    notepad_text[0] = '\0';
    notepad_len = 0;
    notepad_cursor = 0;
    strcpy_c(notepad_current_file, "untitled.txt");
    need_redraw = 1;
    toast_show("New document created");
}

static void notepad_save(void) {
    int r = vfs_write(notepad_current_file, (const uint8_t *)notepad_text, notepad_len);
    if (r == SHK_OK) {
        char toast_msg[64] = "Saved to disk: ";
        strcat_c(toast_msg, notepad_current_file);
        toast_show(toast_msg);
    } else {
        toast_show(vfs_error_text(r));
    }
    need_redraw = 1;
}

/* The secondary display line: "<left> <op> " while an operation is pending... */
static void calc_expr_pending(int32_t left, int op) {
    char num[16];
    char o[2];
    calc_expr[0] = '\0';
    if (op == 0) return;
    itoa(left, num, 10);
    strcat_c(calc_expr, num);
    strcat_c(calc_expr, " ");
    o[0] = (char)op; o[1] = '\0';
    strcat_c(calc_expr, o);
    strcat_c(calc_expr, " ");
}

/* ...and "<left> <op> <right> =" once it has been evaluated. */
static void calc_expr_done(int32_t left, int op, int32_t right) {
    char num[16];
    char o[2];
    calc_expr[0] = '\0';
    if (op == 0) return;
    itoa(left, num, 10);
    strcat_c(calc_expr, num);
    strcat_c(calc_expr, " ");
    o[0] = (char)op; o[1] = '\0';
    strcat_c(calc_expr, o);
    strcat_c(calc_expr, " ");
    itoa(right, num, 10);
    strcat_c(calc_expr, num);
    strcat_c(calc_expr, " =");
}

static void calc_reset(void) {
    calc_display[0] = '0'; calc_display[1] = '\0';
    calc_acc = 0; calc_op = 0; calc_fresh = 1;
    calc_expr[0] = '\0';
    calc_error = 0;
}

static void calc_digit(char d) {
    uint32_t n = strlen(calc_display);
    if (calc_error) calc_reset();               /* a digit starts a fresh sum */
    if (calc_fresh) { calc_display[0] = d; calc_display[1] = '\0'; calc_fresh = 0; return; }
    if (n == 1 && calc_display[0] == '0') { calc_display[0] = d; calc_display[1] = '\0'; return; }
    if (n >= 12) return;
    calc_display[n] = d; calc_display[n + 1] = '\0';
}

static int32_t calc_value(void) {
    int32_t v = 0; int i = 0, neg = 0;
    if (calc_display[0] == '-') { neg = 1; i = 1; }
    while (calc_display[i] >= '0' && calc_display[i] <= '9') { v = v * 10 + (calc_display[i] - '0'); i++; }
    return neg ? -v : v;
}

static void calc_apply(int next_op) {
    int32_t v, r;
    if (calc_error) return;                     /* 'C' or a digit clears it */
    v = calc_value();
    if (calc_op == 0 || calc_fresh) {
        calc_acc = v;
        calc_op = next_op;
        calc_fresh = 1;
        calc_expr_pending(calc_acc, next_op);   /* e.g. "12 + " */
        return;
    }
    r = calc_acc;
    if (calc_op == '+') r = calc_acc + v;
    else if (calc_op == '-') r = calc_acc - v;
    else if (calc_op == '*') r = calc_acc * v;
    else if (calc_op == '/') {
        if (v == 0) {
            calc_error = 1;
            strcpy_c(calc_display, "Error");
            strcpy_c(calc_expr, "divide by zero");
            calc_acc = 0;
            calc_op = 0;
            calc_fresh = 1;
            return;
        }
        r = calc_acc / v;
    }
    /* Build the label from the *old* accumulator and operator. */
    if (next_op == 0) calc_expr_done(calc_acc, calc_op, v);
    else              calc_expr_pending(r, next_op);
    calc_acc = r;
    itoa(r, calc_display, 10);
    calc_op = next_op;
    calc_fresh = 1;
}

static void term_clear(void) {
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_LINE; c++)
            term_lines[r][c] = '\0';
    term_row = 0; term_col = 0; term_in_len = 0; term_input[0] = '\0';
    term_prompt_live = 0;
}

static void term_putc(char ch) {
    if (ch == '\n') {
        term_row++;
        term_col = 0;
        if (term_row >= TERM_ROWS) {
            for (int r = 0; r < TERM_ROWS - 1; r++) strcpy_c(term_lines[r], term_lines[r + 1]);
            for (int c = 0; c < TERM_LINE; c++) term_lines[TERM_ROWS - 1][c] = '\0';
            term_row = TERM_ROWS - 1;
        }
        return;
    }
    if (term_col >= TERM_COLS) term_putc('\n');
    term_lines[term_row][term_col] = ch;
    term_col++;
    term_lines[term_row][term_col] = '\0';
}

static void term_puts(const char *s) {
    for (uint32_t i = 0; s[i] != '\0'; i++) term_putc(s[i]);
}

static void term_prompt(void) {
    if (term_prompt_held) { term_prompt_live = 0; return; }  /* reply draws it */
    if (term_prompt_live) return;                            /* already up     */
    term_puts(">");
    term_in_len = 0;
    term_input[0] = '\0';
    term_prompt_live = 1;
}

/* Status output from the async COM1 bridge: never append to the prompt line. */
static void term_puts_async(const char *s) {
    if (term_prompt_live) term_putc('\n');
    term_prompt_live = 0;
    term_puts(s);
    need_redraw = 1;
}

/* Called once an async exchange finishes (reply or timeout). */
static void term_async_done(void) {
    term_prompt_held = 0;
    term_prompt();
}

static void system_shutdown(void) {
    outw(0x604, 0x2000); io_wait();
    outw(0xB004, 0x2000); io_wait();
    ps2_wait_write(); outb(KBD_COMMAND, 0xFE);
    while (1) hlt();
}

static void system_reboot(void) {
    ps2_wait_write(); outb(KBD_COMMAND, 0xFE);
    while (1) hlt();
}


/* ===== Shkodya EXE loader =====
 *
 * A .exe is a 16-byte shk_exe_header_t followed by a flat image. The file is
 * read straight into EXEC_BASE and the loader jumps to entry_offset, so guest
 * code runs at the address it was linked for (guest64.ld uses the same base).
 *
 * The guest draws through putpixel()/draw_string(), which are redirected to
 * exec_overlay for the duration of the call. The compositor blits that layer
 * last, so guest output survives the desktop repaint underneath it.
 *
 * The guest runs in ring 0 on the kernel stack with no memory protection: this
 * is an extension mechanism, not a sandbox. A bad .exe can take the machine down.
 */

#define EXEC_BASE       0x1000000u    /* 16 MiB: kernel .bss ends at ~8.8 MiB */
#define EXEC_MAX_BYTES  (1024u * 1024u)
#define EXEC_BSS_GUARD  65536u        /* zeroed span after the image */
#define EXEC_OVERLAY_W  1024
#define EXEC_OVERLAY_H  768

static uint32_t exec_overlay[EXEC_OVERLAY_W * EXEC_OVERLAY_H];
static int      exec_overlay_active = 0;
static int      exec_runs = 0;

/* Guest-facing API. Every entry point is bounded so a guest cannot scribble
   outside the overlay. */
static void api_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= EXEC_OVERLAY_W || y >= EXEC_OVERLAY_H) return;
    exec_overlay[y * EXEC_OVERLAY_W + x] = color;
}

static void api_draw_string(int x, int y, const char *text, uint32_t color) {
    if (!text) return;
    /* draw_string() follows draw_target, so point it at the overlay. */
    uint32_t *saved = draw_target;
    draw_target = exec_overlay;
    draw_string(x, y, text, color);
    draw_target = saved;
}

static int api_vfs_read(const char *name, uint8_t *dst, uint32_t capacity) {
    if (!name || !dst || capacity == 0) return SHK_EINVAL;
    return fs_read_file(name, dst, capacity);
}

static void api_sound_beep(uint32_t freq_hz, uint32_t ticks) {
    sound_beep(freq_hz, ticks);
}

static shk_api_t exec_api = {
    SHK_API_VERSION,
    (uint32_t)sizeof(shk_api_t),
    EXEC_OVERLAY_W,
    EXEC_OVERLAY_H,
    api_putpixel,
    api_draw_string,
    api_vfs_read,
    api_sound_beep
};

/* Blitted last by the compositor, so guest output sits above the desktop and
   above any window. Transparent pixels are left alone. */
static void draw_exec_overlay(void) {
    int n;
    if (!exec_overlay_active) return;
    n = EXEC_OVERLAY_W * EXEC_OVERLAY_H;
    for (int i = 0; i < n; i++) {
        uint32_t c = exec_overlay[i];
        if (c & 0xFF000000u) backbuffer[i] = c;
    }
}

static int exec_magic_ok(const shk_exe_header_t *h) {
    if (h->magic[0] == 'S' && h->magic[1] == 'H' && h->magic[2] == 'K' && h->magic[3] == 'E')
        return 1;
    if (h->magic[0] == 'M' && h->magic[1] == 'Z' && h->magic[2] == 0 && h->magic[3] == 0)
        return 1;
    return 0;
}

/* Validates the header, then runs the payload. Returns a SHK_* status. */
static int exec_run(const char *name) {
    uint8_t *base = (uint8_t *)EXEC_BASE;
    shk_exe_header_t hdr;
    shk_entry_t entry;
    uint32_t total;
    int n;

    if (!name || name[0] == '\0') return SHK_EINVAL;
    if (vfs_find(name) < 0) return vfs_last_error;

    /* Straight into the execution buffer: no intermediate copy. */
    n = fs_read_file(name, base, EXEC_MAX_BYTES);
    if (n < 0) return n;
    if ((uint32_t)n < SHK_EXE_HEADER_SIZE) return SHK_ERANGE;

    memcpy(&hdr, base, SHK_EXE_HEADER_SIZE);
    if (!exec_magic_ok(&hdr)) return SHK_EINVAL;

    /* Section sizes must fit inside the bytes we actually read. */
    total = SHK_EXE_HEADER_SIZE + hdr.text_size + hdr.data_size;
    if (hdr.text_size == 0) return SHK_EINVAL;
    if (total > (uint32_t)n) return SHK_ERANGE;
    if (total + EXEC_BSS_GUARD > EXEC_MAX_BYTES) return SHK_ERANGE;

    /* The entry must land inside the code section, not out in the data that
     * follows it. entry_offset is relative to the *image*, which starts after
     * the header, so it is compared against text_size alone. Requiring
     * entry_offset >= SHK_EXE_HEADER_SIZE here (as this used to) rejects any
     * guest whose entry point is the first function in .text, which is what
     * happens to shk_main as soon as the guest is built with -O2. */
    if (hdr.text_size == 0) return SHK_EINVAL;
    if (hdr.entry_offset >= hdr.text_size) return SHK_EINVAL;

    /* The header is in front of the image, so the image currently sits 16
       bytes too high: slide it down to EXEC_BASE, which is exactly the address
       the guest was linked for (guest64.ld). Without this the entry point
       resolves 16 bytes early and lands in the middle of the previous
       function. Byte-wise forward copy: the destination trails the source, so
       no byte is read after it has been overwritten. */
    {
        uint32_t image_bytes = (uint32_t)n - SHK_EXE_HEADER_SIZE;
        if (hdr.text_size + hdr.data_size > image_bytes) return SHK_ERANGE;
        shk_copy(base, base + SHK_EXE_HEADER_SIZE, image_bytes);
        /* A flat image cannot carry .bss, so clear the span right after it. */
        memset(base + image_bytes, 0, EXEC_BSS_GUARD);
    }

    exec_api.screen_width = EXEC_OVERLAY_W;
    exec_api.screen_height = EXEC_OVERLAY_H;

    entry = (shk_entry_t)(uintptr_t)(base + hdr.entry_offset);
    {
        char msg[80];
        strcpy_c(msg, "exec: ");
        strcat_c(msg, name);
        strcat_c(msg, " text=");
        append_int(msg, (int32_t)hdr.text_size);
        strcat_c(msg, " data=");
        append_int(msg, (int32_t)hdr.data_size);
        strcat_c(msg, " entry=0x");
        {
            /* small hex formatter: offsets are tiny */
            char hex[9];
            int v = (int)hdr.entry_offset, k = 0;
            if (v == 0) hex[k++] = '0';
            while (v > 0 && k < 8) { int d = v & 0xF; hex[k++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v >>= 4; }
            while (k > 0) { char c = hex[--k]; char one[2]; one[0] = c; one[1] = '\0'; strcat_c(msg, one); }
        }
        strcat_c(msg, " - ring 0, no isolation\n");
        term_puts(msg);
    }

    exec_overlay_active = 1;
    exec_runs++;
    draw_target = exec_overlay;             /* putpixel/draw_char follow this */
    entry(&exec_api);
    draw_target = (uint32_t *)0;
    term_puts("exec: guest returned\n");

    need_redraw = 1;
    return SHK_OK;
}

/* ===== Terminal command history ===== */

/* Entry `back` steps behind the newest (0 = most recent). */
static const char *term_history_at(int back) {
    if (back < 0 || back >= term_hist_count) return "";
    int slot = (term_hist_next - 1 - back + TERM_HISTORY_MAX * 2) % TERM_HISTORY_MAX;
    return term_history[slot];
}

static void term_history_add(const char *cmd) {
    term_hist_view = -1;                       /* next Enter starts a fresh line */
    if (cmd[0] == '\0') return;                /* empty lines are never stored   */
    strcpy_c(term_history[term_hist_next], cmd);
    term_hist_next = (term_hist_next + 1) % TERM_HISTORY_MAX;
    if (term_hist_count < TERM_HISTORY_MAX) term_hist_count++;
}

/* Replace the line being edited. The prompt is a single '>' glyph, so the typed
   text always occupies columns 1..term_col-1 of the current row -- that is what
   gets wiped, in both term_lines and term_input. */
static void term_line_set(const char *text) {
    while (term_col > 1) {
        term_col--;
        term_lines[term_row][term_col] = '\0';
    }
    term_in_len = 0;
    term_input[0] = '\0';
    if (text) {
        for (uint32_t i = 0; text[i] != '\0' && term_in_len < TERM_COLS - 4; i++)
            term_input[term_in_len++] = text[i];
    }
    term_input[term_in_len] = '\0';
    for (int i = 0; i < term_in_len; i++) {
        if (term_col >= TERM_COLS - 1) break;
        term_lines[term_row][term_col++] = term_input[i];
        term_lines[term_row][term_col] = '\0';
    }
    need_redraw = 1;
}

/* Up walks back through the ring, Down walks forward and past the newest
   entry restores an empty line. */
static void term_history_navigate(int up) {
    if (term_hist_count == 0) return;
    if (up) {
        if (term_hist_view < term_hist_count - 1) term_hist_view++;
    } else {
        if (term_hist_view < 0) return;        /* already on the fresh line */
        term_hist_view--;
    }
    term_line_set(term_hist_view < 0 ? "" : term_history_at(term_hist_view));
}

static void term_exec(void) {
    term_putc('\n');
    term_prompt_live = 0;              /* the newline retired the prompt line */
    if (term_in_len == 0) { term_prompt(); return; }
    term_history_add(term_input);
    if (strcmp_c(term_input, "help") == 0) {
        term_puts("Commands:\n");
        term_puts(" help uname whoami date ver uptime\n");
        term_puts(" clear ls cat touch echo mem ps\n");
        term_puts(" neofetch fortune cowsay banner\n");
        term_puts(" games play paint calc files tasks\n");
        term_puts(" rikki store media settings ai\n");
        term_puts(" sheet word  theme\n");
        term_puts(" shkodya history\n");
        term_puts(" reboot shutdown\n");
        term_puts(" Up/Down arrows browse command history\n");
    } else if (strcmp_c(term_input, "uname") == 0) {
        term_puts("Shkodya OS Core v1.9 i686 (Freestanding)\n");
    } else if (strcmp_c(term_input, "whoami") == 0) {
        term_puts("stepanchik\n");
    } else if (strcmp_c(term_input, "date") == 0) {
        char buf[16];
        uint32_t sec = system_ticks / PIT_FREQ;
        uint32_t mm = (sec / 60) % 60;
        uint32_t hh = (12 + (sec / 3600)) % 24;
        itoa((int32_t)hh, buf, 10); term_puts(buf);
        term_puts(":");
        if (mm < 10) term_puts("0");
        itoa((int32_t)mm, buf, 10); term_puts(buf);
        term_puts("\n");
    } else if (strcmp_c(term_input, "clear") == 0) {
        term_clear();
        term_puts("Shkodya OS Core v1.9\n");
        term_prompt();
        return;
    } else if (strcmp_c(term_input, "ls") == 0) {
        int count = 0;
        for (int i = 0; i < VFS_MAX_FILES; i++) {
            if (vfs_files[i].used) {
                term_puts(vfs_files[i].name);
                term_puts(" ");
                char sz[12];
                itoa((int32_t)vfs_files[i].size, sz, 10);
                term_puts(sz);
                term_puts(" bytes\n");
                count++;
            }
        }
        if (count == 0) term_puts("(empty)\n");
    } else if (strncmp_c(term_input, "cat ", 4) == 0) {
        const char *fname = term_input + 4;
        int idx = vfs_find(fname);
        if (idx < 0) {
            term_puts("File not found\n");
        } else if (vfs_read_text(fname, sizeof(vfs_text_buffer)) < 0) {
            term_puts(vfs_error_text(vfs_last_error));
            term_putc('\n');
        } else {
            term_puts((const char *)vfs_text_buffer);
            term_putc('\n');
        }
    } else if (strncmp_c(term_input, "touch ", 6) == 0) {
        const char *fname = term_input + 6;
        if (vfs_find(fname) >= 0) {
            term_puts("File already exists\n");
        } else {
            int idx = vfs_create(fname);
            if (idx >= 0) {
                term_puts("Created ");
                term_puts(fname);
                term_putc('\n');
            } else {
                term_puts("VFS full\n");
            }
        }
    } else if (strcmp_c(term_input, "about") == 0) {
        term_puts("Shkodya OS Core v1.9\n");
        term_puts("RamFS, Fluent UI, Store, Media Player, Compositor\n");
        term_puts("Author: Stepa\n");
    } else if (strcmp_c(term_input, "rikki") == 0) {
        open_window(APP_RIKKI, "Rikki Assistant", 220, 80, 420, 420);
        term_puts("Assistant launched. Click the cat to purr!\n");
    } else if (strcmp_c(term_input, "store") == 0) {
        open_window(APP_STORE, "Shkodya Store", 100, 48, 820, 580);
        term_puts("Store launched\n");
    } else if (strcmp_c(term_input, "ai") == 0) {
        open_window(APP_AICHAT, "Ai Chat", 200, 70, 560, 440);
    } else if (strcmp_c(term_input, "media") == 0) {
        open_window(APP_MEDIA, "Media Player", 180, 70, 620, 420);
        term_puts("Media player launched\n");
    } else if (strcmp_c(term_input, "settings") == 0) {
        open_window(APP_SETTINGS, "Settings", 200, 80, 560, 420);
        term_puts("Settings launched\n");
    } else if (strcmp_c(term_input, "sheet") == 0) {
        open_window(APP_SHEET, "ShkSheet", 160, 60, 640, 460);
        term_puts("ShkSheet launched\n");
    } else if (strcmp_c(term_input, "word") == 0) {
        open_window(APP_WORD, "ShkWord", 180, 70, 560, 420);
        term_puts("ShkWord launched\n");
    } else if (strcmp_c(term_input, "theme") == 0) {
        term_puts("Themes: midnight sunset forest ocean rose\n");
        term_puts("Usage: theme <name>\n");
    } else if (strncmp_c(term_input, "theme ", 6) == 0) {
        const char *name = term_input + 6;
        int tid = -1;
        for (int i = 0; i < THEME_COUNT; i++) {
            if (strcmp_c(name, theme_names[i]) == 0) { tid = i; break; }
            /* case-insensitive first char */
            if (name[0] >= 'A' && name[0] <= 'Z') {
                if (name[0] + 32 == theme_names[i][0] && strlen(name) == strlen(theme_names[i])) {
                    int match = 1;
                    for (uint32_t j = 1; j < strlen(theme_names[i]); j++) {
                        char a = name[j] >= 'A' && name[j] <= 'Z' ? name[j] + 32 : name[j];
                        if (a != theme_names[i][j]) { match = 0; break; }
                    }
                    if (match) { tid = i; break; }
                }
            }
        }
        if (tid >= 0) { theme_apply(tid); term_puts("Theme applied\n"); }
        else term_puts("Unknown theme\n");
    } else if (strcmp_c(term_input, "reboot") == 0) {
        term_puts("KBD reset...\n");
        blit_backbuffer();
        system_reboot();
    } else if (strcmp_c(term_input, "shutdown") == 0) {
        term_puts("ACPI poweroff...\n");
        blit_backbuffer();
        system_shutdown();
    } else if (strncmp_c(term_input, "echo ", 5) == 0) {
        term_puts(term_input + 5);
        term_putc('\n');
    } else if (strcmp_c(term_input, "echo") == 0) {
        term_putc('\n');
    } else if (strcmp_c(term_input, "neofetch") == 0) {
        term_puts("      _____         stepanchik@shkodya\n");
        term_puts("     /     \\        -------------------\n");
        term_puts("    | SHKODY|       OS: Shkodya OS v1.9\n");
        term_puts("    |  OS   |       Kernel: i686 Free\n");
        term_puts("     \\_____/        Compositor: Fluent UI\n");
        char rbuf[64];
        char num[12];
        strcpy_c(rbuf, "                    Resolution: ");
        itoa((int32_t)fb_width, num, 10); strcat_c(rbuf, num);
        strcat_c(rbuf, "x");
        itoa((int32_t)fb_height, num, 10); strcat_c(rbuf, num);
        strcat_c(rbuf, "\n");
        term_puts(rbuf);
        term_puts("                    Shell: ShkodyaTerm\n");
        term_puts("                    Games: 6 loaded\n");
        term_puts("                    Cat: Rikki (ginger)\n");
    } else if (strcmp_c(term_input, "fortune") == 0) {
        static const char *fortunes[8] = {
            "Rikki says: pat me more!\n",
            "A bug is just a feature in disguise.\n",
            "The best kernel is a well-fed cat.\n",
            "Code today, debug tomorrow.\n",
            "Why did the CPU cross the road? Cache.\n",
            "In ROM we trust.\n",
            "Shkodya purrs at 100Hz.\n",
            "There are 10 types of people.\n"
        };
        int pick = (int)(system_ticks % 8);
        term_puts(fortunes[pick]);
    } else if (strcmp_c(term_input, "uptime") == 0) {
        char buf[32];
        uint32_t sec = system_ticks / PIT_FREQ;
        uint32_t hh = sec / 3600;
        uint32_t mm = (sec / 60) % 60;
        uint32_t ss = sec % 60;
        itoa((int32_t)hh, buf, 10); term_puts(buf); term_puts("h ");
        itoa((int32_t)mm, buf, 10); term_puts(buf); term_puts("m ");
        itoa((int32_t)ss, buf, 10); term_puts(buf); term_puts("s\n");
    } else if (strcmp_c(term_input, "ver") == 0) {
        term_puts("Shkodya OS Core v1.9 [Fluent Pro]\n");
        term_puts("Build: i686 Freestanding Kernel\n");
        term_puts("Author: Stepa\n");
    } else if (strcmp_c(term_input, "mem") == 0) {
        if (g_mbi) {
            char buf[48];
            char num[12];
            strcpy_c(buf, "Lower: ");
            itoa((int32_t)g_mbi->mem_lower, num, 10);
            strcat_c(buf, num); strcat_c(buf, " KB\n");
            term_puts(buf);
            strcpy_c(buf, "Upper: ");
            itoa((int32_t)g_mbi->mem_upper, num, 10);
            strcat_c(buf, num); strcat_c(buf, " KB\n");
            term_puts(buf);
        } else {
            term_puts("Memory info unavailable\n");
        }
    } else if (strcmp_c(term_input, "ps") == 0) {
        int count = 0;
        int i;
        for (i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].used) {
                term_puts(windows[i].title);
                term_puts(" [PID ");
                char nb[12];
                itoa(i, nb, 10); term_puts(nb);
                term_puts("]\n");
                count++;
            }
        }
        if (count == 0) term_puts("No windows open\n");
    } else if (strcmp_c(term_input, "games") == 0) {
        term_puts("Available games:\n");
        term_puts("  ttt     - Tic-Tac-Toe\n");
        term_puts("  snake   - Retro Snake\n");
        term_puts("  clicker - Cat Clicker\n");
        term_puts("  2048    - Puzzle 2048\n");
        term_puts("  pong    - Pong vs AI\n");
        term_puts("  memory  - Memory Match\n");
    } else if (strncmp_c(term_input, "play ", 5) == 0) {
        const char *game = term_input + 5;
        open_window(APP_STORE, "Shkodya Store", 100, 48, 820, 580);
        if (strcmp_c(game, "ttt") == 0) { ttt_init(); store_view = STORE_VIEW_TTT; }
        else if (strcmp_c(game, "snake") == 0) { snake_init(); store_view = STORE_VIEW_SNAKE; }
        else if (strcmp_c(game, "clicker") == 0) { store_view = STORE_VIEW_CLICKER; }
        else if (strcmp_c(game, "2048") == 0) { g2048_init(); store_view = STORE_VIEW_2048; }
        else if (strcmp_c(game, "pong") == 0) { pong_init(); store_view = STORE_VIEW_PONG; }
        else if (strcmp_c(game, "memory") == 0) { mem_init(); store_view = STORE_VIEW_MEMORY; }
        else { term_puts("Unknown game. Try: games\n"); }
        term_puts("Game launched in Store\n");
    } else if (strncmp_c(term_input, "cowsay ", 7) == 0) {
        const char *msg = term_input + 7;
        uint32_t mlen = strlen(msg);
        int i;
        if (mlen > 50) mlen = 50;
        term_puts(" ");
        for (i = 0; i < (int)mlen + 2; i++) term_putc('_');
        term_putc('\n');
        term_puts("< "); term_puts(msg); term_puts(" >\n");
        term_puts(" ");
        for (i = 0; i < (int)mlen + 2; i++) term_putc('-');
        term_putc('\n');
        term_puts("        \\   ^__^\n");
        term_puts("         \\  (oo)\\_______\n");
        term_puts("            (__)\\       )\\/\\\n");
        term_puts("                ||----w |\n");
        term_puts("                ||     ||\n");
    } else if (strcmp_c(term_input, "banner") == 0) {
        term_puts(" ___ _ _ ___ _  _   ___ ___ ___ \n");
        term_puts("| __| | | _ \\ || | | _ \\_ _/ __|\n");
        term_puts("| _||_| |  _/ __| |  _/| | (__ \n");
        term_puts("|___|_|_|_| |_||_| |_| |___\\___|\n");
    } else if (strcmp_c(term_input, "paint") == 0) {
        open_window(APP_PAINT, "Paint", 120, 40, 640, 540);
        term_puts("Paint launched\n");
    } else if (strcmp_c(term_input, "calc") == 0) {
        open_window(APP_CALC, "Calculator", 120, 40, 280, 420);
        term_puts("Calculator launched\n");
    } else if (strcmp_c(term_input, "files") == 0) {
        open_window(APP_EXPLORER, "Explorer", 120, 40, 680, 420);
        term_puts("Explorer launched\n");
    } else if (strcmp_c(term_input, "tasks") == 0) {
        open_window(APP_TASKMGR, "Task Manager", 180, 60, 560, 420);
        term_puts("Task Manager launched\n");
    } else if (strncmp_c(term_input, "exec ", 5) == 0) {
        int r = exec_run(term_input + 5);
        if (r == SHK_OK) {
            term_puts("exec: done\n");
        } else {
            term_puts("exec: failed - ");
            term_puts(vfs_error_text(r));
            term_puts("\n");
        }
    } else if (strcmp_c(term_input, "exec") == 0) {
        term_puts("usage: exec <file.exe>\n");
    } else if (strcmp_c(term_input, "shkodya") == 0) {
        term_puts("Shkodya Package Manager\n");
        term_puts("  shkodya search <query>   search the remote index\n");
        term_puts("  shkodya install <pkg>    fetch a package into ShkodyaFS\n");
        term_puts("  shkodya list             list installed .app files\n");
    } else if (strncmp_c(term_input, "shkodya ", 8) == 0) {
        const char *args = term_input + 8;
        if (strncmp_c(args, "search ", 7) == 0) {
            pkg_cmd_search(args + 7);
        } else if (strcmp_c(args, "search") == 0) {
            term_puts("usage: shkodya search <query>\n");
        } else if (strncmp_c(args, "install ", 8) == 0) {
            pkg_cmd_install(args + 8);
        } else if (strcmp_c(args, "install") == 0) {
            term_puts("usage: shkodya install <package>\n");
        } else if (strcmp_c(args, "list") == 0) {
            pkg_list_installed();
        } else {
            term_puts("unknown shkodya subcommand (try: shkodya)\n");
        }
    } else if (strcmp_c(term_input, "history") == 0) {
        /* Oldest-first dump of the ring, so the buffer is inspectable without
           having to arrow through it. */
        if (term_hist_count == 0) {
            term_puts("history is empty\n");
        } else {
            for (int i = term_hist_count - 1; i >= 0; i--) {
                char line[TERM_LINE];
                strcpy_c(line, "  ");
                append_int(line, term_hist_count - i);
                strcat_c(line, "  ");
                strcat_c(line, term_history_at(i));
                strcat_c(line, "\n");
                term_puts(line);
            }
        }
    } else {
        term_puts("command not found\n");
    }
    term_prompt();
}

static void term_key(char ch) {
    unsigned char uch = (unsigned char)ch;
    if (ch == TERM_KEY_UP)   { term_history_navigate(1); return; }
    if (ch == TERM_KEY_DOWN) { term_history_navigate(0); return; }
    if (ch == '\n') { term_exec(); return; }
    if (ch == '\b') {
        if (term_in_len > 0) {
            term_in_len--;
            term_input[term_in_len] = '\0';
            /* The prompt is a single '>' glyph, so typed input starts at
               column 1; the guard is "> 1" (not "> 2") so that the last
               remaining character is erased on screen as well. */
            if (term_col > 1) {
                term_col--;
                term_lines[term_row][term_col] = ' ';
                term_lines[term_row][term_col + 1] = '\0';
            }
        }
        return;
    }
    if (uch < 32 || uch == 127) return;
    if (term_in_len >= TERM_COLS - 4) return;
    term_input[term_in_len] = ch;
    term_in_len++;
    term_input[term_in_len] = '\0';
    if (term_col < TERM_COLS - 1) {
        term_lines[term_row][term_col] = ch;
        term_col++;
        term_lines[term_row][term_col] = '\0';
    }
}

/* APP_STORE (Mini-Games & Account System) */
static void ttt_init(void) {
    for (int i = 0; i < 9; i++) ttt_board[i] = ' ';
    ttt_turn = 'X';
    ttt_winner = 0;
}

static void ttt_check_winner(void) {
    const int wins[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},
        {0,3,6},{1,4,7},{2,5,8},
        {0,4,8},{2,4,6}
    };
    for (int i = 0; i < 8; i++) {
        char a = ttt_board[wins[i][0]];
        char b = ttt_board[wins[i][1]];
        char c = ttt_board[wins[i][2]];
        if (a != ' ' && a == b && b == c) {
            ttt_winner = (a == 'X') ? 1 : 2;
            if (ttt_winner == 1) ttt_score_x++; else ttt_score_o++;
            sound_beep(ttt_winner == 1 ? 900 : 350, 2);
            return;
        }
    }
    int empty = 0;
    for (int i = 0; i < 9; i++) if (ttt_board[i] == ' ') empty++;
    if (empty == 0) ttt_winner = 3;
}

static void ttt_ai_move(void) {
    if (ttt_winner != 0) return;
    if (ttt_board[4] == ' ') { ttt_board[4] = 'O'; ttt_turn = 'X'; ttt_check_winner(); return; }
    for (int i = 0; i < 9; i++) {
        if (ttt_board[i] == ' ') {
            ttt_board[i] = 'O';
            ttt_turn = 'X';
            ttt_check_winner();
            return;
        }
    }
}

static void snake_init(void) {
    snake_len = 4;
    snake_dir = 0;
    snake_x[0] = 8; snake_y[0] = 6;
    snake_x[1] = 7; snake_y[1] = 6;
    snake_x[2] = 6; snake_y[2] = 6;
    snake_x[3] = 5; snake_y[3] = 6;
    snake_food_x = 14; snake_food_y = 6;
    snake_score = 0;
    snake_alive = 1;
    snake_last_tick = system_ticks;
}

static void snake_update(void) {
    if (!snake_alive) return;
    if (system_ticks - snake_last_tick < 15) return;
    snake_last_tick = system_ticks;
    int next_x = snake_x[0];
    int next_y = snake_y[0];
    if (snake_dir == 0) next_x++;
    else if (snake_dir == 1) next_y++;
    else if (snake_dir == 2) next_x--;
    else if (snake_dir == 3) next_y--;

    if (next_x < 0 || next_x >= 24 || next_y < 0 || next_y >= 14) {
        snake_alive = 0;
        sound_beep(220, 3);
        need_redraw = 1;
        return;
    }
    for (int i = 1; i < snake_len; i++) {
        if (snake_x[i] == next_x && snake_y[i] == next_y) {
            snake_alive = 0;
            sound_beep(220, 3);
            need_redraw = 1;
            return;
        }
    }
    for (int i = snake_len; i > 0; i--) {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }
    snake_x[0] = next_x;
    snake_y[0] = next_y;
    if (next_x == snake_food_x && next_y == snake_food_y) {
        if (snake_len < SNAKE_MAX_LEN - 1) snake_len++;
        snake_score += 10;
        sound_beep(880, 1);
        snake_food_x = (snake_food_x * 7 + 5) % 24;
        snake_food_y = (snake_food_y * 11 + 3) % 14;
    }
    need_redraw = 1;
}

static void clicker_update(void) {
    if (system_ticks - clicker_last_tick >= PIT_FREQ) {
        clicker_last_tick = system_ticks;
        if (clicker_cps > 0) {
            clicker_coins += clicker_cps;
            need_redraw = 1;
        }
    }
}

/* ===== Mini-game 4: 2048 ===== */
static void g2048_add_random(void) {
    int empty[16];
    int count = 0;
    int i;
    for (i = 0; i < G2048_N * G2048_N; i++)
        if (g2048_grid[i] == 0) empty[count++] = i;
    if (count == 0) return;
    int pick = (int)((system_ticks * 7 + 13) % (uint32_t)count);
    g2048_grid[empty[pick]] = (((system_ticks + pick) % 10) < 9) ? 2 : 4;
}

static void g2048_init(void) {
    int i;
    for (i = 0; i < G2048_N * G2048_N; i++) g2048_grid[i] = 0;
    g2048_score = 0;
    g2048_won = 0;
    g2048_add_random();
    g2048_add_random();
}

static int g2048_slide(int *row) {
    int orig[G2048_N];
    int tmp[G2048_N], res[G2048_N];
    int n = 0, m = 0, i = 0, changed = 0;
    for (i = 0; i < G2048_N; i++) orig[i] = row[i];
    for (i = 0; i < G2048_N; i++)
        if (row[i] != 0) tmp[n++] = row[i];
    i = 0;
    while (i < n) {
        if (i + 1 < n && tmp[i] == tmp[i + 1]) {
            res[m] = tmp[i] * 2;
            g2048_score += res[m];
            if (res[m] >= 2048) g2048_won = 1;
            i += 2;
        } else {
            res[m] = tmp[i];
            i++;
        }
        m++;
    }
    for (; m < G2048_N; m++) res[m] = 0;
    for (i = 0; i < G2048_N; i++) {
        if (orig[i] != res[i]) changed = 1;
        row[i] = res[i];
    }
    return changed;
}

static void g2048_move(int dir) {
    int changed = 0;
    int line[G2048_N];
    int i, j;
    for (i = 0; i < G2048_N; i++) {
        for (j = 0; j < G2048_N; j++) {
            int idx;
            if (dir == 0)      idx = i * G2048_N + j;
            else if (dir == 1) idx = i * G2048_N + (G2048_N - 1 - j);
            else if (dir == 2) idx = j * G2048_N + i;
            else               idx = (G2048_N - 1 - j) * G2048_N + i;
            line[j] = g2048_grid[idx];
        }
        if (g2048_slide(line)) changed = 1;
        for (j = 0; j < G2048_N; j++) {
            int idx;
            if (dir == 0)      idx = i * G2048_N + j;
            else if (dir == 1) idx = i * G2048_N + (G2048_N - 1 - j);
            else if (dir == 2) idx = j * G2048_N + i;
            else               idx = (G2048_N - 1 - j) * G2048_N + i;
            g2048_grid[idx] = line[j];
        }
    }
    if (changed) {
        g2048_add_random();
        sound_beep(440 + g2048_score % 200, 1);
    }
    need_redraw = 1;
}

static void g2048_update(void) { (void)0; }

static uint32_t g2048_color(int val) {
    switch (val) {
        case 2:    return 0xFFEEE4DA;
        case 4:    return 0xFFEDE0C8;
        case 8:    return 0xFFF2B179;
        case 16:   return 0xFFF59563;
        case 32:   return 0xFFF67C5F;
        case 64:   return 0xFFF65E3B;
        case 128:  return 0xFFEDCF72;
        case 256:  return 0xFFEDCC61;
        case 512:  return 0xFFEDC850;
        case 1024: return 0xFFEDC53F;
        case 2048: return 0xFFEDC22E;
        default:   return 0xFF3C3A32;
    }
}

static uint32_t g2048_text_color(int val) {
    return (val <= 4) ? 0xFF776E65 : 0xFFFFFFFF;
}

/* ===== Mini-game 5: Pong ===== */
static void pong_init(void) {
    pong_bx = 50; pong_by = 50;
    pong_bdx = 2; pong_bdy = (system_ticks & 1) ? -1 : 1;
    pong_py = 40; pong_ay = 40;
    pong_sp = 0; pong_sa = 0;
    pong_paused = 0;
    pong_tick = system_ticks;
}

static void pong_update(void) {
    if (pong_paused) return;
    if (system_ticks - pong_tick < 3) return;
    pong_tick = system_ticks;
    pong_bx += pong_bdx;
    pong_by += pong_bdy;
    if (pong_by <= 0 || pong_by >= 100) {
        pong_bdy = -pong_bdy;
        sound_beep(300, 1);
    }
    if (pong_bx <= 0) {
        pong_sa++; pong_bx = 50; pong_by = 50;
        pong_bdx = 2; pong_bdy = (system_ticks & 1) ? 1 : -1;
        sound_beep(200, 2);
    }
    if (pong_bx >= 100) {
        pong_sp++; pong_bx = 50; pong_by = 50;
        pong_bdx = -2; pong_bdy = (system_ticks & 1) ? 1 : -1;
        sound_beep(200, 2);
    }
    if (pong_bx <= 6 && pong_by >= pong_py && pong_by < pong_py + 20) {
        if (pong_bdx < 0) {
            pong_bdx = -pong_bdx + 1;
            if (pong_bdx > 4) pong_bdx = 4;
            sound_beep(500, 1);
        }
    }
    if (pong_ay + 10 < pong_by) pong_ay += 2;
    else if (pong_ay + 10 > pong_by) pong_ay -= 2;
    if (pong_ay < 0) pong_ay = 0;
    if (pong_ay > 80) pong_ay = 80;
    if (pong_bx >= 94 && pong_by >= pong_ay && pong_by < pong_ay + 20) {
        if (pong_bdx > 0) {
            pong_bdx = -pong_bdx - 1;
            if (pong_bdx < -4) pong_bdx = -4;
            sound_beep(500, 1);
        }
    }
    need_redraw = 1;
}

/* ===== Mini-game 6: Memory Match ===== */
static void mem_init(void) {
    int i;
    for (i = 0; i < MEM_N; i++) mem_vals[i] = i / 2;
    for (i = MEM_N - 1; i > 0; i--) {
        int j = (int)((system_ticks * 13 + i * 7) % (uint32_t)(i + 1));
        int t = mem_vals[i]; mem_vals[i] = mem_vals[j]; mem_vals[j] = t;
    }
    for (i = 0; i < MEM_N; i++) mem_shown[i] = 0;
    mem_sel1 = -1; mem_sel2 = -1;
    mem_pairs = 0;
    mem_timer = 0;
    mem_lock = 0;
}

static void mem_update(void) {
    if (!mem_lock) return;
    if (system_ticks - mem_timer > 30) {
        if (mem_sel1 >= 0 && mem_sel2 >= 0) {
            if (mem_vals[mem_sel1] != mem_vals[mem_sel2]) {
                mem_shown[mem_sel1] = 0;
                mem_shown[mem_sel2] = 0;
            } else {
                mem_pairs++;
                if (mem_pairs >= 8) {
                    sound_beep(880, 3);
                    toast_show("Memory: All pairs found!");
                }
            }
        }
        mem_sel1 = -1; mem_sel2 = -1;
        mem_lock = 0;
        need_redraw = 1;
    }
}

static void mem_click(int idx) {
    if (mem_lock) return;
    if (idx < 0 || idx >= MEM_N) return;
    if (mem_shown[idx]) return;
    mem_shown[idx] = 1;
    sound_beep(440 + mem_vals[idx] * 50, 1);
    if (mem_sel1 < 0) {
        mem_sel1 = idx;
    } else if (mem_sel2 < 0 && idx != mem_sel1) {
        mem_sel2 = idx;
        mem_lock = 1;
        mem_timer = system_ticks;
    }
    need_redraw = 1;
}

static void store_publish_game_to_vfs(void) {
    if (store_current_user < 0 || !store_users[store_current_user].is_dev) return;
    const char *manifest = "[Shkodya Store App Package]\n"
                           "Title: Mini-Games Pack\n"
                           "Author: stepanchik [DEV]\n"
                           "Items: Tic-Tac-Toe, Snake, Shkodya Clicker\n"
                           "Verified: True\n";
    int r = vfs_write("games.app", (const uint8_t *)manifest, strlen(manifest));
    if (r == SHK_OK) {
        toast_show("Game manifest saved to disk!");
        sound_beep(784, 2);
    } else {
        toast_show(vfs_error_text(r));
    }
}

/* Store-only dark palette: changing the desktop theme does not wash out
   the catalog. All drawing and hit-testing share the same layout below. */
#define STORE_BG       0xFF101116
#define STORE_PANEL    0xFF191B24
#define STORE_RAISED   0xFF222531
#define STORE_LINE     0xFF303444
#define STORE_TEXT     0xFFF2F3FA
#define STORE_MUTED    0xFFA5ABC1
#define STORE_ACCENT   0xFFAC9AFF
#define STORE_INK      0xFF171226
#define STORE_FIELD    0xFF11131B
#define STORE_GREEN    0xFF88DFBA

#define STORE_HIT_LOGIN    1
#define STORE_HIT_REG      2
#define STORE_HIT_LOGOUT   3
#define STORE_HIT_FEATURE  4
#define STORE_HIT_PUBLISH  5
#define STORE_HIT_GAME    10
#define STORE_HIT_USER    20
#define STORE_HIT_PASS    21
#define STORE_HIT_SUBMIT  22
#define STORE_HIT_CANCEL  23

typedef struct { int x, y, w, h; } store_rect_t;
typedef struct {
    store_rect_t login, reg, logout, hero, feature, publish;
    store_rect_t cards[6], play[6], auth, user, pass, submit, cancel;
    int section_y, footer_y;
} store_layout_t;

static int store_hover_window = -1;
static int store_hover_id = 0;
static const char *store_game_names[6] = {
    "Tic-Tac-Toe", "Retro Snake", "Cat Clicker", "2048", "Pong", "Memory Match"
};
static const char *store_game_tags[6] = {
    "STRATEGY", "ARCADE", "IDLE GAME", "PUZZLE", "ARCADE", "PUZZLE"
};
static const char *store_game_desc[6] = {
    "Outsmart the AI.", "Eat, grow, repeat.", "Rikki needs a paw.",
    "One more merge.", "The arcade classic.", "Find every pair."
};
static const uint32_t store_game_colors[6] = {
    0xFF8BAEFF, 0xFF81DEB4, 0xFFF1BD82, 0xFFE2C17C, 0xFF7BD3E7, 0xFFBB9CF5
};

static store_rect_t store_rect(int x, int y, int w, int h) {
    store_rect_t r = { x, y, w, h };
    return r;
}
static int store_contains(store_rect_t r, int x, int y) {
    return point_in_rect(x, y, r.x, r.y, r.w, r.h);
}
static int store_is_dev(void) {
    return store_current_user >= 0 && store_current_user < store_user_count &&
           store_users[store_current_user].is_dev;
}
static int store_hot(window_t *win, int id) {
    return store_hover_window >= 0 && win == &windows[store_hover_window] && store_hover_id == id;
}
static void store_layout(window_t *win, store_layout_t *l) {
    int cx = win->x + 8, cy = win->y + TITLEBAR_H + 4;
    int cw = win->w - 16, vh = win->h - TITLEBAR_H - 60;
    int vy = cy + 48, gap = 12;
    l->reg = store_rect(cx + cw - 100, cy + 5, 88, 32);
    l->login = store_rect(l->reg.x - 80, cy + 5, 72, 32);
    l->logout = store_rect(cx + cw - 92, cy + 5, 80, 32);
    l->hero = store_rect(cx + 12, vy + 8, cw - 24, vh >= 430 ? 84 : 56);
    l->feature = store_rect(l->hero.x + l->hero.w - 132, l->hero.y + (l->hero.h - 32) / 2, 112, 32);
    l->section_y = l->hero.y + l->hero.h + 12;
    l->publish = store_rect(cx + cw - 188, l->section_y - 4, 176, 26);
    l->footer_y = vy + vh - 18;
    int grid_y = l->section_y + 30;
    int card_w = (cw - 24 - 2 * gap) / 3;
    int card_h = (vy + vh - 30 - grid_y - gap) / 2;
    if (card_h > 152) card_h = 152;
    for (int i = 0; i < 6; i++) {
        l->cards[i] = store_rect(cx + 12 + (i % 3) * (card_w + gap),
                                grid_y + (i / 3) * (card_h + gap), card_w, card_h);
        l->play[i] = store_rect(l->cards[i].x + 12, l->cards[i].y + card_h - 38, card_w - 24, 28);
    }
    l->auth = store_rect(cx + (cw - 368) / 2, vy + (vh - 292) / 2, 368, 292);
    l->user = store_rect(l->auth.x + 24, l->auth.y + 88, 320, 34);
    l->pass = store_rect(l->auth.x + 24, l->auth.y + 154, 320, 34);
    l->submit = store_rect(l->auth.x + 24, l->auth.y + 212, 154, 36);
    l->cancel = store_rect(l->auth.x + 190, l->auth.y + 212, 154, 36);
}

static int store_hit(window_t *win, int mx, int my) {
    store_layout_t l;
    store_layout(win, &l);
    if (store_current_user >= 0) {
        if (store_contains(l.logout, mx, my)) return STORE_HIT_LOGOUT;
    } else {
        if (store_contains(l.login, mx, my)) return STORE_HIT_LOGIN;
        if (store_contains(l.reg, mx, my)) return STORE_HIT_REG;
    }
    if (store_view == STORE_VIEW_LOGIN || store_view == STORE_VIEW_REG) {
        if (store_contains(l.user, mx, my)) return STORE_HIT_USER;
        if (store_contains(l.pass, mx, my)) return STORE_HIT_PASS;
        if (store_contains(l.submit, mx, my)) return STORE_HIT_SUBMIT;
        if (store_contains(l.cancel, mx, my)) return STORE_HIT_CANCEL;
    } else if (store_view == STORE_VIEW_CATALOG) {
        if (store_contains(l.feature, mx, my)) return STORE_HIT_FEATURE;
        if (store_is_dev() && store_contains(l.publish, mx, my)) return STORE_HIT_PUBLISH;
        for (int i = 0; i < 6; i++) {
            if (store_contains(l.play[i], mx, my)) return STORE_HIT_GAME + i;
        }
    }
    return 0;
}

/* Repaint only when a control is entered/left, not for every mouse pixel.
   Menus and overlapping windows must never highlight the store behind them. */
static void store_update_hover(void) {
    int idx = -1, hit = 0;
    if (!start_open && !sound_popup_open && !dragging &&
        mouse_y < (int)fb_height - TASKBAR_H) {
        int top = window_at(mouse_x, mouse_y);
        if (top >= 0 && windows[top].app == APP_STORE) {
            hit = store_hit(&windows[top], mouse_x, mouse_y);
            if (hit) idx = top;
        }
    }
    if (idx != store_hover_window || hit != store_hover_id) {
        store_hover_window = idx;
        store_hover_id = hit;
        need_redraw = 1;
    }
}

static void store_panel(store_rect_t r, uint32_t bg, uint32_t border, int radius) {
    if (r.w < 3 || r.h < 3) return;
    fill_rounded_rect_aa(r.x, r.y, r.w, r.h, radius, border);
    fill_rounded_rect_aa(r.x + 1, r.y + 1, r.w - 2, r.h - 2, radius - 1, bg);
}
static void store_text_fit(int x, int y, const char *text, int width, uint32_t color) {
    int count = width / 8, len = (int)strlen(text);
    for (int i = 0; i < count && i < len; i++) {
        char c = (len > count && i >= count - 3) ? '.' : text[i];
        draw_char(x + i * 8, y, c, color);
    }
}
static void store_title(int x, int y, const char *text) {
    /* The same glyph coverage draw_char uses, magnified 2x. This used to sample
     * the 1-bit bitmap and fill 2x2 blocks, which read as coarse squares next to
     * the antialiased body text. No font files, no heap, no floating point. */
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        if (c >= 32 && c < 128) {
            for (int row = 0; row < 16; row++)
                for (int col = 0; col < 8; col++) {
                    int cov = font_aa_ascii[c][row][col];
                    if (cov == 0) continue;
                    for (int dy = 0; dy < 2; dy++)
                        for (int dx = 0; dx < 2; dx++)
                            blend_cov(x + col * 2 + dx, y + row * 2 + dy,
                                      STORE_TEXT, cov);
                }
        }
        x += 16;
    }
}
static void store_button(window_t *win, store_rect_t r, const char *label, int id, int primary) {
    int hot = store_hot(win, id);
    uint32_t bg = primary ? (hot ? 0xFFC3B6FF : STORE_ACCENT) : (hot ? 0xFF363149 : STORE_RAISED);
    store_panel(r, bg, hot ? STORE_ACCENT : (primary ? STORE_ACCENT : STORE_LINE), 8);
    int tx = r.x + (r.w - (int)strlen(label) * 8) / 2;
    draw_string(tx, r.y + (r.h - 16) / 2, label, primary ? STORE_INK : STORE_TEXT);
}
static void store_game_icon(int x, int y, int i) {
    uint32_t accent = store_game_colors[i];
    uint32_t bg = lerp_color(STORE_PANEL, accent, 18, 100);
    store_panel(store_rect(x, y, 40, 40), bg, lerp_color(bg, accent, 25, 100), 10);
    if (i == 0) {
        draw_line(x + 10, y + 11, x + 18, y + 19, accent);
        draw_line(x + 18, y + 11, x + 10, y + 19, accent);
        store_panel(store_rect(x + 22, y + 22, 10, 10), bg, accent, 5);
        fill_rect(x + 20, y + 8, 1, 25, accent);
        fill_rect(x + 8, y + 20, 25, 1, accent);
    } else if (i == 1) {
        fill_rounded_rect_aa(x + 9, y + 10, 21, 7, 3, accent);
        fill_rect(x + 9, y + 13, 7, 13, accent);
        fill_rounded_rect_aa(x + 9, y + 23, 15, 7, 3, accent);
        fill_rect(x + 26, y + 12, 2, 2, STORE_INK);
        fill_rounded_rect_aa(x + 28, y + 26, 5, 5, 2, 0xFFF38FA0);
    } else if (i == 2) {
        draw_rikki(x + 4, y + 4, 2);
    } else if (i == 3) {
        draw_string(x + 4, y + 13, "2048", accent);
    } else if (i == 4) {
        fill_rounded_rect_aa(x + 8, y + 10, 4, 15, 2, accent);
        fill_rounded_rect_aa(x + 28, y + 16, 4, 15, 2, accent);
        fill_rounded_rect_aa(x + 18, y + 14, 5, 5, 2, STORE_TEXT);
        for (int yy = y + 8; yy < y + 34; yy += 6) fill_rect(x + 20, yy, 1, 3, STORE_MUTED);
    } else {
        store_panel(store_rect(x + 8, y + 9, 15, 21), bg, accent, 4);
        store_panel(store_rect(x + 17, y + 13, 15, 21), 0xFF493A68, accent, 4);
        draw_string(x + 21, y + 16, "?", STORE_TEXT);
    }
}

static void draw_store_header(window_t *win, store_layout_t *l) {
    int cx = win->x + 8, cy = win->y + TITLEBAR_H + 4, cw = win->w - 16;
    store_panel(store_rect(cx + 12, cy + 5, 32, 32), 0xFF302747, 0xFF51416E, 9);
    store_panel(store_rect(cx + 24, cy + 11, 9, 10), 0xFF302747, STORE_ACCENT, 4);
    fill_rounded_rect_aa(cx + 20, cy + 17, 17, 14, 3, STORE_ACCENT);
    draw_string(cx + 54, cy + 13, "Shkodya Store", STORE_TEXT);
    fill_rect(cx + 12, cy + 43, cw - 24, 1, STORE_LINE);
    if (store_current_user >= 0) {
        store_button(win, l->logout, "Logout", STORE_HIT_LOGOUT, 0);
        int right = l->logout.x - 12;
        if (store_is_dev()) {
            store_panel(store_rect(right - 44, cy + 9, 44, 24), 0xFF353024, 0xFF574A30, 6);
            draw_string(right - 34, cy + 13, "DEV", 0xFFEAC88C);
            right -= 56;
        }
        int width = (int)strlen(store_users[store_current_user].username) * 8;
        if (width > 144) width = 144;
        store_text_fit(right - width, cy + 13, store_users[store_current_user].username, width, STORE_MUTED);
    } else {
        store_button(win, l->login, "Login", STORE_HIT_LOGIN, 0);
        store_button(win, l->reg, "Register", STORE_HIT_REG, 1);
    }
}

static void draw_store_auth(window_t *win, store_layout_t *l) {
    store_panel(l->auth, STORE_PANEL, STORE_LINE, 14);
    int x = l->auth.x, y = l->auth.y;
    fill_rounded_rect_aa(x + 24, y + 20, 4, 22, 2, STORE_ACCENT);
    draw_string(x + 40, y + 22, store_view == STORE_VIEW_LOGIN ? "Welcome back" : "Create an account", STORE_TEXT);
    draw_string(x + 24, y + 48, "Your corner of Shkodya OS.", STORE_MUTED);
    draw_string(x + 24, y + 68, "Username", STORE_MUTED);
    draw_string(x + 24, y + 134, "Password", STORE_MUTED);
    store_panel(l->user, STORE_FIELD, store_field_focus == 0 ? STORE_ACCENT : STORE_LINE, 8);
    store_panel(l->pass, STORE_FIELD, store_field_focus == 1 ? STORE_ACCENT : STORE_LINE, 8);
    draw_string(l->user.x + 12, l->user.y + 9, store_in_user, STORE_TEXT);
    char stars[USERNAME_LEN];
    uint32_t plen = strlen(store_in_pass);
    if (plen >= sizeof(stars)) plen = sizeof(stars) - 1;
    for (uint32_t i = 0; i < plen; i++) stars[i] = '*';
    stars[plen] = '\0';
    draw_string(l->pass.x + 12, l->pass.y + 9, stars, STORE_TEXT);
    store_rect_t field = store_field_focus == 0 ? l->user : l->pass;
    int len = (int)strlen(store_field_focus == 0 ? store_in_user : stars);
    fill_rect(field.x + 12 + len * 8, field.y + 9, 2, 16, STORE_ACCENT);
    store_button(win, l->submit, store_view == STORE_VIEW_LOGIN ? "Sign In" : "Register", STORE_HIT_SUBMIT, 1);
    store_button(win, l->cancel, "Cancel", STORE_HIT_CANCEL, 0);
    draw_string(x + 24, y + 264, "TAB switches fields", STORE_MUTED);
}

static void draw_store_catalog(window_t *win, store_layout_t *l) {
    store_panel(l->hero, 0xFF242033, 0xFF423651, 12);
    int hx = l->hero.x, hy = l->hero.y;
    if (l->hero.h >= 80) {
        draw_string(hx + 18, hy + 10, "THE SHKODYA COLLECTION", STORE_ACCENT);
        store_title(hx + 16, hy + 28, "Small games. Big fun.");
        draw_string(hx + 18, hy + 62, "Six built-in games. Ready to play.", STORE_MUTED);
    } else {
        draw_string(hx + 18, hy + 10, "Small games. Big fun.", STORE_TEXT);
        draw_string(hx + 18, hy + 30, "Six games. Ready to play.", STORE_MUTED);
    }
    store_button(win, l->feature, "Play Snake", STORE_HIT_FEATURE, 1);
    draw_string(hx, l->section_y, "Explore games", STORE_TEXT);
    if (store_is_dev()) store_button(win, l->publish, "+ Publish to RamFS", STORE_HIT_PUBLISH, 0);
    else draw_string(hx + l->hero.w - 104, l->section_y, "6 apps / FREE", STORE_MUTED);
    for (int i = 0; i < 6; i++) {
        store_rect_t r = l->cards[i];
        int hot = store_hot(win, STORE_HIT_GAME + i);
        store_panel(r, hot ? 0xFF242331 : STORE_PANEL, hot ? STORE_ACCENT : STORE_LINE, 12);
        store_game_icon(r.x + 12, r.y + 12, i);
        store_text_fit(r.x + 64, r.y + 14, store_game_names[i], r.w - 76, STORE_TEXT);
        if (r.h >= 110) draw_string(r.x + 64, r.y + 34, store_game_tags[i], store_game_colors[i]);
        if (r.h >= 132) store_text_fit(r.x + 12, r.y + 70, store_game_desc[i], r.w - 24, STORE_MUTED);
        store_button(win, l->play[i], "Play  >", STORE_HIT_GAME + i, hot);
    }
    fill_rounded_rect_aa(hx, l->footer_y + 5, 5, 5, 2, STORE_GREEN);
    draw_string(hx + 14, l->footer_y, "Built in / No downloads needed", STORE_MUTED);
}

static void draw_store_client(window_t *win) {
    int cx = win->x + 8;
    int cy = win->y + TITLEBAR_H + 4;
    int cw = win->w - 16;
    int ch = win->h - TITLEBAR_H - 12;

    store_layout_t layout;
    store_layout(win, &layout);
    fill_rounded_rect_aa(cx, cy, cw, ch, 8, STORE_BG);
    draw_store_header(win, &layout);

    int view_y = cy + 48;
    int view_h = ch - 48;

    if (store_view == STORE_VIEW_LOGIN || store_view == STORE_VIEW_REG) {
        draw_store_auth(win, &layout);
        return;
    }

    if (store_view == STORE_VIEW_TTT) {
        fill_rounded_rect(cx, view_y, cw, view_h, 6, STORE_BG);
        fill_rounded_rect(cx + 12, view_y + 10, 80, 26, 4, COLOR_SURFACE2);
        draw_string(cx + 20, view_y + 15, "< Back", COLOR_WHITE);
        draw_string(cx + 110, view_y + 15, "Tic-Tac-Toe vs AI (3x3)", COLOR_WHITE);
        char score_str[48] = "X: ";
        char nbuf[12];
        itoa(ttt_score_x, nbuf, 10); strcat_c(score_str, nbuf);
        strcat_c(score_str, " | O: ");
        itoa(ttt_score_o, nbuf, 10); strcat_c(score_str, nbuf);
        draw_string(cx + cw - 150, view_y + 15, score_str, COLOR_ACCENT);

        int gx = cx + (cw - 180) / 2;
        int gy = view_y + 50;
        fill_rounded_rect(gx - 8, gy - 8, 196, 196, 6, COLOR_SURFACE2);
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                int bx = gx + c * 60;
                int by = gy + r * 60;
                fill_rounded_rect(bx, by, 56, 56, 4, COLOR_SURFACE);
                char mark = ttt_board[r * 3 + c];
                if (mark != ' ') {
                    char s[2] = { mark, '\0' };
                    uint32_t col = (mark == 'X') ? COLOR_ACCENT : COLOR_DANGER;
                    draw_string(bx + 24, by + 20, s, col);
                }
            }
        }
        if (ttt_winner == 1) draw_string(gx + 34, gy + 204, "Player X Wins!", COLOR_GOLD);
        else if (ttt_winner == 2) draw_string(gx + 44, gy + 204, "AI (O) Wins!", COLOR_DANGER);
        else if (ttt_winner == 3) draw_string(gx + 64, gy + 204, "Draw!", COLOR_MUTED);
        else draw_string(gx + 44, gy + 204, "Your Turn (X)", COLOR_WHITE);

        fill_rounded_rect(gx + 40, gy + 226, 100, 26, 4, COLOR_ACCENT);
        draw_string(gx + 64, gy + 231, "Restart", COLOR_WHITE);
        return;
    }

    if (store_view == STORE_VIEW_SNAKE) {
        fill_rounded_rect(cx, view_y, cw, view_h, 6, STORE_BG);
        fill_rounded_rect(cx + 12, view_y + 10, 80, 26, 4, COLOR_SURFACE2);
        draw_string(cx + 20, view_y + 15, "< Back", COLOR_WHITE);
        draw_string(cx + 110, view_y + 15, "Snake Game (Controls: WASD / Arrows)", COLOR_WHITE);
        char sc_buf[32] = "Score: ";
        char num[12];
        itoa(snake_score, num, 10); strcat_c(sc_buf, num);
        draw_string(cx + cw - 120, view_y + 15, sc_buf, COLOR_GOLD);

        int bx = cx + (cw - 336) / 2;
        int by = view_y + 46;
        fill_rounded_rect(bx - 4, by - 4, 344, 204, 4, COLOR_SURFACE2);
        fill_rect(bx, by, 336, 196, COLOR_VIEW_BG);

        fill_rounded_rect(bx + snake_food_x * 14 + 1, by + snake_food_y * 14 + 1, 12, 12, 3, COLOR_DANGER);

        for (int i = 0; i < snake_len; i++) {
            uint32_t col = (i == 0) ? 0xFF34D399 : 0xFF10B981;
            fill_rounded_rect(bx + snake_x[i] * 14 + 1, by + snake_y[i] * 14 + 1, 12, 12, 2, col);
        }
        if (!snake_alive) {
            fill_rounded_rect_alpha(bx + 68, by + 70, 200, 56, 6, 0xE01C2233);
            draw_string(bx + 124, by + 80, "GAME OVER", COLOR_DANGER);
            draw_string(bx + 96, by + 102, "Click Restart button", COLOR_WHITE);
        }
        fill_rounded_rect(bx + 118, by + 212, 100, 26, 4, COLOR_ACCENT);
        draw_string(bx + 142, by + 217, "Restart", COLOR_WHITE);
        return;
    }

    if (store_view == STORE_VIEW_CLICKER) {
        fill_rounded_rect(cx, view_y, cw, view_h, 6, STORE_BG);
        fill_rounded_rect(cx + 12, view_y + 10, 80, 26, 4, COLOR_SURFACE2);
        draw_string(cx + 20, view_y + 15, "< Back", COLOR_WHITE);
        draw_string(cx + 110, view_y + 15, "Shkodya Cat Clicker", COLOR_WHITE);

        int card_x = cx + 30;
        int card_y = view_y + 50;
        int card_w = 240, card_h = 240;
        fill_rounded_rect(card_x, card_y, card_w, card_h, 8, COLOR_SURFACE2);
        draw_rikki(card_x + 48, card_y + 24, 9);
        char c_buf[48] = "Coins: ";
        char num[16];
        itoa((int32_t)clicker_coins, num, 10); strcat_c(c_buf, num);
        draw_string(card_x + 50, card_y + 180, c_buf, COLOR_GOLD);
        char cps_buf[32] = "+";
        itoa((int32_t)clicker_cps, num, 10); strcat_c(cps_buf, num);
        strcat_c(cps_buf, " coins/sec");
        draw_string(card_x + 50, card_y + 202, cps_buf, COLOR_MUTED);

        int shop_x = cx + 290;
        int shop_y = view_y + 50;
        int shop_w = cw - 310;
        fill_rounded_rect(shop_x, shop_y, shop_w, 240, 8, COLOR_SURFACE2);
        draw_string(shop_x + 16, shop_y + 14, "Store Upgrades", COLOR_WHITE);

        fill_rounded_rect(shop_x + 14, shop_y + 40, shop_w - 28, 48, 6, COLOR_SURFACE);
        draw_string(shop_x + 24, shop_y + 48, "Auto-Paw (+1/sec)", COLOR_WHITE);
        draw_string(shop_x + 24, shop_y + 66, "Price: 15 coins", COLOR_GOLD);
        fill_rounded_rect(shop_x + shop_w - 90, shop_y + 50, 52, 28, 4, COLOR_ACCENT);
        draw_string(shop_x + shop_w - 80, shop_y + 56, "Buy", COLOR_WHITE);

        fill_rounded_rect(shop_x + 14, shop_y + 100, shop_w - 28, 48, 6, COLOR_SURFACE);
        draw_string(shop_x + 24, shop_y + 108, "Double Click (+2/click)", COLOR_WHITE);
        draw_string(shop_x + 24, shop_y + 126, "Price: 50 coins", COLOR_GOLD);
        fill_rounded_rect(shop_x + shop_w - 90, shop_y + 110, 52, 28, 4, COLOR_ACCENT);
        draw_string(shop_x + shop_w - 80, shop_y + 116, "Buy", COLOR_WHITE);
        return;
    }


    if (store_view == STORE_VIEW_2048) {
        fill_rounded_rect(cx, view_y, cw, view_h, 6, STORE_BG);
        fill_rounded_rect(cx + 12, view_y + 10, 80, 26, 4, COLOR_SURFACE2);
        draw_string(cx + 20, view_y + 15, "< Back", COLOR_WHITE);
        draw_string(cx + 110, view_y + 15, "2048 - Arrows/WASD", COLOR_WHITE);
        char sc[32] = "Score: ";
        char nb[12];
        itoa(g2048_score, nb, 10); strcat_c(sc, nb);
        draw_string(cx + cw - 120, view_y + 15, sc, COLOR_GOLD);

        int bw = 260;
        int bx = cx + (cw - bw) / 2;
        int by = view_y + 50;
        int cell = 56, gap = 6;
        fill_rounded_rect(bx - 8, by - 8, bw + 16, bw + 16, 8, 0xFF3C3A32);
        int r, c;
        for (r = 0; r < G2048_N; r++) {
            for (c = 0; c < G2048_N; c++) {
                int tx = bx + gap + c * (cell + gap);
                int ty = by + gap + r * (cell + gap);
                int val = g2048_grid[r * G2048_N + c];
                uint32_t bg = val ? g2048_color(val) : 0xFF3C3A32;
                fill_rounded_rect(tx, ty, cell, cell, 4, bg);
                if (val > 0) {
                    char num[12];
                    itoa(val, num, 10);
                    int len = strlen(num);
                    int tx_off = (cell - len * 8) / 2;
                    draw_string(tx + tx_off, ty + cell / 2 - 8, num, g2048_text_color(val));
                }
            }
        }
        if (g2048_won) {
            fill_rounded_rect_alpha(bx + 30, by + bw / 2 - 16, bw - 60, 32, 6, 0xE0000000);
            draw_string(bx + bw / 2 - 36, by + bw / 2 - 8, "YOU WIN!", COLOR_GOLD);
        }
        fill_rounded_rect(bx + bw / 2 - 50, by + bw + 16, 100, 26, 4, COLOR_ACCENT);
        draw_string(bx + bw / 2 - 28, by + bw + 21, "Restart", COLOR_WHITE);
        return;
    }

    if (store_view == STORE_VIEW_PONG) {
        fill_rounded_rect(cx, view_y, cw, view_h, 6, STORE_BG);
        fill_rounded_rect(cx + 12, view_y + 10, 80, 26, 4, COLOR_SURFACE2);
        draw_string(cx + 20, view_y + 15, "< Back", COLOR_WHITE);
        draw_string(cx + 110, view_y + 15, "Pong - W/S to move, Space=pause", COLOR_WHITE);
        char sc[32];
        char nb[12];
        strcpy_c(sc, "You: ");
        itoa(pong_sp, nb, 10); strcat_c(sc, nb);
        strcat_c(sc, "  AI: ");
        itoa(pong_sa, nb, 10); strcat_c(sc, nb);
        draw_string(cx + cw - 140, view_y + 15, sc, COLOR_GOLD);

        int fw = 340, fh = 180;
        int fx = cx + (cw - fw) / 2;
        int fy = view_y + 50;
        fill_rounded_rect(fx - 4, fy - 4, fw + 8, fh + 8, 4, COLOR_SURFACE2);
        fill_rect(fx, fy, fw, fh, 0xFF0C101A);

        int ball_px = fx + (pong_bx * fw) / 100;
        int ball_py = fy + (pong_by * fh) / 100;
        fill_rounded_rect(ball_px - 4, ball_py - 4, 8, 8, 2, COLOR_WHITE);

        int pp_y = fy + (pong_py * fh) / 100;
        int pp_h = (20 * fh) / 100;
        fill_rounded_rect(fx + 4, pp_y, 6, pp_h, 2, COLOR_ACCENT);

        int ap_y = fy + (pong_ay * fh) / 100;
        fill_rounded_rect(fx + fw - 10, ap_y, 6, pp_h, 2, COLOR_DANGER);

        int dy;
        for (dy = fy + 4; dy < fy + fh - 4; dy += 10) {
            fill_rect(fx + fw / 2 - 1, dy, 2, 5, 0xFF2A364F);
        }
        if (pong_paused) {
            fill_rounded_rect_alpha(fx + fw / 2 - 50, fy + fh / 2 - 16, 100, 32, 6, 0xE01C2233);
            draw_string(fx + fw / 2 - 24, fy + fh / 2 - 8, "PAUSED", COLOR_WHITE);
        }
        fill_rounded_rect(fx + fw / 2 - 50, fy + fh + 14, 100, 26, 4, COLOR_ACCENT);
        draw_string(fx + fw / 2 - 28, fy + fh + 19, "Restart", COLOR_WHITE);
        return;
    }

    if (store_view == STORE_VIEW_MEMORY) {
        fill_rounded_rect(cx, view_y, cw, view_h, 6, STORE_BG);
        fill_rounded_rect(cx + 12, view_y + 10, 80, 26, 4, COLOR_SURFACE2);
        draw_string(cx + 20, view_y + 15, "< Back", COLOR_WHITE);
        draw_string(cx + 110, view_y + 15, "Memory Match - Find pairs!", COLOR_WHITE);
        char sc[32] = "Pairs: ";
        char nb[12];
        itoa(mem_pairs, nb, 10); strcat_c(sc, nb);
        strcat_c(sc, "/8");
        draw_string(cx + cw - 100, view_y + 15, sc, COLOR_GOLD);

        int cols = 4;
        int card_w = 72, card_h = 72, gap = 10;
        int total_w = cols * card_w + (cols - 1) * gap;
        int total_h = 4 * card_h + 3 * gap;
        int bx = cx + (cw - total_w) / 2;
        int by = view_y + 46;
        uint32_t card_cols[8] = {
            0xFFEF4444, 0xFFF59E0B, 0xFF10B981, 0xFF3B82F6,
            0xFF8B5CF6, 0xFFEC4899, 0xFF14B8A6, 0xFFF97316
        };
        int i;
        for (i = 0; i < MEM_N; i++) {
            int rr = i / cols, cc = i % cols;
            int x = bx + cc * (card_w + gap);
            int y = by + rr * (card_h + gap);
            if (mem_shown[i]) {
                fill_rounded_rect(x, y, card_w, card_h, 6, card_cols[mem_vals[i]]);
                char sym[2] = { (char)('A' + mem_vals[i]), '\0' };
                draw_string(x + card_w / 2 - 4, y + card_h / 2 - 8, sym, COLOR_WHITE);
            } else {
                fill_rounded_rect(x, y, card_w, card_h, 6, COLOR_SURFACE2);
                fill_rounded_rect(x + card_w / 2 - 14, y + card_h / 2 - 14, 28, 28, 4, COLOR_ACCENT);
                draw_string(x + card_w / 2 - 4, y + card_h / 2 - 8, "?", COLOR_WHITE);
            }
        }
        if (mem_pairs >= 8) {
            fill_rounded_rect_alpha(bx + 20, by + total_h / 2 - 16, total_w - 40, 32, 6, 0xE0000000);
            draw_string(bx + total_w / 2 - 48, by + total_h / 2 - 8, "PERFECT!", COLOR_GOLD);
        }
        fill_rounded_rect(bx + total_w / 2 - 50, by + total_h + 14, 100, 26, 4, COLOR_ACCENT);
        draw_string(bx + total_w / 2 - 28, by + total_h + 19, "Restart", COLOR_WHITE);
        return;
    }

    draw_store_catalog(win, &layout);
}


/* APP_MEDIA (Video + Chiptune Audio Player) */
static void media_update(void) {
    if (media_state == MEDIA_STATE_PLAY) {
        media_curr_tick++;
        if (media_curr_tick >= media_total_ticks) media_curr_tick = 0;
        media_progress = (media_curr_tick * 1000) / media_total_ticks;
        if (media_mode == MEDIA_MODE_AUDIO) {
            media_note_tick++;
            if (media_note_tick >= chiptune_track[media_note_idx].ticks) {
                media_note_idx = (media_note_idx + 1) % CHIPTUNE_LEN;
                media_note_tick = 0;
            }
            uint16_t freq = chiptune_track[media_note_idx].freq;
            if (freq > 0) sound_raw_on(freq);
            else sound_stop();

            for (int i = 0; i < 20; i++) {
                int base = (freq / 40) + ((i * 7 + (int)system_ticks) % 25);
                if (base > 80) base = 80;
                eq_heights[i] = (uint8_t)base;
            }
        }
        need_redraw = 1;
    } else {
        if (media_mode == MEDIA_MODE_AUDIO) sound_stop();
        for (int i = 0; i < 20; i++) {
            if (eq_heights[i] > 2) eq_heights[i] -= 2;
            else eq_heights[i] = 0;
        }
    }
}

static void draw_media_client(window_t *win) {
    int cx = win->x + 8;
    int cy = win->y + TITLEBAR_H + 4;
    int cw = win->w - 16;
    int ch = win->h - TITLEBAR_H - 12;

    fill_rounded_rect(cx, cy, cw, 34, 6, COLOR_SURFACE2);
    int tab_w = 120;
    uint32_t v_col = (media_mode == MEDIA_MODE_VIDEO) ? COLOR_ACCENT : COLOR_SURFACE;
    fill_rounded_rect(cx + 6, cy + 4, tab_w, 26, 4, v_col);
    draw_string(cx + 26, cy + 9, "Video Mode", COLOR_WHITE);
    uint32_t a_col = (media_mode == MEDIA_MODE_AUDIO) ? COLOR_ACCENT : COLOR_SURFACE;
    fill_rounded_rect(cx + tab_w + 12, cy + 4, tab_w, 26, 4, a_col);
    draw_string(cx + tab_w + 32, cy + 9, "Audio Mode", COLOR_WHITE);

    int view_y = cy + 40;
    int view_h = ch - 100;
    fill_rounded_rect(cx, view_y, cw, view_h, 6, COLOR_VIEW_BG);

    if (media_mode == MEDIA_MODE_VIDEO) {
        for (int i = 0; i < 25; i++) {
            int sx = cx + ((i * 47 + (int)media_curr_tick / 2) % (cw - 8)) + 4;
            int sy = view_y + ((i * 31) % (view_h / 2)) + 4;
            putpixel(sx, sy, 0xFFB0C4DE);
        }
        int sun_cx = cx + cw / 2;
        int sun_cy = view_y + view_h / 2 + 10;
        int sun_r = 36;
        for (int dy = -sun_r; dy <= sun_r; dy++) {
            if ((dy + (int)media_curr_tick / 4) % 6 == 0 && dy > 0) continue;
            int dx_max = 0;
            while (dx_max * dx_max + dy * dy <= sun_r * sun_r) dx_max++;
            int py = sun_cy + dy;
            if (py >= view_y && py < view_y + view_h) {
                uint32_t sun_col = lerp_color(0xFFFF3366, 0xFFFFCC00, dy + sun_r, sun_r * 2);
                for (int x = sun_cx - dx_max; x <= sun_cx + dx_max; x++) {
                    if (x >= cx && x < cx + cw) putpixel(x, py, sun_col);
                }
            }
        }
        int hor_y = sun_cy;
        for (int y = hor_y + 1; y < view_y + view_h; y += 8) {
            int dy = y - hor_y;
            int off = ((int)media_curr_tick * 2) % 16;
            int line_y = hor_y + dy + off / 2;
            if (line_y < view_y + view_h) {
                draw_line(cx + 4, line_y, cx + cw - 4, line_y, 0xFF381A5E);
            }
        }
        for (int i = -6; i <= 6; i++) {
            int top_x = sun_cx + i * 16;
            int bot_x = sun_cx + i * 65;
            draw_line(top_x, hor_y, bot_x, view_y + view_h - 1, 0xFF4C1D95);
        }
        draw_rikki(cx + cw / 2 - 24, view_y + view_h - 40, 3);
    } else {
        draw_string(cx + 20, view_y + 16, "Now Playing: Korobeiniki (8-bit Tetris Chiptune)", COLOR_WHITE);
        draw_string(cx + 20, view_y + 36, "Hardware: Motherboard PC Speaker Driver", COLOR_MUTED);
        int bar_w = (cw - 60) / 20;
        int max_bar_h = view_h - 80;
        for (int i = 0; i < 20; i++) {
            int bx = cx + 24 + i * (bar_w + 2);
            int bh = (eq_heights[i] * max_bar_h) / 80;
            if (bh > max_bar_h) bh = max_bar_h;
            int by = view_y + view_h - 16 - bh;
            for (int seg_y = by; seg_y < view_y + view_h - 16; seg_y += 4) {
                uint32_t seg_col = 0xFF10B981;
                if (seg_y < view_y + view_h - 16 - (max_bar_h * 2) / 3) seg_col = 0xFFEF4444;
                else if (seg_y < view_y + view_h - 16 - max_bar_h / 3)   seg_col = 0xFFF59E0B;
                fill_rect(bx, seg_y, bar_w, 3, seg_col);
            }
        }
    }

    int bar_x = cx + 12;
    int bar_y = cy + ch - 54;
    int bar_w = cw - 24;
    fill_rounded_rect(bar_x, bar_y, bar_w, 8, 4, COLOR_SURFACE2);
    int fill_w = (bar_w * (int)media_progress) / 1000;
    if (fill_w > 0) fill_rounded_rect(bar_x, bar_y, fill_w, 8, 4, COLOR_ACCENT);
    fill_rounded_rect(bar_x + fill_w - 3, bar_y - 3, 8, 14, 2, COLOR_WHITE);

    int ctrl_y = cy + ch - 38;
    fill_rounded_rect(cx + 12, ctrl_y, 44, 26, 4, (media_state == MEDIA_STATE_PLAY) ? COLOR_ACCENT : COLOR_SURFACE2);
    draw_string(cx + 20, ctrl_y + 6, "Play", COLOR_WHITE);
    fill_rounded_rect(cx + 62, ctrl_y, 48, 26, 4, (media_state == MEDIA_STATE_PAUSE) ? COLOR_ACCENT : COLOR_SURFACE2);
    draw_string(cx + 70, ctrl_y + 6, "Pause", COLOR_WHITE);
    fill_rounded_rect(cx + 116, ctrl_y, 44, 26, 4, (media_state == MEDIA_STATE_STOP) ? 0xFF991B1B : COLOR_SURFACE2);
    draw_string(cx + 124, ctrl_y + 6, "Stop", COLOR_WHITE);

    char time_str[32];
    uint32_t cur_sec = (media_curr_tick / 20);
    (void)media_total_ticks;
    char buf[12];
    time_str[0] = '0'; time_str[1] = '0'; time_str[2] = ':';
    itoa((int32_t)cur_sec, buf, 10);
    if (cur_sec < 10) { time_str[3] = '0'; time_str[4] = buf[0]; time_str[5] = '\0'; }
    else { time_str[3] = buf[0]; time_str[4] = buf[1]; time_str[5] = '\0'; }
    strcat_c(time_str, " / 01:00");
    draw_string(cx + cw - 120, ctrl_y + 6, time_str, COLOR_MUTED);
}

/* ===== Paint: one layout shared by painting and hit testing ===== */

typedef struct {
    int bar_x, bar_y, bar_w, bar_h;               /* toolbar container       */
    int sw_x, sw_y, sw_d, sw_step;                /* 16 palette swatches      */
    int brush_x, brush_y, brush_w, brush_h;
    int eraser_x, eraser_y, eraser_w, eraser_h;
    int size_x, size_y, size_w, size_h, size_step;
    int clear_x, clear_y, clear_w, clear_h;
    int save_x, save_y, save_w, save_h;
    int frame_x, frame_y, frame_w, frame_h;       /* paper frame             */
    int canvas_x, canvas_y, canvas_w, canvas_h;   /* drawable viewport       */
    int status_x, status_y, status_w, status_h;
} paint_layout_t;

static void paint_get_layout(window_t *win, paint_layout_t *L) {
    int pad = 8;
    L->bar_x = win->x + pad;
    L->bar_y = win->y + TITLEBAR_H + 4;
    L->bar_w = win->w - pad * 2;
    L->bar_h = PAINT_BAR_H;
    if (L->bar_w < 240) L->bar_w = 240;

    /* Swatch row: squeezed when the window is narrower than the full palette. */
    L->sw_d = 18;
    L->sw_step = 24;
    if (12 + PAINT_PALETTE_SIZE * L->sw_step > L->bar_w - 12) {
        L->sw_step = (L->bar_w - 24) / PAINT_PALETTE_SIZE;
        if (L->sw_step < 14) L->sw_step = 14;
        L->sw_d = L->sw_step - 6;
        if (L->sw_d < 8) L->sw_d = 8;
    }
    L->sw_x = L->bar_x + 12;
    L->sw_y = L->bar_y + 10;

    /* Action row. */
    {
        int by = L->bar_y + 36, bh = 26;
        L->brush_w = 66;  L->brush_h = bh;  L->brush_y = by;
        L->brush_x = L->bar_x + 12;
        L->eraser_w = 70; L->eraser_h = bh; L->eraser_y = by;
        L->eraser_x = L->brush_x + L->brush_w + 8;
        L->size_w = 26;   L->size_h = bh;   L->size_y = by; L->size_step = 30;
        L->size_x = L->eraser_x + L->eraser_w + 14;
        L->clear_w = 56;  L->clear_h = bh;  L->clear_y = by;
        L->clear_x = L->size_x + 3 * L->size_step + 8;
        L->save_w = 68;   L->save_h = bh;   L->save_y = by;
        L->save_x = L->bar_x + L->bar_w - 12 - L->save_w;  /* primary, right-aligned */
    }

    /* Footer. */
    L->status_h = PAINT_STATUS_H;
    L->status_x = L->bar_x;
    L->status_w = L->bar_w;
    L->status_y = win->y + win->h - 10 - L->status_h;

    /* Canvas viewport, then the paper frame wrapped around and centred. */
    {
        int avail_w = L->bar_w;
        int avail_h = L->status_y - 8 - (L->bar_y + L->bar_h) - 8;
        int cw = avail_w - 6;
        int ch = avail_h - 6;
        if (cw > PAINT_W) cw = PAINT_W;
        if (ch > PAINT_H) ch = PAINT_H;
        if (cw < 32) cw = 32;
        if (ch < 32) ch = 32;
        L->canvas_w = cw;
        L->canvas_h = ch;
        L->frame_w = cw + 6;
        L->frame_h = ch + 6;
        L->frame_x = L->bar_x + (L->bar_w - L->frame_w) / 2;
        L->frame_y = L->bar_y + L->bar_h + 8;
        L->canvas_x = L->frame_x + 3;
        L->canvas_y = L->frame_y + 3;
    }
}

static void paint_tool_button(int x, int y, int w, int h, const char *label, int active) {
    fill_rounded_rect(x, y, w, h, 8, active ? COLOR_ACCENT : COLOR_SURFACE);
    {
        int lw = (int)strlen(label) * 8;
        draw_string(x + (w - lw) / 2, y + (h - 16) / 2, label,
                    active ? C_BLACK : COLOR_WHITE);
    }
}

/* Serialise the canvas as a binary PPM (P6) and commit it to ShkodyaFS. */
static void paint_save(void) {
    char msg[64];
    uint32_t len = 0, t0, t1, total = (uint32_t)PAINT_W * PAINT_H;
    int r;
    char num[12];

    paint_ppm[len++] = 'P';
    paint_ppm[len++] = '6';
    paint_ppm[len++] = '\n';
    itoa(PAINT_W, num, 10);
    for (int i = 0; num[i] != '\0'; i++) paint_ppm[len++] = (uint8_t)num[i];
    paint_ppm[len++] = ' ';
    itoa(PAINT_H, num, 10);
    for (int i = 0; num[i] != '\0'; i++) paint_ppm[len++] = (uint8_t)num[i];
    paint_ppm[len++] = '\n';
    itoa(255, num, 10);
    for (int i = 0; num[i] != '\0'; i++) paint_ppm[len++] = (uint8_t)num[i];
    paint_ppm[len++] = '\n';

    /* Canvas stores 0x00RRGGBB; P6 wants R,G,B in that order. */
    for (uint32_t i = 0; i < total; i++) {
        uint32_t c = paint_canvas[i];
        paint_ppm[len++] = (uint8_t)((c >> 16) & 0xFFu);
        paint_ppm[len++] = (uint8_t)((c >> 8) & 0xFFu);
        paint_ppm[len++] = (uint8_t)(c & 0xFFu);
    }

    t0 = system_ticks;
    r = vfs_write(PAINT_SAVE_FILE, paint_ppm, len);
    t1 = system_ticks;

    if (r == SHK_OK) {
        strcpy_c(msg, "Saved ");
        strcat_c(msg, PAINT_SAVE_FILE);
        strcat_c(msg, " (");
        append_int(msg, (int32_t)(len / 1024));
        strcat_c(msg, " KB)");
        /* The commit is a blocking PIO write, so surface whole seconds when the
           canvas is big enough for the pause to be noticeable. */
        if (t1 - t0 >= PIT_FREQ) {
            strcat_c(msg, " ");
            append_int(msg, (int32_t)((t1 - t0) / PIT_FREQ));
            strcat_c(msg, "s");
        }
        toast_show(msg);
    } else {
        strcpy_c(msg, "Save failed: ");
        strcat_c(msg, vfs_error_text(r));
        toast_show(msg);
    }
    need_redraw = 1;
}

static void draw_paint_client(window_t *win) {
    paint_layout_t L;
    paint_get_layout(win, &L);

    /* ---- Fluent toolbar: rounded container with a subtle 1px border ---- */
    fill_rounded_rect(L.bar_x, L.bar_y, L.bar_w, L.bar_h, 10, COLOR_BORDER);
    fill_rounded_rect(L.bar_x + 1, L.bar_y + 1, L.bar_w - 2, L.bar_h - 2, 9, COLOR_SURFACE2);

    /* ---- Palette: circular swatches, active one ringed ---- */
    for (int i = 0; i < PAINT_PALETTE_SIZE; i++) {
        int px = L.sw_x + i * L.sw_step;
        int active = (!paint_is_eraser && i == paint_color_index);
        if (active) {
            fill_rounded_rect(px - 3, L.sw_y - 3, L.sw_d + 6, L.sw_d + 6,
                              (L.sw_d + 6) / 2, COLOR_ACCENT);
            fill_rounded_rect(px - 1, L.sw_y - 1, L.sw_d + 2, L.sw_d + 2,
                              (L.sw_d + 2) / 2, 0xFF191B1F);
        }
        fill_rounded_rect(px, L.sw_y, L.sw_d, L.sw_d, L.sw_d / 2, paint_palette[i]);
    }

    /* ---- Tools, sizes, clear, save ---- */
    paint_tool_button(L.brush_x, L.brush_y, L.brush_w, L.brush_h, "Brush", !paint_is_eraser);
    paint_tool_button(L.eraser_x, L.eraser_y, L.eraser_w, L.eraser_h, "Eraser", paint_is_eraser);

    for (int i = 1; i <= 3; i++) {
        int bx = L.size_x + (i - 1) * L.size_step;
        int sel = (paint_brush_size == i);
        char sz_str[2] = { (char)('0' + i), '\0' };
        fill_rounded_rect(bx, L.size_y, L.size_w, L.size_h, 8,
                          sel ? COLOR_ACCENT : COLOR_SURFACE);
        draw_string(bx + (L.size_w - 8) / 2, L.size_y + 5, sz_str,
                    sel ? C_BLACK : COLOR_WHITE);
    }

    fill_rounded_rect(L.clear_x, L.clear_y, L.clear_w, L.clear_h, 8, 0xFF5A1F27);
    draw_string(L.clear_x + (L.clear_w - 40) / 2, L.clear_y + 5, "Clear", 0xFFFFB4B4);

    fill_rounded_rect(L.save_x, L.save_y, L.save_w, L.save_h, 8, 0xFF2F6B45);
    draw_string(L.save_x + (L.save_w - 32) / 2, L.save_y + 5, "Save", COLOR_WHITE);

    /* ---- Paper frame: dark surround so the white sheet reads as raised ---- */
    fill_rounded_rect(L.frame_x, L.frame_y, L.frame_w, L.frame_h, 8, COLOR_BORDER);
    fill_rounded_rect(L.frame_x + 1, L.frame_y + 1, L.frame_w - 2, L.frame_h - 2, 7, 0xFF14161A);
    for (int y = 0; y < L.canvas_h; y++) {
        memcpy(&backbuffer[(L.canvas_y + y) * fb_width + L.canvas_x],
               &paint_canvas[y * PAINT_W], L.canvas_w * 4);
    }
    /* Hairline on top of the blit so the sheet edge stays crisp. */
    draw_line(L.canvas_x - 1, L.canvas_y - 1, L.canvas_x + L.canvas_w, L.canvas_y - 1, 0xFF35393F);
    draw_line(L.canvas_x - 1, L.canvas_y + L.canvas_h, L.canvas_x + L.canvas_w, L.canvas_y + L.canvas_h, 0xFF35393F);
    draw_line(L.canvas_x - 1, L.canvas_y - 1, L.canvas_x - 1, L.canvas_y + L.canvas_h, 0xFF35393F);
    draw_line(L.canvas_x + L.canvas_w, L.canvas_y - 1, L.canvas_x + L.canvas_w, L.canvas_y + L.canvas_h, 0xFF35393F);

    /* ---- Footer: tool, size, canvas size, live pointer readout ---- */
    fill_rounded_rect(L.status_x, L.status_y, L.status_w, L.status_h, 7, COLOR_SURFACE2);
    {
        char buf[80];
        strcpy_c(buf, paint_is_eraser ? "Tool: Eraser" : "Tool: Brush");
        strcat_c(buf, "   Size: ");
        append_int(buf, paint_brush_size);
        strcat_c(buf, "   Canvas: ");
        append_int(buf, PAINT_W);
        strcat_c(buf, " x ");
        append_int(buf, PAINT_H);
        draw_string(L.status_x + 12, L.status_y + 3, buf, COLOR_MUTED);
    }
    {
        char pos[40];
        int inside = (paint_cursor_x >= 0 && paint_cursor_y >= 0);
        if (inside) {
            strcpy_c(pos, "X: ");
            append_int(pos, paint_cursor_x);
            strcat_c(pos, "   Y: ");
            append_int(pos, paint_cursor_y);
        } else {
            strcpy_c(pos, "X: --   Y: --");
        }
        int pw = (int)strlen(pos) * 8;
        draw_string(L.status_x + L.status_w - 12 - pw, L.status_y + 3, pos,
                    inside ? COLOR_ACCENT : COLOR_MUTED);
    }
}

static void draw_notepad_client(window_t *win) {
    int tb_y = win->y + TITLEBAR_H + 4;
    int tb_h = 30;
    fill_rounded_rect(win->x + 6, tb_y, win->w - 12, tb_h, 6, COLOR_SURFACE2);
    int btn_x = win->x + 12;
    int btn_y = tb_y + 4;
    fill_rounded_rect(btn_x, btn_y, 50, 22, 4, COLOR_ACCENT);
    draw_string(btn_x + 8, btn_y + 4, "New", COLOR_WHITE);
    btn_x += 56;
    fill_rounded_rect(btn_x, btn_y, 50, 22, 4, COLOR_SUCCESS);
    draw_string(btn_x + 8, btn_y + 4, "Save", COLOR_WHITE);

    int paper_x = win->x + 8;
    int paper_y = tb_y + tb_h + 2;
    int paper_w = win->w - 16;
    int paper_h = win->h - TITLEBAR_H - tb_h - 30;
    fill_rounded_rect(paper_x, paper_y, paper_w, paper_h, 6, COLOR_PAPER);
    draw_line(paper_x, paper_y + paper_h, paper_x + paper_w, paper_y + paper_h, COLOR_BORDER);

    int x = paper_x + 8;
    int y = paper_y + 8;
    for (uint32_t i = 0; i < notepad_len; i++) {
        if (notepad_text[i] == '\n') { x = paper_x + 8; y += 18; continue; }
        draw_char(x, y, notepad_text[i], COLOR_PAPER_FG);
        x += 8;
        if (x > paper_x + paper_w - 16) { x = paper_x + 8; y += 18; }
    }
    if ((system_ticks / 40) % 2 && focused >= 0 && windows[focused].app == APP_NOTEPAD) {
        fill_rect(x, y + 1, 2, 14, COLOR_ACCENT);
    }
    char status_str[64] = "File: ";
    strcat_c(status_str, notepad_current_file);
    strcat_c(status_str, " | UTF-8 PlainText");
    draw_string(paper_x + 4, win->y + win->h - 20, status_str, COLOR_MUTED);
}

/* ===== Interactive Rikki Assistant =====
 * The mascot reacts to the "Pet Rikki" button (PC-speaker chirp + purr) and the
 * window shows live metric gauges for uptime, memory and the VBE mode. */

#define RIKKI_WELL 124          /* mascot well edge length (px)            */
#define RIKKI_SPRITE_SCALE 5    /* 16x16 sprite at scale 5 => 80x80 px     */
#define RIKKI_MASCOT_H 144      /* height of the mascot card               */
#define RIKKI_PET_H 44          /* height of the action card               */
#define RIKKI_GAUGE_SPAN 42     /* vertical step between metric rows       */

static const char *rikki_tips[6][2] = {
    { "ShkodyaFS online!",  "Your files live on disk." },
    { "System healthy",     "All subsystems nominal." },
    { "Meow!",              "Rikki watches the PIT." },
    { "Scratch my ears",    "and I will purr for you." },
    { "Chiptune ready",     "Korobeiniki is loaded." },
    { "Nice to see you!",   "Let's build something." }
};
#define RIKKI_TIP_COUNT 6

static int rikki_pets = 0;
static uint32_t rikki_happy_until = 0;

/* Happy while the purr runs, and for a short tail after a pet. */
static int rikki_happy(void) {
    return purr_active || (int32_t)(rikki_happy_until - system_ticks) > 0;
}

/* Round-grained heart: two lobes plus a converging tip. */
static void draw_heart(int x, int y, int s, uint32_t col) {
    if (s < 1) s = 1;
    fill_rounded_rect(x, y, 2 * s, 2 * s, s, col);
    fill_rounded_rect(x + 2 * s, y, 2 * s, 2 * s, s, col);
    for (int i = 0; i < 2 * s; i++) {
        int w = 4 * s - 2 * i;
        if (w <= 0) break;
        fill_rect(x + i, y + 2 * s + i, w, 1, col);
    }
}

/* Left-pointing speech bubble anchored beside the mascot. */
static void draw_speech_bubble(int x, int y, int w, int h, int tail_y) {
    fill_rounded_rect(x, y, w, h, 11, 0xFF2A2E37);
    fill_rounded_rect(x + 1, y + 1, w - 2, h - 2, 10, COLOR_FIELD);
    fill_rect(x - 1, tail_y, 1, 10, 0xFF2A2E37);
    fill_rect(x - 2, tail_y + 1, 1, 8, 0xFF2A2E37);
    fill_rect(x - 3, tail_y + 2, 1, 6, 0xFF2A2E37);
    fill_rect(x - 4, tail_y + 3, 1, 4, 0xFF2A2E37);
}

/* Label / value / bar row. pct is a normalized gauge position, not a fake value. */
static void draw_metric(int x, int y, int w, const char *label, const char *value,
                        uint32_t accent, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    draw_string(x, y, label, COLOR_MUTED);
    int vw = (int)strlen(value) * 8;
    draw_string(x + w - vw, y, value, COLOR_WHITE);
    int by = y + 20;
    fill_rounded_rect(x, by, w, 8, 4, 0xFF23272F);
    int fw = (w * pct) / 100;
    if (fw > 0) fill_rounded_rect(x, by, fw, 8, 4, accent);
}

/* Highest address the fixed-address image occupies: the framebuffer back
   buffer (declared in boot.S) plus the 64 KiB boot stack that follows it. */
static uintptr_t rikki_image_bytes(void) {
    return (uintptr_t)backbuffer + (uintptr_t)(1024u * 768u * 4u) + (uintptr_t)65536u;
}

static void draw_rikki_client(window_t *win) {
    int cx = win->x + 10;
    int cy = win->y + TITLEBAR_H + 8;
    int cw = win->w - 20;
    if (cw < 220) cw = 220;
    int bottom = win->y + win->h - 10;

    char buf[64];
    int happy = rikki_happy();

    /* ---- Card 1: polished mascot well + speech bubble ---- */
    fill_rounded_rect(cx, cy, cw, RIKKI_MASCOT_H, 10, COLOR_SURFACE2);
    fill_rounded_rect(cx + 1, cy + 1, cw - 2, RIKKI_MASCOT_H - 2, 9, COLOR_SURFACE);

    int wx = cx + 10, wy = cy + 10;
    fill_rounded_rect(wx, wy, RIKKI_WELL, RIKKI_WELL, 12, 0xFF2A1E28);
    fill_rounded_rect(wx + 2, wy + 2, RIKKI_WELL - 4, RIKKI_WELL - 4, 10, 0xFF191119);
    /* warm halo behind the sprite */
    fill_rounded_rect_alpha(wx + 14, wy + 14, RIKKI_WELL - 28, RIKKI_WELL - 28, 30,
                            happy ? 0x38FF9E7A : 0x22FFB37A);

    int sprite_x = wx + (RIKKI_WELL - 16 * RIKKI_SPRITE_SCALE) / 2;
    int sprite_y = wy + (RIKKI_WELL - 16 * RIKKI_SPRITE_SCALE) / 2;
    draw_rikki(sprite_x, sprite_y, RIKKI_SPRITE_SCALE);
    if (happy) {
        int s = RIKKI_SPRITE_SCALE;
        /* blush on both cheeks (map rows 7-8, cols 2-3 / 12-13 are head fur) */
        fill_rect_alpha(sprite_x + 2 * s, sprite_y + 7 * s, s, 2 * s, 0x8CF06292);
        fill_rect_alpha(sprite_x + 13 * s, sprite_y + 7 * s, s, 2 * s, 0x8CF06292);
        /* floating hearts above the well */
        uint32_t phase = system_ticks / 20;
        for (int i = 0; i < 3; i++) {
            int hy = wy + 14 - (int)((phase + (uint32_t)i * 5) % 24);
            if (hy < cy + 2) hy = cy + 2;
            draw_heart(wx + 16 + i * 36, hy, 3, 0xCCF06292);
        }
    }

    int bx = wx + RIKKI_WELL + 12;
    int bw = cx + cw - 10 - bx;
    if (bw < 140) bw = 140;
    int byy = cy + 14, bhh = 72;
    draw_speech_bubble(bx, byy, bw, bhh, byy + 40);

    const char *line1, *line2;
    if (happy) {
        line1 = "Purrrrr...";
        line2 = "<3   <3   <3";
    } else {
        int tip = (int)((system_ticks / (PIT_FREQ * 4) + (uint32_t)rikki_pets) % RIKKI_TIP_COUNT);
        line1 = rikki_tips[tip][0];
        line2 = rikki_tips[tip][1];
    }
    draw_string(bx + 14, byy + 14, "Rikki says", COLOR_ACCENT);
    draw_string(bx + 14, byy + 32, line1, COLOR_WHITE);
    draw_string(bx + 14, byy + 50, line2, COLOR_MUTED);

    int pill_y = byy + bhh + 8;
    fill_rounded_rect(bx, pill_y, 66, 18, 5, happy ? 0xFF6B2A44 : 0xFF065F46);
    fill_rounded_rect(bx + 6, pill_y + 6, 6, 6, 3, happy ? 0xFFF9A8D4 : 0xFF34D399);
    draw_string(bx + 18, pill_y + 1, "Active", 0xFFD1FAE5);
    draw_string(bx + 74, pill_y + 1, happy ? "mood: happy" : "mood: idle",
                happy ? 0xFFF06292 : COLOR_MUTED);

    strcpy_c(buf, "Pets: ");
    append_int(buf, rikki_pets);
    strcat_c(buf, "  |  PIT 100 Hz  |  i686");
    draw_string(bx, cy + RIKKI_MASCOT_H - 22, buf, COLOR_MUTED);

    /* ---- Card 2: "Pet Rikki" action ---- */
    int pet_y = cy + RIKKI_MASCOT_H + 8;
    int pb_x = cx + 10, pb_y = pet_y + 7, pb_w = 150, pb_h = 30;
    fill_rounded_rect(cx, pet_y, cw, RIKKI_PET_H, 10, COLOR_SURFACE2);
    fill_rounded_rect(pb_x, pb_y, pb_w, pb_h, 9, happy ? 0xFF8B3A6A : 0xFF35507E);
    {
        int lw = (int)strlen("Pet Rikki") * 8;
        draw_string(pb_x + (pb_w - lw) / 2, pb_y + 7, "Pet Rikki", COLOR_WHITE);
    }
    draw_string(pb_x + pb_w + 14, pet_y + 14,
                happy ? "Rikki: purrr! <3" : "Click to make Rikki happy",
                happy ? 0xFFF06292 : COLOR_MUTED);

    /* ---- Card 3: styled metric gauges ---- */
    int m_y = pet_y + RIKKI_PET_H + 8;
    int m_h = bottom - m_y;
    if (m_h < 130) m_h = 130;
    fill_rounded_rect(cx, m_y, cw, m_h, 10, COLOR_SURFACE2);
    draw_string(cx + 12, m_y + 10, "System", COLOR_ACCENT);

    int row_x = cx + 12, row_w = cw - 24;
    if (row_w < 120) row_w = 120;
    int r0 = m_y + 32;

    /* Uptime — gauge normalized to a 5 minute session window. */
    uint32_t secs = system_ticks / PIT_FREQ;
    uint32_t mm = secs / 60, ss = secs % 60;
    char up[8];
    up[0] = (char)('0' + (char)((mm / 10) % 10));
    up[1] = (char)('0' + (char)(mm % 10));
    up[2] = ':';
    up[3] = (char)('0' + (char)((ss / 10) % 10));
    up[4] = (char)('0' + (char)(ss % 10));
    up[5] = '\0';
    int up_pct = (int)((secs * 100u) / 300u);
    draw_metric(row_x, r0, row_w, "Uptime", up, 0xFF6BBF8A, up_pct);

    /* Memory — fixed image extent against the multiboot RAM figure (KB). */
    uint32_t ram_kb = g_mbi ? (g_mbi->mem_lower + g_mbi->mem_upper) : 0;
    uintptr_t img = rikki_image_bytes();
    strcpy_c(buf, "");
    if (ram_kb) {
        uintptr_t tenths = (img * 10u) / (1024u * 1024u);
        append_int(buf, (int32_t)(tenths / 10));
        strcat_c(buf, ".");
        append_int(buf, (int32_t)(tenths % 10));
        strcat_c(buf, "/");
        append_int(buf, (int32_t)(ram_kb / 1024u));
        strcat_c(buf, " MB");
    } else {
        strcpy_c(buf, "unknown");
    }
    int mem_pct = ram_kb ? (int)(((img / 1024u) * 100u) / ram_kb) : 0;
    draw_metric(row_x, r0 + RIKKI_GAUGE_SPAN, row_w, "Memory", buf, 0xFF8FA4C4, mem_pct);

    /* Screen — VBE mode against a 1080p reference. */
    strcpy_c(buf, "");
    append_int(buf, (int32_t)fb_width);
    strcat_c(buf, "x");
    append_int(buf, (int32_t)fb_height);
    int scr_pct = (int)(((uint32_t)fb_width * fb_height * 100u) / (1920u * 1080u));
    draw_metric(row_x, r0 + RIKKI_GAUGE_SPAN * 2, row_w, "Screen", buf, 0xFFE8985C, scr_pct);

    draw_string(row_x, r0 + RIKKI_GAUGE_SPAN * 2 + 34,
                "VBE linear framebuffer 32bpp", COLOR_MUTED);
}

/* "Pet Rikki" — happy chirp on the PC speaker, purr, and a mood change. */
static void handle_rikki_click(window_t *win, int mx, int my) {
    int cx = win->x + 10;
    int cy = win->y + TITLEBAR_H + 8;
    int pet_y = cy + RIKKI_MASCOT_H + 8;
    int pb_x = cx + 10, pb_y = pet_y + 7, pb_w = 150, pb_h = 30;
    int pet_hit = point_in_rect(mx, my, pb_x, pb_y, pb_w, pb_h);
    int cat_hit = point_in_rect(mx, my, cx + 12, cy + 12, RIKKI_WELL - 4, RIKKI_WELL - 4);
    if (!pet_hit && !cat_hit) return;
    rikki_pets++;
    rikki_happy_until = system_ticks + PIT_FREQ * 3;
    sound_beep(1046, 3);            /* happy chirp, then the purr takes over */
    sound_purr_start(4);
    toast_show(pet_hit ? "Rikki purrs happily! <3" : "Rikki is purring! <3");
    need_redraw = 1;
}

/* ===== Calculator: one layout shared by painting, hover and hit testing ===== */

#define CALC_COLS 4
#define CALC_ROWS 4
#define CALC_PAD  10
#define CALC_GAP  8
#define CALC_R    12

typedef struct {
    int disp_x, disp_y, disp_w, disp_h;
    int keys_x, keys_y;
    int bw, bh, step_x, step_y;
} calc_layout_t;

static void calc_get_layout(window_t *win, calc_layout_t *L) {
    L->disp_x = win->x + CALC_PAD;
    L->disp_y = win->y + TITLEBAR_H + 8;
    L->disp_w = win->w - CALC_PAD * 2;
    L->disp_h = 78;
    if (L->disp_w < 160) L->disp_w = 160;

    L->keys_x = win->x + CALC_PAD;
    L->keys_y = L->disp_y + L->disp_h + 10;
    L->bw = (L->disp_w - (CALC_COLS - 1) * CALC_GAP) / CALC_COLS;
    L->step_x = L->bw + CALC_GAP;
    {
        int avail_h = (win->y + win->h - CALC_PAD) - L->keys_y - (CALC_ROWS - 1) * CALC_GAP;
        L->bh = avail_h / CALC_ROWS;
        if (L->bh > 72) L->bh = 72;
        if (L->bh < 30) L->bh = 30;
        L->step_y = L->bh + CALC_GAP;
    }
}

/* 4x4 holds all 16 tokens exactly once -- ten digits, four operators, C and
   '=' -- which is why the old grid had to duplicate '0' and '=' to fit, and
   still had nowhere to put '*' or '/'. */
static char calc_key_at(int idx) {
    static const char keys[CALC_COLS * CALC_ROWS] = {
        '7','8','9','/',
        '4','5','6','*',
        '1','2','3','-',
        '0','C','=','+'
    };
    if (idx < 0 || idx >= CALC_COLS * CALC_ROWS) return '\0';
    return keys[idx];
}

static int calc_hit_test(const calc_layout_t *L, int px, int py) {
    for (int r = 0; r < CALC_ROWS; r++) {
        for (int c = 0; c < CALC_COLS; c++) {
            int bx = L->keys_x + c * L->step_x;
            int by = L->keys_y + r * L->step_y;
            if (point_in_rect(px, py, bx, by, L->bw, L->bh))
                return r * CALC_COLS + c;
        }
    }
    return -1;
}

/* Hover/press feedback. Throttled like the Paint readout: a recomposite per
   mouse IRQ would be wasteful and the loop revisits within a tick. */
static void calc_hover_update(void) {
    int hover = -1, active = -1;
    if (focused >= 0 && windows[focused].used && windows[focused].app == APP_CALC) {
        calc_layout_t L;
        calc_get_layout(&windows[focused], &L);
        hover = calc_hit_test(&L, mouse_x, mouse_y);
        if (mouse_left) active = hover;
    }
    if (hover == calc_hover && active == calc_active) return;
    if (system_ticks - calc_hover_tick < 2) return;
    calc_hover_tick = system_ticks;
    calc_hover = hover;
    calc_active = active;
    need_redraw = 1;
}

static void draw_calc_client(window_t *win) {
    calc_layout_t L;
    calc_get_layout(win, &L);

    /* ---- Display: raised inset, deep field tone, 1px border ---- */
    fill_rounded_rect_aa(L.disp_x, L.disp_y, L.disp_w, L.disp_h, 10, COLOR_BORDER);
    fill_rounded_rect(L.disp_x + 1, L.disp_y + 1, L.disp_w - 2, L.disp_h - 2, 9, COLOR_FIELD);
    {
        /* secondary line: the expression in progress */
        int el = (int)strlen(calc_expr);
        draw_string(L.disp_x + L.disp_w - 14 - el * 8, L.disp_y + 14, calc_expr, COLOR_MUTED);
    }
    {
        /* primary line: current value, right-aligned with right padding */
        int tl = (int)strlen(calc_display);
        draw_string(L.disp_x + L.disp_w - 14 - tl * 8, L.disp_y + 46, calc_display,
                    calc_error ? COLOR_DANGER : COLOR_PAPER_FG);
    }
    draw_line(L.disp_x + 10, L.disp_y + L.disp_h - 12,
              L.disp_x + L.disp_w - 10, L.disp_y + L.disp_h - 12, 0x1FFFFFFF);

    /* ---- Keypad: category colouring + AA corners + hover/press ---- */
    for (int r = 0; r < CALC_ROWS; r++) {
        for (int c = 0; c < CALC_COLS; c++) {
            int idx = r * CALC_COLS + c;
            char k = calc_key_at(idx);
            int bx = L.keys_x + c * L.step_x;
            int by = L.keys_y + r * L.step_y;
            int radius = CALC_R;
            int inset = 0;
            uint32_t face, glyph;

            if (k >= '0' && k <= '9')   { face = COLOR_SURFACE2;  glyph = COLOR_WHITE; }
            else if (k == 'C')          { face = COLOR_DANGER_BG; glyph = COLOR_DANGER; }
            else if (k == '=')          { face = COLOR_ACCENT;    glyph = COLOR_WHITE;
                                          radius = L.bh / 2; inset = 6; }   /* pill */
            else                        { face = COLOR_CHROME_HI; glyph = COLOR_ACCENT; }

            {
                int px = bx + inset, pw = L.bw - inset * 2;
                int ir = radius > 1 ? radius - 1 : 1;
                fill_rounded_rect_aa(px, by, pw, L.bh, radius, COLOR_BORDER);
                fill_rounded_rect(px + 1, by + 1, pw - 2, L.bh - 2, ir, face);
                if (idx == calc_active) {
                    fill_rounded_rect_alpha(px + 1, by + 1, pw - 2, L.bh - 2, ir, 0x45000000);
                } else if (idx == calc_hover) {
                    fill_rounded_rect_alpha(px + 1, by + 1, pw - 2, L.bh - 2, ir, 0x30FFFFFF);
                }
            }
            {
                char s[2] = { k, '\0' };
                draw_string(bx + (L.bw - 8) / 2, by + (L.bh - 16) / 2, s, glyph);
            }
        }
    }
}

static void draw_term_client(window_t *win) {
    int term_x = win->x + 6;
    int term_y = win->y + TITLEBAR_H + 6;
    int term_w = win->w - 12;
    int term_h = win->h - TITLEBAR_H - 12;
    fill_rounded_rect(term_x, term_y, term_w, term_h, 6, COLOR_VIEW_BG);
    int y = term_y + 6;
    for (int r = 0; r < TERM_ROWS; r++) {
        if (term_lines[r][0] != '\0')
            draw_string(term_x + 8, y, term_lines[r], 0xFF4ADE80);
        y += 16;
    }
}

/* ===== Upgraded File Explorer (ShkodyaFS browser) =====
 * One layout struct is shared by painting and hit testing so they cannot drift. */

#define EXP_FILE_TEXT  0    /* .txt and friends  -> blue text sheet */
#define EXP_FILE_BIN   1    /* binaries/system   -> indigo gear     */
#define EXP_FILE_MEDIA 2    /* media             -> orange badge    */
#define EXP_FILE_GEN   3    /* anything else     -> neutral sheet   */
#define EXP_FILE_WORD  4    /* Word documents    -> folded sheet    */

typedef struct {
    int cx, cy, cw, ch;                       /* client area            */
    int tb_y, tb_h;                           /* toolbar band           */
    int addr_x, addr_y, addr_w, addr_h;       /* address bar            */
    int up_x, up_y, up_w, up_h;               /* parent-directory button */
    int new_x, new_y, new_w, new_h;           /* "+ New" button         */
    int ref_x, ref_y, ref_w, ref_h;           /* "Refresh" button       */
    int sb_x, sb_y, sb_w, sb_h;               /* sidebar                */
    int cards_x, cards_y, cards_w, cards_h;   /* file card viewport     */
    int status_y, status_h;                   /* footer status bar      */
    int card_w, card_h, gap;
} explorer_layout_t;

static void explorer_get_layout(window_t *win, explorer_layout_t *L) {
    L->cx = win->x + 6;
    L->cy = win->y + TITLEBAR_H + 6;
    L->cw = win->w - 12;
    L->ch = win->h - TITLEBAR_H - 12;
    if (L->cw < 240) L->cw = 240;
    if (L->ch < 160) L->ch = 160;

    L->tb_y = L->cy;
    L->tb_h = 34;
    L->addr_h = 28;
    L->addr_y = L->tb_y + 3;

    L->ref_w = 76; L->ref_h = 28; L->ref_y = L->addr_y;
    L->new_w = 64; L->new_h = 28; L->new_y = L->addr_y;
    L->ref_x = L->cx + L->cw - 8 - L->ref_w;
    L->new_x = L->ref_x - 6 - L->new_w;
    L->up_w = 34; L->up_h = 28; L->up_y = L->addr_y;
    L->up_x = L->new_x - 6 - L->up_w;
    L->addr_x = L->cx + 8;
    L->addr_w = L->up_x - 6 - L->addr_x;
    if (L->addr_w < 90) L->addr_w = 90;

    L->status_h = 22;
    L->status_y = L->cy + L->ch - L->status_h;

    L->sb_x = L->cx;
    L->sb_y = L->cy + L->tb_h + 6;
    L->sb_w = 120;
    L->sb_h = L->status_y - 6 - L->sb_y;
    if (L->sb_h < 60) L->sb_h = 60;

    L->cards_x = L->sb_x + L->sb_w + 8;
    L->cards_y = L->sb_y;
    L->cards_w = L->cw - L->sb_w - 8;
    L->cards_h = L->sb_h;

    L->card_w = 132; L->card_h = 88; L->gap = 8;
}

/* Parse a path once, then commit it atomically. A malformed or oversized
 * input leaves current_path untouched. Both slash styles are accepted, but
 * the canonical UI form always starts with one '/' and has no trailing slash
 * except at the root. */
static int explorer_path_change(const char *input) {
    char next[EXPLORER_PATH_MAX];
    uint32_t len = 1, i = 0;

    if (!input) return 0;
    next[0] = '/';
    next[1] = '\0';

    /* Relative paths are appended to the current directory; absolute paths
     * begin at root. Bound this copy even though current_path is internal. */
    if (input[0] != '/' && input[0] != '\\') {
        for (len = 0; len < EXPLORER_PATH_MAX - 1 && current_path[len]; len++)
            next[len] = current_path[len];
        if (len == 0) { next[0] = '/'; len = 1; }
        next[len] = '\0';
    }

    while (input[i]) {
        uint32_t start, part_len;
        while (input[i] == '/' || input[i] == '\\') i++;
        if (!input[i]) break;
        start = i;
        while (input[i] && input[i] != '/' && input[i] != '\\') i++;
        part_len = i - start;

        if (part_len == 1 && input[start] == '.') continue;
        if (part_len == 2 && input[start] == '.' && input[start + 1] == '.') {
            /* Drop the final component, retaining exactly the previous '/'. */
            while (len > 1 && next[len - 1] != '/') len--;
            if (len > 1) len--;
            next[len] = '\0';
            continue;
        }

        /* Check before every write, including the trailing NUL. */
        if (len + (len > 1 ? 1u : 0u) + part_len >= EXPLORER_PATH_MAX)
            return 0;
        if (len > 1) next[len++] = '/';
        for (uint32_t j = 0; j < part_len; j++) next[len++] = input[start + j];
        next[len] = '\0';
    }

    for (i = 0; i < len; i++) current_path[i] = next[i];
    current_path[len] = '\0';
    return 1;
}

static int explorer_path_enter(const char *child) {
    return explorer_path_change(child);
}

static int explorer_path_up(void) {
    return explorer_path_change("..");
}

static void explorer_path_reset(void) {
    current_path[0] = '/';
    current_path[1] = '\0';
}

/* ShkWord documents open in the word processor, everything else in Notepad. */
static int explorer_is_word_doc(const char *name) {
    uint32_t n = strlen(name);
    if (n > 4 && strcmp_c(name + n - 4, ".doc") == 0) return 1;
    if (n > 5 && strcmp_c(name + n - 5, ".docx") == 0) return 1;
    if (n > 4 && strcmp_c(name + n - 4, ".rtf") == 0) return 1;
    if (n >= 3 && strncmp_c(name, "doc", 3) == 0) return 1;
    return 0;
}

/* Extension-based file classification (drives the card icon). */
static int explorer_file_kind(const char *name) {
    static const char *media_ext[10] = {
        ".png", ".jpg", ".jpeg", ".bmp", ".gif",
        ".mp4", ".wav", ".mp3", ".avi", ".ppm"
    };
    static const char *bin_ext[7] = {
        ".bin", ".sys", ".app", ".exe", ".img", ".o", ".elf"
    };
    uint32_t n = strlen(name);
    for (int i = 0; i < 10; i++) {
        uint32_t m = strlen(media_ext[i]);
        if (n > m && strcmp_c(name + n - m, media_ext[i]) == 0) return EXP_FILE_MEDIA;
    }
    for (int i = 0; i < 7; i++) {
        uint32_t m = strlen(bin_ext[i]);
        if (n > m && strcmp_c(name + n - m, bin_ext[i]) == 0) return EXP_FILE_BIN;
    }
    if (explorer_is_word_doc(name)) return EXP_FILE_WORD;
    if (n > 4 && strcmp_c(name + n - 4, ".txt") == 0) return EXP_FILE_TEXT;
    if (n > 3 && strcmp_c(name + n - 3, ".md") == 0)  return EXP_FILE_TEXT;
    if (n > 4 && strcmp_c(name + n - 4, ".log") == 0) return EXP_FILE_TEXT;
    if (n > 4 && strcmp_c(name + n - 4, ".csv") == 0) return EXP_FILE_TEXT;
    return EXP_FILE_GEN;
}

static void explorer_short_name(const char *name, char *out, int max_chars) {
    int n = (int)strlen(name);
    if (n <= max_chars) { strcpy_c(out, name); return; }
    int keep = max_chars - 2;
    for (int i = 0; i < keep; i++) out[i] = name[i];
    out[keep] = '.'; out[keep + 1] = '.'; out[keep + 2] = '\0';
}

/* Stylized file card glyph: sheet silhouette plus a kind-specific badge. */
static void explorer_file_icon(int x, int y, int kind, uint32_t accent) {
    uint32_t base, face;
    if (kind == EXP_FILE_TEXT)      { base = 0xFF16273F; face = 0xFF1E3350; }
    else if (kind == EXP_FILE_BIN)  { base = 0xFF232548; face = 0xFF2C2F5C; }
    else if (kind == EXP_FILE_MEDIA){ base = 0xFF3A2617; face = 0xFF4A3320; }
    else if (kind == EXP_FILE_WORD) { base = 0xFF1B2A47; face = 0xFF243558; }
    else                            { base = 0xFF242833; face = 0xFF2E333F; }

    fill_rounded_rect_aa(x + 1, y, 24, 30, 4, base);
    fill_rounded_rect_aa(x + 2, y + 1, 22, 28, 3, face);
    /* top accent band (squared off along its lower edge) */
    fill_rounded_rect(x + 2, y + 1, 22, 9, 3, accent);
    fill_rect(x + 2, y + 6, 22, 4, accent);

    int mx = x + 13, my = y + 20;
    if (kind == EXP_FILE_BIN) {
        /* indigo gear */
        fill_rounded_rect(mx - 7, my - 7, 15, 15, 7, accent);
        fill_rect(mx - 3, my - 11, 6, 4, accent);
        fill_rect(mx - 3, my + 7, 6, 4, accent);
        fill_rect(mx - 11, my - 3, 4, 6, accent);
        fill_rect(mx + 7, my - 3, 4, 6, accent);
        fill_rounded_rect(mx - 3, my - 3, 6, 6, 3, face);
    } else if (kind == EXP_FILE_MEDIA) {
        /* orange badge with a play mark */
        fill_rounded_rect(mx - 8, my - 8, 17, 17, 5, accent);
        for (int i = 0; i < 5; i++) {
            int h = 9 - 2 * i;
            fill_rect(mx - 2 + i, my - h / 2, 1, h, 0xFFFFE4CB);
        }
    } else if (kind == EXP_FILE_WORD) {
        /* light page with a folded corner: reads as a word-processor document */
        fill_rect(mx - 7, my - 8, 14, 16, 0xFFEAF2FB);
        for (int i = 0; i < 5; i++)
            fill_rect(mx + 6 - i, my - 8 + i, 1 + i, 1, face);
        fill_rect(mx - 5, my - 3, 10, 2, 0xFF478CE1);
        fill_rect(mx - 5, my + 1, 10, 2, 0xFF7B93B4);
        fill_rect(mx - 5, my + 5, 6, 2, 0xFF7B93B4);
    } else if (kind == EXP_FILE_TEXT) {
        fill_rect(mx - 6, my - 7, 12, 2, 0xFFCFE2FA);
        fill_rect(mx - 6, my - 2, 9, 2, 0xFF9FC2EC);
        fill_rect(mx - 6, my + 3, 11, 2, 0xFFCFE2FA);
    } else {
        fill_rect(mx - 6, my - 5, 12, 2, 0xFFCBD5E1);
        fill_rect(mx - 6, my, 8, 2, 0xFF9AA6B5);
    }
}

static const char *explorer_kind_label(int kind) {
    if (kind == EXP_FILE_TEXT)  return "TEXT";
    if (kind == EXP_FILE_BIN)   return "BINARY";
    if (kind == EXP_FILE_MEDIA) return "MEDIA";
    if (kind == EXP_FILE_WORD)  return "WORD";
    return "FILE";
}

static uint32_t explorer_kind_color(int kind) {
    if (kind == EXP_FILE_TEXT)  return 0xFF3B82F6;
    if (kind == EXP_FILE_BIN)   return 0xFF6366F1;
    if (kind == EXP_FILE_MEDIA) return 0xFFF97316;
    if (kind == EXP_FILE_WORD)  return 0xFF478CE1;
    return 0xFF94A3B8;
}

static void draw_explorer_client(window_t *win) {
    explorer_layout_t L;
    explorer_get_layout(win, &L);

    /* ---- Toolbar: address bar + sleek actions ---- */
    fill_rounded_rect(L.cx, L.cy, L.cw, L.tb_h, 8, COLOR_SURFACE);
    fill_rounded_rect(L.addr_x, L.addr_y, L.addr_w, L.addr_h, 8, COLOR_BORDER);
    fill_rounded_rect(L.addr_x + 1, L.addr_y + 1, L.addr_w - 2, L.addr_h - 2, 7, COLOR_FIELD);
    /* small drive glyph */
    fill_rect(L.addr_x + 12, L.addr_y + 9, 4, 10, 0xFFDBAE50);
    fill_rounded_rect(L.addr_x + 11, L.addr_y + 7, 6, 5, 1, 0xFFF1C563);
    fill_rect(L.addr_x + 12, L.addr_y + 12, 4, 1, 0xFF6B5424);
    {
        char address[EXPLORER_PATH_MAX + 12];
        strcpy_c(address, "ShkodyaFS:");
        strcat_c(address, current_path);
        draw_string(L.addr_x + 26, L.addr_y + 6, address, COLOR_PAPER_FG);
    }

    fill_rounded_rect(L.up_x, L.up_y, L.up_w, L.up_h, 8, COLOR_CHROME_HI);
    fill_rounded_rect(L.new_x, L.new_y, L.new_w, L.new_h, 8, COLOR_CHROME_HI);
    fill_rounded_rect(L.ref_x, L.ref_y, L.ref_w, L.ref_h, 8, COLOR_SURFACE2);
    draw_string(L.up_x + 9, L.up_y + 6, "..", COLOR_WHITE);
    draw_string(L.new_x + 12, L.new_y + 6, "+ New", COLOR_WHITE);
    draw_string(L.ref_x + 10, L.ref_y + 6, "Refresh", COLOR_WHITE);

    /* ---- Sidebar ---- */
    fill_rounded_rect(L.sb_x, L.sb_y, L.sb_w, L.sb_h, 8, COLOR_SURFACE2);
    fill_rounded_rect(L.sb_x + 6, L.sb_y + 8, L.sb_w - 12, 22, 6, COLOR_CHROME_HI);
    draw_string(L.sb_x + 14, L.sb_y + 11, "Desktop", COLOR_WHITE);
    draw_string(L.sb_x + 14, L.sb_y + 42, "Documents", COLOR_MUTED);
    draw_string(L.sb_x + 14, L.sb_y + 66, "Downloads", COLOR_MUTED);
    draw_string(L.sb_x + 14, L.sb_y + 90, "ShkodyaFS", COLOR_ACCENT);

    /* ---- File card viewport ---- */
    fill_rounded_rect(L.cards_x, L.cards_y, L.cards_w, L.cards_h, 8, COLOR_SURFACE);

    int cols = (L.cards_w - L.gap) / (L.card_w + L.gap);
    if (cols < 1) cols = 1;
    int start_x = L.cards_x + L.gap;
    int start_y = L.cards_y + L.gap;
    int used = 0;

    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs_files[i].used) continue;
        used++;
        int row = (used - 1) / cols;
        int col = (used - 1) % cols;
        int x = start_x + col * (L.card_w + L.gap);
        int y = start_y + row * (L.card_h + L.gap);
        if (y + L.card_h > L.cards_y + L.cards_h - L.gap) break;

        int kind = explorer_file_kind(vfs_files[i].name);
        uint32_t accent = explorer_kind_color(kind);

        fill_rounded_rect_aa(x, y, L.card_w, L.card_h, 8, COLOR_BORDER);
        fill_rounded_rect_aa(x + 1, y + 1, L.card_w - 2, L.card_h - 2, 7, COLOR_SURFACE2);
        /* accent rail so the card type reads at a glance */
        fill_rounded_rect(x + 1, y + 1, 3, L.card_h - 2, 2, accent);
        explorer_file_icon(x + 12, y + 10, kind, accent);

        char short_name[16];
        explorer_short_name(vfs_files[i].name, short_name, 11);
        draw_string(x + 46, y + 12, short_name, COLOR_WHITE);
        draw_string(x + 46, y + 30, explorer_kind_label(kind), accent);

        char sz[24];
        strcpy_c(sz, "");
        append_int(sz, (int32_t)vfs_files[i].size);
        strcat_c(sz, " bytes");
        draw_string(x + 12, y + 66, sz, COLOR_MUTED);
    }

    if (used == 0) {
        draw_string(L.cards_x + 20, L.cards_y + 26, "No files on ShkodyaFS yet.", COLOR_MUTED);
        draw_string(L.cards_x + 20, L.cards_y + 46, "Use \"+ New\" to create one.", COLOR_MUTED);
    }

    /* ---- Footer status bar ---- */
    int fs_state = fs_status();
    fill_rounded_rect(L.cx, L.status_y, L.cw, L.status_h, 7, COLOR_SURFACE2);
    fill_rounded_rect(L.cx + 9, L.status_y + 8, 6, 6, 3,
                      fs_state == SHK_OK ? 0xFF34D399 : 0xFFE47070);
    char status[88];
    strcpy_c(status, "");
    append_int(status, used);
    strcat_c(status, used == 1 ? " file  |  " : " files  |  ");
    if (fs_state == SHK_OK) {
        strcat_c(status, "ShkodyaFS (ATA PIO) ");
        append_int(status, (int32_t)(ata_total_sectors >> 11));
        strcat_c(status, " MB");
    } else {
        strcat_c(status, vfs_error_text(fs_state));
    }
    draw_string(L.cx + 22, L.status_y + 3, status, COLOR_MUTED);
}

static void explorer_refresh(void) {
    int st = fs_status();
    vfs_refresh();
    toast_show(st == SHK_OK ? "Directory refreshed" : vfs_error_text(st));
    need_redraw = 1;
}

static void explorer_new_file(void) {
    char name[VFS_FILENAME_LEN];
    int chosen = 0;
    for (int i = 1; i < 10 && !chosen; i++) {
        strcpy_c(name, "new");
        append_int(name, i);
        strcat_c(name, ".txt");
        int found = vfs_find(name);
        if (found == SHK_ENOENT) chosen = 1;
        else if (found < 0) { toast_show(vfs_error_text(found)); return; }
    }
    if (!chosen) { toast_show("No free file name left"); return; }
    int r = vfs_create(name);
    if (r < 0) { toast_show(vfs_error_text(r)); return; }
    char msg[48] = "Created ";
    strcat_c(msg, name);
    toast_show(msg);
    need_redraw = 1;
}

/* ===== Task Manager =====
 * Windows are the kernel's GUI processes, so windows[] is the source of truth
 * for the list. RAM is explicitly labelled as an estimate: there is no page
 * allocator or per-process accounting in this freestanding kernel yet. */
typedef struct {
    int cx, cy, cw, ch;
    int list_x, list_y, list_w, row_h;
    int kill_x, kill_y, kill_w, kill_h;
} taskmgr_layout_t;

static int taskmgr_selected = -1;

static int taskmgr_window_index(const window_t *win) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (&windows[i] == win) return i;
    return -1;
}

static uint32_t taskmgr_window_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].used) count++;
    return count;
}

static uint32_t taskmgr_memory_kib(void) {
    uint32_t bytes = 4096u * 1024u;             /* conservative core base */
    bytes += 1024u * 768u * 4u;                 /* compositor back buffer */
    bytes += EXEC_OVERLAY_W * EXEC_OVERLAY_H * 4u;
    bytes += PAINT_W * PAINT_H * 4u;
    bytes += PAINT_PPM_HDR_MAX + PAINT_W * PAINT_H * 3u;
    bytes += taskmgr_window_count() * 64u * 1024u;
    return (bytes + 1023u) / 1024u;
}

static uint32_t taskmgr_total_memory_kib(void) {
    if (g_mbi && (g_mbi->flags & 1u))
        return g_mbi->mem_lower + g_mbi->mem_upper;
    return 64u * 1024u;                         /* display-only fallback */
}

static void taskmgr_get_layout(window_t *win, taskmgr_layout_t *L) {
    L->cx = win->x + 8;
    L->cy = win->y + TITLEBAR_H + 6;
    L->cw = win->w - 16;
    L->ch = win->h - TITLEBAR_H - 14;
    L->list_x = L->cx + 8;
    L->list_y = L->cy + 122;
    L->list_w = L->cw - 16;
    L->row_h = 27;
    L->kill_w = 152;
    L->kill_h = 28;
    L->kill_x = L->cx + L->cw - L->kill_w - 10;
    L->kill_y = L->cy + L->ch - L->kill_h - 8;
}

static void taskmgr_draw_uptime(int x, int y) {
    char text[48] = "Uptime: ";
    char number[16];
    uint32_t seconds = system_ticks / PIT_FREQ;
    append_int(text, (int32_t)(seconds / 3600u));
    strcat_c(text, "h ");
    append_int(text, (int32_t)((seconds / 60u) % 60u));
    strcat_c(text, "m ");
    append_int(text, (int32_t)(seconds % 60u));
    strcat_c(text, "s  (");
    itoa((int32_t)system_ticks, number, 10);
    strcat_c(text, number);
    strcat_c(text, " ticks)");
    draw_string(x, y, text, COLOR_MUTED);
}

static void draw_taskmgr_client(window_t *win) {
    taskmgr_layout_t L;
    uint32_t used_kib, total_kib, percent;
    int manager_index = taskmgr_window_index(win);
    taskmgr_get_layout(win, &L);

    if (taskmgr_selected < 0 || taskmgr_selected >= MAX_WINDOWS ||
        !windows[taskmgr_selected].used)
        taskmgr_selected = -1;

    fill_rounded_rect(L.cx, L.cy, L.cw, L.ch, 8, COLOR_SURFACE2);
    draw_string(L.cx + 12, L.cy + 10, "System overview", COLOR_WHITE);
    taskmgr_draw_uptime(L.cx + 12, L.cy + 32);

    used_kib = taskmgr_memory_kib();
    total_kib = taskmgr_total_memory_kib();
    if (total_kib == 0) total_kib = 1;
    percent = used_kib >= total_kib ? 100u : (used_kib * 100u) / total_kib;
    {
        char ram[56] = "RAM (UI estimate): ";
        char number[16];
        int bar_x = L.cx + 12, bar_y = L.cy + 70, bar_w = L.cw - 24;
        append_int(ram, (int32_t)used_kib);
        strcat_c(ram, " / ");
        itoa((int32_t)total_kib, number, 10);
        strcat_c(ram, number);
        strcat_c(ram, " KiB");
        draw_string(L.cx + 12, L.cy + 52, ram, COLOR_MUTED);
        fill_rounded_rect(bar_x, bar_y, bar_w, 10, 5, COLOR_FIELD);
        if (percent)
            fill_rounded_rect(bar_x, bar_y, (bar_w * (int)percent) / 100, 10, 5,
                              percent >= 85u ? COLOR_DANGER : COLOR_ACCENT);
    }

    draw_string(L.list_x + 8, L.list_y - 22, "Active windows", COLOR_WHITE);
    for (int i = 0, row = 0; i < MAX_WINDOWS; i++) {
        char line[56] = "PID ";
        int y;
        if (!windows[i].used) continue;
        y = L.list_y + row * L.row_h;
        if (y + L.row_h > L.kill_y - 6) break;
        fill_rounded_rect(L.list_x, y, L.list_w, L.row_h - 3, 5,
                          i == taskmgr_selected ? COLOR_CHROME_HI : COLOR_FIELD);
        append_int(line, i);
        strcat_c(line, "  ");
        strcat_c(line, windows[i].title);
        if (i == manager_index) strcat_c(line, " (this)");
        draw_string(L.list_x + 8, y + 4, line,
                    i == taskmgr_selected ? COLOR_WHITE : COLOR_PAPER_FG);
        row++;
    }

    {
        int kill_enabled = taskmgr_selected >= 0 && taskmgr_selected != manager_index;
        fill_rounded_rect(L.kill_x, L.kill_y, L.kill_w, L.kill_h, 7,
                          kill_enabled ? COLOR_DANGER_BG : COLOR_BORDER);
        draw_string(L.kill_x + 13, L.kill_y + 6, "Terminate process",
                    kill_enabled ? COLOR_WHITE : COLOR_MUTED);
    }
}

/* ===== ShkSheet (Excel-like spreadsheet) ===== */
static void sheet_init(void) {
    memset((uint8_t *)sheet_cells, 0, sizeof(sheet_cells));
    strcpy_c(sheet_cells[0][0], "Product");
    strcpy_c(sheet_cells[0][1], "Qty");
    strcpy_c(sheet_cells[0][2], "Price");
    strcpy_c(sheet_cells[0][3], "Total");
    strcpy_c(sheet_cells[1][0], "Apples");
    strcpy_c(sheet_cells[1][1], "10");
    strcpy_c(sheet_cells[1][2], "25");
    strcpy_c(sheet_cells[1][3], "250");
    strcpy_c(sheet_cells[2][0], "Oranges");
    strcpy_c(sheet_cells[2][1], "8");
    strcpy_c(sheet_cells[2][2], "30");
    strcpy_c(sheet_cells[2][3], "240");
    sheet_sel_row = 1; sheet_sel_col = 0; sheet_editing = 0; sheet_cursor = 0;
}

static int sheet_parse_int(const char *s) {
    int v = 0, i = 0, neg = 0;
    if (s[0] == '-') { neg = 1; i = 1; }
    while (s[i] >= '0' && s[i] <= '9') { v = v * 10 + (s[i] - '0'); i++; }
    return neg ? -v : v;
}

/* Evaluate a simple formula: =SUM(R1C1:R2C2) or =R1C1+R2C1 */
static void sheet_eval_cell(int row, int col) {
    char *cell = sheet_cells[row][col];
    if (cell[0] != '=') return;
    /* =SUM(col) — sum a column from row 1 to row-1 */
    if (strncmp_c(cell, "=SUM(", 5) == 0) {
        int sum_col = cell[5] - 'A';
        if (sum_col < 0 || sum_col >= SHEET_COLS) return;
        int sum = 0;
        for (int r = 1; r < row; r++) {
            if (sheet_cells[r][sum_col][0] >= '0' && sheet_cells[r][sum_col][0] <= '9')
                sum += sheet_parse_int(sheet_cells[r][sum_col]);
        }
        itoa(sum, cell, 10);
    }
}

static void sheet_insert_char(char ch) {
    int len = strlen(sheet_cells[sheet_sel_row][sheet_sel_col]);
    if (ch == '\b') {
        if (sheet_cursor > 0) {
            char *cell = sheet_cells[sheet_sel_row][sheet_sel_col];
            for (int i = sheet_cursor - 1; i < len; i++) cell[i] = cell[i + 1];
            sheet_cursor--;
        }
        return;
    }
    if (ch == '\n') {
        sheet_eval_cell(sheet_sel_row, sheet_sel_col);
        sheet_editing = 0;
        need_redraw = 1;
        return;
    }
    if (len >= SHEET_TEXT_LEN - 1) return;
    char *cell = sheet_cells[sheet_sel_row][sheet_sel_col];
    for (int i = len; i > sheet_cursor; i--) cell[i] = cell[i - 1];
    cell[sheet_cursor] = ch;
    cell[len + 1] = '\0';
    sheet_cursor++;
}

static void draw_sheet_client(window_t *win) {
    int gx = win->x + 6;
    int gy = win->y + TITLEBAR_H + 6;
    int gw = win->w - 12;
    int gh = win->h - TITLEBAR_H - 12;

    /* Toolbar */
    fill_rounded_rect(gx, gy, gw, 30, 6, COLOR_SURFACE2);
    draw_string(gx + 10, gy + 6, "ShkSheet - Formula: =SUM(col) Enter in cell", COLOR_MUTED);
    if (sheet_editing) {
        fill_rounded_rect(gx + gw - 80, gy + 4, 70, 22, 4, COLOR_ACCENT);
        draw_string(gx + gw - 72, gy + 8, "Edit", COLOR_WHITE);
    }

    int grid_y = gy + 36;
    int grid_h = gh - 36;
    int label_w = 28;

    /* Column headers A-H */
    for (int c = 0; c < SHEET_COLS; c++) {
        int cx = gx + label_w + c * SHEET_CELL_W;
        if (cx + SHEET_CELL_W > gx + gw) break;
        fill_rounded_rect(cx, grid_y, SHEET_CELL_W, SHEET_CELL_H, 3, COLOR_TITLE_LO);
        char hdr[2] = { (char)('A' + c), '\0' };
        draw_string(cx + SHEET_CELL_W / 2 - 4, grid_y + 5, hdr, COLOR_ACCENT);
    }

    /* Row headers 1-16 */
    for (int r = 0; r < SHEET_ROWS; r++) {
        int ry = grid_y + (r + 1) * SHEET_CELL_H;
        if (ry + SHEET_CELL_H > grid_y + grid_h) break;
        fill_rounded_rect(gx, ry, label_w, SHEET_CELL_H, 3, COLOR_TITLE_LO);
        char hdr[4];
        itoa(r + 1, hdr, 10);
        draw_string(gx + 8, ry + 5, hdr, COLOR_ACCENT);
    }

    /* Cells */
    for (int r = 0; r < SHEET_ROWS; r++) {
        int ry = grid_y + (r + 1) * SHEET_CELL_H;
        if (ry + SHEET_CELL_H > grid_y + grid_h) break;
        for (int c = 0; c < SHEET_COLS; c++) {
            int cx = gx + label_w + c * SHEET_CELL_W;
            if (cx + SHEET_CELL_W > gx + gw) break;
            int is_sel = (r == sheet_sel_row && c == sheet_sel_col);
            uint32_t bg = is_sel ? C_ACCENT : (r == 0 ? C_SURFACE2 : C_SURFACE);
            uint32_t fg = is_sel ? C_BLACK : C_WHITE;
            fill_rounded_rect(cx, ry, SHEET_CELL_W - 1, SHEET_CELL_H - 1, 2, bg);
            if (sheet_cells[r][c][0] != '\0') {
                int len = strlen(sheet_cells[r][c]);
                int tx = cx + 4;
                /* Right-align numbers */
                if (sheet_cells[r][c][0] >= '0' && sheet_cells[r][c][0] <= '9')
                    tx = cx + SHEET_CELL_W - 6 - len * 8;
                draw_string(tx, ry + 5, sheet_cells[r][c], fg);
            }
            /* Cursor blink in editing cell */
            if (is_sel && sheet_editing && (system_ticks / 30) % 2) {
                int cx_text = cx + 4;
                for (int i = 0; i < sheet_cursor; i++) {
                    char ch = sheet_cells[r][c][i];
                    if (ch >= 32) cx_text += 8;
                }
                fill_rect(cx_text, ry + 3, 2, 16, C_BLACK);
            }
        }
    }
}

/* ===== ShkWord (Word-like text editor) ===== */
static void word_reset(void) {
    const char *seed = "Shkodya Word Processor\n\nWelcome to ShkWord!\nType your document here.\nUse the toolbar to format text.\n\n- Bold, Italic, Underline\n- Font size control\n- Word count\n- Save to RamFS\n";
    uint32_t i = 0;
    while (seed[i] != '\0' && i < WORD_SIZE - 1) { word_text[i] = seed[i]; i++; }
    word_text[i] = '\0';
    word_len = i;
    word_cursor = (int)i;
    strcpy_c(word_current_file, "doc1.txt");
}

static void word_insert(char ch) {
    if (word_len + 1 >= WORD_SIZE) return;
    for (uint32_t i = word_len; (int)i > word_cursor; i--) word_text[i] = word_text[i - 1];
    word_text[word_cursor] = ch;
    word_len++;
    word_cursor++;
    word_text[word_len] = '\0';
}

static void word_backspace(void) {
    if (word_cursor <= 0) return;
    for (uint32_t i = (uint32_t)word_cursor; i < word_len; i++) word_text[i - 1] = word_text[i];
    word_len--;
    word_cursor--;
    word_text[word_len] = '\0';
}

static int word_count_words(void) {
    int count = 0, in_word = 0;
    for (uint32_t i = 0; i < word_len; i++) {
        unsigned char c = (unsigned char)word_text[i];
        if (c >= 32 && c != 127) in_word = 1;
        else { if (in_word) count++; in_word = 0; }
    }
    if (in_word) count++;
    return count;
}

static void draw_word_client(window_t *win) {
    int tb_y = win->y + TITLEBAR_H + 4;
    int tb_h = 30;
    /* Formatting toolbar */
    fill_rounded_rect(win->x + 6, tb_y, win->w - 12, tb_h, 6, COLOR_SURFACE2);

    int bx = win->x + 12, by = tb_y + 4;
    /* Bold */
    fill_rounded_rect(bx, by, 36, 22, 4, word_bold ? COLOR_ACCENT : COLOR_SURFACE);
    draw_string(bx + 10, by + 4, "B", word_bold ? C_BLACK : COLOR_WHITE);
    /* Italic */
    fill_rounded_rect(bx + 40, by, 36, 22, 4, word_italic ? COLOR_ACCENT : COLOR_SURFACE);
    draw_string(bx + 50, by + 4, "I", word_italic ? C_BLACK : COLOR_WHITE);
    /* Font size buttons */
    fill_rounded_rect(bx + 84, by, 28, 22, 4, (word_font_size == 1) ? COLOR_ACCENT : COLOR_SURFACE);
    draw_string(bx + 92, by + 4, "S", (word_font_size == 1) ? C_BLACK : COLOR_WHITE);
    fill_rounded_rect(bx + 114, by, 28, 22, 4, (word_font_size == 2) ? COLOR_ACCENT : COLOR_SURFACE);
    draw_string(bx + 122, by + 4, "M", (word_font_size == 2) ? C_BLACK : COLOR_WHITE);
    fill_rounded_rect(bx + 146, by, 28, 22, 4, (word_font_size == 3) ? COLOR_ACCENT : COLOR_SURFACE);
    draw_string(bx + 154, by + 4, "L", (word_font_size == 3) ? C_BLACK : COLOR_WHITE);
    /* Save button */
    fill_rounded_rect(bx + 184, by, 50, 22, 4, COLOR_SUCCESS);
    draw_string(bx + 192, by + 4, "Save", COLOR_WHITE);
    /* New button */
    fill_rounded_rect(bx + 238, by, 50, 22, 4, COLOR_CHROME_HI);
    draw_string(bx + 246, by + 4, "New", COLOR_WHITE);

    /* Paper area */
    int paper_x = win->x + 8;
    int paper_y = tb_y + tb_h + 2;
    int paper_w = win->w - 16;
    int paper_h = win->h - TITLEBAR_H - tb_h - 30;
    fill_rounded_rect(paper_x, paper_y, paper_w, paper_h, 6, COLOR_PAPER);

    /* Text rendering — inherits the dark theme's document surface/ink pair */
    uint32_t text_color = word_bold ? COLOR_PAPER_FG : blend_color(COLOR_PAPER_FG, COLOR_PAPER, 40);
    int char_w = 8;
    if (word_font_size == 1) char_w = 6;
    else if (word_font_size == 3) char_w = 10;
    int line_h = (word_font_size == 3) ? 22 : 18;
    int x = paper_x + 8;
    int y = paper_y + 8;
    for (uint32_t i = 0; i < word_len; i++) {
        if (word_text[i] == '\n') { x = paper_x + 8; y += line_h; continue; }
        if (x + char_w > paper_x + paper_w - 8) { x = paper_x + 8; y += line_h; }
        if (y + line_h > paper_y + paper_h) break;
        draw_char(x, y, word_text[i], text_color);
        x += char_w;
    }
    /* Cursor */
    if ((system_ticks / 40) % 2 && focused >= 0 && windows[focused].app == APP_WORD) {
        fill_rect(x, y + 1, 2, line_h - 2, COLOR_ACCENT);
    }
    /* Status bar */
    char status[80];
    strcpy_c(status, "Words: ");
    char num[12];
    itoa(word_count_words(), num, 10);
    strcat_c(status, num);
    strcat_c(status, " | Chars: ");
    itoa((int32_t)word_len, num, 10);
    strcat_c(status, num);
    strcat_c(status, " | ");
    strcat_c(status, word_current_file);
    draw_string(paper_x + 4, win->y + win->h - 20, status, COLOR_MUTED);
}


/* ===== UART COM1 serial bridge + "Shkodya Update" system =====
 *
 * The guest polls COM1 and never blocks on it: the RX drain is bounded per
 * call, the TX wait is bounded by a guard, and the whole thing is driven from
 * the main loop instead of an IRQ handler.
 *
 * Wire protocol (ASCII, '\n'-terminated lines):
 *      kernel -> host : "CMD:CHECK\n"
 *      host -> kernel : "RESP:LATEST\n"
 *                       "RESP:AVAILABLE|<ver>|<changelog>\n"
 *                       "RESP:PROGRESS|<0-100>\n"
 *                       "RESP:READY\n"
 */

#define COM1_BASE   0x3F8
#define COM1_RBR    (COM1_BASE + 0)   /* receive buffer / transmit hold */
#define COM1_IER    (COM1_BASE + 1)
#define COM1_FCR    (COM1_BASE + 2)
#define COM1_LCR    (COM1_BASE + 3)
#define COM1_MCR    (COM1_BASE + 4)
#define COM1_LSR    (COM1_BASE + 5)

#define COM1_LSR_RX_READY 0x01
#define COM1_LSR_TX_READY 0x20

#define SERIAL_RX_CAP     640         /* fits a full PKG_DATA line incl. payload */
#define SERIAL_TX_GUARD   200000u     /* bounded THR wait, never spins forever   */
#define SERIAL_RX_BUDGET  256         /* bytes drained per serial_poll() call    */

static char     serial_rx[SERIAL_RX_CAP];
static uint32_t serial_rx_len = 0;
static int      serial_lines = 0;     /* RESP: lines accepted from the host */
static int      serial_online = 0;    /* COM1 answered the loopback probe */

static void serial_init(void) {
    outb(COM1_IER, 0x00);   /* interrupts stay off: we poll */
    outb(COM1_LCR, 0x80);   /* DLAB on */
    outb(COM1_RBR, 0x01);   /* divisor 1 => 115200 baud */
    outb(COM1_IER, 0x00);
    outb(COM1_LCR, 0x03);   /* 8 data bits, no parity, 1 stop bit */
    outb(COM1_FCR, 0xC7);   /* FIFO on, cleared, 14-byte trigger */
    outb(COM1_MCR, 0x0B);   /* DTR / RTS / OUT2 */

    /* Loopback probe: a dead port reads back 0xFF instead of our scratch byte. */
    outb(COM1_MCR, 0x1E);
    outb(COM1_RBR, 0xAE);
    for (int i = 0; i < 1000; i++) {
        if (inb(COM1_LSR) & COM1_LSR_RX_READY) break;
    }
    serial_online = (inb(COM1_RBR) == 0xAE);
    outb(COM1_MCR, 0x0B);
}

static void serial_putc(char c) {
    for (uint32_t guard = 0; guard < SERIAL_TX_GUARD; guard++) {
        if (inb(COM1_LSR) & COM1_LSR_TX_READY) {
            outb(COM1_RBR, (uint8_t)c);
            return;
        }
    }
}

static void serial_send_str(const char *s) {
    for (uint32_t i = 0; s[i] != '\0'; i++) serial_putc(s[i]);
}

/* --- packet field helpers ------------------------------------------------- */

/* Copy the idx-th '|'-separated field of `line` into `out` (always NUL-terminated). */
static void proto_field(const char *line, int idx, char *out, uint32_t cap) {
    if (cap == 0) return;
    out[0] = '\0';
    int cur = 0;
    uint32_t o = 0;
    for (uint32_t i = 0; ; i++) {
        char c = line[i];
        int sep = (c == '|' || c == '\0');
        if (!sep && cur == idx) {
            if (o + 1 < cap) out[o++] = c;
        }
        if (sep) {
            if (cur == idx) { out[o] = '\0'; return; }
            cur++;
            o = 0;
            if (c == '\0') return;
        }
    }
}

static int proto_atoi(const char *s) {
    int v = 0, neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return neg ? -v : v;
}

/* --- Shkodya package manager ---------------------------------------------
 *
 *   kernel -> host : "CMD:PKG_SEARCH|<query>\n"
 *                    "CMD:PKG_GET|<package>\n"
 *   host -> kernel : "RESP:PKG_META|<app-filename>|<total_bytes>\n"
 *                    "RESP:PKG_DATA|<app-filename>|<base64 chunk>\n"
 *                    "RESP:PKG_END|<app-filename>\n"
 *                    "RESP:PKG_NOTFOUND|<package>\n"
 *                    "RESP:PKG_RESULT|<spec>|<kb>|<spec>|<kb>|...\n"
 *
 * A package arrives as META + one-or-more DATA + END. The chunks are spooled
 * in RAM and handed to vfs_write() exactly once at END, because a partially
 * written .app would be a corrupt install. A lone PKG_DATA opens its own
 * session, so a single-packet transfer still commits when END follows.
 */

#define PKG_MAX_BYTES 16384u
#define PKG_NAME_CAP  24
#define PKG_CHUNK_CAP 600            /* must fit SERIAL_RX_CAP minus the header */

static uint8_t  pkg_buffer[PKG_MAX_BYTES];
static uint32_t pkg_len = 0;
static uint32_t pkg_total = 0;
static char     pkg_name[PKG_NAME_CAP];
static int      pkg_active = 0;      /* a session is open                         */
static int      pkg_pending = 0;     /* a CMD was sent, awaiting the first reply  */
static uint32_t pkg_pending_tick = 0;
static int      pkg_last_pct = -1;   /* throttles terminal progress lines         */
static uint32_t pkg_installed = 0;   /* committed transfers this boot             */

static int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;                       /* '=' padding and anything unexpected */
}

/* Decode one base64 chunk into dst, appending from *len. Every chunk the host
   sends is a whole number of 4-character groups, so the tail padding of the
   final chunk is handled here too. Shared by packages and web pages. */
static uint32_t b64_decode_into(const char *src, uint8_t *dst, uint32_t *len,
                                uint32_t cap) {
    int q[4];
    int qn = 0;
    uint32_t written = 0;
    for (uint32_t i = 0; ; i++) {
        char c = src[i];
        int v;
        if (c == '\0') break;
        v = b64_value(c);
        if (v < 0) continue;
        q[qn++] = v;
        if (qn == 4) {
            uint32_t triple = ((uint32_t)q[0] << 18) | ((uint32_t)q[1] << 12) |
                              ((uint32_t)q[2] << 6)  | (uint32_t)q[3];
            for (int k = 0; k < 3; k++) {
                if (*len < cap) {
                    dst[(*len)++] = (uint8_t)((triple >> (16 - 8 * k)) & 0xFFu);
                    written++;
                }
            }
            qn = 0;
        }
    }
    if (qn > 1) {                    /* 2 or 3 leftover chars => 1 or 2 bytes */
        for (int k = qn; k < 4; k++) q[k] = 0;
        uint32_t triple = ((uint32_t)q[0] << 18) | ((uint32_t)q[1] << 12) |
                          ((uint32_t)q[2] << 6)  | (uint32_t)q[3];
        for (int k = 0; k < qn - 1; k++) {
            if (*len < cap) {
                dst[(*len)++] = (uint8_t)((triple >> (16 - 8 * k)) & 0xFFu);
                written++;
            }
        }
    }
    return written;
}

static uint32_t pkg_decode_base64(const char *src) {
    return b64_decode_into(src, pkg_buffer, &pkg_len, PKG_MAX_BYTES);
}

static void pkg_session_start(const char *name, uint32_t total) {
    uint32_t i = 0;
    while (name[i] != '\0' && i < PKG_NAME_CAP - 1) { pkg_name[i] = name[i]; i++; }
    pkg_name[i] = '\0';
    pkg_len = 0;
    pkg_total = total;
    pkg_last_pct = -1;
    pkg_active = (pkg_name[0] != '\0');
}

/* Single write point: spool -> ShkodyaFS, then report the outcome. */
static void pkg_commit(void) {
    char msg[80];
    int r;
    if (!pkg_active) return;
    r = vfs_write(pkg_name, pkg_buffer, pkg_len);
    pkg_active = 0;
    strcpy_c(msg, "shkodya: ");
    if (r == SHK_OK) {
        pkg_installed++;
        strcat_c(msg, "installed ");
        strcat_c(msg, pkg_name);
        strcat_c(msg, " (");
        append_int(msg, (int32_t)pkg_len);
        strcat_c(msg, " bytes)\n");
        term_puts_async(msg);
        toast_show("Package installed");
    } else {
        strcat_c(msg, "install failed: ");
        strcat_c(msg, vfs_error_text(r));
        strcat_c(msg, "\n");
        term_puts_async(msg);
        toast_show("Package install failed");
    }
    term_async_done();
    need_redraw = 1;
}

/* Gives up on a silent index so the terminal never waits forever. */
static void pkg_tick(void) {
    if (!pkg_pending) return;
    if (system_ticks - pkg_pending_tick > PIT_FREQ * 10) {
        pkg_pending = 0;
        term_puts_async("shkodya: no response from the package index\n");
        toast_show("Package request timed out");
        term_async_done();
        need_redraw = 1;
    }
}

/* Quote-safe framing: the wire format is '-'/'|' delimited, so strip anything
   that would break a field boundary. */
static void pkg_send(const char *prefix, const char *arg) {
    char cmd[128];
    strcpy_c(cmd, prefix);
    for (uint32_t i = 0; arg[i] != '\0' && strlen(cmd) + 2 < sizeof(cmd); i++) {
        char c = arg[i];
        uint32_t n;
        if (c == '|' || c == '\n' || c == '\r') c = ' ';
        n = strlen(cmd);
        cmd[n] = c;
        cmd[n + 1] = '\0';
    }
    strcat_c(cmd, "\n");
    serial_send_str(cmd);
}

static void pkg_cmd_search(const char *query) {
    char msg[96] = "shkodya: searching for \"";
    strcat_c(msg, query);
    strcat_c(msg, "\" ...\n");
    term_puts(msg);
    pkg_send("CMD:PKG_SEARCH|", query);
    pkg_pending = 1;
    pkg_pending_tick = system_ticks;
    term_prompt_held = 1;          /* the reply will draw the next prompt */
    need_redraw = 1;
}

static void pkg_cmd_install(const char *pkg) {
    char msg[96] = "shkodya: requesting \"";
    strcat_c(msg, pkg);
    strcat_c(msg, "\" ...\n");
    term_puts(msg);
    pkg_send("CMD:PKG_GET|", pkg);
    pkg_pending = 1;
    pkg_pending_tick = system_ticks;
    term_prompt_held = 1;          /* the reply will draw the next prompt */
    need_redraw = 1;
}

/* shkodya list: everything with a .app name that already lives on ShkodyaFS. */
static void pkg_list_installed(void) {
    char line[80];
    int count = 0;
    vfs_refresh();
    term_puts("Installed packages (.app on ShkodyaFS):\n");
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        uint32_t n;
        if (!vfs_files[i].used) continue;
        n = strlen(vfs_files[i].name);
        if (n < 4 || strcmp_c(vfs_files[i].name + n - 4, ".app") != 0) continue;
        strcpy_c(line, "  ");
        strcat_c(line, vfs_files[i].name);
        while (strlen(line) < 24u) strcat_c(line, " ");
        append_int(line, (int32_t)vfs_files[i].size);
        strcat_c(line, " bytes\n");
        term_puts(line);
        count++;
    }
    if (count == 0) {
        term_puts("  (none yet - try: shkodya install doom)\n");
    } else {
        strcpy_c(line, "  ");
        append_int(line, count);
        strcat_c(line, " package(s), ");
        append_int(line, (int32_t)pkg_installed);
        strcat_c(line, " installed this boot\n");
        term_puts(line);
    }
    need_redraw = 1;
}

/* --- update state --------------------------------------------------------- */

#define UPDATE_CURRENT_VERSION "2.2 FlameX"
#define UPDATE_IDLE        0
#define UPDATE_CHECKING    1
#define UPDATE_LATEST      2
#define UPDATE_AVAILABLE   3
#define UPDATE_DOWNLOADING 4
#define UPDATE_READY       5
#define UPDATE_ERROR       6

#define UPDATE_VER_CAP 40
#define UPDATE_LOG_CAP 120

static int      update_state = UPDATE_IDLE;
static int      update_progress = 0;
static char     update_version[UPDATE_VER_CAP];
static char     update_changelog[UPDATE_LOG_CAP];
static uint32_t update_last_rx = 0;

static const char *update_state_text(void) {
    switch (update_state) {
        case UPDATE_CHECKING:    return "Contacting update server...";
        case UPDATE_LATEST:      return "Up to date";
        case UPDATE_AVAILABLE:   return "Update available";
        case UPDATE_DOWNLOADING: return "Downloading from host bridge...";
        case UPDATE_READY:       return "Ready to apply";
        case UPDATE_ERROR:       return "Update check failed";
        default:                 return "Not checked yet";
    }
}

static int update_busy(void) {
    return update_state == UPDATE_CHECKING || update_state == UPDATE_DOWNLOADING;
}

/* One parsed RESP: line. Unknown packet kinds leave the state untouched. */
static void serial_handle_line(const char *line) {
    if (strncmp_c(line, "RESP:", 5) != 0) return;

    if (strcmp_c(line, "RESP:LATEST") == 0) {
        update_state = UPDATE_LATEST;
        update_progress = 0;
        update_version[0] = '\0';
        update_changelog[0] = '\0';
        toast_show("Already on the latest version");
    } else if (strncmp_c(line, "RESP:AVAILABLE|", 15) == 0) {
        proto_field(line, 1, update_version, UPDATE_VER_CAP);
        proto_field(line, 2, update_changelog, UPDATE_LOG_CAP);
        update_state = UPDATE_AVAILABLE;
        update_progress = 0;
        {
            char msg[64] = "Update available: ";
            strcat_c(msg, update_version);
            toast_show(msg);
        }
    } else if (strncmp_c(line, "RESP:PROGRESS|", 14) == 0) {
        char field[8];
        proto_field(line, 1, field, sizeof(field));
        int p = proto_atoi(field);
        if (p < 0) p = 0;
        if (p > 100) p = 100;
        update_progress = p;
        /* Stays DOWNLOADING until the host confirms with RESP:READY. */
        update_state = UPDATE_DOWNLOADING;
    } else if (strcmp_c(line, "RESP:READY") == 0) {
        update_progress = 100;
        update_state = UPDATE_READY;
        /* Kept short: the toast card fits ~30 characters at 8px per glyph. */
        toast_show("Update ready to apply");
    } else if (strncmp_c(line, "RESP:PKG_META|", 14) == 0) {
        char name[PKG_NAME_CAP];
        char total[16];
        char msg[80];
        proto_field(line, 1, name, sizeof(name));
        proto_field(line, 2, total, sizeof(total));
        pkg_session_start(name, (uint32_t)proto_atoi(total));
        pkg_pending = 0;
        if (pkg_active) {
            strcpy_c(msg, "shkodya: receiving ");
            strcat_c(msg, pkg_name);
            strcat_c(msg, " (");
            append_int(msg, (int32_t)pkg_total);
            strcat_c(msg, " bytes)\n");
            term_puts_async(msg);
        }
    } else if (strncmp_c(line, "RESP:PKG_DATA|", 14) == 0) {
        char chunk[PKG_CHUNK_CAP];
        pkg_pending = 0;
        if (!pkg_active) {
            /* A bare PKG_DATA still works: its filename field opens the session. */
            char name[PKG_NAME_CAP];
            proto_field(line, 1, name, sizeof(name));
            pkg_session_start(name, 0);
        }
        proto_field(line, 2, chunk, sizeof(chunk));
        pkg_decode_base64(chunk);
        if (pkg_total > 0) {
            int pct = (int)((pkg_len * 100u) / pkg_total);
            if (pct >= pkg_last_pct + 25 || pkg_len >= pkg_total) {
                char msg[64] = "shkodya: ";
                pkg_last_pct = pct;
                append_int(msg, (int32_t)pkg_len);
                strcat_c(msg, " / ");
                append_int(msg, (int32_t)pkg_total);
                strcat_c(msg, " bytes\n");
                term_puts_async(msg);
            }
        }
    } else if (strncmp_c(line, "RESP:PKG_END|", 13) == 0) {
        pkg_pending = 0;
        pkg_commit();
    } else if (strncmp_c(line, "RESP:PKG_NOTFOUND|", 18) == 0) {
        char name[PKG_NAME_CAP];
        char msg[80] = "shkodya: package not found: ";
        pkg_pending = 0;
        proto_field(line, 1, name, sizeof(name));
        strcat_c(msg, name);
        strcat_c(msg, "\n");
        term_puts_async(msg);
        toast_show("Package not found");
        term_async_done();
    } else if (strncmp_c(line, "RESP:PKG_RESULT|", 16) == 0) {
        int hits = 0;
        pkg_pending = 0;
        term_puts_async("shkodya: search results\n");
        for (int f = 1; f < 40; f += 2) {
            char name[PKG_NAME_CAP];
            char kb[16];
            char msg[80];
            proto_field(line, f, name, sizeof(name));
            if (name[0] == '\0') break;
            proto_field(line, f + 1, kb, sizeof(kb));
            strcpy_c(msg, "  ");
            strcat_c(msg, name);
            while (strlen(msg) < 22u) strcat_c(msg, " ");
            append_int(msg, proto_atoi(kb));
            strcat_c(msg, " KB\n");
            term_puts_async(msg);
            hits++;
        }
        if (hits == 0) term_puts_async("  no matching packages\n");
        term_async_done();
    } else if (strncmp_c(line, "RESP:WEB_", 9) == 0) {
        /* Page assembly lives in the browser module, which is defined further
           down this file; only the packet sniffing belongs to this section. */
        web_handle_packet(line);
    } else {
        return;
    }

    serial_lines++;
    update_last_rx = system_ticks;
    need_redraw = 1;
}

/* Non-blocking COM1 drain: bounded so a flooding host cannot stall the UI. */
static void serial_poll(void) {
    for (int budget = 0; budget < SERIAL_RX_BUDGET; budget++) {
        if (!(inb(COM1_LSR) & COM1_LSR_RX_READY)) return;
        char c = (char)inb(COM1_RBR);
        if (c == '\r') continue;               /* tolerate CRLF from raw terminals */
        if (c == '\n') {
            serial_rx[serial_rx_len] = '\0';
            if (serial_rx_len > 0) serial_handle_line(serial_rx);
            serial_rx_len = 0;
        } else if (serial_rx_len < SERIAL_RX_CAP - 1) {
            serial_rx[serial_rx_len++] = c;
        } else {
            serial_rx_len = 0;                 /* oversized line: drop it whole */
        }
    }
}

static void update_start_check(void) {
    if (update_busy()) return;                 /* one exchange at a time */
    serial_rx_len = 0;
    update_progress = 0;
    update_version[0] = '\0';
    update_changelog[0] = '\0';
    update_state = UPDATE_CHECKING;
    update_last_rx = system_ticks;
    serial_send_str("CMD:CHECK\n");
    toast_show(serial_online ? "Sent CMD:CHECK on COM1" : "COM1 absent - CMD:CHECK sent anyway");
    need_redraw = 1;
}

/* Gives up on a silent bridge so the UI never spins forever. */
static void update_tick(void) {
    if (!update_busy()) return;
    if (system_ticks - update_last_rx > PIT_FREQ * 6) {
        update_state = UPDATE_ERROR;
        strcpy_c(update_changelog, "No reply from the host bridge on COM1");
        toast_show("Update check timed out");
        need_redraw = 1;
    }
}

/* Shared by painting and hit testing so the Update tab cannot drift. */
typedef struct {
    int cx, cw;                        /* card left edge / width (inset by 10) */
    int hdr_y, hdr_h;
    int st_y, st_h;
    int bar_y, bar_h;
    int chk_x, chk_y, chk_w, chk_h;
    int app_x, app_y, app_w, app_h;
    int note_y;
} update_layout_t;

static void update_layout(int cx, int cw, int content_y, update_layout_t *L) {
    L->cx     = cx + 10;  L->cw     = cw - 20;
    L->hdr_y  = content_y + 6;   L->hdr_h  = 56;
    /* st_h leaves room for four 18px text lines without touching the bar label. */
    L->st_y   = content_y + 70;  L->st_h   = 94;
    L->bar_y  = content_y + 194; L->bar_h  = 12;
    L->chk_x  = cx + 10;  L->chk_y  = content_y + 226; L->chk_w = 190; L->chk_h = 32;
    L->app_x  = cx + 210; L->app_y  = content_y + 226; L->app_w = 170; L->app_h = 32;
    L->note_y = content_y + 268;
}

/* Tab geometry, shared by painting and hit testing so the 4 tabs cannot drift. */
static int settings_tab_w(int cw) { return (cw - 20) / SETTINGS_TAB_COUNT; }
static int settings_tab_x(int cx, int cw, int i) {
    return cx + 4 + i * (settings_tab_w(cw) + 4);
}
static const char *settings_tab_label(int i) {
    if (i == SETTINGS_VIEW_THEMES) return "Themes";
    if (i == SETTINGS_VIEW_SOUND)  return "Sound";
    if (i == SETTINGS_VIEW_SYSTEM) return "System";
    return "Update";
}

/* ===== Settings App ===== */
static void draw_settings_client(window_t *win) {
    int cx = win->x + 8;
    int cy = win->y + TITLEBAR_H + 4;
    int cw = win->w - 16;
    int ch = win->h - TITLEBAR_H - 12;

    /* Tab bar: Themes / Sound / System / Update */
    fill_rounded_rect(cx, cy, cw, 34, 6, COLOR_SURFACE2);
    int tab_w = settings_tab_w(cw);
    for (int i = 0; i < SETTINGS_TAB_COUNT; i++) {
        int tx = settings_tab_x(cx, cw, i);
        int sel = (settings_view == i);
        fill_rounded_rect(tx, cy + 4, tab_w, 26, 4, sel ? COLOR_ACCENT : COLOR_SURFACE);
        const char *lbl = settings_tab_label(i);
        int lw = (int)strlen(lbl) * 8;
        draw_string(tx + (tab_w - lw) / 2, cy + 9, lbl, sel ? C_BLACK : COLOR_WHITE);
    }

    int content_y = cy + 40;
    int content_h = ch - 40;

    if (settings_view == SETTINGS_VIEW_THEMES) {
        draw_string(cx + 10, content_y, "Select Theme:", COLOR_WHITE);
        int card_w = (cw - 40) / 3;
        int card_h = 60;
        for (int i = 0; i < THEME_COUNT; i++) {
            int row = i / 3;
            int col = i % 3;
            int tx = cx + 10 + col * (card_w + 8);
            int ty = content_y + 22 + row * (card_h + 8);
            if (ty + card_h > content_y + content_h) break;
            int is_sel = (current_theme == i);
            fill_rounded_rect(tx, ty, card_w, card_h, 6,
                is_sel ? COLOR_ACCENT : COLOR_SURFACE2);
            if (is_sel) draw_line(tx, ty, tx + card_w, ty, COLOR_WHITE);
            draw_string(tx + 8, ty + 8, theme_names[i], is_sel ? C_BLACK : COLOR_WHITE);
            /* Color preview swatches */
            /* We need to show the theme colors without applying them.
               Use static preview colors per theme. */
            uint32_t swatches[5][3] = {
                {0xFF1C2233, 0xFF3D4866, 0xFF8FA4C4}, /* Midnight */
                {0xFF2D1810, 0xFF6B3522, 0xFFE8985C}, /* Sunset */
                {0xFF1B2E26, 0xFF2E5D44, 0xFF6BBF8A}, /* Forest */
                {0xFF0D2E44, 0xFF1E5A7E, 0xFF4FC3F7}, /* Ocean */
                {0xFF2D1424, 0xFF6B224E, 0xFFF06292}, /* Rose */
            };
            for (int s = 0; s < 3; s++) {
                fill_rounded_rect(tx + 8 + s * 18, ty + 30, 14, 14, 3, swatches[i][s]);
            }
        }
    } else if (settings_view == SETTINGS_VIEW_SOUND) {
        draw_string(cx + 10, content_y, "Sound Settings:", COLOR_WHITE);
        /* Volume */
        draw_string(cx + 10, content_y + 24, "Volume:", COLOR_WHITE);
        char vol_str[8];
        itoa(sound_volume, vol_str, 10);
        uint32_t vlen = strlen(vol_str);
        vol_str[vlen] = '%'; vol_str[vlen + 1] = '\0';
        draw_string(cx + cw - 50, content_y + 24, sound_muted ? "0%" : vol_str, COLOR_ACCENT);
        int bar_x = cx + 10, bar_y = content_y + 44, bar_w = cw - 20, bar_h = 10;
        fill_rounded_rect(bar_x, bar_y, bar_w, bar_h, 4, COLOR_SURFACE2);
        int fill_w = sound_muted ? 0 : (bar_w * sound_volume) / 100;
        if (fill_w > 0) fill_rounded_rect(bar_x, bar_y, fill_w, bar_h, 4, COLOR_ACCENT);

        /* Mute toggle */
        fill_rounded_rect(cx + 10, content_y + 68, 100, 26, 4,
            sound_muted ? 0xFFEB6F92 : COLOR_SURFACE2);
        draw_string(cx + 32, content_y + 74, sound_muted ? "Unmute" : "Mute", COLOR_WHITE);

        /* UI sound effects toggle */
        fill_rounded_rect(cx + 10, content_y + 100, 180, 26, 4,
            sound_effects_enabled ? COLOR_ACCENT : COLOR_SURFACE2);
        draw_string(cx + 24, content_y + 106,
            sound_effects_enabled ? "Click Sounds: ON" : "Click Sounds: OFF",
            sound_effects_enabled ? C_BLACK : COLOR_WHITE);
    } else if (settings_view == SETTINGS_VIEW_SYSTEM) {
        /* System info */
        draw_string(cx + 10, content_y, "System Information:", COLOR_WHITE);
        char buf[64], num[16];
        int y = content_y + 22;
        strcpy_c(buf, "OS: Shkodya OS Core v1.9");
        draw_string(cx + 10, y, buf, COLOR_MUTED); y += 18;
        strcpy_c(buf, "Kernel: i686 Freestanding");
        draw_string(cx + 10, y, buf, COLOR_MUTED); y += 18;
        strcpy_c(buf, "Display: ");
        itoa((int32_t)fb_width, num, 10); strcat_c(buf, num);
        strcat_c(buf, "x");
        itoa((int32_t)fb_height, num, 10); strcat_c(buf, num);
        draw_string(cx + 10, y, buf, COLOR_MUTED); y += 18;
        strcpy_c(buf, "Theme: ");
        strcat_c(buf, theme_names[current_theme]);
        draw_string(cx + 10, y, buf, COLOR_MUTED); y += 18;
        strcat_c(buf, num);
        draw_string(cx + 10, y, buf, COLOR_MUTED); y += 18;
        if (g_mbi) {
            strcpy_c(buf, "RAM Lower: ");
            itoa((int32_t)g_mbi->mem_lower, num, 10);
            strcat_c(buf, num); strcat_c(buf, " KB");
            draw_string(cx + 10, y, buf, COLOR_MUTED); y += 18;
            strcpy_c(buf, "RAM Upper: ");
            itoa((int32_t)g_mbi->mem_upper, num, 10);
            strcat_c(buf, num); strcat_c(buf, " KB");
            draw_string(cx + 10, y, buf, COLOR_MUTED); y += 18;
        }
        strcpy_c(buf, "Uptime (s): ");
        uint32_t sec = system_ticks / PIT_FREQ;
        itoa((int32_t)sec, num, 10);
        strcat_c(buf, num);
        draw_string(cx + 10, y, buf, COLOR_MUTED);
    } else if (settings_view == SETTINGS_VIEW_UPDATE) {
        update_layout_t U;
        update_layout(cx, cw, content_y, &U);

        /* --- Header card: installed version + COM1 link state --- */
        fill_rounded_rect(U.cx, U.hdr_y, U.cw, U.hdr_h, 8, COLOR_SURFACE2);
        draw_string(U.cx + 12, U.hdr_y + 10, "Current Version", COLOR_MUTED);
        draw_string(U.cx + 12, U.hdr_y + 28, UPDATE_CURRENT_VERSION, COLOR_WHITE);
        {
            /* right-aligned so the label can never overrun the card */
            int lw = (int)strlen("Serial bridge COM1") * 8;
            draw_string(U.cx + U.cw - lw - 8, U.hdr_y + 10, "Serial bridge COM1", COLOR_MUTED);
            int px = U.cx + U.cw - 92 - 8;
            fill_rounded_rect(px, U.hdr_y + 28, 92, 18, 5,
                              serial_online ? 0xFF065F46 : 0xFF5A2020);
            fill_rounded_rect(px + 6, U.hdr_y + 34, 6, 6, 3,
                              serial_online ? 0xFF34D399 : 0xFFE47070);
            draw_string(px + 18, U.hdr_y + 29, serial_online ? "linked" : "absent",
                        0xFFD1FAE5);
        }

        /* --- Status card: state machine readout --- */
        fill_rounded_rect(U.cx, U.st_y, U.cw, U.st_h, 8, COLOR_SURFACE2);
        {
            uint32_t dot = (update_state == UPDATE_READY)    ? 0xFF34D399
                         : (update_state == UPDATE_ERROR)    ? 0xFFE47070
                         : (update_state == UPDATE_AVAILABLE ||
                            update_state == UPDATE_DOWNLOADING) ? 0xFFE8B84B
                         : (update_state == UPDATE_CHECKING) ? 0xFF6FA8DC
                         : 0xFF9AA6B5;
            fill_rounded_rect(U.cx + 12, U.st_y + 14, 8, 8, 4, dot);
        }
        draw_string(U.cx + 28, U.st_y + 12, update_state_text(), COLOR_WHITE);

        char line[96];
        if (update_state == UPDATE_AVAILABLE || update_state == UPDATE_DOWNLOADING ||
            update_state == UPDATE_READY) {
            strcpy_c(line, "New version: ");
            strcat_c(line, update_version);
        } else if (update_state == UPDATE_ERROR) {
            strcpy_c(line, update_changelog);
        } else if (update_state == UPDATE_LATEST) {
            strcpy_c(line, "Installed build is the newest on the channel");
        } else if (update_state == UPDATE_CHECKING) {
            /* animated ellipsis so the tab is visibly alive while we wait */
            strcpy_c(line, "Waiting for RESP:");
            uint32_t d = 1 + (system_ticks / 25) % 3;
            for (uint32_t i = 0; i < d; i++) strcat_c(line, ".");
        } else {
            strcpy_c(line, "Press Check for Updates to query the host bridge");
        }
        draw_string(U.cx + 12, U.st_y + 32, line, COLOR_MUTED);

        if (update_changelog[0] != '\0' && update_state != UPDATE_ERROR) {
            char clog[64];                       /* truncated to fit the card */
            strcpy_c(clog, "Changelog: ");
            uint32_t used = strlen(clog), i = 0;
            while (update_changelog[i] != '\0' && used + 3 < sizeof(clog)) {
                clog[used++] = update_changelog[i++];
            }
            if (update_changelog[i] != '\0') { clog[used++] = '.'; clog[used++] = '.'; clog[used++] = '.'; }
            clog[used] = '\0';
            draw_string(U.cx + 12, U.st_y + 52, clog, COLOR_MUTED);
        }
        {
            char rx[48] = "RESP packets: ";
            append_int(rx, serial_lines);
            draw_string(U.cx + 12, U.st_y + 70, rx, 0xFF6E7681);
        }

        /* --- Progress bar, driven by RESP:PROGRESS --- */
        draw_string(U.cx, U.bar_y - 18, "Download progress", COLOR_MUTED);
        {
            char pct[8];
            strcpy_c(pct, "");
            append_int(pct, update_progress);
            strcat_c(pct, "%");
            int pw = (int)strlen(pct) * 8;
            draw_string(U.cx + U.cw - pw, U.bar_y - 18, pct, COLOR_WHITE);
        }
        {
            fill_rounded_rect(U.cx, U.bar_y, U.cw, U.bar_h, 5, COLOR_SURFACE2);
            int fill = (U.cw * update_progress) / 100;
            if (fill > 0) {
                uint32_t bc = (update_state == UPDATE_READY) ? 0xFF34D399
                            : (update_state == UPDATE_ERROR) ? 0xFFE47070
                            : COLOR_ACCENT;
                fill_rounded_rect(U.cx, U.bar_y, fill, U.bar_h, 5, bc);
            }
        }

        /* --- Actions --- */
        {
            int busy = update_busy();
            fill_rounded_rect(U.chk_x, U.chk_y, U.chk_w, U.chk_h, 8,
                              busy ? COLOR_SURFACE2 : COLOR_CHROME_HI);
            const char *lbl = busy ? "Checking..." : "Check for Updates";
            int lw = (int)strlen(lbl) * 8;
            draw_string(U.chk_x + (U.chk_w - lw) / 2, U.chk_y + 8, lbl,
                        busy ? COLOR_MUTED : COLOR_WHITE);
        }
        {
            int ready = (update_state == UPDATE_READY);
            fill_rounded_rect(U.app_x, U.app_y, U.app_w, U.app_h, 8,
                              ready ? 0xFF2F6B45 : COLOR_SURFACE2);
            int lw = (int)strlen("Restart & Apply") * 8;
            draw_string(U.app_x + (U.app_w - lw) / 2, U.app_y + 8, "Restart & Apply",
                        ready ? COLOR_WHITE : 0xFF5C636E);
        }
        {
            const char *note = "Restart & Apply unlocks when the host sends RESP:READY.";
            uint32_t col = COLOR_MUTED;
            if (update_state == UPDATE_READY) {
                note = "Update staged - rebooting applies it on next boot.";
                col = 0xFF8FD9B0;
            } else if (update_state == UPDATE_DOWNLOADING && update_progress >= 100) {
                note = "Download complete - waiting for RESP:READY.";
            } else if (update_state == UPDATE_ERROR) {
                note = update_changelog;
                col = 0xFFE47070;
            }
            draw_string(U.cx, U.note_y, note, col);
        }
    }
}

/* Fluent UI Window Frame — shadowless, rounded, with subtle outline */
/* ===== Shkodya Web (APP_BROWSER) =====
 *
 * A deliberately small browser. A page is plain text with five markup forms:
 *      "# "  heading          "## " sub-heading      "- "  bullet
 *      "[field]text[/field]"  search field          "[label](url)" inline link
 * Everything else is a wrapped paragraph.
 *
 * "shkodya://" pages are built locally so the app is useful with no bridge
 * attached at all. Any other URL is requested as "CMD:WEB_GET|<url>" and
 * assembled from RESP:WEB_META / RESP:WEB_DATA / RESP:WEB_END packets.
 */

#define WEB_BODY_CAP   4096
#define WEB_URL_CAP    96
#define WEB_TITLE_CAP  48
#define WEB_MAX_HIST   6
#define WEB_MAX_LINKS  24
#define WEB_LINE_MAX   200

typedef struct {
    char url[WEB_URL_CAP];
    char title[WEB_TITLE_CAP];
    char body[WEB_BODY_CAP];
    int  scroll;
    int  error;
} web_page_t;

static web_page_t web_hist[WEB_MAX_HIST];
static int web_hist_len = 0;
static int web_hist_pos = -1;

static int  web_input_active = 0;
static char web_input[WEB_URL_CAP];
static int  web_input_len = 0;

static int      web_pending = 0;
static uint32_t web_pending_tick = 0;

/* Assembly buffers for an in-flight fetch. */
static char     web_rx_body[WEB_BODY_CAP];
static uint32_t web_rx_len = 0;
static uint32_t web_rx_total = 0;
static char     web_rx_title[WEB_TITLE_CAP];
static char     web_rx_url[WEB_URL_CAP];   /* canonical URL, post-redirect */

static int web_links_n = 0;
static struct { int x, y, w, h; char url[WEB_URL_CAP]; } web_links[WEB_MAX_LINKS];

#define WEB_FOCUS_SENTINEL "@focus-url"

typedef struct {
    int nav_x, nav_y, nav_w, nav_h;
    int back_x, back_w, fwd_x, fwd_w, ref_x, ref_w;
    int url_x, url_y, url_w, url_h, go_x, go_w;
    int view_x, view_y, view_w, view_h;
    int status_x, status_y, status_w, status_h;
} web_layout_t;

static void web_get_layout(window_t *win, web_layout_t *L) {
    int pad = 8;
    L->nav_x = win->x + pad;
    L->nav_y = win->y + TITLEBAR_H + 4;
    L->nav_w = win->w - pad * 2;
    L->nav_h = 40;
    L->url_h = 26;
    L->url_y = L->nav_y + 7;

    L->back_w = 32; L->fwd_w = 32; L->ref_w = 68; L->go_w = 32;
    L->back_x = L->nav_x + 8;
    L->fwd_x  = L->back_x + L->back_w + 6;
    L->ref_x  = L->fwd_x + L->fwd_w + 6;
    L->go_x   = L->nav_x + L->nav_w - 8 - L->go_w;
    L->url_x  = L->ref_x + L->ref_w + 8;
    L->url_w  = L->go_x - 8 - L->url_x;
    if (L->url_w < 80) L->url_w = 80;

    L->status_h = 20;
    L->status_x = L->nav_x;
    L->status_w = L->nav_w;
    L->status_y = win->y + win->h - 10 - L->status_h;

    L->view_x = L->nav_x;
    L->view_y = L->nav_y + L->nav_h + 6;
    L->view_w = L->nav_w;
    L->view_h = L->status_y - 6 - L->view_y;
    if (L->view_h < 40) L->view_h = 40;
}

static web_page_t *web_current(void) {
    if (web_hist_pos < 0 || web_hist_pos >= web_hist_len) return (web_page_t *)0;
    return &web_hist[web_hist_pos];
}

/* --- local pages --------------------------------------------------------- */

static void web_page_reset(web_page_t *pg, const char *url, const char *title) {
    strcpy_c(pg->url, url);
    strcpy_c(pg->title, title);
    pg->body[0] = '\0';
    pg->scroll = 0;
    pg->error = 0;
}

static void web_add_count(web_page_t *pg, const char *label, int32_t value,
                          const char *suffix) {
    char num[16];
    strcat_c(pg->body, label);
    itoa(value, num, 10);
    strcat_c(pg->body, num);
    if (suffix) strcat_c(pg->body, suffix);
    strcat_c(pg->body, "\n");
}

static void web_build_home(web_page_t *pg) {
    int files = 0;
    web_page_reset(pg, "shkodya://home", "Shkodya Web");
    strcat_c(pg->body, "# Shkodya Web\n");
    strcat_c(pg->body, "[field]Search the web or type a URL[/field]\n");
    strcat_c(pg->body, "## Quick links\n");
    strcat_c(pg->body, "- [Shkodya Store](shkodya://store) install apps over COM1\n");
    strcat_c(pg->body, "- [Docs](shkodya://docs) shell and system reference\n");
    strcat_c(pg->body, "- [Cat Wiki](shkodya://catwiki) everything about Rikki\n");
    strcat_c(pg->body, "## System status\n");
    web_add_count(pg, "Uptime: ", (int32_t)(system_ticks / PIT_FREQ), " s");
    {
        char num[16];
        strcat_c(pg->body, "Display: ");
        itoa((int32_t)fb_width, num, 10);
        strcat_c(pg->body, num);
        strcat_c(pg->body, " x ");
        itoa((int32_t)fb_height, num, 10);
        strcat_c(pg->body, num);
        strcat_c(pg->body, " 32bpp\n");
    }
    for (int i = 0; i < VFS_MAX_FILES; i++) if (vfs_files[i].used) files++;
    web_add_count(pg, "ShkodyaFS files: ", files, "");
    strcat_c(pg->body, serial_online ? "COM1 bridge: linked\n" : "COM1 bridge: absent\n");
    strcat_c(pg->body, "! Local page - no network round trip needed\n");
}

static void web_build_store(web_page_t *pg) {
    web_page_reset(pg, "shkodya://store", "Shkodya Store");
    strcat_c(pg->body, "# Shkodya Store\n");
    strcat_c(pg->body, "Packages are fetched by the shell over the COM1 bridge.\n");
    strcat_c(pg->body, "## Installable\n");
    strcat_c(pg->body, "- doom 1.0 - 320x200 EGA shooter\n");
    strcat_c(pg->body, "- tetris 1.2 - falling-block puzzler\n");
    strcat_c(pg->body, "- calc_pro 2.0 - scientific calculator\n");
    strcat_c(pg->body, "## How to install\n");
    strcat_c(pg->body, "Open the Terminal and run:\n");
    strcat_c(pg->body, "- shkodya search doom\n");
    strcat_c(pg->body, "- shkodya install doom\n");
    strcat_c(pg->body, "- shkodya list\n");
    strcat_c(pg->body, "Back to [home](shkodya://home).\n");
}

static void web_build_docs(web_page_t *pg) {
    web_page_reset(pg, "shkodya://docs", "Docs");
    strcat_c(pg->body, "# Documentation\n");
    strcat_c(pg->body, "## Shell\n");
    strcat_c(pg->body, "- ls, cat, touch, clear, help\n");
    strcat_c(pg->body, "- shkodya search|install|list\n");
    strcat_c(pg->body, "- Up and Down arrows walk the command history\n");
    strcat_c(pg->body, "## Serial bridge\n");
    strcat_c(pg->body, "COM1 lives at 0x3F8 and is polled, never interrupt driven.\n");
    strcat_c(pg->body, "- CMD:CHECK for OS updates\n");
    strcat_c(pg->body, "- CMD:PKG_GET for store packages\n");
    strcat_c(pg->body, "- CMD:WEB_GET for this browser\n");
    strcat_c(pg->body, "## Desktop\n");
    strcat_c(pg->body, "Icons can be dragged anywhere on the wallpaper and snap back\n");
    strcat_c(pg->body, "to the grid when dropped near a slot.\n");
    strcat_c(pg->body, "Back to [home](shkodya://home).\n");
}

static void web_build_catwiki(web_page_t *pg) {
    web_page_reset(pg, "shkodya://catwiki", "Cat Wiki");
    strcat_c(pg->body, "# Cat Wiki\n");
    strcat_c(pg->body, "## Rikki\n");
    strcat_c(pg->body, "The desktop assistant. Open the Rikki Assistant window and\n");
    strcat_c(pg->body, "press Pet Rikki for a PC-speaker chirp and a purr.\n");
    strcat_c(pg->body, "## Known facts\n");
    strcat_c(pg->body, "- orange tabby, 16x16 sprite at scale 5\n");
    strcat_c(pg->body, "- purrs between roughly 25 Hz and 40 Hz\n");
    strcat_c(pg->body, "- happiness is tracked in PIT ticks\n");
    strcat_c(pg->body, "Back to [home](shkodya://home).\n");
}

/* Dispatches the built-in pages; anything unrecognised falls back to home. */
static void web_build_local(web_page_t *pg, const char *url) {
    if (strcmp_c(url, "shkodya://store") == 0)        web_build_store(pg);
    else if (strcmp_c(url, "shkodya://docs") == 0)    web_build_docs(pg);
    else if (strcmp_c(url, "shkodya://catwiki") == 0) web_build_catwiki(pg);
    else                                              web_build_home(pg);
}

/* --- markup renderer ----------------------------------------------------- */

static void web_text(const web_layout_t *L, int x, int y, const char *s,
                     uint32_t col, int do_draw) {
    if (!do_draw) return;
    if (y < L->view_y || y + 16 > L->view_y + L->view_h) return;   /* vertical clip */
    draw_string(x, y, s, col);
}

/* Emits a whitespace-separated run of plain words with wrapping. */
static int web_words(const web_layout_t *L, int cx, int *yp, int x, int limit,
                     const char *text, uint32_t col, int do_draw) {
    const char *p = text;
    while (*p) {
        char word[WEB_LINE_MAX];
        int n = 0;
        while (*p == ' ') p++;
        if (!*p) break;
        while (*p && *p != ' ' && n < WEB_LINE_MAX - 1) word[n++] = *p++;
        word[n] = '\0';
        {
            int w = n * 8;
            if (cx + w > limit) { cx = x; (*yp) += 20; }
            web_text(L, cx, *yp, word, col, do_draw);
            cx += w + 8;
        }
    }
    return cx;
}

/* Wrapped text run with inline [label](url) links. Returns the next y.
   Links are located before word wrapping, because a label may contain spaces
   ("[Shkodya Store](...)") and would otherwise be split apart and printed raw.
   do_draw == 0 only refreshes web_links[], which the click handler needs before
   the first paint of a fresh page. */
static int web_run(const web_layout_t *L, int x, int y, const char *text,
                   int wrap, uint32_t col, int do_draw) {
    int cx = x;
    int limit = x + wrap * 8;
    const char *p = text;

    while (*p) {
        const char *lb = p;
        while (*lb && *lb != '[') lb++;

        if (lb > p) {                       /* plain text before the next link */
            char chunk[WEB_LINE_MAX];
            int n = (int)(lb - p);
            if (n > WEB_LINE_MAX - 1) n = WEB_LINE_MAX - 1;
            for (int i = 0; i < n; i++) chunk[i] = p[i];
            chunk[n] = '\0';
            cx = web_words(L, cx, &y, x, limit, chunk, col, do_draw);
            p = lb;
            if (!*p) break;
        }

        {
            const char *rb = p + 1;
            while (*rb && *rb != ']') rb++;
            if (*rb == ']' && rb[1] == '(') {
                const char *rp = rb + 2;
                while (*rp && *rp != ')') rp++;
                if (*rp == ')') {
                    int ll = (int)(rb - (p + 1));
                    int ul = (int)(rp - (rb + 2));
                    if (ll > 0 && ll < 63 && ul > 0 && ul < WEB_URL_CAP) {
                        char label[64], url[WEB_URL_CAP];
                        for (int i = 0; i < ll; i++) label[i] = p[1 + i];
                        label[ll] = '\0';
                        for (int i = 0; i < ul; i++) url[i] = rb[2 + i];
                        url[ul] = '\0';
                        {
                            int w = ll * 8;
                            if (cx + w > limit) { cx = x; y += 20; }
                            if (web_links_n < WEB_MAX_LINKS) {
                                web_links[web_links_n].x = cx;
                                web_links[web_links_n].y = y;
                                web_links[web_links_n].w = w;
                                web_links[web_links_n].h = 18;
                                strcpy_c(web_links[web_links_n].url, url);
                                web_links_n++;
                            }
                            if (do_draw && y >= L->view_y && y + 16 <= L->view_y + L->view_h) {
                                draw_string(cx, y, label, 0xFF6FA8DC);
                                draw_line(cx, y + 17, cx + w - 1, y + 17, 0xFF6FA8DC);
                            }
                            cx += w + 8;
                        }
                        p = rp + 1;
                        continue;
                    }
                }
            }
        }

        /* A stray '[': print it literally and carry on. */
        {
            char one[2];
            one[0] = *p; one[1] = '\0';
            if (cx + 8 > limit) { cx = x; y += 20; }
            web_text(L, cx, y, one, col, do_draw);
            cx += 16;
            p++;
        }
    }
    return y + 20;
}

/* Walks the body and paints it. Returns the total content height (measured
   from the first line down to the last), used to clamp the scroll. */
static int web_render(const web_page_t *pg, const web_layout_t *L, int do_draw) {
    int x0 = L->view_x + 16;
    int wrap = (L->view_w - 32) / 8;
    int top = L->view_y + 12;
    int y = top - pg->scroll;
    const char *s = pg->body;
    char line[WEB_LINE_MAX];

    web_links_n = 0;

    if (do_draw)
        fill_rounded_rect(L->view_x, L->view_y, L->view_w, L->view_h, 8, COLOR_SURFACE);

    if (pg->error) {
        web_text(L, x0, y, "Could not load this page.", 0xFFE47070, do_draw);
        y += 24;
        web_text(L, x0, y, pg->body, COLOR_MUTED, do_draw);
        return (y - top) + pg->scroll + 40;
    }

    while (*s) {
        int n = 0;
        while (*s && *s != '\n' && n < WEB_LINE_MAX - 1) line[n++] = *s++;
        if (*s == '\n') s++;
        line[n] = '\0';

        if (n == 0) { y += 8; continue; }

        if (line[0] == '#' && line[1] == ' ' && line[2] != '#') {
            y += 6;
            web_text(L, x0, y, line + 2, 0xFF8DCBFA, do_draw);
            if (do_draw && y >= L->view_y && y + 16 <= L->view_y + L->view_h)
                draw_line(x0, y + 21, x0 + (int)strlen(line + 2) * 8, y + 21, 0xFF3A4A63);
            y += 32;
        } else if (line[0] == '#' && line[1] == '#' && line[2] == ' ') {
            y += 4;
            web_text(L, x0, y, line + 3, COLOR_WHITE, do_draw);
            y += 24;
        } else if (line[0] == '-' && line[1] == ' ') {
            if (do_draw && y >= L->view_y && y + 16 <= L->view_y + L->view_h)
                fill_rounded_rect(x0 + 2, y + 6, 5, 5, 2, 0xFF8DCBFA);
            y = web_run(L, x0 + 16, y, line + 2, wrap - 2, 0xFFC9CED8, do_draw);
        } else if (line[0] == '!' && line[1] == ' ') {
            web_text(L, x0, y, line + 2, 0xFF6E7681, do_draw);
            y += 22;
        } else if (strncmp_c(line, "[field]", 7) == 0) {
            char ftext[80];
            int fi = 0, fw = wrap * 8, fh = 26;
            for (int k = 7; line[k] && line[k] != '[' && fi < 79; k++) ftext[fi++] = line[k];
            ftext[fi] = '\0';
            if (web_links_n < WEB_MAX_LINKS) {
                web_links[web_links_n].x = x0;
                web_links[web_links_n].y = y;
                web_links[web_links_n].w = fw;
                web_links[web_links_n].h = fh;
                strcpy_c(web_links[web_links_n].url, WEB_FOCUS_SENTINEL);
                web_links_n++;
            }
            if (do_draw && y >= L->view_y && y + fh <= L->view_y + L->view_h) {
                fill_rounded_rect_aa(x0, y, fw, fh, 8, COLOR_BORDER);
                fill_rounded_rect(x0 + 1, y + 1, fw - 2, fh - 2, 7, COLOR_FIELD);
                draw_string(x0 + 12, y + 5, ftext, 0xFF6E7681);
            }
            y += fh + 12;
        } else {
            y = web_run(L, x0, y, line, wrap, 0xFFC9CED8, do_draw);
        }
    }
    return (y - top) + pg->scroll + 40;
}

/* --- navigation ---------------------------------------------------------- */

static void web_push(web_page_t *pg) {
    if (web_hist_pos + 1 < web_hist_len) web_hist_len = web_hist_pos + 1;  /* drop fwd tail */
    if (web_hist_len >= WEB_MAX_HIST) {
        for (int i = 1; i < WEB_MAX_HIST; i++) web_hist[i - 1] = web_hist[i];
        web_hist_len = WEB_MAX_HIST - 1;
    }
    web_hist[web_hist_len] = *pg;
    web_hist_pos = web_hist_len;
    web_hist_len++;
}

static void web_fetch(const char *url) {
    char cmd[160] = "CMD:WEB_GET|";
    uint32_t o = strlen(cmd);
    for (uint32_t i = 0; url[i] && o + 2 < sizeof(cmd); i++) {
        char c = url[i];
        if (c == '|' || c == '\n' || c == '\r') c = '_';
        cmd[o++] = c;
        cmd[o] = '\0';
    }
    strcat_c(cmd, "\n");
    serial_send_str(cmd);
    web_pending = 1;
    web_pending_tick = system_ticks;
    toast_show("Fetching over COM1");
}

static void web_navigate(const char *url) {
    web_page_t pg;
    if (url[0] == '\0') return;

    if (strncmp_c(url, "shkodya://", 10) == 0) {
        web_build_local(&pg, url);
        web_push(&pg);
        toast_show("Loaded local page");
        need_redraw = 1;
        return;
    }

    /* Anything without a scheme and without a dot is treated as a search. */
    {
        int has_scheme = 0, has_dot = 0;
        for (int i = 0; url[i]; i++) {
            if (url[i] == ':' && url[i + 1] == '/' && url[i + 2] == '/') has_scheme = 1;
            if (url[i] == '.') has_dot = 1;
        }
        web_page_reset(&pg, url, url);
        strcat_c(pg.body, "# Loading\n\nWaiting for the COM1 bridge to answer...\n");
        web_push(&pg);
        if (!has_scheme && !has_dot) {
            char search[WEB_URL_CAP + 8] = "search:";
            strcat_c(search, url);
            web_fetch(search);
        } else {
            web_fetch(url);
        }
    }
    need_redraw = 1;
}

static void web_back(void) {
    if (web_hist_pos > 0) { web_hist_pos--; need_redraw = 1; }
}

static void web_forward(void) {
    if (web_hist_pos + 1 < web_hist_len) { web_hist_pos++; need_redraw = 1; }
}

static void web_refresh(void) {
    web_page_t *pg = web_current();
    if (!pg) return;
    if (strncmp_c(pg->url, "shkodya://", 10) == 0) {
        web_page_t tmp;
        char url[WEB_URL_CAP];
        strcpy_c(url, pg->url);
        web_build_local(&tmp, url);
        *pg = tmp;                         /* rebuilt in place: no new history row */
        toast_show("Page reloaded");
    } else {
        web_fetch(pg->url);
    }
    need_redraw = 1;
}

/* RESP:WEB_* assembly, called from serial_handle_line(). Lives here rather than
   in the UART section so it can use the page types and buffers directly. */
static void web_handle_packet(const char *line) {
    if (strncmp_c(line, "RESP:WEB_META|", 14) == 0) {
        char url[WEB_URL_CAP];
        char title[WEB_TITLE_CAP];
        char total[16];
        proto_field(line, 1, url, sizeof(url));
        proto_field(line, 2, title, sizeof(title));
        proto_field(line, 3, total, sizeof(total));
        strcpy_c(web_rx_url, url);
        strcpy_c(web_rx_title, title);
        web_rx_len = 0;
        web_rx_total = (uint32_t)proto_atoi(total);
        web_rx_body[0] = '\0';
    } else if (strncmp_c(line, "RESP:WEB_DATA|", 14) == 0) {
        /* Field 1 is the payload itself; base64-armoured so newlines and '|'
           inside a page cannot break the line protocol. */
        char chunk[PKG_CHUNK_CAP];
        proto_field(line, 1, chunk, sizeof(chunk));
        b64_decode_into(chunk, (uint8_t *)web_rx_body, &web_rx_len, WEB_BODY_CAP - 1);
        web_rx_body[web_rx_len] = '\0';
    } else if (strncmp_c(line, "RESP:WEB_END", 12) == 0) {
        web_page_t *pg;
        web_pending = 0;
        web_rx_body[web_rx_len] = '\0';
        pg = web_current();
        if (pg) {
            if (web_rx_url[0]) strcpy_c(pg->url, web_rx_url);   /* follow redirects */
            strcpy_c(pg->title, web_rx_title[0] ? web_rx_title : pg->url);
            strcpy_c(pg->body, web_rx_body);
            pg->scroll = 0;
            pg->error = 0;
            toast_show("Page loaded over COM1");
        }
        need_redraw = 1;
    } else if (strncmp_c(line, "RESP:WEB_ERROR|", 15) == 0) {
        char reason[96];
        web_page_t *pg;
        web_pending = 0;
        proto_field(line, 1, reason, sizeof(reason));
        pg = web_current();
        if (pg) {
            pg->error = 1;
            strcpy_c(pg->body, reason[0] ? reason : "the host bridge could not fetch it");
        }
        toast_show("Page fetch failed");
        need_redraw = 1;
    }
}

/* The browser always has something to show: the home page is loaded lazily the
   first time the window is painted, so opening it never lands on a blank view. */
static void web_ensure_home(void) {
    if (web_hist_len > 0) return;
    {
        web_page_t pg;
        web_build_home(&pg);
        web_push(&pg);
        need_redraw = 1;
    }
}

/* Gives up on a silent bridge so the browser never spins on "Loading". */
static void web_tick(void) {
    if (!web_pending) return;
    if (system_ticks - web_pending_tick > PIT_FREQ * 12) {
        web_page_t *pg = web_current();
        web_pending = 0;
        if (pg) {
            pg->error = 1;
            strcpy_c(pg->body, "No reply from the COM1 bridge");
        }
        toast_show("Page fetch timed out");
        need_redraw = 1;
    }
}

/* --- input --------------------------------------------------------------- */

static void web_key(char ch) {
    web_page_t *pg = web_current();
    if (ch == TERM_KEY_UP) {
        if (pg && pg->scroll > 0) {
            pg->scroll -= 28;
            if (pg->scroll < 0) pg->scroll = 0;
            need_redraw = 1;
        }
        return;
    }
    if (ch == TERM_KEY_DOWN) {
        if (pg) { pg->scroll += 28; need_redraw = 1; }
        return;
    }
    if (!web_input_active) return;

    if (ch == '\n') {
        char url[WEB_URL_CAP];
        web_input_active = 0;
        strcpy_c(url, web_input);
        if (url[0]) web_navigate(url);
        else need_redraw = 1;
        return;
    }
    if (ch == '\b') {
        if (web_input_len > 0) web_input[--web_input_len] = '\0';
        need_redraw = 1;
        return;
    }
    if (ch == 27) { web_input_active = 0; need_redraw = 1; return; }
    {
        unsigned char u = (unsigned char)ch;
        if (u >= 32 && u < 127 && web_input_len < WEB_URL_CAP - 1) {
            web_input[web_input_len++] = ch;
            web_input[web_input_len] = '\0';
            need_redraw = 1;
        }
    }
}

static void handle_browser_click(window_t *win, int px, int py) {
    web_layout_t L;
    web_page_t *pg;
    web_ensure_home();
    pg = web_current();
    web_get_layout(win, &L);
    if (!pg) return;

    if (point_in_rect(px, py, L.back_x, L.nav_y + 7, L.back_w, L.url_h)) { web_back(); return; }
    if (point_in_rect(px, py, L.fwd_x,  L.nav_y + 7, L.fwd_w,  L.url_h)) { web_forward(); return; }
    if (point_in_rect(px, py, L.ref_x,  L.nav_y + 7, L.ref_w,  L.url_h)) { web_refresh(); return; }
    if (point_in_rect(px, py, L.go_x,   L.nav_y + 7, L.go_w,   L.url_h)) {
        char url[WEB_URL_CAP];
        strcpy_c(url, web_input);
        web_input_active = 0;
        if (url[0]) web_navigate(url);
        return;
    }
    if (point_in_rect(px, py, L.url_x, L.url_y, L.url_w, L.url_h)) {
        web_input_active = 1;
        strcpy_c(web_input, pg->url);
        web_input_len = (int)strlen(web_input);
        toast_show("Type a URL, Enter to go");
        need_redraw = 1;
        return;
    }
    if (point_in_rect(px, py, L.view_x, L.view_y, L.view_w, L.view_h)) {
        web_render(pg, &L, 0);                 /* refresh the link boxes */
        for (int i = 0; i < web_links_n; i++) {
            if (!point_in_rect(px, py, web_links[i].x, web_links[i].y,
                               web_links[i].w, web_links[i].h)) continue;
            if (strcmp_c(web_links[i].url, WEB_FOCUS_SENTINEL) == 0) {
                web_input_active = 1;
                web_input[0] = '\0';
                web_input_len = 0;
                toast_show("Type to search, Enter to go");
            } else {
                char url[WEB_URL_CAP];
                strcpy_c(url, web_links[i].url);
                web_navigate(url);
            }
            need_redraw = 1;
            return;
        }
    }
}

/* --- painting ------------------------------------------------------------ */

static void web_nav_button(int x, int y, int w, int h, const char *label,
                           int enabled) {
    fill_rounded_rect_aa(x, y, w, h, 7, COLOR_BORDER);
    fill_rounded_rect(x + 1, y + 1, w - 2, h - 2, 6,
                      enabled ? COLOR_CHROME_HI : COLOR_SURFACE);
    {
        int lw = (int)strlen(label) * 8;
        draw_string(x + (w - lw) / 2, y + (h - 16) / 2, label,
                    enabled ? COLOR_WHITE : 0xFF5C636E);
    }
}

static void draw_browser_client(window_t *win) {
    web_layout_t L;
    web_page_t *pg;
    web_ensure_home();
    pg = web_current();
    web_get_layout(win, &L);

    /* ---- navigation bar ---- */
    fill_rounded_rect(L.nav_x, L.nav_y, L.nav_w, L.nav_h, 8, COLOR_SURFACE2);
    web_nav_button(L.back_x, L.nav_y + 7, L.back_w, L.url_h, "<", web_hist_pos > 0);
    web_nav_button(L.fwd_x,  L.nav_y + 7, L.fwd_w,  L.url_h, ">",
                   web_hist_pos + 1 < web_hist_len);
    web_nav_button(L.ref_x,  L.nav_y + 7, L.ref_w,  L.url_h, "Refresh", 1);
    web_nav_button(L.go_x,   L.nav_y + 7, L.go_w,   L.url_h, "Go", 1);

    /* ---- address bar ---- */
    fill_rounded_rect_aa(L.url_x, L.url_y, L.url_w, L.url_h, 7, COLOR_BORDER);
    fill_rounded_rect(L.url_x + 1, L.url_y + 1, L.url_w - 2, L.url_h - 2, 6,
                      web_input_active ? 0xFF101720 : COLOR_FIELD);
    {
        const char *shown = web_input_active ? web_input : (pg ? pg->url : "");
        int maxc = (L.url_w - 24) / 8;
        int len = (int)strlen(shown);
        const char *start = (len > maxc) ? shown + (len - maxc) : shown;
        draw_string(L.url_x + 12, L.url_y + 5, start,
                    web_input_active ? COLOR_WHITE : COLOR_MUTED);
        if (web_input_active) {
            int cx = L.url_x + 12 + (int)strlen(start) * 8;
            draw_line(cx, L.url_y + 4, cx, L.url_y + L.url_h - 4, COLOR_ACCENT);
        }
    }

    /* ---- content ---- */
    if (!pg) {
        fill_rounded_rect(L.view_x, L.view_y, L.view_w, L.view_h, 8, COLOR_SURFACE);
        draw_string(L.view_x + 16, L.view_y + 16, "No page loaded.", COLOR_MUTED);
    } else {
        int total = web_render(pg, &L, 0);          /* measure first ... */
        {
            int maxs = total - L.view_h;
            if (maxs < 0) maxs = 0;
            if (pg->scroll > maxs) pg->scroll = maxs;
            if (pg->scroll < 0) pg->scroll = 0;
        }
        web_render(pg, &L, 1);                      /* ... then paint */
    }

    /* ---- status strip ---- */
    fill_rounded_rect(L.status_x, L.status_y, L.status_w, L.status_h, 6, COLOR_SURFACE2);
    {
        char buf[160];
        uint32_t col = COLOR_MUTED;
        if (web_pending) {
            strcpy_c(buf, "Loading ");
            strcat_c(buf, pg ? pg->url : "");
            strcat_c(buf, " ...");
            col = 0xFFE8B84B;
        } else if (pg && pg->error) {
            strcpy_c(buf, "Error: ");
            strcat_c(buf, pg->body);
            col = 0xFFE47070;
        } else {
            strcpy_c(buf, pg ? pg->title : "");
            strcat_c(buf, "   |   ");
            strcat_c(buf, serial_online ? "COM1 linked" : "COM1 absent");
            strcat_c(buf, "   |   ");
            append_int(buf, web_links_n);
            strcat_c(buf, " links   |   scroll ");
            append_int(buf, pg ? pg->scroll : 0);
        }
        {
            int maxc = (L.status_w - 24) / 8;
            if ((int)strlen(buf) > maxc) buf[maxc] = '\0';
            draw_string(L.status_x + 12, L.status_y + 2, buf, col);
        }
    }
}

/* ===== Ai Chat (APP_AICHAT) =====
 *
 * A small assistant that lives entirely inside the image. Its "brain" is two
 * hashed bag-of-words retrievers trained offline by tools/train_ai.py - one
 * over a code Q&A corpus, one over a small-talk corpus - and shipped as
 * ai/ai_model.bin through ai_model.S.
 *
 * Everything here re-implements the trainer's vectorisation *exactly*: the same
 * byte-wise lowercasing, the same token boundaries, the same FNV-1a bucket, and
 * the same integer term-frequency table. That is not stylistic - the trained
 * weights only mean anything if a query is encoded the same way the corpus was,
 * so the two tokenisers have to agree byte for byte.
 *
 * The image has no FPU, so the arithmetic is integer end to end:
 *
 *     raw[b] += weight * TFW[tf]        for each query token
 *     vec[b]  = raw[b] * 127 / max(raw)
 *
 * which is precisely the trainer's max-normalise step. Scoring is a dot product
 * over the buckets plus an argmax, so an answer costs ~100k multiply-adds: a few
 * milliseconds, no allocation, no heap.
 *
 * It is honest about what it is. It retrieves the nearest corpus entry, it does
 * not reason. The accept thresholds below are a floor meaning "did anything in
 * the corpus match at all"; anything under them falls through to a canned line.
 */

#define AI_MAX_VOCAB  4096     /* upper bound the header is checked against */
#define AI_LOG_LINES  40
#define AI_LINE_LEN   96
#define AI_INPUT_MAX  64
/* A query sharing one vocabulary item with an entry already means something:
 * the trainer reports zero overlap between a correct query and every wrong row,
 * so any positive score is a real match and 0 is "nothing matched at all". */
#define AI_QA_MIN     1000
#define AI_CHAT_MIN   1000
/* Text rows: a 16px glyph cell plus 4px of leading. Without the leading the ink
 * of adjacent lines nearly touches, and because ascenders and descenders vary
 * from line to line the block reads as unevenly spaced. */
#define AI_LINE_PITCH 20
/* One text column for every line, user and assistant alike. */
#define AI_TEXT_X     38
#define AI_ROLE_USER  0
#define AI_ROLE_AI    1
#define AI_TEXT_COL   0xFFD8E4F0
#define AI_PROMPT_COL 0xFF8FD9B0
#define AI_USER_COL   0xFFFFFFFF
#define AI_HINT_COL   0xFF6E7681

/* Mirrors TFW in tools/train_ai.py: round(1000 * (1 + ln(tf))). */
static const int32_t ai_tfw[11] = {
    0, 1000, 1693, 2099, 2386, 2609, 2792, 2946, 3079, 3194, 3297
};

/* The stop-word table is generated by the trainer rather than duplicated here,
 * so the query tokeniser cannot drift away from the one that built the corpus.
 * Without it a query like "how do i bake bread" scores on "how"/"do" alone and
 * clears the accept threshold against a corpus that has nothing to do with it. */
#include "ai/stopwords.inc"

static int ai_is_stop(const char *tok) {
    for (int i = 0; i < AI_STOPWORD_COUNT; i++) {
        if (strcmp_c(tok, ai_stopwords[i]) == 0) return 1;
    }
    return 0;
}

/* Header layout, fixed by tools/train_ai.py. */
#define AI_OFF_N_QA          4
#define AI_OFF_N_CHAT        8
#define AI_OFF_N_VOCAB       12
#define AI_OFF_QA_INDEX      16
#define AI_OFF_QA_W          20
#define AI_OFF_QA_POOL       24
#define AI_OFF_QA_POOL_LEN   28
#define AI_OFF_CHAT_INDEX    32
#define AI_OFF_CHAT_W        36
#define AI_OFF_CHAT_POOL     40
#define AI_OFF_CHAT_POOL_LEN 44
#define AI_OFF_VOCAB_INDEX   48
#define AI_OFF_VOCAB_POOL    52
#define AI_OFF_VOCAB_POOL_LEN 56
#define AI_OFF_TOTAL         60

extern const uint8_t  ai_model_blob[];
extern const uint32_t ai_model_size;

static int      ai_log_count = 0;
static uint8_t  ai_log_role[AI_LOG_LINES];
static char     ai_log_text[AI_LOG_LINES][AI_LINE_LEN];
static char     ai_input[AI_INPUT_MAX + 1];
static int      ai_input_len = 0;
static int      ai_model_ok = -1;      /* -1 unprobed, 0 bad, 1 good */
static int      ai_scroll = 0;
static int      ai_greeted = 0;
static int      ai_wrap_cols = 60;     /* refreshed from the window width */
static int      ai_last_score = 0;
static int      ai_last_source = 0;    /* 0 none, 1 qa, 2 chat */
static int16_t  ai_tf[AI_MAX_VOCAB];
static int8_t   ai_qvec[AI_MAX_VOCAB];

static uint32_t ai_u32(uint32_t off) {
    const uint8_t *p = ai_model_blob + off;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Validate the header once. A model that fails these checks is treated as
 * absent rather than trusted, so a truncated or stale blob cannot walk the
 * kernel off the end of the image. */
static int ai_model_check(void) {
    if (ai_model_ok >= 0) return ai_model_ok;
    ai_model_ok = 0;
    if (ai_model_size < 64 || ai_model_size > (32u << 20)) return 0;
    if (ai_model_blob[0] != 'A' || ai_model_blob[1] != 'I' ||
        ai_model_blob[2] != 'M' || ai_model_blob[3] != '1') return 0;
    if (ai_u32(AI_OFF_TOTAL) != ai_model_size) return 0;
    if (ai_u32(AI_OFF_N_QA) == 0 || ai_u32(AI_OFF_N_CHAT) == 0) return 0;
    /* The query vector is a fixed-size static, so a model with a larger
     * vocabulary than we reserved must be refused rather than overflowed. */
    if (ai_u32(AI_OFF_N_VOCAB) == 0) return 0;
    if (ai_u32(AI_OFF_N_VOCAB) > AI_MAX_VOCAB) return 0;
    ai_model_ok = 1;
    return 1;
}

/* CP866 + ASCII lowercasing, byte-wise. Mirrors lower_byte() in the trainer. */
static uint8_t ai_lower(uint8_t b) {
    if (b >= 0x80 && b <= 0x8F) return (uint8_t)(b + 0x20);   /* А..П -> а..п */
    if (b >= 0x90 && b <= 0x9F) return (uint8_t)(b + 0x50);   /* Р..Я -> р..я */
    if (b >= 'A' && b <= 'Z')   return (uint8_t)(b + 32);
    return b;
}

static int ai_is_word(uint8_t b) {
    if (b >= '0' && b <= '9') return 1;
    if (b >= 'a' && b <= 'z') return 1;
    if (b >= 0xA0 && b <= 0xAF) return 1;   /* а..п */
    if (b >= 0xE0 && b <= 0xEF) return 1;   /* р..я */
    return 0;
}

/* Binary search for a token in the sorted vocabulary table, returning its index
 * or -1 when the corpus never contained it. The table is sorted by raw CP866
 * bytes, which is the order the trainer emits, so a plain byte compare works.
 *
 * A miss is meaningful, not a nuisance: it means the query word appears nowhere
 * in the corpus, so it contributes nothing to the score. That is what keeps
 * "how do i bake bread" from matching the code corpus. */
static int ai_lookup(const char *tok, int len) {
    int lo = 0, hi = (int)ai_u32(AI_OFF_N_VOCAB) - 1;
    uint32_t idx = ai_u32(AI_OFF_VOCAB_INDEX);
    const uint8_t *pool = ai_model_blob + ai_u32(AI_OFF_VOCAB_POOL);
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        uint32_t off = ai_u32(idx + (uint32_t)mid * 8);
        uint32_t vlen = ai_u32(idx + (uint32_t)mid * 8 + 4);
        int n = (int)(vlen < (uint32_t)len ? vlen : (uint32_t)len);
        int cmp = 0;
        for (int i = 0; i < n; i++) {
            uint8_t a = pool[off + (uint32_t)i];
            uint8_t b = (uint8_t)tok[i];
            if (a != b) { cmp = (a < b) ? -1 : 1; break; }
        }
        if (cmp == 0) {
            if (vlen == (uint32_t)len) return mid;
            cmp = (vlen < (uint32_t)len) ? -1 : 1;
        }
        if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* Query text -> int8 vector, exactly as the trainer encodes a corpus entry.
 * Returns 0 when the query held no usable token, in which case nothing should
 * be answered. Tokens longer than 32 bytes are dropped on both sides. */
static int ai_encode(const char *s, int len, int8_t *out, int n_vocab) {
    char tok[33];                              /* NUL kept for table lookups */
    int tlen = 0, over = 0, peak = 0;

    for (int i = 0; i < n_vocab; i++) ai_tf[i] = 0;

    for (int i = 0; i <= len; i++) {
        uint8_t b = (i < len) ? ai_lower((uint8_t)s[i]) : 0;
        if (i < len && ai_is_word(b)) {
            if (tlen < 32) tok[tlen] = (char)b;
            else over = 1;
            tlen++;
            continue;
        }
        /* MIN_TOKEN and the stop list both mirror the trainer. */
        if (!over && tlen >= 2) {
            tok[tlen] = '\0';
            if (!ai_is_stop(tok)) {
                int id = ai_lookup(tok, tlen);
                if (id >= 0) ai_tf[id]++;
            }
        }
        tlen = 0;
        over = 0;
    }

    /* Two passes so the max-normalisation needs no scratch array. */
    for (int i = 0; i < n_vocab; i++) {
        if (ai_tf[i]) {
            int w = ai_tfw[ai_tf[i] > 10 ? 10 : ai_tf[i]];
            if (w > peak) peak = w;
        }
    }
    for (int i = 0; i < n_vocab; i++) out[i] = 0;
    if (!peak) return 0;

    for (int i = 0; i < n_vocab; i++) {
        if (!ai_tf[i]) continue;
        int w = ai_tfw[ai_tf[i] > 10 ? 10 : ai_tf[i]];
        out[i] = (int8_t)((w * 127) / peak);
    }
    return 1;
}

/* Argmax over one weight matrix. Returns the winning row index and reports its
 * dot product through *score. Both vectors are max-normalised to 127, so the
 * score is comparable across queries. */
static int ai_best(const int8_t *q, uint32_t w_off, int count, int n_vocab, int *score) {
    const int8_t *base = (const int8_t *)(ai_model_blob + w_off);
    int best = -1, best_s = -1;
    for (int i = 0; i < count; i++) {
        const int8_t *row = base + (uint32_t)i * (uint32_t)n_vocab;
        int32_t s = 0;
        for (int b = 0; b < n_vocab; b++) s += (int32_t)q[b] * (int32_t)row[b];
        if (s > best_s) { best_s = s; best = i; }
    }
    *score = best_s < 0 ? 0 : best_s;
    return best;
}

/* index entry i -> (left_off, left_len, right_off, right_len) */
static void ai_entry(uint32_t index_off, int i, uint32_t *lo, uint32_t *ll,
                     uint32_t *ro, uint32_t *rl) {
    uint32_t at = index_off + (uint32_t)i * 16;
    *lo = ai_u32(at);
    *ll = ai_u32(at + 4);
    *ro = ai_u32(at + 8);
    *rl = ai_u32(at + 12);
}

static void ai_push(uint8_t role, const char *text) {
    if (ai_log_count >= AI_LOG_LINES) {                 /* drop the oldest line */
        for (int i = 1; i < AI_LOG_LINES; i++) {
            ai_log_role[i - 1] = ai_log_role[i];
            for (int k = 0; k < AI_LINE_LEN; k++) ai_log_text[i - 1][k] = ai_log_text[i][k];
        }
        ai_log_count = AI_LOG_LINES - 1;
    }
    int n = 0;
    while (text[n] && n < AI_LINE_LEN - 1) { ai_log_text[ai_log_count][n] = text[n]; n++; }
    ai_log_text[ai_log_count][n] = '\0';
    ai_log_role[ai_log_count] = role;
    ai_log_count++;
    ai_scroll = 0;
}

/* Break a pool string into window-width lines, preferring spaces.
 *
 * ai_wrap_cols is refreshed by draw_ai_client from the real window width, so a
 * widened window wraps wider instead of drawing text out past its right edge.
 * The buffer itself is still AI_LINE_LEN, which is only an upper bound. */
static void ai_push_wrapped(uint8_t role, const char *text, uint32_t len) {
    char buf[AI_LINE_LEN];
    int room = ai_wrap_cols;
    if (room < 16) room = 16;
    if (room > AI_LINE_LEN - 1) room = AI_LINE_LEN - 1;
    uint32_t i = 0;
    if (len == 0) { ai_push(role, ""); return; }
    while (i < len) {
        uint32_t take = len - i;
        if (take > (uint32_t)room) {
            take = (uint32_t)room;
            for (uint32_t k = take; k > 0; k--) {
                if (text[i + k - 1] == ' ') { take = k - 1; break; }
            }
            if (take == 0) take = (uint32_t)room;
        }
        int n = 0;
        while ((uint32_t)n < take && n < room) { buf[n] = text[i + n]; n++; }
        buf[n] = '\0';
        ai_push(role, buf);
        i += take;
        while (i < len && text[i] == ' ') i++;
    }
}

static void ai_chat_reset(void) {
    ai_log_count = 0;
    ai_input_len = 0;
    ai_input[0] = '\0';
    ai_scroll = 0;
    ai_last_score = 0;
    ai_last_source = 0;
    ai_push(AI_ROLE_AI, "Ai Chat. Small offline model - ask me about C,");
    ai_push(AI_ROLE_AI, "pointers, memory, data structures or OS internals.");
    ai_push(AI_ROLE_AI, "Try: what is a page fault");
}

static void ai_submit(void) {
    char q[AI_INPUT_MAX + 1];
    uint32_t lo, ll, ro, rl;
    int score = 0;

    if (ai_input_len == 0) return;
    for (int i = 0; i < ai_input_len; i++) q[i] = ai_input[i];
    q[ai_input_len] = '\0';
    ai_input_len = 0;
    ai_input[0] = '\0';

    ai_push(AI_ROLE_USER, q);

    if (!ai_model_check()) {
        ai_push(AI_ROLE_AI, "Model blob missing or corrupt.");
        return;
    }
    int n_vocab = (int)ai_u32(AI_OFF_N_VOCAB);
    if (!ai_encode(q, (int)strlen(q), ai_qvec, n_vocab)) {
        ai_push(AI_ROLE_AI, "None of those words are in my corpus. I only");
        ai_push(AI_ROLE_AI, "know code topics - try: struct padding");
        return;
    }

    int n_qa = (int)ai_u32(AI_OFF_N_QA);
    int n_chat = (int)ai_u32(AI_OFF_N_CHAT);
    int idx = ai_best(ai_qvec, ai_u32(AI_OFF_QA_W), n_qa, n_vocab, &score);
    if (idx >= 0 && score >= AI_QA_MIN) {
        ai_entry(ai_u32(AI_OFF_QA_INDEX), idx, &lo, &ll, &ro, &rl);
        ai_push_wrapped(AI_ROLE_AI,
                        (const char *)(ai_model_blob + ai_u32(AI_OFF_QA_POOL) + ro), rl);
        ai_last_score = score;
        ai_last_source = 1;
        return;
    }

    idx = ai_best(ai_qvec, ai_u32(AI_OFF_CHAT_W), n_chat, n_vocab, &score);
    if (idx >= 0 && score >= AI_CHAT_MIN) {
        ai_entry(ai_u32(AI_OFF_CHAT_INDEX), idx, &lo, &ll, &ro, &rl);
        ai_push_wrapped(AI_ROLE_AI,
                        (const char *)(ai_model_blob + ai_u32(AI_OFF_CHAT_POOL) + ro), rl);
        ai_last_score = score;
        ai_last_source = 2;
        return;
    }

    ai_push(AI_ROLE_AI, "Not in my corpus. I am narrow: ask about C, memory,");
    ai_push(AI_ROLE_AI, "algorithms or how the kernel boots and runs.");
    ai_last_score = score;
    ai_last_source = 0;
}

/* Note the unsigned handling: Cyrillic arrives as CP866 bytes 0x80..0xEF, which
 * are negative in a signed char. Comparing the signed value against 32 would
 * silently drop every Russian character the user types. */
static void ai_chat_key(char ch) {
    unsigned char c = (unsigned char)ch;
    if (c == '\n' || c == '\r') { ai_submit(); return; }
    if (c == '\b') {
        if (ai_input_len > 0) ai_input[--ai_input_len] = '\0';
        return;
    }
    if (c == 0x13) { if (ai_scroll < ai_log_count) ai_scroll += 3; return; }  /* page up   */
    if (c == 0x14) { ai_scroll -= 3; if (ai_scroll < 0) ai_scroll = 0; return; } /* page down */
    if (c >= 32 && c != 127 && ai_input_len < AI_INPUT_MAX) {
        ai_input[ai_input_len++] = (char)c;
        ai_input[ai_input_len] = '\0';
    }
}

static void draw_ai_client(window_t *win) {
    char buf[80];
    int cx = win->x + 8;
    int cy = win->y + TITLEBAR_H + 6;
    int cw = win->w - 16;
    int bottom = win->y + win->h - 8;
    int iy = bottom - 30;                     /* input row */
    int lb = iy - 8;
    /* The header and every log line share one grid, so nothing sits off-step. */
    int ly = cy + 10;
    int log_top = ly + AI_LINE_PITCH;

    if (!ai_greeted) { ai_greeted = 1; ai_chat_reset(); }

    /* Text must stop before the client's right padding; 8 px per character.
     * Recomputed every frame so resizing the window reflows the next answer. */
    ai_wrap_cols = (cw - AI_TEXT_X - 10) / 8;

    fill_rounded_rect(cx, cy, cw, bottom - cy, 10, COLOR_SURFACE);

    /* Header: proves at a glance which model is loaded. */
    if (ai_model_check()) {
        strcpy_c(buf, "offline model - ");
        append_int(buf, (int32_t)ai_u32(AI_OFF_N_QA));
        strcat_c(buf, " Q&A, ");
        append_int(buf, (int32_t)ai_u32(AI_OFF_N_CHAT));
        strcat_c(buf, " chat, ");
        append_int(buf, (int32_t)ai_u32(AI_OFF_N_VOCAB));
        strcat_c(buf, " words");
    } else {
        strcpy_c(buf, "model blob missing - rebuild with tools/train_ai.py");
    }
    draw_string(cx + 12, ly, buf, AI_HINT_COL);

    /* Transcript. Lines are drawn from a scrolling offset measured in rows
     * back from the newest, so new output always lands at the bottom.
     *
     * The role label is right-aligned against the shared text column, so "Ai"
     * and ">" both butt up to it and the text itself never moves sideways
     * between lines. */
    fill_rounded_rect(cx + 8, log_top - 4, cw - 16, lb - log_top + 8, 8, COLOR_VIEW_BG);
    int rows = (lb - log_top) / AI_LINE_PITCH;
    if (rows < 1) rows = 1;
    int first = ai_log_count - rows - ai_scroll;
    if (first < 0) first = 0;
    for (int i = 0; i < rows; i++) {
        int idx = first + i;
        if (idx >= ai_log_count) break;
        int y = log_top + i * AI_LINE_PITCH;
        if (ai_log_role[idx] == AI_ROLE_USER) {
            draw_string(cx + AI_TEXT_X - 8, y, ">", AI_PROMPT_COL);
            draw_string(cx + AI_TEXT_X, y, ai_log_text[idx], AI_USER_COL);
        } else {
            draw_string(cx + AI_TEXT_X - 16, y, "Ai", AI_PROMPT_COL);
            draw_string(cx + AI_TEXT_X, y, ai_log_text[idx], AI_TEXT_COL);
        }
    }

    /* Input row with a blinking caret, on the same text column as the log. */
    fill_rounded_rect(cx + 8, iy, cw - 16, 26, 8, COLOR_FIELD);
    draw_string(cx + AI_TEXT_X - 8, iy + 5, ">", AI_PROMPT_COL);
    draw_string(cx + AI_TEXT_X, iy + 5, ai_input, AI_USER_COL);
    if ((system_ticks / 40) % 2) {
        fill_rect(cx + AI_TEXT_X + ai_input_len * 8, iy + 5, 2, 16, COLOR_ACCENT);
    }

    /* Status: which table answered and how strong the match was. */
    if (ai_last_source == 1)      strcpy_c(buf, "qa match, score ");
    else if (ai_last_source == 2) strcpy_c(buf, "chat match, score ");
    else if (ai_last_score)       strcpy_c(buf, "no match, best score ");
    else { strcpy_c(buf, "enter to send"); }
    if (ai_last_source || ai_last_score) append_int(buf, (int32_t)ai_last_score);
    draw_string(cx + 12, iy - 20, buf, AI_HINT_COL);
}

static void draw_window(window_t *win, int active) {
    if (!win->used) return;
    /* Subtle outer glow ring (very light, no heavy alpha blending) */
    draw_window_shadow(win->x, win->y, win->w, win->h);

    /* Window body — rounded corners, no shadow */
    fill_rounded_rect_aa(win->x, win->y, win->w, win->h, 12,
                         win->app == APP_STORE ? STORE_BG : COLOR_SURFACE);

    /* Title bar gradient */
    uint32_t c_hi = active ? COLOR_TITLE_HI : COLOR_SURFACE2;
    uint32_t c_lo = active ? COLOR_TITLE_LO : C_SURFACE;
    if (win->app == APP_STORE) {
        c_hi = active ? 0xFF292533 : STORE_PANEL;
        c_lo = STORE_BG;
    }
    for (int y = 0; y < TITLEBAR_H; y++) {
        uint32_t line_color = lerp_color(c_hi, c_lo, y, TITLEBAR_H);
        int cur_y = win->y + y;
        if (cur_y < 0 || cur_y >= (int)fb_height) continue;
        /* Rounded top, square bottom: the bar meets the body below it. The arc
         * uses the same coverage function the body uses, so the two agree pixel
         * for pixel rather than one stepping across the other. */
        fill_row_rounded_top(win->x, cur_y, win->w, 12, win->y, line_color);
    }

    /* Border outline, drawn after the title bar so its corner arcs are on top
     * rather than blended away by the gradient. Previously only the four
     * straight edges were drawn here, so the outline simply disappeared where
     * the window curved. */
    {
        /* One complete outline the whole way round: white for the focused
         * window, a muted grey for the others so focus is still readable.
         *
         * This used to be drawn in the accent colour, with a separate white line
         * added along the top edge alone. That line ran from x+12 to x+w-13, so
         * it never reached the corners, and the outline changed colour part way
         * down the sides - the white edge simply was not there for most of the
         * window. The stroke now covers all four sides and both arcs, and the
         * partial line is gone because it has nothing left to add. */
        uint32_t bc = win->app == APP_STORE ? (active ? 0xFF64577F : STORE_LINE) :
                      (active ? COLOR_WHITE : COLOR_MUTED);
        stroke_rounded_rect_aa(win->x, win->y, win->w, win->h, 12, bc);
    }
    if (active) {
        /* small accent tab so the focused window is readable at a glance */
        fill_rounded_rect(win->x + 7, win->y + 8, 3, 12, 1, COLOR_ACCENT);
    }
    draw_line(win->x, win->y + TITLEBAR_H, win->x + win->w - 1, win->y + TITLEBAR_H, COLOR_BORDER);
    draw_string(win->x + 14, win->y + 7, win->title, active ? COLOR_WHITE : COLOR_MUTED);

    /* Close button — rounded, no shadow */
    int btn_w = 28, btn_h = 20;
    int btn_x = win->x + win->w - btn_w - 6;
    int btn_y = win->y + 4;
    fill_rounded_rect(btn_x, btn_y, btn_w, btn_h, 5, active ? COLOR_DANGER : COLOR_DANGER_BG);
    draw_string(btn_x + 10, btn_y + 2, "X", COLOR_WHITE);

    switch (win->app) {
        case APP_PAINT:    draw_paint_client(win); break;
        case APP_NOTEPAD:  draw_notepad_client(win); break;
        case APP_RIKKI:    draw_rikki_client(win); break;
        case APP_CALC:     draw_calc_client(win); break;
        case APP_TERM:     draw_term_client(win); break;
        case APP_EXPLORER: draw_explorer_client(win); break;
        case APP_STORE:    draw_store_client(win); break;
        case APP_MEDIA:    draw_media_client(win); break;
        case APP_SETTINGS: draw_settings_client(win); break;
        case APP_SHEET:    draw_sheet_client(win); break;
        case APP_WORD:     draw_word_client(win); break;
        case APP_BROWSER:  draw_browser_client(win); break;
        case APP_AICHAT:   draw_ai_client(win); break;
        case APP_TASKMGR:  draw_taskmgr_client(win); break;
    }
}

static void sort_windows(void) {
    int n = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) if (windows[i].used) z_order[n++] = i;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (windows[z_order[i]].z > windows[z_order[j]].z) {
                int tmp = z_order[i]; z_order[i] = z_order[j]; z_order[j] = tmp;
            }
        }
    }
    z_order[n] = -1;
}

static void draw_app_icon(int x, int y, int app_type) {
    if (app_type == APP_CALC) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF1E2638);
        fill_rounded_rect(x + 1, y + 1, 30, 30, 5, 0xFF2A364F);
        fill_rounded_rect(x + 5, y + 5, 22, 7, 2, 0xFF111722);
        fill_rect(x + 18, y + 7, 7, 3, 0xFF58A6FF);
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                uint32_t btn_col = (r == 2 && c == 2) ? 0xFF0078D4 : 0xFF3D4B66;
                fill_rounded_rect(x + 5 + c * 8, y + 15 + r * 5, 6, 4, 1, btn_col);
            }
        }
    } else if (app_type == APP_NOTEPAD) {
        fill_rounded_rect(x + 5, y + 3, 23, 27, 3, 0x40000000);
        fill_rounded_rect(x + 4, y + 2, 24, 28, 3, 0xFFF3F4F6);
        fill_rounded_rect(x + 4, y + 2, 24, 8, 3, 0xFF0078D4);
        fill_rect(x + 4, y + 6, 24, 4, 0xFF0078D4);
        fill_rect(x + 8, y + 13, 16, 2, 0xFF9CA3AF);
        fill_rect(x + 8, y + 17, 12, 2, 0xFF9CA3AF);
        fill_rect(x + 8, y + 21, 14, 2, 0xFF9CA3AF);
        fill_rect(x + 8, y + 25, 8, 2, 0xFF0078D4);
    } else if (app_type == APP_PAINT) {
        fill_rounded_rect(x + 3, y + 4, 26, 24, 8, 0xFFE2E8F0);
        fill_rounded_rect(x + 4, y + 5, 24, 22, 7, 0xFFFFFFFF);
        fill_rounded_rect(x + 7, y + 8, 5, 5, 2, 0xFFEF4444);
        fill_rounded_rect(x + 14, y + 7, 5, 5, 2, 0xFFF59E0B);
        fill_rounded_rect(x + 20, y + 10, 5, 5, 2, 0xFF10B981);
        fill_rounded_rect(x + 8, y + 16, 5, 5, 2, 0xFF3B82F6);
        fill_rounded_rect(x + 15, y + 18, 5, 5, 2, 0xFF8B5CF6);
        fill_rounded_rect(x + 21, y + 18, 5, 5, 2, 0xFFCBD5E1);
    } else if (app_type == APP_TERM) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF0F172A);
        fill_rounded_rect(x + 1, y + 1, 30, 30, 5, 0xFF1E293B);
        fill_rect(x + 1, y + 8, 30, 1, 0xFF334155);
        fill_rect(x + 5, y + 4, 2, 2, 0xFFEF4444);
        fill_rect(x + 9, y + 4, 2, 2, 0xFFF59E0B);
        fill_rect(x + 13, y + 4, 2, 2, 0xFF10B981);
        draw_line(x + 6, y + 14, x + 10, y + 17, 0xFF4ADE80);
        draw_line(x + 6, y + 20, x + 10, y + 17, 0xFF4ADE80);
        draw_line(x + 7, y + 14, x + 11, y + 17, 0xFF4ADE80);
        draw_line(x + 7, y + 20, x + 11, y + 17, 0xFF4ADE80);
        fill_rect(x + 14, y + 19, 7, 2, 0xFF4ADE80);
    } else if (app_type == APP_RIKKI) {
        fill_rounded_rect(x, y, 32, 32, 8, 0xFFFFE8D6);
        fill_rounded_rect(x + 5, y + 4, 6, 8, 2, 0xFFF97316);
        fill_rounded_rect(x + 21, y + 4, 6, 8, 2, 0xFFF97316);
        fill_rect(x + 7, y + 7, 2, 3, 0xFFF472B6);
        fill_rect(x + 23, y + 7, 2, 3, 0xFFF472B6);
        fill_rounded_rect(x + 4, y + 8, 24, 19, 6, 0xFFFB923C);
        fill_rounded_rect(x + 9, y + 14, 3, 4, 1, 0xFF1E293B);
        fill_rounded_rect(x + 20, y + 14, 3, 4, 1, 0xFF1E293B);
        fill_rect(x + 9, y + 14, 1, 1, 0xFFFFFFFF);
        fill_rect(x + 20, y + 14, 1, 1, 0xFFFFFFFF);
        fill_rect(x + 15, y + 18, 2, 2, 0xFFF43F5E);
        fill_rounded_rect(x + 12, y + 20, 8, 4, 2, 0xFFFFF7ED);
    } else if (app_type == APP_EXPLORER) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF2D3748);
        fill_rounded_rect(x + 4, y + 4, 24, 24, 4, 0xFF4A5A7A);
        draw_string(x + 8, y + 8, "F", 0xFFFFFFFF);
        draw_line(x + 6, y + 16, x + 26, y + 16, 0xFF80BFFF);
        draw_line(x + 6, y + 22, x + 26, y + 22, 0xFF80BFFF);
    } else if (app_type == APP_STORE) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF1E3A8A);
        fill_rounded_rect(x + 2, y + 2, 28, 28, 5, 0xFF2563EB);
        fill_rounded_rect(x + 6, y + 8, 20, 18, 4, 0xFFFFFFFF);
        fill_rounded_rect(x + 9, y + 11, 14, 12, 3, 0xFF3B82F6);
        draw_line(x + 12, y + 5, x + 19, y + 5, 0xFFFFD700);
        draw_string(x + 12, y + 13, "S", COLOR_GOLD);
    } else if (app_type == APP_MEDIA) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF4C1D95);
        fill_rounded_rect(x + 2, y + 2, 28, 28, 5, 0xFF7C3AED);
        fill_rounded_rect(x + 6, y + 6, 20, 20, 10, 0xFF06B6D4);
        draw_line(x + 12, y + 11, x + 12, y + 21, COLOR_WHITE);
        draw_line(x + 13, y + 12, x + 20, y + 16, COLOR_WHITE);
        draw_line(x + 13, y + 20, x + 20, y + 16, COLOR_WHITE);
    } else if (app_type == APP_SETTINGS) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF334155);
        fill_rounded_rect(x + 2, y + 2, 28, 28, 5, 0xFF475569);
        /* Gear icon */
        fill_rounded_rect(x + 10, y + 10, 12, 12, 3, 0xFFE2E8F0);
        fill_rounded_rect(x + 13, y + 13, 6, 6, 2, 0xFF475569);
        draw_line(x + 16, y + 4, x + 16, y + 8, 0xFFE2E8F0);
        draw_line(x + 16, y + 24, x + 16, y + 28, 0xFFE2E8F0);
        draw_line(x + 4, y + 16, x + 8, y + 16, 0xFFE2E8F0);
        draw_line(x + 24, y + 16, x + 28, y + 16, 0xFFE2E8F0);
    } else if (app_type == APP_SHEET) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF065F46);
        fill_rounded_rect(x + 2, y + 2, 28, 28, 5, 0xFF047857);
        fill_rect(x + 6, y + 6, 20, 20, 0xFFD1FAE5);
        draw_line(x + 6, y + 12, x + 26, y + 12, 0xFF065F46);
        draw_line(x + 6, y + 18, x + 26, y + 18, 0xFF065F46);
        draw_line(x + 6, y + 24, x + 26, y + 24, 0xFF065F46);
        draw_line(x + 13, y + 6, x + 13, y + 26, 0xFF065F46);
        draw_line(x + 20, y + 6, x + 20, y + 26, 0xFF065F46);
    } else if (app_type == APP_WORD) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF1E3A8A);
        fill_rounded_rect(x + 2, y + 2, 28, 28, 5, 0xFF2563EB);
        fill_rounded_rect(x + 6, y + 4, 20, 24, 3, 0xFFFFFFFF);
        draw_line(x + 9, y + 9, x + 23, y + 9, 0xFF1E3A8A);
        draw_line(x + 9, y + 13, x + 23, y + 13, 0xFF3B82F6);
        draw_line(x + 9, y + 17, x + 23, y + 17, 0xFF1E3A8A);
        draw_line(x + 9, y + 21, x + 23, y + 21, 0xFF3B82F6);
        draw_line(x + 9, y + 25, x + 18, y + 25, 0xFF1E3A8A);
    } else if (app_type == APP_BROWSER) {
        fill_rounded_rect(x, y + 2, 32, 28, 6, 0xFF1B2740);
        fill_rounded_rect(x + 1, y + 3, 30, 26, 5, 0xFF243352);
        fill_rounded_rect(x + 3, y + 5, 26, 6, 2, 0xFF0F1723);
        fill_rounded_rect(x + 5, y + 6, 11, 4, 2, 0xFF58A6FF);
        fill_rounded_rect(x + 5, y + 14, 22, 3, 1, 0xFF7FA8D8);
        fill_rounded_rect(x + 5, y + 19, 16, 3, 1, 0xFF5E82AB);
        fill_rounded_rect(x + 5, y + 24, 20, 3, 1, 0xFF7FA8D8);
    } else if (app_type == APP_TASKMGR) {
        fill_rounded_rect(x, y, 32, 32, 6, 0xFF1E334D);
        fill_rounded_rect(x + 2, y + 2, 28, 28, 5, 0xFF294D70);
        fill_rect(x + 7, y + 19, 4, 6, 0xFF64D8B1);
        fill_rect(x + 14, y + 13, 4, 12, 0xFF77BDF5);
        fill_rect(x + 21, y + 8, 4, 17, 0xFFF2C66D);
    }
}

static void draw_desktop_icon(int x, int y, int app, const char *label) {
    fill_rounded_rect_alpha(x, y, 56, 56, 12, 0x991E2538);
    draw_app_icon(x + 12, y + 12, app);
    int len = strlen(label);
    int text_x = x + (56 - len * 8) / 2;
    draw_string(text_x, y + 60, label, COLOR_WHITE);
}

/* ===== Desktop icons: data-driven so they can be dragged around ===== */

typedef struct {
    int app;              /* APP_* id, drives draw_app_icon()   */
    const char *label;
    int x, y;             /* top-left of the 56x56 tile         */
    int dragging;         /* currently held by the pointer      */
} desktop_icon_t;

#define DESKTOP_ICON_COUNT 14     /* additional icons continue into column four */
#define DESKTOP_ICON_W     56
#define DESKTOP_ICON_H     56
#define DESKTOP_ICON_HIT_H 76      /* tile plus the caption under it */
#define DESKTOP_GRID_X0    24
#define DESKTOP_GRID_Y0    24
#define DESKTOP_GRID_COL_W 80
#define DESKTOP_GRID_ROW_H 88
#define DESKTOP_SNAP_DIST  14      /* magnetic pull-back radius      */
#define DESKTOP_DRAG_SLOP  3       /* movement that turns a click into a drag */

static desktop_icon_t desktop_icons[DESKTOP_ICON_COUNT];
static int desktop_drag_idx = -1;      /* icon index being carried, -1 = none */
static int desktop_drag_moved = 0;     /* crossed the slop threshold          */
static int desktop_drag_grab_x = 0, desktop_drag_grab_y = 0;
static int desktop_drag_press_x = 0, desktop_drag_press_y = 0;

static int desktop_abs(int v) { return v < 0 ? -v : v; }

/* Starts life on the same 3x4 grid the hardcoded version used, but the
   positions are plain data now and the drag handler is free to move them. */
static void desktop_icons_init(void) {
    static const struct { int app; const char *label; } defs[DESKTOP_ICON_COUNT] = {
        { APP_PAINT,    "Paint" }, { APP_NOTEPAD,  "Notes" },
        { APP_RIKKI,    "Rikki" }, { APP_CALC,     "Calc"  },
        { APP_TERM,     "Term"  }, { APP_EXPLORER, "Files" },
        { APP_STORE,    "Store" }, { APP_MEDIA,    "Media" },
        { APP_SETTINGS, "Set"   }, { APP_SHEET,    "Sheet" },
        { APP_WORD,     "Word"  }, { APP_BROWSER,  "Web"   },
        { APP_AICHAT,   "Chat"  }, { APP_TASKMGR,  "Tasks" }
    };
    for (int i = 0; i < DESKTOP_ICON_COUNT; i++) {
        desktop_icons[i].app = defs[i].app;
        desktop_icons[i].label = defs[i].label;
        desktop_icons[i].x = DESKTOP_GRID_X0 + (i / 4) * DESKTOP_GRID_COL_W;
        desktop_icons[i].y = DESKTOP_GRID_Y0 + (i % 4) * DESKTOP_GRID_ROW_H;
        desktop_icons[i].dragging = 0;
    }
}

/* Topmost icon whose hit box contains the point (later entries paint last). */
static int desktop_icon_hit(int px, int py) {
    for (int i = DESKTOP_ICON_COUNT - 1; i >= 0; i--) {
        if (point_in_rect(px, py, desktop_icons[i].x, desktop_icons[i].y,
                          DESKTOP_ICON_W, DESKTOP_ICON_HIT_H))
            return i;
    }
    return -1;
}

/* Magnetic snap: only pulls the icon in when it was dropped close to a slot, so
   deliberate free placement survives. */
static void desktop_icon_snap(desktop_icon_t *ic) {
    int col = (ic->x - DESKTOP_GRID_X0 + DESKTOP_GRID_COL_W / 2) / DESKTOP_GRID_COL_W;
    int row = (ic->y - DESKTOP_GRID_Y0 + DESKTOP_GRID_ROW_H / 2) / DESKTOP_GRID_ROW_H;
    int sx, sy;
    if (col < 0) col = 0;
    if (row < 0) row = 0;
    sx = DESKTOP_GRID_X0 + col * DESKTOP_GRID_COL_W;
    sy = DESKTOP_GRID_Y0 + row * DESKTOP_GRID_ROW_H;
    if (desktop_abs(ic->x - sx) <= DESKTOP_SNAP_DIST &&
        desktop_abs(ic->y - sy) <= DESKTOP_SNAP_DIST) {
        ic->x = sx;
        ic->y = sy;
    }
}

static void draw_icons(void) {
    /* The carried icon paints last so it floats above its neighbours. */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < DESKTOP_ICON_COUNT; i++) {
            desktop_icon_t *ic = &desktop_icons[i];
            if ((pass == 0) == (ic->dragging != 0)) continue;
            if (ic->dragging)
                fill_rounded_rect_alpha(ic->x + 3, ic->y + 5, DESKTOP_ICON_W,
                                        DESKTOP_ICON_W, 12, 0x55000000);
            draw_desktop_icon(ic->x, ic->y, ic->app, ic->label);
            if (ic->dragging)
                fill_rounded_rect_alpha(ic->x, ic->y, DESKTOP_ICON_W,
                                        DESKTOP_ICON_W, 12, 0x2EFFFFFF);
        }
    }
}

/* RTC Clock */
static uint8_t rtc_read(uint8_t reg) { outb(0x70, reg); return inb(0x71); }
static uint8_t bcd_to_bin(uint8_t val) { return ((val / 16) * 10) + (val & 0x0F); }

static void get_rtc_time(uint8_t *hours, uint8_t *mins, uint8_t *secs) {
    *secs  = rtc_read(0x00);
    *mins  = rtc_read(0x02);
    *hours = rtc_read(0x04);
    uint8_t regB = rtc_read(0x0B);
    if (!(regB & 0x04)) {
        *secs  = bcd_to_bin(*secs);
        *mins  = bcd_to_bin(*mins);
        *hours = bcd_to_bin(*hours);
    }
    *hours = (*hours + 7) % 24;
}

/* ===== Floating black taskbar =====
 * One layout is shared by painting, hit testing and popup anchoring.
 * All drawing is integer-only: safe before an x86 FPU/SSE setup.
 * The neutral black finish intentionally does not follow app-window themes.
 */
typedef struct { int x, y, w, h; } tb_rect_t;
typedef struct {
    tb_rect_t panel, start, sound, language, caps, clock;
    tb_rect_t item[MAX_WINDOWS + 4];
    int app[MAX_WINDOWS + 4], window[MAX_WINDOWS + 4], count;
} tb_layout_t;

#define TB_WHITE  0xFFF1F3F5
#define TB_MUTED  0xFF92969F
#define TB_ACCENT 0xFF86C9FF
#define TB_HIT_NONE  (-1)
#define TB_HIT_START (-2)
#define TB_HIT_SOUND (-3)
#define TB_HIT_LANG  (-4)
#define TB_HIT_CAPS  (-5)
#define TB_HIT_CLOCK (-6)
static int tb_hover = TB_HIT_NONE;
static int tb_pressed = 0;

static tb_rect_t tb_rect(int x, int y, int w, int h) {
    tb_rect_t r = {x, y, w, h};
    return r;
}

static int tb_contains(tb_rect_t r, int x, int y) {
    return point_in_rect(x, y, r.x, r.y, r.w, r.h);
}

/* Coverage at 4x4 subpixel samples, only evaluated in rounded corners.
 * Straight spans remain fast. Source opacity is multiplied by coverage,
 * not overwritten (the older generic AA primitive only handles opaque fills).
 */
static void tb_round(int x, int y, int w, int h, int radius, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int r = radius;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 1) { fill_rect_alpha(x, y, w, h, color); return; }
    int opaque = (color >> 24) == 255;
    for (int yy = 0; yy < h; yy++) {
        if (yy >= r && yy < h - r) {
            if (opaque) span_opaque(x, y + yy, w, color);
            else span_alpha(x, y + yy, w, color);
            continue;
        }
        if (opaque) span_opaque(x + r, y + yy, w - 2 * r, color);
        else span_alpha(x + r, y + yy, w - 2 * r, color);
        int cy = yy < r ? r : h - r;
        for (int xx = 0; xx < r; xx++) {
            int cover = 0;
            for (int sy = 1; sy < 8; sy += 2) {
                int dy = yy * 8 + sy - cy * 8;
                for (int sx = 1; sx < 8; sx += 2) {
                    int dx = xx * 8 + sx - r * 8;
                    if (dx * dx + dy * dy <= r * r * 64) cover++;
                }
            }
            if (!cover) continue;
            uint32_t a = ((color >> 24) * (uint32_t)cover + 8) / 16;
            uint32_t c = (color & 0x00FFFFFF) | (a << 24);
            putpixel_alpha(x + xx, y + yy, c);
            putpixel_alpha(x + w - 1 - xx, y + yy, c);
        }
    }
}

static void tb_shadow(tb_rect_t r, int radius) {
    /* Six translucent falloff layers: no framebuffer-sized blur buffer. */
    for (int spread = 12; spread >= 2; spread -= 2) {
        uint32_t a = (uint32_t)(16 - spread);
        tb_round(r.x - spread, r.y + 4 - spread,
                 r.w + spread * 2, r.h + spread * 2,
                 radius + spread, a << 24);
    }
}

static void tb_surface(tb_rect_t r, int radius) {
    tb_shadow(r, radius);
    tb_round(r.x, r.y, r.w, r.h, radius, 0xFF36373A);
    tb_round(r.x + 1, r.y + 1, r.w - 2, r.h - 2, radius - 1, 0xFF111214);
    /* Very restrained vertical highlight; never a bright bevel. */
    for (int yy = 0; yy < r.h - 2; yy++) {
        uint32_t a = (uint32_t)(9 - (yy * 9) / (r.h - 2));
        /* Follow the corner arc instead of jumping from `radius` to 2 and back,
         * which left a flat rectangular band at each end of the surface. */
        int inset = 2;
        int arc = corner_inset_top(radius, yy);
        int arc_b = corner_inset_bottom(radius, yy, r.h - 2);
        if (arc_b > arc) arc = arc_b;
        if (arc > inset) inset = arc;
        if (inset * 2 >= r.w) continue;
        span_alpha(r.x + inset, r.y + 1 + yy, r.w - inset * 2,
                   (a << 24) | 0x00FFFFFF);
    }
}

static int tb_window_for(int app) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].used && windows[i].app == app) return i;
    return -1;
}

static void taskbar_layout(tb_layout_t *t) {
    static const int pins[4] = {APP_EXPLORER, APP_NOTEPAD, APP_TERM, APP_SETTINGS};
    int margin = (fb_width < 720) ? 8 : TASKBAR_MARGIN;
    int w = (int)fb_width - margin * 2;
    if (w > TASKBAR_MAX_W) w = TASKBAR_MAX_W;
    if (w < 1) w = 1;
    t->panel = tb_rect(((int)fb_width - w) / 2,
        (int)fb_height - TASKBAR_MARGIN - TASKBAR_PANEL_H, w, TASKBAR_PANEL_H);
    int y = t->panel.y + (TASKBAR_PANEL_H - TASKBAR_BUTTON) / 2;
    int right = t->panel.x + w - 12;
    t->clock = tb_rect(right - 52, y, 52, TASKBAR_BUTTON);
    t->language = tb_rect(right - 90, y, 34, TASKBAR_BUTTON);
    t->caps = tb_rect(right - 120, y, 26, TASKBAR_BUTTON);
    t->sound = tb_rect(right - 160, y, 36, TASKBAR_BUTTON);
    t->count = 0;
    for (int p = 0; p < 4; p++) {
        t->app[t->count] = pins[p];
        t->window[t->count++] = tb_window_for(pins[p]);
    }
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].used) continue;
        int exists = 0;
        for (int j = 0; j < t->count; j++)
            if (t->app[j] == windows[i].app) exists = 1;
        if (!exists) {
            t->app[t->count] = windows[i].app;
            t->window[t->count++] = i;
        }
    }
    int step = TASKBAR_STEP;
    int available = w - TASKBAR_TRAY_W - 24;
    /* Hide only unopened pins if needed; running windows keep their buttons. */
    while ((t->count + 1) * step > available) {
        int remove = -1;
        for (int j = t->count - 1; j >= 0; j--)
            if (t->window[j] < 0) { remove = j; break; }
        if (remove < 0) break;
        for (int j = remove; j < t->count - 1; j++) {
            t->app[j] = t->app[j + 1];
            t->window[j] = t->window[j + 1];
        }
        t->count--;
    }
    if ((t->count + 1) * step > available) step = available / (t->count + 1);
    if (step < 28) step = 28; /* intended framebuffer modes: 640px and wider */
    int bw = step - 6;
    if (bw > TASKBAR_BUTTON) bw = TASKBAR_BUTTON;
    int group_w = (t->count + 1) * step - 6;
    int gx = ((int)fb_width - group_w) / 2;
    int limit = t->sound.x - 16;
    if (gx + group_w > limit) gx = limit - group_w;
    if (gx < t->panel.x + 12) gx = t->panel.x + 12;
    t->start = tb_rect(gx, y, bw, TASKBAR_BUTTON);
    for (int j = 0; j < t->count; j++)
        t->item[j] = tb_rect(gx + (j + 1) * step, y, bw, TASKBAR_BUTTON);
}

static int tb_panel_contains(tb_rect_t r, int x, int y) {
    if (!tb_contains(r, x, y)) return 0;
    int radius = TASKBAR_RADIUS;
    int dx = x - r.x, dy = y - r.y;
    if (dx >= radius && dx < r.w - radius) return 1;
    if (dy >= radius && dy < r.h - radius) return 1;
    dx = dx < radius ? radius - dx : dx - (r.w - radius - 1);
    dy = dy < radius ? radius - dy : dy - (r.h - radius - 1);
    return dx * dx + dy * dy <= radius * radius;
}

static int taskbar_hit(const tb_layout_t *t, int x, int y) {
    if (!tb_panel_contains(t->panel, x, y)) return TB_HIT_NONE;
    if (tb_contains(t->start, x, y)) return TB_HIT_START;
    if (tb_contains(t->sound, x, y)) return TB_HIT_SOUND;
    if (tb_contains(t->language, x, y)) return TB_HIT_LANG;
    if (tb_contains(t->caps, x, y)) return TB_HIT_CAPS;
    if (tb_contains(t->clock, x, y)) return TB_HIT_CLOCK;
    for (int i = 0; i < t->count; i++)
        if (tb_contains(t->item[i], x, y)) return i;
    return TB_HIT_NONE;
}

/* Floating start card: 360x420 with 10px corners, anchored over the dock. */
#define SM_W        360
#define SM_H        470
#define SM_RADIUS   10
#define SM_PAD      14
#define SM_CELL_W   106
#define SM_CELL_H   68
#define SM_CELL_GX  7
#define SM_CELL_GY  4
#define SM_GRID_X   SM_PAD
#define SM_GRID_Y   118
#define SM_BAR_Y    416
#define SM_BAR_H    34

static tb_rect_t tb_start_popup(const tb_layout_t *t) {
    int x = t->start.x + t->start.w / 2 - SM_W / 2;
    if (x < 14) x = 14;
    if (x + SM_W > (int)fb_width - 14) x = (int)fb_width - SM_W - 14;
    int y = t->panel.y - SM_H - 10;
    if (y < 8) y = 8;
    return tb_rect(x, y, SM_W, SM_H);
}

static tb_rect_t tb_volume_popup(const tb_layout_t *t) {
    int x = t->sound.x + t->sound.w / 2 - 90;
    if (x + 180 > t->panel.x + t->panel.w) x = t->panel.x + t->panel.w - 180;
    if (x < 8) x = 8;
    return tb_rect(x, t->panel.y - 112, 180, 100);
}

static void tb_button(tb_rect_t r, int hover, int active) {
    if (!hover && !active) return;
    uint32_t edge = active ? 0xFF414349 : 0xFF35373C;
    uint32_t face = active ? 0xFF2C2E33 : 0xFF25272B;
    if (hover && tb_pressed) face = 0xFF1D1F23;
    else if (hover && active) face = 0xFF34363C;
    tb_round(r.x, r.y, r.w, r.h, 9, edge);
    tb_round(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 8, face);
}

/* Purpose-built 24px glyphs, with consistent optical size and no text tiles. */
static void tb_icon(int x, int y, int app) {
    if (app == APP_NONE) {
        for (int row = 0; row < 2; row++)
            for (int col = 0; col < 2; col++)
                tb_round(x + col * 12, y + row * 12, 10, 10, 2,
                         row ? 0xFF48A7F6 : 0xFF75CAFF);
    } else if (app == APP_EXPLORER) {
        tb_round(x + 1, y + 4, 11, 7, 2, 0xFFCCA34C);
        tb_round(x + 1, y + 7, 22, 15, 3, 0xFFDBAE50);
        tb_round(x + 3, y + 8, 18, 5, 1, 0xFFF2E6CA);
        tb_round(x + 1, y + 11, 22, 11, 3, 0xFFF1C563);
    } else if (app == APP_NOTEPAD || app == APP_WORD) {
        tb_round(x + 4, y + 1, 17, 22, 3, 0xFFDDEAF4);
        tb_round(x + 4, y + 1, 17, 5, 2, 0xFF74BDF0);
        for (int j = 0; j < 3; j++)
            fill_rect(x + 8, y + 9 + j * 4, j == 2 ? 7 : 10, 1, 0xFF6C91B1);
        if (app == APP_WORD) tb_round(x + 1, y + 9, 5, 11, 1, 0xFF478CE1);
    } else if (app == APP_AICHAT) {
        /* Speech bubble: violet shell, dark interior, tail and three dots. */
        tb_round(x + 1, y + 2, 22, 17, 5, 0xFF8B7BF2);
        tb_round(x + 2, y + 3, 20, 15, 4, 0xFF221A38);
        tb_round(x + 4, y + 16, 6, 6, 1, 0xFF8B7BF2);
        for (int j = 0; j < 3; j++)
            tb_round(x + 6 + j * 5, y + 8, 3, 4, 1, 0xFFBFAFFF);
        fill_rect(x + 17, y + 4, 2, 2, 0xFFE9E3FF);
    } else if (app == APP_TERM) {
        tb_round(x, y + 2, 24, 20, 4, 0xFF707781);
        tb_round(x + 1, y + 3, 22, 18, 3, 0xFF202730);
        for (int j = 0; j < 4; j++) {
            fill_rect(x + 5 + j, y + 8 + j, 2, 1, 0xFFDBE9F5);
            fill_rect(x + 5 + j, y + 14 - j, 2, 1, 0xFFDBE9F5);
        }
        tb_round(x + 12, y + 15, 7, 2, 1, 0xFF9DD3F4);
    } else if (app == APP_SETTINGS) {
        tb_round(x + 3, y + 3, 18, 18, 9, 0xFFADB8C6);
        for (int j = 0; j < 2; j++) {
            tb_round(x + 9, y + j * 19, 6, 5, 1, 0xFFADB8C6);
            tb_round(x + j * 19, y + 9, 5, 6, 1, 0xFFADB8C6);
        }
        tb_round(x + 7, y + 7, 10, 10, 5, 0xFF434F60);
        tb_round(x + 10, y + 10, 4, 4, 2, 0xFF8DCBFA);
    } else if (app == APP_PAINT) {
        tb_round(x + 1, y + 2, 22, 20, 10, 0xFFE5DDD2);
        tb_round(x + 5, y + 7, 4, 4, 2, 0xFFE47D78);
        tb_round(x + 11, y + 5, 4, 4, 2, 0xFFE2B952);
        tb_round(x + 16, y + 10, 4, 4, 2, 0xFF79B994);
        tb_round(x + 6, y + 14, 4, 4, 2, 0xFF6EABDD);
        tb_round(x + 12, y + 17, 7, 6, 3, 0xFF33353B);
    } else if (app == APP_CALC) {
        tb_round(x + 4, y, 17, 24, 3, 0xFFBBC5D3);
        tb_round(x + 6, y + 3, 13, 5, 1, 0xFF3D5368);
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 3; col++)
                tb_round(x + 6 + col * 5, y + 11 + row * 4, 3, 2, 1,
                         col == 2 ? 0xFF478ED0 : 0xFF64758A);
    } else if (app == APP_MEDIA) {
        tb_round(x, y, 24, 24, 12, 0xFFC196EA);
        for (int j = 0; j < 10; j++)
            fill_rect(x + 9 + j, y + 6 + j / 2, 1, 12 - j, 0xFFF5F0FC);
    } else if (app == APP_STORE) {
        tb_round(x + 7, y + 1, 10, 9, 5, 0xFFA9CFF0);
        tb_round(x + 9, y + 3, 6, 7, 3, 0xFF17191C);
        tb_round(x + 3, y + 7, 19, 16, 3, 0xFF72B8E8);
        fill_rect(x + 10, y + 11, 3, 3, TB_WHITE);
        fill_rect(x + 14, y + 11, 3, 3, TB_WHITE);
        fill_rect(x + 10, y + 15, 3, 3, TB_WHITE);
        fill_rect(x + 14, y + 15, 3, 3, TB_WHITE);
    } else if (app == APP_SHEET) {
        tb_round(x + 3, y + 1, 19, 22, 3, 0xFFA4D8BD);
        fill_rect(x + 6, y + 5, 13, 3, 0xFF37866A);
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 2; col++)
                fill_rect(x + 6 + col * 7, y + 10 + row * 4, 5, 2, 0xFF4B9577);
    } else if (app == APP_RIKKI) {
        tb_round(x + 3, y + 2, 6, 10, 2, 0xFFE5AA71);
        tb_round(x + 15, y + 2, 6, 10, 2, 0xFFE5AA71);
        tb_round(x + 2, y + 7, 20, 16, 7, 0xFFE5AA71);
        tb_round(x + 7, y + 12, 3, 4, 1, 0xFF382C27);
        tb_round(x + 15, y + 12, 3, 4, 1, 0xFF382C27);
        tb_round(x + 11, y + 17, 3, 2, 1, 0xFF9E6060);
    } else if (app == APP_BROWSER) {
        /* mini browser: chrome bar with an address pill + content lines */
        tb_round(x + 1, y + 2, 22, 20, 5, 0xFF1B2740);
        tb_round(x + 2, y + 3, 20, 18, 4, 0xFF243352);
        tb_round(x + 4, y + 5, 16, 5, 2, 0xFF0F1723);
        tb_round(x + 5, y + 6, 8, 3, 1, 0xFF58A6FF);
        fill_rect(x + 4, y + 12, 16, 2, 0xFF7FA8D8);
        fill_rect(x + 4, y + 16, 10, 2, 0xFF5E82AB);
    } else if (app == APP_TASKMGR) {
        tb_round(x + 1, y + 2, 22, 20, 4, 0xFF4D789D);
        fill_rect(x + 6, y + 14, 3, 5, 0xFF71D6A8);
        fill_rect(x + 11, y + 10, 3, 9, 0xFF9CC9F2);
        fill_rect(x + 16, y + 6, 3, 13, 0xFFF0C772);
    } else {
        tb_round(x + 1, y + 2, 22, 20, 5, 0xFF91ACD0);
        fill_rect(x + 5, y + 7, 14, 2, TB_WHITE);
    }
}

static void tb_speaker(int x, int y) {
    uint32_t c = sound_muted ? TB_MUTED : TB_WHITE;
    tb_round(x, y + 7, 5, 7, 1, c);
    for (int j = 0; j < 6; j++) fill_rect(x + 4 + j, y + 7 - j, 1, 7 + j * 2, c);
    if (sound_muted) {
        draw_line(x + 14, y + 7, x + 20, y + 13, c);
        draw_line(x + 14, y + 13, x + 20, y + 7, c);
    } else {
        draw_line(x + 13, y + 6, x + 15, y + 9, c);
        draw_line(x + 15, y + 9, x + 15, y + 12, c);
        draw_line(x + 15, y + 12, x + 13, y + 15, c);
        if (sound_volume > 45) {
            draw_line(x + 17, y + 3, x + 20, y + 8, TB_MUTED);
            draw_line(x + 20, y + 8, x + 20, y + 13, TB_MUTED);
            draw_line(x + 20, y + 13, x + 17, y + 18, TB_MUTED);
        }
    }
}

/* ===== Floating modern start menu: pinned grid, header, power bar ===== */

typedef struct {
    int app;            /* APP_* id, drives tb_icon() */
    const char *label;
    int launch;         /* launch_app() index */
} sm_item_t;

/* Pinned 3x4 grid:
       Files  Term   Notes
       Paint  Calc   Word
       Store  Media  Rikki
       Web    Sheet  Settings                                            */
#define SM_ITEM_COUNT 12
#define SM_GRID_ROWS  4

static const sm_item_t sm_items[SM_ITEM_COUNT] = {
    { APP_EXPLORER, "Files",  5 },
    { APP_TERM,     "Term",   4 },
    { APP_NOTEPAD,  "Notes",  1 },
    { APP_PAINT,    "Paint",  0 },
    { APP_CALC,     "Calc",   3 },
    { APP_WORD,     "Word",  10 },
    { APP_STORE,    "Store",  6 },
    { APP_MEDIA,    "Media",  7 },
    { APP_RIKKI,    "Rikki",  2 },
    { APP_BROWSER,  "Web",   11 },
    { APP_SHEET,    "Sheet",  9 },
    { APP_SETTINGS, "Set",    8 }
};

#define SM_HIT_NONE     (-1)
#define SM_HIT_RESTART  (-2)
#define SM_HIT_SHUTDOWN (-3)

#define SM_GLYPH_POWER   0
#define SM_GLYPH_RESTART 1

static int sm_hover = SM_HIT_NONE;

/* Integer circle so the power/restart glyphs need no FPU. */
static void sm_ring(int cx, int cy, int r, uint32_t col, int glyph) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        int dxs[8] = { x,  y, -y, -x, -x, -y,  y,  x };
        int dys[8] = { y,  x,  x,  y, -y, -x, -x, -y };
        for (int i = 0; i < 8; i++) {
            int dx = dxs[i], dy = dys[i];
            if (glyph == SM_GLYPH_POWER) {
                if (dy <= -5 && dx >= -2 && dx <= 2) continue;  /* top notch   */
            } else {
                if (dx >= 3 && dy <= -3) continue;              /* clock notch */
            }
            putpixel(cx + dx, cy + dy, col);
        }
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

static void sm_power_button(int x, int y, int w, int h, const char *label,
                            int glyph, int hover, uint32_t face, uint32_t face_hi,
                            uint32_t glyph_col) {
    tb_round(x, y, w, h, 9, hover ? face_hi : face);
    int cx = x + 22, cy = y + h / 2;
    sm_ring(cx, cy, 7, glyph_col, glyph);
    if (glyph == SM_GLYPH_POWER) {
        fill_rect(cx - 1, cy - 8, 2, 6, glyph_col);              /* power stem */
    } else {
        for (int j = 0; j < 3; j++)
            fill_rect(cx + 1 + j, cy - 8 + j, 3 - j, 1, glyph_col); /* arrow */
    }
    int lw = (int)strlen(label) * 8;
    draw_string(x + (w - lw) / 2 + 8, y + (h - 16) / 2, label,
                hover ? TB_WHITE : 0xFFDDE1E8);
}

static int sm_hit_test(tb_rect_t r, int px, int py) {
    if (py >= r.y + SM_BAR_Y && py < r.y + SM_BAR_Y + SM_BAR_H) {
        int bx = r.x + SM_PAD;
        int bw = (SM_W - SM_PAD * 2 - 8) / 2;
        if (px >= bx && px < bx + bw) return SM_HIT_RESTART;
        if (px >= bx + bw + 8 && px < bx + bw + 8 + bw) return SM_HIT_SHUTDOWN;
        return SM_HIT_NONE;
    }
    for (int i = 0; i < SM_ITEM_COUNT; i++) {
        int col = i % 3, row = i / 3;
        int cx = r.x + SM_GRID_X + col * (SM_CELL_W + SM_CELL_GX);
        int cy = r.y + SM_GRID_Y + row * (SM_CELL_H + SM_CELL_GY);
        if (point_in_rect(px, py, cx, cy, SM_CELL_W, SM_CELL_H)) return i;
    }
    return SM_HIT_NONE;
}

/* Hover needs a recomposite; the dock already repaints on the same rule.
   Reuses the caller's layout so the taskbar is only laid out once per event. */
static void start_menu_hover_update(const tb_layout_t *t) {
    int h = SM_HIT_NONE;
    if (start_open) h = sm_hit_test(tb_start_popup(t), mouse_x, mouse_y);
    if (h != sm_hover) { sm_hover = h; need_redraw = 1; }
}

static void draw_taskbar(void) {
    tb_layout_t t;
    taskbar_layout(&t);
    tb_surface(t.panel, TASKBAR_RADIUS);
    tb_button(t.start, tb_hover == TB_HIT_START, start_open);
    tb_icon(t.start.x + (t.start.w - 24) / 2, t.start.y + 7, APP_NONE);
    for (int i = 0; i < t.count; i++) {
        int active = t.window[i] >= 0 && t.window[i] == focused;
        tb_rect_t r = t.item[i];
        tb_button(r, tb_hover == i, active);
        tb_icon(r.x + (r.w - 24) / 2, r.y + 6, t.app[i]);
        if (t.window[i] >= 0) {
            int iw = active ? 16 : 5;
            tb_round(r.x + (r.w - iw) / 2, r.y + 35, iw, 3, 1,
                     active ? TB_ACCENT : 0xFF7B8088);
        }
    }
    fill_rect_alpha(t.sound.x - 10, t.panel.y + 19, 1, 20, 0x22FFFFFF);
    tb_button(t.sound, tb_hover == TB_HIT_SOUND, sound_popup_open);
    tb_button(t.language, tb_hover == TB_HIT_LANG, 0);
    tb_button(t.caps, tb_hover == TB_HIT_CAPS, kbd_caps);
    tb_button(t.clock, tb_hover == TB_HIT_CLOCK, 0);
    tb_speaker(t.sound.x + 7, t.sound.y + 9);
    draw_string(t.language.x + 9, t.language.y + 13,
                current_lang == LANG_RU ? "RU" : "EN", TB_WHITE);
    /* CapsLock is a quiet up-arrow indicator, not a permanently loud badge. */
    int cx = t.caps.x + 8, cy = t.caps.y + 13;
    uint32_t cc = kbd_caps ? TB_ACCENT : 0xFF646A74;
    draw_line(cx, cy + 5, cx + 5, cy, cc);
    draw_line(cx + 5, cy, cx + 10, cy + 5, cc);
    fill_rect(cx + 4, cy + 4, 3, 6, cc);
    fill_rect(cx + 2, cy + 12, 7, 2, cc);

    uint8_t hh, mm, ss;
    char clock[6];
    get_rtc_time(&hh, &mm, &ss);
    clock[0] = (char)('0' + hh / 10); clock[1] = (char)('0' + hh % 10);
    clock[2] = ':';
    clock[3] = (char)('0' + mm / 10); clock[4] = (char)('0' + mm % 10);
    clock[5] = '\0';
    draw_string(t.clock.x + 6, t.clock.y + 13, clock, TB_WHITE);
}

static void draw_start_menu(void) {
    tb_layout_t t;
    taskbar_layout(&t);
    tb_rect_t r = tb_start_popup(&t);
    int x = r.x, y = r.y;

    /* Floating card: dark shell surface with the same restrained border as the dock. */
    tb_surface(r, SM_RADIUS);

    /* ---- Header: mini-cat avatar + user tag + mock search field ---- */
    tb_round(x + 16, y + 14, 38, 38, 12, 0xFF2A2F38);
    tb_round(x + 17, y + 15, 36, 36, 11, 0xFF17191D);
    tb_icon(x + 23, y + 21, APP_RIKKI);
    draw_string(x + 64, y + 18, "User", TB_WHITE);
    draw_string(x + 64, y + 35, "Local account", TB_MUTED);

    tb_round(x + 16, y + 60, SM_W - 32, 30, 9, 0xFF2A2D33);
    tb_round(x + 17, y + 61, SM_W - 34, 28, 8, 0xFF14161A);
    {   /* magnifier glyph */
        int gx = x + 33, gy = y + 74;
        int rx = 4, ry = 0, err = 1 - 4;
        while (rx >= ry) {
            putpixel(gx + rx, gy + ry, TB_MUTED); putpixel(gx + ry, gy + rx, TB_MUTED);
            putpixel(gx - ry, gy + rx, TB_MUTED); putpixel(gx - rx, gy + ry, TB_MUTED);
            putpixel(gx - rx, gy - ry, TB_MUTED); putpixel(gx - ry, gy - rx, TB_MUTED);
            putpixel(gx + ry, gy - rx, TB_MUTED); putpixel(gx + rx, gy - ry, TB_MUTED);
            ry++;
            if (err < 0) err += 2 * ry + 1;
            else { rx--; err += 2 * (ry - rx) + 1; }
        }
        draw_line(gx + 3, gy + 3, gx + 6, gy + 6, TB_MUTED);
        draw_string(x + 48, y + 67, "Search apps", 0xFF767B85);
    }

    /* ---- Pinned 3x4 app grid with hover states ---- */
    draw_string(x + 20, y + 99, "Pinned", TB_MUTED);
    for (int i = 0; i < SM_ITEM_COUNT; i++) {
        int col = i % 3, row = i / 3;
        int cx = x + SM_GRID_X + col * (SM_CELL_W + SM_CELL_GX);
        int cy = y + SM_GRID_Y + row * (SM_CELL_H + SM_CELL_GY);
        int hov = (sm_hover == i);
        if (hov) {
            tb_round(cx, cy, SM_CELL_W, SM_CELL_H, 10, 0xFF3A3E46);
            tb_round(cx + 1, cy + 1, SM_CELL_W - 2, SM_CELL_H - 2, 9, 0xFF22262D);
        }
        tb_icon(cx + (SM_CELL_W - 24) / 2, cy + 8, sm_items[i].app);
        int lw = (int)strlen(sm_items[i].label) * 8;
        draw_string(cx + (SM_CELL_W - lw) / 2, cy + 42, sm_items[i].label,
                    hov ? TB_WHITE : 0xFFC9CED8);
    }

    /* ---- Bottom bar: power actions wired to the QEMU port/ACPI routines ---- */
    fill_rect_alpha(x + SM_PAD, y + 406, SM_W - SM_PAD * 2, 1, 0x26FFFFFF);
    int bw = (SM_W - SM_PAD * 2 - 8) / 2;
    int by = y + SM_BAR_Y;
    sm_power_button(x + SM_PAD, by, bw, SM_BAR_H, "Restart", SM_GLYPH_RESTART,
                    sm_hover == SM_HIT_RESTART, 0xFF2B2E35, 0xFF383C45, TB_ACCENT);
    sm_power_button(x + SM_PAD + bw + 8, by, bw, SM_BAR_H, "Shut down", SM_GLYPH_POWER,
                    sm_hover == SM_HIT_SHUTDOWN, 0xFF3A2126, 0xFF4C2A31, 0xFFFF8A8A);
}

static void draw_volume_popup(void) {
    if (!sound_popup_open) return;
    tb_layout_t t;
    taskbar_layout(&t);
    tb_rect_t r = tb_volume_popup(&t);
    int x = r.x, y = r.y, w = r.w;
    tb_surface(r, TASKBAR_RADIUS);
    draw_string(x + 12, y + 10, "Volume", TB_WHITE);
    char vol_str[8];
    itoa(sound_volume, vol_str, 10);
    uint32_t vlen = strlen(vol_str);
    vol_str[vlen] = '%'; vol_str[vlen + 1] = '\0';
    draw_string(x + w - 45, y + 10, sound_muted ? "0%" : vol_str, TB_ACCENT);
    int bar_x = x + 12, bar_y = y + 44, bar_w = w - 24;
    tb_round(bar_x, bar_y, bar_w, 4, 2, 0xFF454951);
    int fill_w = sound_muted ? 0 : (bar_w * sound_volume) / 100;
    if (fill_w > 0) tb_round(bar_x, bar_y, fill_w, 4, 2, TB_ACCENT);
    tb_round(bar_x + fill_w - 6, bar_y - 4, 12, 12, 6, TB_WHITE);
    tb_round(bar_x + fill_w - 3, bar_y - 1, 6, 6, 3, TB_ACCENT);
    int btn_y = y + 62;
    tb_round(bar_x, btn_y, bar_w, 26, 7, sound_muted ? 0xFF394454 : 0xFF292C31);
    const char *label = sound_muted ? "Unmute" : "Mute";
    draw_string(bar_x + (bar_w - (int)strlen(label) * 8) / 2, btn_y + 6, label, TB_WHITE);
}

/* Compositor — paints the whole scene into the back buffer (cursor apart) */
static void compositor(void) {
    draw_wallpaper();
    draw_icons();
    sort_windows();
    for (int i = 0; i < MAX_WINDOWS && z_order[i] >= 0; i++) {
        int idx = z_order[i];
        draw_window(&windows[idx], idx == focused);
    }
    draw_taskbar();
    if (start_open) draw_start_menu();
    draw_volume_popup();
    draw_toast();
    draw_exec_overlay();   /* guest layer sits above everything else */
    /* the cursor is drawn by frame_present_full() / frame_present_cursor() */
}

/* Full frame: scene + cursor + single full present */
static void frame_present_full(void) {
    compositor();
    cursor_saved = 0;          /* back buffer is clean again after compositing */
    cursor_capture();          /* remember the pixels under the cursor       */
    draw_cursor(cursor_sx, cursor_sy); /* use the captured position even if an IRQ moves the mouse */
    blit_backbuffer();
}

/* Cheap frame: cursor only — restores the previous 16x20 patch, draws the
   cursor at the new spot and presents just the two small rectangles. */
static void frame_present_cursor(void) {
    int old_x = cursor_sx, old_y = cursor_sy;
    int had_saved = cursor_saved;
    if (had_saved) cursor_restore();  /* erase old cursor from back buffer */
    cursor_capture();                 /* snapshot clean pixels at new spot  */
    draw_cursor(cursor_sx, cursor_sy); /* use the captured position even if an IRQ moves the mouse */
    if (had_saved) fb_blit_rect(old_x, old_y, CURSOR_W, CURSOR_H);
    fb_blit_rect(cursor_sx, cursor_sy, CURSOR_W, CURSOR_H);
}

/* ===== Boot screen =====
 * The progress animation is driven by IRQ0/PIT ticks. This avoids a
 * CPU-speed-dependent busy wait and lets hardware interrupts stay healthy. */
#define BOOTSCREEN_TICKS_PER_STEP 2u
static int bootscreen_progress;

static void bootscreen_render(int progress, const char *status) {
    const uint32_t bg = 0xFF090E18;
    const uint32_t panel = 0xFF141D2D;
    const uint32_t accent = 0xFF67B7FF;
    const int logo_w = 10 * 8;
    const int bar_w = 360;
    const int bar_h = 10;
    int logo_x = ((int)fb_width - logo_w) / 2;
    int logo_y = (int)fb_height / 2 - 86;
    int bar_x = ((int)fb_width - bar_w) / 2;
    int bar_y = (int)fb_height - 92;
    char percent[8];

    fill_rect(0, 0, (int)fb_width, (int)fb_height, bg);
    fill_rounded_rect(logo_x - 22, logo_y - 22, logo_w + 44, 76, 12, panel);
    /* Pixel mark beside the text logo. */
    fill_rect(logo_x - 10, logo_y - 5, 6, 26, accent);
    fill_rect(logo_x - 16, logo_y + 1, 6, 14, 0xFF9AD0FF);
    fill_rect(logo_x - 22, logo_y + 7, 6, 2, 0xFF5C84B5);
    draw_string(logo_x, logo_y, "shkodya-os", COLOR_WHITE);
    draw_string(logo_x, logo_y + 22, "desktop environment", COLOR_MUTED);

    fill_rounded_rect(bar_x, bar_y, bar_w, bar_h, 5, 0xFF27354B);
    if (progress > 0)
        fill_rounded_rect(bar_x, bar_y, (bar_w * progress) / 100, bar_h, 5, accent);
   draw_string(bar_x, bar_y - 25, status ? status : "Starting...", 0xFFFFFFFF);
    itoa(progress, percent, 10);
    strcat_c(percent, "%");
    draw_string(bar_x + bar_w - 28, bar_y - 25, percent, COLOR_MUTED);
    blit_backbuffer();
}

static void bootscreen_wait_ticks(uint32_t ticks) {
    uint32_t start = system_ticks;
    while ((uint32_t)(system_ticks - start) < ticks) hlt();
}

static void bootscreen_step(int target, const char *status) {
    if (target < 0) target = 0;
    if (target > 100) target = 100;
    while (bootscreen_progress < target) {
        int next = bootscreen_progress + 2;
        if (next > target) next = target;
        bootscreen_progress = next;
        bootscreen_render(bootscreen_progress, status);
        bootscreen_wait_ticks(BOOTSCREEN_TICKS_PER_STEP);
    }
}

/* Input Routing & App Launching */
/* launch_app() indexes its switch by launch order, which is also the desktop
   icon and Start menu order. Map the APP_* id onto it in one place so the two
   cannot drift apart now that icons carry a real app id. */
static int launch_index_for_app(int app) {
    switch (app) {
        case APP_PAINT:    return 0;
        case APP_NOTEPAD:  return 1;
        case APP_RIKKI:    return 2;
        case APP_CALC:     return 3;
        case APP_TERM:     return 4;
        case APP_EXPLORER: return 5;
        case APP_STORE:    return 6;
        case APP_MEDIA:    return 7;
        case APP_SETTINGS: return 8;
        case APP_SHEET:    return 9;
        case APP_WORD:     return 10;
        case APP_BROWSER:  return 11;
        case APP_AICHAT:   return 12;
        case APP_TASKMGR:  return 13;
        default:           return -1;
    }
}

static void launch_app(int which) {
    int ox, oy;
    if (which < 0) return;
    ox = 120 + (which % 4) * 28;
    oy = 40 + (which % 4) * 24;
    switch (which) {
        case 0: open_window(APP_PAINT,    "Paint",           ox, oy, 640, 540); break;
        case 1: open_window(APP_NOTEPAD,  "Notepad",         ox, oy, 520, 380); break;
        case 2: open_window(APP_RIKKI,    "Rikki Assistant", ox, oy, 460, 450); break;
        case 3: open_window(APP_CALC,     "Calculator",      ox, oy, 280, 420); break;
        case 4: open_window(APP_TERM,     "Terminal",        ox, oy, 640, 400); break;
        case 5: open_window(APP_EXPLORER, "Explorer",        ox, oy, 680, 420); break;
        case 6: open_window(APP_STORE,    "Shkodya Store",   ox, oy, 820, 580); break;
        case 7: open_window(APP_MEDIA,    "Media Player",    ox, oy, 620, 420); break;
        case 8: open_window(APP_SETTINGS, "Settings",        ox, oy, 560, 420); break;
        case 9: open_window(APP_SHEET,    "ShkSheet",        ox, oy, 640, 460); break;
        case 10: open_window(APP_WORD,    "ShkWord",         ox, oy, 560, 420); break;
        case 11: open_window(APP_BROWSER, "Shkodya Web",     ox, oy, 860, 600); break;
        case 12: open_window(APP_AICHAT,  "Ai Chat",         ox, oy, 560, 440); break;
        case 13: open_window(APP_TASKMGR, "Task Manager",    ox, oy, 560, 420); break;
    }
    sound_click();
}

/* Grab / carry / drop. Runs every iteration so the release is seen too; a press
   that never crosses DESKTOP_DRAG_SLOP is treated as a plain click and launches
   the app, which is what keeps single-click launch working. */
static void desktop_icons_update(void) {
    if (start_open) return;                    /* the menu owns the pointer */

    if (!mouse_left) {
        if (desktop_drag_idx < 0) return;
        {
            desktop_icon_t *ic = &desktop_icons[desktop_drag_idx];
            ic->dragging = 0;
            desktop_drag_idx = -1;
            if (desktop_drag_moved) {
                desktop_icon_snap(ic);
                toast_show("Icon moved");
            } else {
                launch_app(launch_index_for_app(ic->app));
            }
            need_redraw = 1;
        }
        return;
    }

    if (desktop_drag_idx < 0) {
        int hit = desktop_icon_hit(mouse_x, mouse_y);
        if (hit < 0) return;
        desktop_drag_idx = hit;
        desktop_drag_moved = 0;
        desktop_drag_press_x = mouse_x;
        desktop_drag_press_y = mouse_y;
        desktop_drag_grab_x = mouse_x - desktop_icons[hit].x;
        desktop_drag_grab_y = mouse_y - desktop_icons[hit].y;
        desktop_icons[hit].dragging = 1;
        need_redraw = 1;
        return;
    }

    if (!desktop_drag_moved) {
        if (desktop_abs(mouse_x - desktop_drag_press_x) <= DESKTOP_DRAG_SLOP &&
            desktop_abs(mouse_y - desktop_drag_press_y) <= DESKTOP_DRAG_SLOP)
            return;
        desktop_drag_moved = 1;
    }

    {
        desktop_icon_t *ic = &desktop_icons[desktop_drag_idx];
        int nx = mouse_x - desktop_drag_grab_x;
        int ny = mouse_y - desktop_drag_grab_y;
        int max_x = (int)fb_width - DESKTOP_ICON_W;
        int max_y = (int)fb_height - 14 - 58 - DESKTOP_ICON_HIT_H;  /* clear of the dock */
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx > max_x) nx = max_x;
        if (ny > max_y) ny = max_y;
        if (nx != ic->x || ny != ic->y) {
            ic->x = nx;
            ic->y = ny;
            need_redraw = 1;
        }
    }
}

/* Clicking outside the card (desktop, dock, another window) closes the menu. */
static void handle_start_click(int px, int py) {
    tb_layout_t t;
    taskbar_layout(&t);
    tb_rect_t r = tb_start_popup(&t);
    if (!tb_contains(r, px, py)) { start_open = 0; return; }

    int hit = sm_hit_test(r, px, py);
    if (hit >= 0 && hit <= 8) {
        launch_app(sm_items[hit].launch);   /* app icon click closes the menu */
        start_open = 0;
    } else if (hit == SM_HIT_RESTART) {
        sound_click();
        system_reboot();                    /* PS/2 0xFE reset line */
    } else if (hit == SM_HIT_SHUTDOWN) {
        sound_click();
        system_shutdown();                  /* QEMU ACPI ports 0x604 / 0xB004 */
    }
}

static void handle_calc_click(window_t *win, int px, int py) {
    calc_layout_t L;
    char k;
    int idx;

    /* Same table and the same layout the painter used -- the old version kept
       its own copy of the geometry and had drifted 2px out of step with it. */
    calc_get_layout(win, &L);
    idx = calc_hit_test(&L, px, py);
    if (idx < 0) return;
    k = calc_key_at(idx);

    if (k >= '0' && k <= '9')                          calc_digit(k);
    else if (k == 'C')                                 calc_reset();
    else if (k == '=')                                 calc_apply(0);
    else if (k == '+' || k == '-' || k == '*' || k == '/') calc_apply(k);
    need_redraw = 1;
}

/* Keeps the footer's X/Y readout live. Throttled: the readout does not need a
   full recomposite for every mouse IRQ, and the loop revisits within a tick. */
static void paint_hover_update(void) {
    int nx = -1, ny = -1;
    if (focused >= 0 && windows[focused].used && windows[focused].app == APP_PAINT) {
        paint_layout_t L;
        paint_get_layout(&windows[focused], &L);
        if (point_in_rect(mouse_x, mouse_y, L.canvas_x, L.canvas_y, L.canvas_w, L.canvas_h)) {
            nx = mouse_x - L.canvas_x;
            ny = mouse_y - L.canvas_y;
        }
    }
    if (nx == paint_cursor_x && ny == paint_cursor_y) return;
    if (system_ticks - paint_cursor_tick < 3) return;
    paint_cursor_tick = system_ticks;
    paint_cursor_x = nx;
    paint_cursor_y = ny;
    need_redraw = 1;
}

static void handle_paint_click(window_t *win, int mx, int my, int is_down) {
    paint_layout_t L;
    paint_get_layout(win, &L);

    /* Toolbar hits first, so a control click can never paint on the canvas. */
    if (point_in_rect(mx, my, L.bar_x, L.bar_y, L.bar_w, L.bar_h)) {
        paint_drawing = 0; paint_last_x = -1; paint_last_y = -1;
        for (int i = 0; i < PAINT_PALETTE_SIZE; i++) {
            int px = L.sw_x + i * L.sw_step;
            if (point_in_rect(mx, my, px - 3, L.sw_y - 3, L.sw_d + 6, L.sw_d + 6)) {
                paint_color_index = i;
                paint_is_eraser = 0;
                need_redraw = 1;
                return;
            }
        }
        if (point_in_rect(mx, my, L.brush_x, L.brush_y, L.brush_w, L.brush_h)) {
            paint_is_eraser = 0; need_redraw = 1; return;
        }
        if (point_in_rect(mx, my, L.eraser_x, L.eraser_y, L.eraser_w, L.eraser_h)) {
            paint_is_eraser = 1; need_redraw = 1; return;
        }
        for (int i = 1; i <= 3; i++) {
            int bx = L.size_x + (i - 1) * L.size_step;
            if (point_in_rect(mx, my, bx, L.size_y, L.size_w, L.size_h)) {
                paint_brush_size = i; need_redraw = 1; return;
            }
        }
        if (point_in_rect(mx, my, L.clear_x, L.clear_y, L.clear_w, L.clear_h)) {
            paint_clear();
            toast_show("Canvas cleared");
            need_redraw = 1;
            return;
        }
        if (point_in_rect(mx, my, L.save_x, L.save_y, L.save_w, L.save_h)) {
            paint_save();
            return;
        }
        return;
    }

    if (is_down && point_in_rect(mx, my, L.canvas_x, L.canvas_y, L.canvas_w, L.canvas_h)) {
        int canvas_x = mx - L.canvas_x, canvas_y = my - L.canvas_y;
        uint32_t color = paint_is_eraser ? COLOR_WHITE : paint_palette[paint_color_index];
        if (paint_drawing && paint_last_x >= 0 && paint_last_y >= 0) {
            paint_stroke(paint_last_x, paint_last_y, canvas_x, canvas_y, color);
        } else {
            paint_draw_brush_point(canvas_x, canvas_y, color);
        }
        paint_last_x = canvas_x; paint_last_y = canvas_y;
        paint_drawing = 1;
        need_redraw = 1;
    } else {
        paint_drawing = 0; paint_last_x = -1; paint_last_y = -1;
    }
}

static void handle_explorer_click(window_t *win, int mx, int my) {
    explorer_layout_t L;
    explorer_get_layout(win, &L);

    /* Toolbar actions first: they must never fall through to a card hit. */
    if (point_in_rect(mx, my, L.up_x, L.up_y, L.up_w, L.up_h)) {
        if (!explorer_path_up()) toast_show("Path is too long");
        need_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, L.new_x, L.new_y, L.new_w, L.new_h)) {
        explorer_new_file();
        return;
    }
    if (point_in_rect(mx, my, L.ref_x, L.ref_y, L.ref_w, L.ref_h)) {
        explorer_refresh();
        return;
    }
    if (point_in_rect(mx, my, L.addr_x, L.addr_y, L.addr_w, L.addr_h)) {
        char notice[EXPLORER_PATH_MAX + 18] = "Current path: ";
        strcat_c(notice, current_path);
        toast_show(notice);
        return;
    }

    /* Logical Explorer locations for the flat v1 filesystem. Once ShkodyaFS
     * gains directory entries these paths can be passed to that layer; today
     * all disk calls remain basename-only and therefore format-safe. */
    if (point_in_rect(mx, my, L.sb_x + 6, L.sb_y + 8, L.sb_w - 12, 22)) {
        explorer_path_reset();
        need_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, L.sb_x + 6, L.sb_y + 34, L.sb_w - 12, 22)) {
        if (!explorer_path_change("/Documents")) toast_show("Path is too long");
        need_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, L.sb_x + 6, L.sb_y + 58, L.sb_w - 12, 22)) {
        /* The sidebar points to the root Downloads location. From root, use
         * the child-entry helper; elsewhere, switch to its absolute path. */
        int ok = (current_path[0] == '/' && current_path[1] == '\0')
            ? explorer_path_enter("Downloads")
            : explorer_path_change("/Downloads");
        if (!ok) toast_show("Path is too long");
        need_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, L.sb_x + 6, L.sb_y + 82, L.sb_w - 12, 22)) {
        explorer_path_reset();
        need_redraw = 1;
        return;
    }

    if (!point_in_rect(mx, my, L.cards_x, L.cards_y, L.cards_w, L.cards_h)) return;

    int cols = (L.cards_w - L.gap) / (L.card_w + L.gap);
    if (cols < 1) cols = 1;
    int start_x = L.cards_x + L.gap;
    int start_y = L.cards_y + L.gap;
    int used = 0;

    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs_files[i].used) continue;
        used++;
        int row = (used - 1) / cols;
        int col = (used - 1) % cols;
        int x = start_x + col * (L.card_w + L.gap);
        int y = start_y + row * (L.card_h + L.gap);
        if (y + L.card_h > L.cards_y + L.cards_h - L.gap) break;
        if (!point_in_rect(mx, my, x, y, L.card_w, L.card_h)) continue;

        /* Load before opening: a failed read must not leave an empty editor. */
        if (explorer_is_word_doc(vfs_files[i].name)) {
            int n = vfs_read_text(vfs_files[i].name, WORD_SIZE);
            if (n < 0) { toast_show(vfs_error_text(n)); return; }
            int wi = open_window(APP_WORD, "ShkWord", win->x + 40, win->y + 30, 560, 420);
            if (wi >= 0) {
                strcpy_c(word_current_file, vfs_files[i].name);
                memcpy(word_text, vfs_text_buffer, (uint32_t)n);
                word_text[n] = '\0';
                word_len = (uint32_t)n;
                word_cursor = n;
                toast_show("Document opened in ShkWord");
            }
            return;
        }
        int n = vfs_read_text(vfs_files[i].name, NOTEPAD_SIZE);
        if (n < 0) { toast_show(vfs_error_text(n)); return; }
        int note_idx = open_window(APP_NOTEPAD, "Notepad", win->x + 40, win->y + 40, 520, 380);
        if (note_idx >= 0) {
            strcpy_c(notepad_current_file, vfs_files[i].name);
            memcpy(notepad_text, vfs_text_buffer, (uint32_t)n);
            notepad_text[n] = '\0';
            notepad_len = (uint32_t)n;
            notepad_cursor = n;
            toast_show("File opened in Notepad");
        }
        return;
    }
}

static void handle_taskmgr_click(window_t *win, int mx, int my) {
    taskmgr_layout_t L;
    int manager_index = taskmgr_window_index(win);
    taskmgr_get_layout(win, &L);

    for (int i = 0, row = 0; i < MAX_WINDOWS; i++) {
        int y;
        if (!windows[i].used) continue;
        y = L.list_y + row * L.row_h;
        if (y + L.row_h > L.kill_y - 6) break;
        if (point_in_rect(mx, my, L.list_x, y, L.list_w, L.row_h - 3)) {
            taskmgr_selected = i;
            need_redraw = 1;
            return;
        }
        row++;
    }

    if (!point_in_rect(mx, my, L.kill_x, L.kill_y, L.kill_w, L.kill_h)) return;
    if (taskmgr_selected < 0 || taskmgr_selected >= MAX_WINDOWS ||
        !windows[taskmgr_selected].used) {
        toast_show("Select a process first");
    } else if (taskmgr_selected == manager_index) {
        toast_show("Task Manager cannot terminate itself");
    } else {
        char notice[64] = "Terminated: ";
        strcat_c(notice, windows[taskmgr_selected].title);
        close_window(taskmgr_selected);
        taskmgr_selected = -1;
        toast_show(notice);
        sound_click();
    }
    need_redraw = 1;
}

static void handle_notepad_toolbar_click(window_t *win, int mx, int my) {
    int tb_y = win->y + TITLEBAR_H + 4;
    if (my >= tb_y && my <= tb_y + 30) {
        int btn_x = win->x + 12;
        int btn_y = tb_y + 4;
        if (point_in_rect(mx, my, btn_x, btn_y, 50, 22)) {
            notepad_new();
            need_redraw = 1;
            return;
        }
        btn_x += 56;
        if (point_in_rect(mx, my, btn_x, btn_y, 50, 22)) {
            notepad_save();
            need_redraw = 1;
            return;
        }
    }
}

static void handle_store_click(window_t *win, int mx, int my) {
    int cx = win->x + 8;
    int cy = win->y + TITLEBAR_H + 4;
    int cw = win->w - 16;

    int hit = store_hit(win, mx, my);
    if (hit == STORE_HIT_LOGOUT) {
        store_current_user = -1;
        store_in_pass[0] = '\0';
        toast_show("Logged out");
        need_redraw = 1;
        return;
    }
    if (hit == STORE_HIT_LOGIN || hit == STORE_HIT_REG) {
        store_view = hit == STORE_HIT_LOGIN ? STORE_VIEW_LOGIN : STORE_VIEW_REG;
        store_in_user[0] = '\0'; store_in_pass[0] = '\0';
        store_field_focus = 0;
        need_redraw = 1;
        return;
    }

    int view_y = cy + 48;

    if (store_view == STORE_VIEW_LOGIN || store_view == STORE_VIEW_REG) {
        if (hit == STORE_HIT_USER) {
            store_field_focus = 0; need_redraw = 1; return;
        }
        if (hit == STORE_HIT_PASS) {
            store_field_focus = 1; need_redraw = 1; return;
        }
        if (hit == STORE_HIT_CANCEL) {
            store_view = STORE_VIEW_CATALOG;
            store_in_pass[0] = '\0';
            need_redraw = 1;
            return;
        }
        if (hit == STORE_HIT_SUBMIT) {
            if (store_view == STORE_VIEW_LOGIN) {
                uint32_t ph = hash_password(store_in_pass);
                int found = -1;
                for (int i = 0; i < store_user_count; i++) {
                    if (strcmp_c(store_users[i].username, store_in_user) == 0 &&
                        store_users[i].password_hash == ph) {
                        found = i; break;
                    }
                }
                if (found >= 0) {
                    store_current_user = found;
                    store_view = STORE_VIEW_CATALOG;
                    toast_show("Signed in successfully!");
                    sound_beep(660, 2);
                } else {
                    toast_show("Invalid username or password");
                    sound_beep(220, 2);
                }
            } else {
                if (strlen(store_in_user) < 3 || strlen(store_in_pass) < 3) {
                    toast_show("Credentials too short (min 3 chars)");
                    sound_beep(220, 2);
                } else if (store_user_count >= MAX_USERS) {
                    toast_show("User limit reached");
                } else {
                    int exists = 0;
                    for (int i = 0; i < store_user_count; i++) {
                        if (strcmp_c(store_users[i].username, store_in_user) == 0) exists = 1;
                    }
                    if (exists) {
                        toast_show("User already exists");
                        sound_beep(220, 2);
                    } else {
                        strcpy_c(store_users[store_user_count].username, store_in_user);
                        store_users[store_user_count].password_hash = hash_password(store_in_pass);
                        store_users[store_user_count].is_dev = 0;
                        store_current_user = store_user_count;
                        store_user_count++;
                        store_view = STORE_VIEW_CATALOG;
                        toast_show("Registration complete!");
                        sound_beep(880, 2);
                    }
                }
            }
            need_redraw = 1;
            return;
        }
        return;
    }

    if (store_view == STORE_VIEW_TTT) {
        if (point_in_rect(mx, my, cx + 12, view_y + 10, 80, 26)) {
            store_view = STORE_VIEW_CATALOG; need_redraw = 1; return;
        }
        int gx = cx + (cw - 180) / 2;
        int gy = view_y + 50;
        if (point_in_rect(mx, my, gx + 40, gy + 226, 100, 26)) {
            ttt_init(); need_redraw = 1; return;
        }
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                int bx = gx + c * 60;
                int by = gy + r * 60;
                if (point_in_rect(mx, my, bx, by, 56, 56) && ttt_winner == 0) {
                    int idx = r * 3 + c;
                    if (ttt_board[idx] == ' ') {
                        ttt_board[idx] = 'X';
                        sound_beep(523, 1);
                        ttt_check_winner();
                        if (ttt_winner == 0) ttt_ai_move();
                        need_redraw = 1;
                        return;
                    }
                }
            }
        }
        return;
    }

    if (store_view == STORE_VIEW_SNAKE) {
        if (point_in_rect(mx, my, cx + 12, view_y + 10, 80, 26)) {
            store_view = STORE_VIEW_CATALOG; need_redraw = 1; return;
        }
        int bx = cx + (cw - 336) / 2;
        int by = view_y + 46;
        if (point_in_rect(mx, my, bx + 118, by + 212, 100, 26)) {
            snake_init(); need_redraw = 1; return;
        }
        return;
    }

    if (store_view == STORE_VIEW_CLICKER) {
        if (point_in_rect(mx, my, cx + 12, view_y + 10, 80, 26)) {
            store_view = STORE_VIEW_CATALOG; need_redraw = 1; return;
        }
        int card_x = cx + 30;
        int card_y = view_y + 50;
        if (point_in_rect(mx, my, card_x, card_y, 240, 240)) {
            clicker_coins += clicker_power;
            sound_beep(880 + (clicker_coins % 200), 1);
            need_redraw = 1;
            return;
        }
        int shop_x = cx + 290;
        int shop_y = view_y + 50;
        int shop_w = cw - 310;
        if (point_in_rect(mx, my, shop_x + shop_w - 90, shop_y + 50, 52, 28)) {
            if (clicker_coins >= 15) {
                clicker_coins -= 15;
                clicker_cps += 1;
                sound_beep(784, 2);
                toast_show("Auto-Paw Upgrade Purchased!");
            } else {
                toast_show("Not enough coins!");
            }
            need_redraw = 1;
            return;
        }
        if (point_in_rect(mx, my, shop_x + shop_w - 90, shop_y + 110, 52, 28)) {
            if (clicker_coins >= 50) {
                clicker_coins -= 50;
                clicker_power += 2;
                sound_beep(1046, 2);
                toast_show("Double Click Upgrade Purchased!");
            } else {
                toast_show("Not enough coins!");
            }
            need_redraw = 1;
            return;
        }
        return;
    }


    if (store_view == STORE_VIEW_2048) {
        if (point_in_rect(mx, my, cx + 12, view_y + 10, 80, 26)) {
            store_view = STORE_VIEW_CATALOG; need_redraw = 1; return;
        }
        int bw = 260;
        int bx = cx + (cw - bw) / 2;
        int by = view_y + 50;
        if (point_in_rect(mx, my, bx + bw / 2 - 50, by + bw + 16, 100, 26)) {
            g2048_init(); need_redraw = 1; return;
        }
        return;
    }

    if (store_view == STORE_VIEW_PONG) {
        if (point_in_rect(mx, my, cx + 12, view_y + 10, 80, 26)) {
            store_view = STORE_VIEW_CATALOG; need_redraw = 1; return;
        }
        int fw = 340, fh = 180;
        int fx = cx + (cw - fw) / 2;
        int fy = view_y + 50;
        if (point_in_rect(mx, my, fx + fw / 2 - 50, fy + fh + 14, 100, 26)) {
            pong_init(); need_redraw = 1; return;
        }
        return;
    }

    if (store_view == STORE_VIEW_MEMORY) {
        if (point_in_rect(mx, my, cx + 12, view_y + 10, 80, 26)) {
            store_view = STORE_VIEW_CATALOG; need_redraw = 1; return;
        }
        int cols = 4;
        int card_w = 72, card_h = 72, gap = 10;
        int total_w = cols * card_w + (cols - 1) * gap;
        int total_h = 4 * card_h + 3 * gap;
        int bx = cx + (cw - total_w) / 2;
        int by = view_y + 46;
        int i;
        for (i = 0; i < MEM_N; i++) {
            int rr = i / cols, cc = i % cols;
            int x = bx + cc * (card_w + gap);
            int y = by + rr * (card_h + gap);
            if (point_in_rect(mx, my, x, y, card_w, card_h)) {
                mem_click(i);
                return;
            }
        }
        if (point_in_rect(mx, my, bx + total_w / 2 - 50, by + total_h + 14, 100, 26)) {
            mem_init(); need_redraw = 1; return;
        }
        return;
    }

    if (hit == STORE_HIT_PUBLISH) {
        store_publish_game_to_vfs();
        need_redraw = 1;
        return;
    }
    if (hit == STORE_HIT_FEATURE) hit = STORE_HIT_GAME + 1;
    switch (hit - STORE_HIT_GAME) {
        case 0: ttt_init(); store_view = STORE_VIEW_TTT; break;
        case 1: snake_init(); store_view = STORE_VIEW_SNAKE; break;
        case 2: store_view = STORE_VIEW_CLICKER; break;
        case 3: g2048_init(); store_view = STORE_VIEW_2048; break;
        case 4: pong_init(); store_view = STORE_VIEW_PONG; break;
        case 5: mem_init(); store_view = STORE_VIEW_MEMORY; break;
        default: return;
    }
    need_redraw = 1;
}


static void handle_media_click(window_t *win, int mx, int my) {
    int cx = win->x + 8;
    int cy = win->y + TITLEBAR_H + 4;
    int cw = win->w - 16;
    int ch = win->h - TITLEBAR_H - 12;

    int tab_w = 120;
    if (point_in_rect(mx, my, cx + 6, cy + 4, tab_w, 26)) {
        media_mode = MEDIA_MODE_VIDEO;
        sound_stop();
        need_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, cx + tab_w + 12, cy + 4, tab_w, 26)) {
        media_mode = MEDIA_MODE_AUDIO;
        if (media_state == MEDIA_STATE_PLAY) {
            uint16_t freq = chiptune_track[media_note_idx].freq;
            if (freq > 0) sound_raw_on(freq);
        }
        need_redraw = 1;
        return;
    }

    int bar_x = cx + 12;
    int bar_y = cy + ch - 54;
    int bar_w = cw - 24;
    if (point_in_rect(mx, my, bar_x, bar_y - 6, bar_w, 20)) {
        int scrub = ((mx - bar_x) * 1000) / bar_w;
        if (scrub < 0) scrub = 0;
        if (scrub > 1000) scrub = 1000;
        media_progress = (uint32_t)scrub;
        media_curr_tick = (media_progress * media_total_ticks) / 1000;
        if (media_mode == MEDIA_MODE_AUDIO) {
            media_note_idx = (media_progress * CHIPTUNE_LEN) / 1000;
            media_note_tick = 0;
        }
        need_redraw = 1;
        return;
    }

    int ctrl_y = cy + ch - 38;
    if (point_in_rect(mx, my, cx + 12, ctrl_y, 44, 26)) {
        media_state = MEDIA_STATE_PLAY;
        if (media_mode == MEDIA_MODE_AUDIO) {
            uint16_t freq = chiptune_track[media_note_idx].freq;
            if (freq > 0) sound_raw_on(freq);
        }
        need_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, cx + 62, ctrl_y, 48, 26)) {
        media_state = MEDIA_STATE_PAUSE;
        sound_stop();
        need_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, cx + 116, ctrl_y, 44, 26)) {
        media_state = MEDIA_STATE_STOP;
        media_curr_tick = 0;
        media_progress = 0;
        media_note_idx = 0;
        media_note_tick = 0;
        sound_stop();
        need_redraw = 1;
        return;
    }
}

/* ===== Settings click handler ===== */
static void handle_settings_click(window_t *win, int mx, int my) {
    int cx = win->x + 8;
    int cy = win->y + TITLEBAR_H + 4;
    int cw = win->w - 16;
    int tab_w = settings_tab_w(cw);

    /* Tab clicks: Themes / Sound / System / Update */
    for (int i = 0; i < SETTINGS_TAB_COUNT; i++) {
        if (point_in_rect(mx, my, settings_tab_x(cx, cw, i), cy + 4, tab_w, 26)) {
            settings_view = i;
            sound_click();
            need_redraw = 1;
            return;
        }
    }

    int content_y = cy + 40;
    if (settings_view == SETTINGS_VIEW_THEMES) {
        int card_w = (cw - 40) / 3;
        int card_h = 60;
        for (int i = 0; i < THEME_COUNT; i++) {
            int row = i / 3;
            int col = i % 3;
            int tx = cx + 10 + col * (card_w + 8);
            int ty = content_y + 22 + row * (card_h + 8);
            if (point_in_rect(mx, my, tx, ty, card_w, card_h)) {
                theme_apply(i);
                sound_beep(440 + i * 80, 2);
                toast_show("Theme changed!");
                need_redraw = 1;
                return;
            }
        }
    } else if (settings_view == SETTINGS_VIEW_SOUND) {
        /* Mute toggle */
        if (point_in_rect(mx, my, cx + 10, content_y + 68, 100, 26)) {
            sound_muted = !sound_muted;
            if (!sound_muted && sound_volume == 0) sound_volume = 50;
            if (!sound_muted) sound_beep(700, 2); else sound_stop();
            need_redraw = 1; return;
        }
        /* Click sounds toggle */
        if (point_in_rect(mx, my, cx + 10, content_y + 100, 180, 26)) {
            sound_effects_enabled = !sound_effects_enabled;
            if (sound_effects_enabled) sound_click();
            need_redraw = 1; return;
        }
        /* Volume bar */
        int bar_x = cx + 10, bar_y = content_y + 44, bar_w = cw - 20;
        if (mx >= bar_x && mx <= bar_x + bar_w && my >= bar_y - 6 && my <= bar_y + 16) {
            int new_vol = ((mx - bar_x) * 100) / bar_w;
            if (new_vol < 0) new_vol = 0;
            if (new_vol > 100) new_vol = 100;
            if (new_vol != sound_volume) {
                sound_volume = new_vol;
                sound_muted = (sound_volume == 0);
                if (!sound_muted) sound_beep(480, 1);
                else sound_stop();
                need_redraw = 1;
            }
            return;
        }
    } else if (settings_view == SETTINGS_VIEW_UPDATE) {
        update_layout_t U;
        update_layout(cx, cw, content_y, &U);

        if (point_in_rect(mx, my, U.chk_x, U.chk_y, U.chk_w, U.chk_h)) {
            if (update_busy()) return;      /* ignore re-clicks mid exchange */
            update_start_check();
            return;
        }
        if (point_in_rect(mx, my, U.app_x, U.app_y, U.app_w, U.app_h)) {
            if (update_state == UPDATE_READY) {
                toast_show("Applying update - rebooting");
                need_redraw = 1;
                system_reboot();
            } else {
                toast_show("Nothing to apply yet");
                need_redraw = 1;
            }
            return;
        }
    }
}

/* ===== Sheet click handler ===== */
static void handle_sheet_click(window_t *win, int mx, int my) {
    int gx = win->x + 6;
    int gy = win->y + TITLEBAR_H + 6;
    int gw = win->w - 12;
    int label_w = 28;
    int grid_y = gy + 36;

    for (int r = 0; r < SHEET_ROWS; r++) {
        int ry = grid_y + (r + 1) * SHEET_CELL_H;
        if (ry + SHEET_CELL_H > gy + win->h - TITLEBAR_H - 12) break;
        for (int c = 0; c < SHEET_COLS; c++) {
            int cx = gx + label_w + c * SHEET_CELL_W;
            if (cx + SHEET_CELL_W > gx + gw) break;
            if (point_in_rect(mx, my, cx, ry, SHEET_CELL_W, SHEET_CELL_H)) {
                /* Commit previous cell if editing */
                if (sheet_editing) sheet_eval_cell(sheet_sel_row, sheet_sel_col);
                sheet_sel_row = r;
                sheet_sel_col = c;
                sheet_editing = 1;
                sheet_cursor = strlen(sheet_cells[r][c]);
                sound_click();
                need_redraw = 1;
                return;
            }
        }
    }
}

/* ===== Word toolbar click handler ===== */
static void handle_word_toolbar_click(window_t *win, int mx, int my) {
    int tb_y = win->y + TITLEBAR_H + 4;
    int bx = win->x + 12, by = tb_y + 4;
    if (my < tb_y || my > tb_y + 30) return;

    if (point_in_rect(mx, my, bx, by, 36, 22)) { word_bold = !word_bold; sound_click(); need_redraw = 1; return; }
    if (point_in_rect(mx, my, bx + 40, by, 36, 22)) { word_italic = !word_italic; sound_click(); need_redraw = 1; return; }
    if (point_in_rect(mx, my, bx + 84, by, 28, 22)) { word_font_size = 1; sound_click(); need_redraw = 1; return; }
    if (point_in_rect(mx, my, bx + 114, by, 28, 22)) { word_font_size = 2; sound_click(); need_redraw = 1; return; }
    if (point_in_rect(mx, my, bx + 146, by, 28, 22)) { word_font_size = 3; sound_click(); need_redraw = 1; return; }
    if (point_in_rect(mx, my, bx + 184, by, 50, 22)) {
        /* Save */
        if (vfs_write(word_current_file, (const uint8_t *)word_text, word_len) >= 0) {
            char msg[64] = "Saved: ";
            strcat_c(msg, word_current_file);
            toast_show(msg);
            sound_beep(880, 2);
        } else {
            toast_show(vfs_error_text(vfs_last_error));
        }
        need_redraw = 1; return;
    }
    if (point_in_rect(mx, my, bx + 238, by, 50, 22)) {
        /* New */
        word_text[0] = '\0'; word_len = 0; word_cursor = 0;
        strcpy_c(word_current_file, "untitled.txt");
        toast_show("New document created");
        need_redraw = 1; return;
    }
}


/* Central Keyboard Processing */
static void handle_keys(void) {
    uint8_t ch;
    while (kbd_pop(&ch)) {
        if (focused < 0 || !windows[focused].used) continue;
        if (windows[focused].app == APP_NOTEPAD) {
            if (ch == '\b') notepad_backspace();
            else if (ch == '\n' || (ch >= 32 && ch != 127)) notepad_insert((char)ch);
        } else if (windows[focused].app == APP_TERM) {
            term_key((char)ch);
        } else if (windows[focused].app == APP_STORE) {
            if (store_view == STORE_VIEW_LOGIN || store_view == STORE_VIEW_REG) {
                char *target = (store_field_focus == 0) ? store_in_user : store_in_pass;
                uint32_t len = strlen(target);
                if (ch == '\b') {
                    if (len > 0) target[len - 1] = '\0';
                } else if (ch == '\t') {
                    store_field_focus = !store_field_focus;
                } else if (ch >= 32 && ch < 127 && len < USERNAME_LEN - 1) {
                    target[len] = (char)ch;
                    target[len + 1] = '\0';
                }
                need_redraw = 1;
            } else if (store_view == STORE_VIEW_SNAKE) {
                if (ch == 'd' || ch == 'D' || ch == '6') { if (snake_dir != 2) snake_dir = 0; }
                else if (ch == 's' || ch == 'S' || ch == '2') { if (snake_dir != 3) snake_dir = 1; }
                else if (ch == 'a' || ch == 'A' || ch == '4') { if (snake_dir != 0) snake_dir = 2; }
                else if (ch == 'w' || ch == 'W' || ch == '8') { if (snake_dir != 1) snake_dir = 3; }
            } else if (store_view == STORE_VIEW_2048) {
                if (ch == 'a' || ch == 'A' || ch == '4') g2048_move(0);
                else if (ch == 'd' || ch == 'D' || ch == '6') g2048_move(1);
                else if (ch == 'w' || ch == 'W' || ch == '8') g2048_move(2);
                else if (ch == 's' || ch == 'S' || ch == '2') g2048_move(3);
            } else if (store_view == STORE_VIEW_PONG) {
                if (ch == 'w' || ch == 'W' || ch == '8') { if (pong_py > 0) pong_py -= 5; need_redraw = 1; }
                else if (ch == 's' || ch == 'S' || ch == '2') { if (pong_py < 80) pong_py += 5; need_redraw = 1; }
                else if (ch == ' ') { pong_paused = !pong_paused; need_redraw = 1; }
            }
        } else if (windows[focused].app == APP_MEDIA) {

            if (ch == ' ') {
                if (media_state == MEDIA_STATE_PLAY) { media_state = MEDIA_STATE_PAUSE; sound_stop(); }
                else { media_state = MEDIA_STATE_PLAY; }
                need_redraw = 1;
            } else if (ch == 's' || ch == 'S') {
                media_state = MEDIA_STATE_STOP;
                media_curr_tick = 0;
                media_progress = 0;
                sound_stop();
                need_redraw = 1;
            }
        } else if (windows[focused].app == APP_WORD) {
            if (ch == '\b') word_backspace();
            else if (ch == '\n' || (ch >= 32 && ch != 127)) word_insert((char)ch);
            need_redraw = 1;
        } else if (windows[focused].app == APP_AICHAT) {
            ai_chat_key((char)ch);
            need_redraw = 1;
        } else if (windows[focused].app == APP_BROWSER) {
            web_key((char)ch);
        } else if (windows[focused].app == APP_SHEET) {
            if (sheet_editing) {
                if (ch == '\b') sheet_insert_char('\b');
                else if (ch == '\n') sheet_insert_char('\n');
                else if (ch >= 32 && ch != 127) sheet_insert_char((char)ch);
                need_redraw = 1;
            }
        }
    }
}

static void taskbar_activate(const tb_layout_t *t, int item) {
    if (item < 0 || item >= t->count) return;
    if (t->window[item] >= 0) {
        focus_window(t->window[item]);
        return;
    }
    switch (t->app[item]) {
        case APP_EXPLORER: launch_app(5); break;
        case APP_NOTEPAD:  launch_app(1); break;
        case APP_TERM:     launch_app(4); break;
        case APP_SETTINGS: launch_app(8); break;
    }
}

/* Central Mouse & Window Manager Event Processing */
static void handle_mouse_ui(void) {
    int clicked = (mouse_left && !mouse_left_prev);
    tb_layout_t t;
    taskbar_layout(&t);
    int hit = taskbar_hit(&t, mouse_x, mouse_y);
    int hover = dragging ? TB_HIT_NONE : hit;
    int pressed = mouse_left && hover != TB_HIT_NONE;
    /* Repaint on state transitions, not on every pixel of mouse motion. */
    if (hover != tb_hover || pressed != tb_pressed) {
        tb_hover = hover;
        tb_pressed = pressed;
        need_redraw = 1;
    }

    if (dragging) {
        if (!mouse_left) {
            dragging = 0;
        } else if (drag_index >= 0 && windows[drag_index].used) {
            windows[drag_index].x = mouse_x - drag_off_x;
            windows[drag_index].y = mouse_y - drag_off_y;
            clamp_window(&windows[drag_index]);
            need_redraw = 1;
        }
        if (focused >= 0 && windows[focused].used && windows[focused].app == APP_PAINT)
            handle_paint_click(&windows[focused], mouse_x, mouse_y, mouse_left);
        mouse_left_prev = mouse_left;
        return;
    }

    int shell_owns_pointer = tb_panel_contains(t.panel, mouse_x, mouse_y)
        || start_open || sound_popup_open;
    if (mouse_left && !shell_owns_pointer && focused >= 0
        && windows[focused].used && windows[focused].app == APP_PAINT)
        handle_paint_click(&windows[focused], mouse_x, mouse_y, 1);
    if (!mouse_left || shell_owns_pointer) {
        paint_drawing = 0; paint_last_x = -1; paint_last_y = -1;
    }

    if (sound_popup_open) {
        tb_rect_t popup = tb_volume_popup(&t);
        int pw = popup.w;
        int px = popup.x, py = popup.y;
        int bar_x = px + 12, bar_y = py + 38, bar_w = pw - 24, bar_h = 16;
        if (mouse_left) {
            if (mouse_x >= bar_x && mouse_x <= bar_x + bar_w && mouse_y >= bar_y && mouse_y <= bar_y + bar_h) {
                int new_vol = ((mouse_x - bar_x) * 100) / bar_w;
                if (new_vol < 0) new_vol = 0;
                if (new_vol > 100) new_vol = 100;
                if (new_vol != sound_volume) {
                    sound_volume = new_vol;
                    sound_muted = (sound_volume == 0);
                    if (!sound_muted) sound_beep(480, 1);
                    else sound_stop();
                    need_redraw = 1;
                }
                mouse_left_prev = mouse_left;
                return;
            }
            if (clicked && mouse_x >= bar_x && mouse_x <= bar_x + bar_w && mouse_y >= py + 62 && mouse_y <= py + 88) {
                sound_muted = !sound_muted;
                if (!sound_muted) { if (sound_volume == 0) sound_volume = 50; sound_beep(700, 2); }
                else sound_stop();
                need_redraw = 1;
                mouse_left_prev = mouse_left;
                return;
            }
        }
        /* Popup background must not click through to an application. */
        if (tb_contains(popup, mouse_x, mouse_y)) {
            mouse_left_prev = mouse_left;
            return;
        }
        if (clicked && hit != TB_HIT_SOUND) {
            sound_popup_open = 0;
            need_redraw = 1;
            if (!tb_panel_contains(t.panel, mouse_x, mouse_y)) {
                mouse_left_prev = mouse_left;
                return;
            }
        }
    }

    if (clicked && tb_panel_contains(t.panel, mouse_x, mouse_y)) {
        if (hit == TB_HIT_START) {
            start_open = !start_open;
            sound_popup_open = 0;
            sound_click();
        } else if (hit == TB_HIT_SOUND) {
            sound_popup_open = !sound_popup_open;
            start_open = 0;
        } else if (hit == TB_HIT_LANG) {
            current_lang = current_lang == LANG_EN ? LANG_RU : LANG_EN;
            start_open = 0;
        } else if (hit == TB_HIT_CAPS) {
            kbd_caps = !kbd_caps;
            start_open = 0;
        } else {
            start_open = 0;
            if (hit >= 0) taskbar_activate(&t, hit);
        }
        need_redraw = 1;
        mouse_left_prev = mouse_left;
        return;
    }

    start_menu_hover_update(&t);
    paint_hover_update();          /* keeps the Paint footer X/Y readout live */
    calc_hover_update();           /* Calculator keypad hover/press feedback  */
    desktop_icons_update();        /* desktop icon drag-and-drop              */

    if (start_open && clicked) {
        handle_start_click(mouse_x, mouse_y);
        need_redraw = 1;
        mouse_left_prev = mouse_left;
        return;
    }
    if (!clicked) { mouse_left_prev = mouse_left; return; }

    int idx = window_at(mouse_x, mouse_y);
    if (idx >= 0) {
        focus_window(idx);
        if (point_in_rect(mouse_x, mouse_y, windows[idx].x + windows[idx].w - 34, windows[idx].y + 4, 28, 20)) {
            close_window(idx);
            need_redraw = 1;
            mouse_left_prev = mouse_left;
            return;
        }
        if (point_in_rect(mouse_x, mouse_y, windows[idx].x, windows[idx].y, windows[idx].w, TITLEBAR_H)) {
            dragging = 1; drag_index = idx;
            drag_off_x = mouse_x - windows[idx].x;
            drag_off_y = mouse_y - windows[idx].y;
            mouse_left_prev = mouse_left;
            return;
        }
        if (windows[idx].app == APP_BROWSER) {
            handle_browser_click(&windows[idx], mouse_x, mouse_y);
            need_redraw = 1;
        } else if (windows[idx].app == APP_CALC) {
            handle_calc_click(&windows[idx], mouse_x, mouse_y);
            need_redraw = 1;
        } else if (windows[idx].app == APP_PAINT) {
            handle_paint_click(&windows[idx], mouse_x, mouse_y, 1);
            need_redraw = 1;
        } else if (windows[idx].app == APP_NOTEPAD) {
            handle_notepad_toolbar_click(&windows[idx], mouse_x, mouse_y);
        } else if (windows[idx].app == APP_EXPLORER) {
            handle_explorer_click(&windows[idx], mouse_x, mouse_y);
        } else if (windows[idx].app == APP_TASKMGR) {
            handle_taskmgr_click(&windows[idx], mouse_x, mouse_y);
        } else if (windows[idx].app == APP_STORE) {
            handle_store_click(&windows[idx], mouse_x, mouse_y);
        } else if (windows[idx].app == APP_MEDIA) {
            handle_media_click(&windows[idx], mouse_x, mouse_y);
        } else if (windows[idx].app == APP_SETTINGS) {
            handle_settings_click(&windows[idx], mouse_x, mouse_y);
        } else if (windows[idx].app == APP_SHEET) {
            handle_sheet_click(&windows[idx], mouse_x, mouse_y);
        } else if (windows[idx].app == APP_WORD) {
            handle_word_toolbar_click(&windows[idx], mouse_x, mouse_y);
        } else if (windows[idx].app == APP_RIKKI) {
            /* "Pet Rikki" button, or the mascot itself, triggers the chirp + purr. */
            handle_rikki_click(&windows[idx], mouse_x, mouse_y);
            need_redraw = 1;
        }
        mouse_left_prev = mouse_left;
        return;
    }

    /* Desktop icons are handled by desktop_icons_update(), which launches on
       release so that a drag can begin on top of an icon. */
    mouse_left_prev = mouse_left;
}

static multiboot_info_t *g_mbi;

/* Framebuffer & Application Initialization */
static void framebuffer_init(multiboot_info_t *mbi) {
    if (mbi && (mbi->flags & (1 << 12))) {
        fb_width = mbi->framebuffer_width;
        fb_height = mbi->framebuffer_height;
        fb_pitch = mbi->framebuffer_pitch;
        /* The multiboot field is 64-bit; go through uintptr_t so the address
           survives intact on x86_64 while i386 keeps its historical
           truncation (the LFB has always lived below 4 GiB). */
        framebuffer_addr = (uint32_t *)(uintptr_t)mbi->framebuffer_addr;
    } else {
        fb_width = 1024;
        fb_height = 768;
        fb_pitch = fb_width * 4;
        framebuffer_addr = (uint32_t *)0xE0000000;
    }
}

void kernel_main(uintptr_t magic, uintptr_t mbi_addr) {
    (void)magic;
    multiboot_info_t *mbi = (multiboot_info_t *)mbi_addr;
    g_mbi = mbi;

    framebuffer_init(mbi);
    gdt_install();
    idt_install();
    pic_remap();
    pit_install();
    mouse_install();
    /* The PS/2 mouse handshake above polls the controller directly. Do not
     * unmask IRQ12 until it is complete, or its handler could consume a reply. */
    pic_unmask_irqs();
    sti();                         /* PIT drives the splash progress below. */
    bootscreen_progress = 0;
    bootscreen_step(12, "Checking hardware...");
    bootscreen_step(30, "Loading input drivers...");
    serial_init();          /* COM1 0x3F8: host bridge for the Shkodya Update system */
    bootscreen_step(48, "Loading components...");
    vfs_init();
    bootscreen_step(64, "Mounting ShkodyaFS...");
    paint_clear();
    notepad_reset();
    calc_reset();
    term_clear();
    desktop_icons_init();       /* data-driven desktop icon positions */
    explorer_path_reset();
    store_current_user = -1;

    /* Initialize new subsystems */
    theme_apply(THEME_MIDNIGHT);
    sheet_init();
    word_reset();
    bootscreen_step(84, "Initializing GUI...");

    /* Default dev account */
    strcpy_c(store_users[0].username, "stepanchik");
    store_users[0].password_hash = hash_password("rikki");
    store_users[0].is_dev = 1;
    store_user_count = 1;

    bootscreen_step(100, "Starting desktop...");

    term_puts("Shkodya OS Core v2.2 Fluent \n");
    term_puts("Boot OK. Type 'help' for commands.\n\n");
    term_prompt();

    need_redraw = 1;   /* paint the desktop once before the first idle hlt */
    while (1) {
        toast_update();
        purr_update();
        if (store_view == STORE_VIEW_SNAKE)   snake_update();
        if (store_view == STORE_VIEW_2048)    g2048_update();
        if (store_view == STORE_VIEW_PONG)    pong_update();
        if (store_view == STORE_VIEW_MEMORY)  mem_update();
        if (store_view == STORE_VIEW_CLICKER) clicker_update();
        media_update();
        serial_poll();      /* bounded COM1 drain: never blocks the compositor */
        update_tick();      /* gives up on a silent host bridge */
        pkg_tick();         /* gives up on a silent package index */
        web_tick();         /* gives up on a silent browser fetch */
        handle_keys();
        handle_mouse_ui();
        store_update_hover();
        sound_update();
        if (need_redraw) {
            /* Clear before painting: a flag raised by an IRQ while the frame
               is being composited must not be swallowed (old order lost it). */
            need_redraw = 0;
            mouse_moved = 0;
            frame_present_full();
        } else if (mouse_moved) {
            mouse_moved = 0;
            frame_present_cursor();   /* two small presents, no recomposite */
        }
        hlt();
    }
}
