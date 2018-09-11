/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */



/*-*************************************
*  Dependencies
***************************************/
#include <stdlib.h>      /* malloc, calloc, free */
#include <string.h>      /* memset */
#include <stdio.h>       /* fprintf(), stderr */
#include <signal.h>      /* signal() */
#ifndef _WIN32
#include <execinfo.h>    /* backtrace, backtrace_symbols, symbollist */
#endif
#include "error_private.h"
#include "zstd_internal.h"


/*-************************************
*  Display Macros
**************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)


/*-****************************************
*  Version
******************************************/
unsigned ZSTD_versionNumber(void) { return ZSTD_VERSION_NUMBER; }

const char* ZSTD_versionString(void) { return ZSTD_VERSION_STRING; }


/*-****************************************
*  ZSTD Error Management
******************************************/
/*! ZSTD_isError() :
 *  tells if a return value is an error code */
unsigned ZSTD_isError(size_t code) { return ERR_isError(code); }

/*! ZSTD_getErrorName() :
 *  provides error code string from function result (useful for debugging) */
const char* ZSTD_getErrorName(size_t code) { return ERR_getErrorName(code); }

/*! ZSTD_getError() :
 *  convert a `size_t` function result into a proper ZSTD_errorCode enum */
ZSTD_ErrorCode ZSTD_getErrorCode(size_t code) { return ERR_getErrorCode(code); }

/*! ZSTD_getErrorString() :
 *  provides error code string from enum */
const char* ZSTD_getErrorString(ZSTD_ErrorCode code) { return ERR_getErrorString(code); }



/*=**************************************************************
*  Custom allocator
****************************************************************/
void* ZSTD_malloc(size_t size, ZSTD_customMem customMem)
{
    if (customMem.customAlloc)
        return customMem.customAlloc(customMem.opaque, size);
    return malloc(size);
}

void* ZSTD_calloc(size_t size, ZSTD_customMem customMem)
{
    if (customMem.customAlloc) {
        /* calloc implemented as malloc+memset;
         * not as efficient as calloc, but next best guess for custom malloc */
        void* const ptr = customMem.customAlloc(customMem.opaque, size);
        memset(ptr, 0, size);
        return ptr;
    }
    return calloc(1, size);
}

void ZSTD_free(void* ptr, ZSTD_customMem customMem)
{
    if (ptr!=NULL) {
        if (customMem.customFree)
            customMem.customFree(customMem.opaque, ptr);
        else
            free(ptr);
    }
}


/*-*********************************************************
*  Termination signal trapping (Print debug stack trace)
***********************************************************/
#define MAX_STACK_FRAMES    50

#ifndef _WIN32

#ifdef __linux__
#define START_STACK_FRAME  2
#elif defined __APPLE__
#define START_STACK_FRAME  4
#endif

static void ABRThandler(int sig)
{
   const char* name;
   void* addrlist[MAX_STACK_FRAMES + 1];
   char** symbollist;
   U32 addrlen, i;

   switch (sig) {
      case SIGABRT: name = "SIGABRT"; break;
      case SIGFPE:  name = "SIGFPE"; break;
      case SIGILL:  name = "SIGILL"; break;
      case SIGINT:  name = "SIGINT"; break;
      case SIGSEGV: name = "SIGSEGV"; break;
      default: name = "UNKNOWN"; break;
   }

   DISPLAY("Caught %s signal, printing stack:\n", name);
   // Retrieve current stack addresses.
   addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));
   if (addrlen == 0) {
      DISPLAY("\n");
      return;
   }
   // Create readable strings to each frame.
   symbollist = backtrace_symbols(addrlist, addrlen);
   // Print the stack trace, excluding calls handling the signal.
   for (i = START_STACK_FRAME; i < addrlen; i++) {
      DISPLAY("%s\n", symbollist[i]);
   }
   free(symbollist);
   // Reset and raise the signal so default handler runs.
   signal(sig, SIG_DFL);
   raise(sig);
}
#endif

void ZSTD_addAbortHandler()
{
#ifndef _WIN32
    signal(SIGABRT, ABRThandler);
    signal(SIGFPE, ABRThandler);
    signal(SIGILL, ABRThandler);
    signal(SIGSEGV, ABRThandler);
    signal(SIGBUS, ABRThandler);
#endif
}
