#include "main.h"
#include <map>
#include <thread>
#define MAP_SIZE 256
#define VIEW_SIZE 256
//FPS; PF ; SOFT; CSPEED; NAIVE
#ifdef PF
#include "pf.h"
#endif
#ifdef SOFT
#include "soft.h"
#endif
#ifdef CSPEED
#include "fps.h"
#endif
#ifdef NAIVE
#include "fps.h"
#endif
#ifdef FPS
#include "fps.h"
#endif
