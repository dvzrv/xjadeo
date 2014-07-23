/* shared header file for xjadeo */
#ifndef XJADEO_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h> 	/* uint8_t */
#include <string.h> 	/* memcpy */
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef PLATFORM_WINDOWS
# include <windows.h>
# include <winsock.h>
# include <winuser.h>

# define vsnprintf _vsnprintf
# define snprintf _snprintf
#else
# include <sys/select.h>
#endif

#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>

#ifndef PIX_FMT_BGRA32
#define PIX_FMT_BGRA32 PIX_FMT_BGRA
#endif
#ifndef PIX_FMT_RGBA32
#define PIX_FMT_RGBA32 PIX_FMT_RGBA
#endif

/* xjadeo seek modes */
enum {
	SEEK_ANY, ///< directly seek to givenvideo frame
	SEEK_KEY, ///< seek to next keyframe after given frame.
	SEEK_CONTINUOUS ///< seek to keframe before this frame and advance to current frame.
};

/* freetype - On screen display */
enum { OSD_LEFT=-1, OSD_CENTER=-2, OSD_RIGHT=-3 }; ///< use positive values as percent or pixel.

/* override bitwise flags -- see xjadeo.h
 * 0x0001 : ignore 'q', ESC  / quit
 * 0x0002 : ignore "window closed by WM" / quit
 * 0x0004 : (osx only) menu-exit / quit
 * 0x0008 : ignore mouse-button 1 -- resize
 * 0x0010 : no A/V offset control with keyboard
 * 0x0020 : don't use jack-session
 * 0x0040 : disable jack transport control
 * 0x0080 : disallow sync source change (OSX menu)
 * 0x0100 : disallow file open (OSX menu, X11 DnD)
 */
enum {
	OVR_QUIT_KEY = 0x0001,
	OVR_QUIT_WMG = 0x0002,
	OVR_QUIT_OSX = 0x0004,
	OVR_MOUSEBTN = 0x0008,
	OVR_AVOFFSET = 0x0010,
	OVR_JSESSION = 0x0020,
	OVR_JCONTROL = 0x0040,
	OVR_MENUSYNC = 0x0080,
	OVR_LOADFILE = 0x0100
};

/* async notficy */
enum {
	NTY_FRAMELOOP = 0x01,
	NTY_FRAMECHANGE = 0x02,
	NTY_SETTINGS = 0x04,
	NTY_KEYBOARD = 0x08
};

#define OSD_FRAME (1)
#define OSD_SMPTE (2)

#define OSD_EQ    (8)
#define OSD_OFFS (16)
#define OSD_OFFF (32)
#define OSD_TEXT (64)

#define OSD_BOX (256)

#ifdef TTFFONTFILE
# define FONT_FILE TTFFONTFILE
#else
# define FONT_FILE "/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf"
#endif

/* define maximum size for OSD in pixel */
#ifdef HAVE_FT
# define ST_WIDTH   (1920)
# define ST_HEIGHT  (128)
#else
# define ST_WIDTH   (0)
# define ST_HEIGHT  (0)
#endif

#define ST_PADDING  (10)


/* X11 only - but defined here since needed in remote.c and display.c
 * EWMH state actions, see
	 http://freedesktop.org/Standards/wm-spec/index.html#id2768769 */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

/* prototypes of fn's in  display.c */
void newsourcebuffer (void);
void close_window(void);
void open_window(void);

int vidoutmode(int user_req);
int parsevidoutname (char *arg);
int vidoutsupported (int i);
int getvidmode (void);
const char *vidoutname (int i);
int try_next_vidoutmode(int user_req);
void splash (uint8_t *mybuffer);

void render_buffer (uint8_t *mybuffer);
void handle_X_events (void);
void Xresize (unsigned int x, unsigned int y);
void Xfullscreen (int a);
void Xmousepointer (int a);
void Xletterbox (int a);
void Xontop (int a);
void Xgetsize (unsigned int *x, unsigned int *y);
void Xposition (int x, int y);
void Xgetpos (int *x, int *y);
int  Xgetontop (void);
int  Xgetfullscreen (void);
int Xgetletterbox (void);

void XCresize_percent (float p);
void XCresize_aspect (int relscale);
void XCresize_scale (int relscale);
void XCtimeoffset (int mode, unsigned int charcode);

/* remote.c */
void exec_remote_cmd (char *cmd);
void close_remote_ctrl (void) ;
void open_remote_ctrl (void);
void close_mq_ctrl (void) ;
void open_mq_ctrl (void);
void close_ipcmsg_ctrl (void) ;
int  open_ipcmsg_ctrl (const char *);
int  remote_read_mq(void);
int  remote_read_ipc(void);
int  remote_read_io(void);
#ifdef PLATFORM_WINDOWS
int remote_read_h(void);
#endif
void remote_printf(int val, const char *format, ...);
void remote_notify(int mode, int rv, const char *format, ...);
int remote_fd_set(fd_set *fd);

/* xjadeo.c */
void display_frame(int64_t timestamp, int force_update, int do_render);
int open_movie(char* file_name);
int close_movie();
void avinit (void);
void override_fps (double fps);
void init_moviebuffer(void);
void event_loop(void);

/* common.c */
void ui_seek_cont ();
void ui_seek_any ();
void ui_seek_key ();

void INT_sync_to_jack(int remote_msg);
void INT_sync_to_ltc(char *port, int remote_msg);

void ui_sync_none ();
void ui_sync_to_jack ();
void ui_sync_to_ltc ();
void ui_sync_to_mtc_jack ();
void ui_sync_to_mtc_portmidi ();
void ui_sync_to_mtc_alsaraw ();
void ui_sync_to_mtc_alsaseq ();

void ui_osd_clear();
void ui_osd_offset_cycle();
void ui_osd_offset_tc();
void ui_osd_offset_fn();
void ui_osd_offset_none();
void ui_osd_tc();
void ui_osd_fn();
void ui_osd_box();

enum SyncSource {
	SYNC_JACK,
	SYNC_LTC,
	SYNC_MTC_JACK,
	SYNC_MTC_PORTMIDI,
	SYNC_MTC_ALSASEQ,
	SYNC_MTC_ALSARAW,
	SYNC_NONE
};

enum SyncSource ui_syncsource();

/* jack.c function prototypes */
long jack_poll_frame (void);
void open_jack(void );
void close_jack(void);
int jack_connected(void);

/* ltc-jack.c function prototypes */
long ltc_poll_frame (void);
void open_ltcjack(char *autoconnect);
void close_ltcjack(void);
int ltcjack_connected(void);
const char *ltc_jack_client_name();

/* smpte.c prototypes */
long int smptestring_to_frame (char *str);
void frame_to_smptestring(char *smptestring, long int frame);
long int smpte_to_frame(int type, int f, int s, int m, int h, int overflow);

/* midi.c function prototype */
int midi_connected(void);
const char *midi_driver_name();
#ifdef HAVE_MIDI
long midi_poll_frame (void);
void midi_open(char *midiid);
void midi_close(void);
int midi_choose_driver(const char *);
#endif

/* xjosc.c */
int initialize_osc(int osc_port);
void shutdown_osc(void);
int process_osc(void);

/* configfile.c */
void xjadeorc (void);
int testfile (char *filename);
int saveconfig (const char *filename);
int readconfig (char *fn);

/* freetype - On screen display */
int render_font (char *fontfile, char *text, int px);
void free_freetype ();

#endif
