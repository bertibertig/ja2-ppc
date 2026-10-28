#ifndef SGP_MSS_H
#define SGP_MSS_H

/* JA2 PPC: the part of the Miles Sound System 2D API (mss.h, RAD Game
 * Tools) that soundman.c calls, implemented on the Mac Sound Manager in
 * sgp_mss.c. soundman.c keeps its own logic (cache, channels, priorities,
 * random sounds, fades); only the Miles calls below it are replaced.
 *
 * Semantics follow Miles: volume and pan 0..127 (pan 64 = centre), loop
 * count 0 = forever, sample and stream status SMP_*. Samples play from
 * memory (the WAV image soundman.c loaded); streams read a WAV file through
 * the Win32 file layer while they play. The 3D API exists only so that
 * soundman.c compiles: no 3D provider is ever found, so it is never used. */

#include "sgp_win32.h"

typedef int S32;
typedef unsigned int U32;
typedef float F32;
typedef char C8;

#define AILCALL
#define AILEXPORT

#define AIL_NO_ERROR 0
#define YES 1
#define NO 0

#define DIG_MIXER_CHANNELS 1
#define DIG_USE_WAVEOUT 15

#define SMP_FREE 0x0001
#define SMP_DONE 0x0002
#define SMP_PLAYING 0x0004
#define SMP_STOPPED 0x0008
#define SMP_PLAYINGBUTRELEASED 0x0010

typedef struct SGPMSSDriver *HDIGDRIVER;
typedef struct SGPMSSSample *HSAMPLE;
typedef struct SGPMSSStream *HSTREAM;
typedef void *H3DSAMPLE;
typedef void *H3DPOBJECT;
typedef U32 HPROVIDER;
typedef U32 HPROENUM;
typedef S32 M3DRESULT;
#define HPROENUM_FIRST 0
#define M3D_NOERR 0

/* The stream fields soundman.c's (unused) stream-callback path reads. */
typedef struct {
    S32 format;
} SGPMSSStreamSample;

struct SGPMSSStream {
    S32 datarate;
    S32 bufsize;
    void *bufs[ 3 ];
    S32 buf1;
    SGPMSSStreamSample *samp;
};

typedef struct _AILSOUNDINFO {
    S32 format;
    void const *data_ptr;
    U32 data_len;
    U32 rate;
    S32 bits;
    S32 channels;
    U32 samples;
    U32 block_size;
    void const *initial_ptr;
} AILSOUNDINFO;

/* <mmsystem.h> wave format, as soundman.c fills it in. */
#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IMA_ADPCM 0x0011
typedef struct {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
} WAVEFORMAT, *LPWAVEFORMAT;
typedef struct {
    WAVEFORMAT wf;
    WORD wBitsPerSample;
} PCMWAVEFORMAT;
typedef void *LPHWAVEOUT;

#define Sleep( uiMilliseconds ) SGPSleep( uiMilliseconds )
void SGPSleep( DWORD uiMilliseconds );

#ifdef __cplusplus
extern "C" {
#endif

S32 AIL_startup( void );
void AIL_shutdown( void );
S32 AIL_set_preference( U32 number, S32 value );
char *AIL_last_error( void );
void *AIL_mem_alloc_lock( U32 size );
void AIL_mem_free_lock( void *ptr );
S32 AIL_WAV_info( void const *data, AILSOUNDINFO *info );

S32 AIL_waveOutOpen( HDIGDRIVER *drvr, LPHWAVEOUT *lphWaveOut, S32 wDeviceID, LPWAVEFORMAT lpFormat );
void AIL_waveOutClose( HDIGDRIVER drvr );
void AIL_digital_configuration( HDIGDRIVER dig, S32 *rate, S32 *format, char *string );
void AIL_set_digital_master_volume( HDIGDRIVER dig, S32 master_volume );
S32 AIL_digital_master_volume( HDIGDRIVER dig );

HSAMPLE AIL_allocate_sample_handle( HDIGDRIVER dig );
void AIL_release_sample_handle( HSAMPLE S );
void AIL_init_sample( HSAMPLE S );
S32 AIL_set_named_sample_file( HSAMPLE S, C8 const *file_type_suffix, void const *file_image, S32 file_size, S32 block );
void AIL_start_sample( HSAMPLE S );
void AIL_stop_sample( HSAMPLE S );
U32 AIL_sample_status( HSAMPLE S );
void AIL_set_sample_playback_rate( HSAMPLE S, S32 playback_rate );
S32 AIL_sample_playback_rate( HSAMPLE S );
void AIL_set_sample_volume( HSAMPLE S, S32 volume );
S32 AIL_sample_volume( HSAMPLE S );
void AIL_set_sample_pan( HSAMPLE S, S32 pan );
S32 AIL_sample_pan( HSAMPLE S );
void AIL_set_sample_loop_count( HSAMPLE S, S32 loop_count );
S32 AIL_sample_loop_count( HSAMPLE S );
U32 AIL_sample_position( HSAMPLE S );
void AIL_sample_ms_position( HSAMPLE S, S32 *total_milliseconds, S32 *current_milliseconds );

HSTREAM AIL_open_stream( HDIGDRIVER dig, char const *filename, S32 stream_mem );
void AIL_close_stream( HSTREAM stream );
S32 AIL_service_stream( HSTREAM stream, S32 fillup );
void AIL_start_stream( HSTREAM stream );
void AIL_auto_service_stream( HSTREAM stream, S32 onoff );
S32 AIL_stream_status( HSTREAM stream );
void AIL_set_stream_volume( HSTREAM stream, S32 volume );
S32 AIL_stream_volume( HSTREAM stream );
void AIL_set_stream_pan( HSTREAM stream, S32 pan );
S32 AIL_stream_pan( HSTREAM stream );
void AIL_set_stream_playback_rate( HSTREAM stream, S32 rate );
S32 AIL_stream_playback_rate( HSTREAM stream );
void AIL_set_stream_loop_count( HSTREAM stream, S32 count );
S32 AIL_stream_loop_count( HSTREAM stream );
S32 AIL_stream_position( HSTREAM stream );
void AIL_stream_ms_position( HSTREAM S, S32 *total_milliseconds, S32 *current_milliseconds );

/* 3D: no providers. */
S32 AIL_enumerate_3D_providers( HPROENUM *next, HPROVIDER *dest, C8 **name );
M3DRESULT AIL_open_3D_provider( HPROVIDER lib );
void AIL_close_3D_provider( HPROVIDER lib );
H3DPOBJECT AIL_open_3D_listener( HPROVIDER lib );
void AIL_close_3D_listener( H3DPOBJECT listener );
void AIL_3D_provider_attribute( HPROVIDER lib, C8 const *name, void *val );
void AIL_set_3D_provider_preference( HPROVIDER lib, C8 const *name, void const *val );
S32 AIL_active_3D_sample_count( HPROVIDER lib );
H3DSAMPLE AIL_allocate_3D_sample_handle( HPROVIDER lib );
void AIL_release_3D_sample_handle( H3DSAMPLE S );
S32 AIL_set_3D_sample_file( H3DSAMPLE S, void const *file_image );
void AIL_start_3D_sample( H3DSAMPLE S );
void AIL_stop_3D_sample( H3DSAMPLE S );
U32 AIL_3D_sample_status( H3DSAMPLE S );
void AIL_set_3D_sample_volume( H3DSAMPLE S, S32 volume );
S32 AIL_3D_sample_volume( H3DSAMPLE S );
void AIL_set_3D_sample_playback_rate( H3DSAMPLE S, S32 playback_rate );
S32 AIL_3D_sample_playback_rate( H3DSAMPLE S );
void AIL_set_3D_sample_loop_count( H3DSAMPLE S, U32 loops );
U32 AIL_3D_sample_loop_count( H3DSAMPLE S );
void AIL_set_3D_sample_distances( H3DSAMPLE S, F32 max_dist, F32 min_dist );
void AIL_set_3D_position( H3DPOBJECT obj, F32 X, F32 Y, F32 Z );
void AIL_set_3D_velocity_vector( H3DPOBJECT obj, F32 dX_per_ms, F32 dY_per_ms, F32 dZ_per_ms );
void AIL_set_3D_orientation( H3DPOBJECT obj, F32 X_face, F32 Y_face, F32 Z_face, F32 X_up, F32 Y_up, F32 Z_up );

#ifdef __cplusplus
}
#endif

#endif
