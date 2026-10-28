#ifndef SGP_MSS_H
#define SGP_MSS_H

/* JA2 PPC: the handle types of the Miles Sound System (mss.h, RAD Game
 * Tools) that soundman.h exposes. The Mac sound manager port backs them
 * with Sound Manager channels; until then they are opaque pointers. */

typedef void *HDIGDRIVER;
typedef void *HSAMPLE;
typedef void *HSTREAM;
typedef void *H3DSAMPLE;
typedef void *H3DPOBJECT;
typedef void *HPROVIDER;

#endif
