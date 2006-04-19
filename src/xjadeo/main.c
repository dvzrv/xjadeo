/* xjadeo - jack video monitor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 *
 * Credits:
 *
 * xjadeo:  (c) 2006 
 *  Luis Garrido <luisgarrido@users.sourceforge.net>
 *  Robin Gareus <robin@gareus.org>
 *
 * XLib code:
 * http://www.ac3.edu.au/SGI_Developer/books/XLib_PG/sgi_html/index.html
 *
 * WM_DELETE_WINDOW code:
 * http://biology.ncsa.uiuc.edu/library/SGI_bookshelves/SGI_Developer/books/OpenGL_Porting/sgi_html/apf.html
 *
 * ffmpeg code:
 * http://www.inb.uni-luebeck.de/~boehme/using_libavcodec.html
 *
 */

#define EXIT_FAILURE 1

#include "xjadeo.h"

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


//------------------------------------------------
// Globals
//------------------------------------------------

// Display loop

/* int loop_flag: main xjadeo event loop 
 * if loop_flag is set to 0, xjadeo will exit
 */
int       loop_flag = 1; 

/* int loop_run: video update enable
 * if set to 0 no smpte will be polled and 
 * no video frame updates are performed.
 */
int 	  loop_run = 1; 

      
// Video Decoder 

int               movie_width = 100;
int               movie_height = 100;
AVFormatContext   *pFormatCtx;
int               videoStream=-1;
AVCodecContext    *pCodecCtx;
AVFrame           *pFrame;
AVFrame           *pFrameFMT = NULL;
uint8_t           *buffer = NULL;

// needs to be set before calling movie_open
int               render_fmt = PIX_FMT_YUV420P;

/* Video File Info */
double 	duration = 1;
double 	framerate = 1;
long	frames = 1;


/* Option flags and variables */
char	*current_file = NULL;
long	ts_offset = 0;
long	userFrame = 0; // seek to this frame is jack and midi are N/A
long	dispFrame = 0; // global strorage... = (SMPTE+offset) with boundaries to [0..movie_file_frames]

int want_quiet   =0;	/* --quiet, --silent */
int want_verbose =0;	/* --verbose */
int remote_en =0;	/* --remote, -R */
int remote_mode =0;	/* 0: undirectional ; >0: bidir
			 * bitwise enable async-messages 
			 *  so far only: 
			 *   (1) notify changed timecode 
			 */

int try_codec =0;	/* --try-codec */

#ifdef HAVE_MIDI
char midiid[32] = "-2";	/* --midi # -1: autodetect -2: jack*/
int midi_clkconvert =0;	/* --midifps [0:MTC|1:VIDEO|2:RESAMPLE] */
#endif

double 	delay = 0.1; // default update rate 10 Hz
int	videomode = 0; // --vo <int>  - default: autodetect

#if LIBAVFORMAT_BUILD > 4622
 int 	seekflags    = AVSEEK_FLAG_ANY; /* non keyframe */
#else
 int 	seekflags    = AVSEEK_FLAG_BACKWARD; /* keyframe */
#endif


// On screen display
char OSD_fontfile[1024] = FONT_FILE;
char OSD_text[128] = "xjadeo!";
char OSD_frame[48] = "";
char OSD_smpte[13] = "";
int OSD_mode = 0;

int OSD_fx = OSD_CENTER;
int OSD_tx = OSD_CENTER;
int OSD_sx = OSD_CENTER;
int OSD_fy = 5; // percent
int OSD_sy = 98; // percent
int OSD_ty = 50; // percent

/* The name the program was run with, stripped of any leading path. */
char *program_name;


// prototypes .

static void usage (int status);

static struct option const long_options[] =
{
  {"quiet", no_argument, 0, 'q'},
  {"silent", no_argument, 0, 'q'},
  {"verbose", no_argument, 0, 'v'},
  {"keyframes", no_argument, 0, 'k'},
  {"offset", no_argument, 0, 'o'},
  {"fps", required_argument, 0, 'f'},
  {"videomode", required_argument, 0, 'm'},
  {"vo", required_argument, 0, 'x'},
  {"remote", no_argument, 0, 'R'},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {"try-codec", no_argument, 0, 't'},
  {"info", no_argument, 0, 'i'},
#ifdef HAVE_MIDI
  {"midi", required_argument, 0, 'm'},
  {"midifps", required_argument, 0, 'M'},
#endif
  {NULL, 0, NULL, 0}
};



/* Set all the option flags according to the switches specified.
   Return the index of the first non-option argument.  */
static int
decode_switches (int argc, char **argv)
{
  int c;
  while ((c = getopt_long (argc, argv, 
			   "q"	/* quiet or silent */
			   "v"	/* verbose */
			   "h"	/* help */
			   "R"	/* remote control */
			   "k"	/* keyframes */
			   "o:"	/* offset */
			   "t"	/* try-codec */
			   "f:"	/* fps */
			   "x:"	/* video-mode */
			   "i:"	/* info - OSD-mode */
#ifdef HAVE_MIDI
			   "m:"	/* midi interface */
			   "M:"	/* midi clk convert */
#endif
			   "V",	/* version */
			   long_options, (int *) 0)) != EOF)
    {
      switch (c)
	{
	case 'q':		/* --quiet, --silent */
	  want_quiet = 1;
	  want_verbose = 0;
	  break;
	case 'v':		/* --verbose */
	  want_verbose = !remote_en;
	  break;
	case 'R':		/* --remote */
	  remote_en = 1;
	  want_quiet = 1;
	  want_verbose = 0;
	  break;
	case 't':		/* --try */
	  try_codec = 1;
	  break;
	case 'i':		/* --info */
	  OSD_mode=atoi(optarg)&3;
	  printf("On screen display: [%s%s%s] \n",
		(!OSD_mode)?"off": 
		(OSD_mode&OSD_FRAME)?"frames":"",
		(OSD_mode&(OSD_FRAME|OSD_SMPTE))==(OSD_FRAME|OSD_SMPTE)?" ":"",
		(OSD_mode&OSD_SMPTE)?"SMPTE":""
		);
	  break;
	case 'o':		/* --offset */
	  ts_offset=atoi(optarg);
	  printf("set time offset to %li frames\n",ts_offset);
	  break;
	case 'k':		/* --keyframes */
	  seekflags=AVSEEK_FLAG_BACKWARD;
	  printf("seeking to keyframes only\n");
	  break;
	case 'f':		/* --fps */
          delay = 1.0 / atof(optarg);
	  break;
	case 'x':		/* --vo --videomode */
          videomode = atoi(optarg);
	  if (videomode == 0) videomode = parsevidoutname(optarg);
	  break;
#ifdef HAVE_MIDI
	case 'm':		/* --midi */
	  strncpy(midiid,optarg,32);
	  break;
	case 'M':		/* --midifps */
          midi_clkconvert = atoi(optarg);
	  break;
#endif
	case 'V':
	  printf ("xjadeo %s\n", VERSION);
  	  printf("compiled with LIBAVFORMAT_BUILD %i\n", LIBAVFORMAT_BUILD);
	  exit (0);

	case 'h':
	  usage (0);

	default:
	  usage (EXIT_FAILURE);
	}
    }
  return optind;
}

static void
usage (int status)
{
  printf ("%s - \
jack video monitor\n", program_name);
  printf ("usage: %s [Options] <video-file>\n", program_name);
  printf ("       %s -R [Options] [<video-file>]\n", program_name);
  printf (""
"Options:\n"
"  -q, --quiet, --silent     inhibit usual output\n"
"  -v, --verbose             print more information\n"
"  -R, --remote              remote control (stdin) - implies non verbose&quiet\n"
"  -f <val>, --fps <val>     video display update fps - default 10.0 fps\n"
"  -k, --keyframes           seek to keyframes only\n"
"  -o <int>, --offset <int>  add/subtract <int> video-frames to/from timecode\n"
"  -x <int>, --vo <int>,     set the video output mode (default: 0 - autodetect\n"
"      --videomode <int>     -1 prints a list of available modes.\n"
"  -i <int> --info <int>     render OnScreenDisplay info: 0:off, %i:frame,\n"
"                            %i:smpte, %i:both. (use remote ctrl for more opts.)\n",
	OSD_FRAME,OSD_SMPTE,OSD_FRAME|OSD_SMPTE); // :)

  printf (""
#ifdef HAVE_MIDI
#ifdef HAVE_PORTMIDI
"  -m <int>, --midi <int>,   use portmidi instead of jack (-1: autodetect)\n"
"                            value > -1 specifies a (input) midi port to use\n" 	  
"                            use -v -m -1 to list midi ports.\n" 	  
#else /* alsa midi */
"  -m <port>,                use alsamidi instead of jack\n"
"      --midi <port>,        specify alsa seq id to connect to. (-1: none)\n" 	  
"                            eg. -m ardour or -m 80 \n"
#endif /* HAVE_PORTMIDI */
"  -M <int>, --midifps <int> how to 'convert' MTC SMPTE to framenumber:\n"
"                            0: use framerate of MTC clock\n" 
"                            2: use video file FPS\n" 
"                            3: resample: videoFPS / MTC \n" 
#endif /* HAVE_MIDI */
"  -t, --try-codec           checks if the video-file can be played by jadeo.\n"
"                            exits with code 1 if the file is not supported.\n"
"			     no window is opened in this mode.\n"
"  -h, --help                display this help and exit\n"
"  -V, --version             output version information and exit\n"
"  \n"
"  Check the docs to learn how the video should be encoded.\n"
);
  exit (status);
}

const char fontfile[][128] = {
	FONT_FILE,
	"/var/lib/defoma/gs.d/dirs/fonts/FreeMonoBold.ttf"
	"/usr/share/xplanet/fonts/FreeMonoBold.ttf",
	"/usr/share/fonts/truetype/freefont/FreeMono.ttf",
	"/var/lib/defoma/gs.d/dirs/fonts/FreeMono.ttf"
	"/var/lib/defoma/gs.d/dirs/fonts/Courier_New_Bold.ttf",
	"/var/lib/defoma/gs.d/dirs/fonts/Courier_New.ttf",
	"/usr/share/fonts/truetype/msttcorefonts/arial.ttf",
	"/usr/share/fonts/truetype/vera.ttf",
	""
};

void stat_osd_fontfile(void) {
#ifdef HAVE_FT
	struct stat s;
	int i=0;
	while (fontfile[i][0]!=0) {
		if ( stat(OSD_fontfile, &s) ==0 ) {
			strcpy(OSD_fontfile,fontfile[i]);
   			if (want_verbose) fprintf(stdout,"OSD font file: %s\n",OSD_fontfile);
			return;
		}
		i++;
	}
   	if (!want_quiet)
		fprintf(stderr,"no TTF font found. OSD will not be available until you set one.\n");
#endif
}

//--------------------------------------------
// Main
//--------------------------------------------

int
main (int argc, char **argv)
{
  int i;
  char*   movie= NULL;

  program_name = argv[0];

  i = decode_switches (argc, argv);

  if (videomode < 0) vidoutmode(videomode); // dump modes and exit.

  if ((i+1)== argc) movie = argv[i];
  else if (remote_en && i==argc) movie = "";
  else usage (EXIT_FAILURE);

  if (want_verbose) printf ("xjadeo %s\n", VERSION);

  stat_osd_fontfile();
    
  /* do the work */
  avinit();

  // format needs to be set before calling init_moviebuffer
  render_fmt = vidoutmode(videomode);

  // only try to seek to frame 1 and decode it.
  if (try_codec) do_try_this_file_and_exit (movie);


  open_movie(movie);
  
  open_window(&argc,&argv);

 // try fallbacks if window open failed in autodetect mode
  if (videomode==0 && getvidmode() ==0) { // re-use cmd-option variable as counter.
   if (want_verbose) printf("trying video driver fallbacks.\n");
   while (getvidmode() ==0) { // check if window is open.
	videomode++;
	int tv=try_next_vidoutmode(videomode);
	if (tv<0) break; // no videomode found!
        if (want_verbose) printf("trying videomode: %i: %s\n",videomode,vidoutname(videomode));
	if (tv==0) continue; // this mode is not available
	render_fmt = vidoutmode(videomode);
	open_window(&argc,&argv); 
   }
  }

  if (getvidmode() ==0) {
	fprintf(stderr,"Could not open display.\n");
  	if(!remote_en) {
		// FIXME: cleanup close jack, midi and file ??
		exit(1);
	}
  }

  init_moviebuffer();

#ifdef HAVE_MIDI
  if (atoi(midiid) < -1 ) {
    open_jack();
  } else {
    midi_open(midiid);
  }
#else
  open_jack();
#endif

  if(remote_en) open_remote_ctrl();

  display_frame(0LL,1);
  
  event_loop();

  if(remote_en) close_remote_ctrl();
  
  close_window();
  
  close_movie();

#ifdef HAVE_MIDI
  if (midi_connected()) midi_close(); 
  else
#endif
  close_jack();
  
  if (!want_quiet)
    fprintf(stdout, "\nBye!\n");

  exit (0);
}
