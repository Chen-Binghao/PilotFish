#include "main.h"
#include <thread>
#include <map>
#include <iostream>
#include <process.h>

#ifdef OFF
#include "main_OFF.h"
#endif
#ifdef FULL
#include "main_FULL.h"
#endif
#ifdef FPS
#include "main_FPS.h"
#endif
#ifdef PF
#include "main_PF.h"
#endif
#ifdef SOFT
#include "main_SOFT_or_NOSTOP.h"
#endif
#ifdef CSPEED
#include "main_CSPEED.h"
#endif
#ifdef NAIVE
#include "main_NAIVE.h"
#endif
using namespace std;

//=========================================================================================================================//
