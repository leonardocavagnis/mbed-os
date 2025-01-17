/*
*
* Copyright 2018,2020 NXP
* SPDX-License-Identifier: Apache-2.0
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <nxLog.h>
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

#include "sm_printf.h"

#if USE_RTOS
#include "FreeRTOS.h"
#include "semphr.h"
#endif

#if (__GNUC__ && !AX_EMBEDDED) || (USE_RTOS) || (__MBED__)
#define USE_LOCK 1
#else
#define USE_LOCK 0
#endif
#if defined(_MSC_VER)
#include <windows.h>
#endif

#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_RESET "\033[0m"

#define szCRLF "\r\n"
#define szLF "\n"

static void setColor(int level);
static void reSetColor(void);

#if defined(_MSC_VER)
static HANDLE sStdOutConsoleHandle = INVALID_HANDLE_VALUE;
static void msvc_setColor(int level);
static void msvc_reSetColor(void);
#define szEOL szLF
#endif

#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
#include <unistd.h>
static void ansi_setColor(int level);
static void ansi_reSetColor(void);
#if AX_EMBEDDED
#define szEOL szCRLF
#else
#define szEOL szLF
#endif
#endif /* __GNUC__ && !defined(__ARMCC_VERSION) */

#ifndef szEOL
#define szEOL szCRLF
#endif

/* Set this to do not widen the logs.
 *
 * When set to 0, and logging is verbose, it looks like this
 *
 *    APDU:DEBUG:ReadECCurveList []
 *   smCom:DEBUG:Tx> (Len=4)
 *    80 02 0B 25
 *   smCom:DEBUG:<Rx (Len=23)
 *    41 82 00 11    01 01 02 01    01 01 01 01    01 01 01 01
 *    01 01 01 01    01 90 00
 *
 * When set to 1, same log looks like this
 *
 *       APDU:DEBUG:ReadECCurveList []
 *      smCom:DEBUG:Tx> (Len=4)
 * =>   80 02 0B 25
 *      smCom:DEBUG:<Rx (Len=23)
 * =>   41 82 00 11 01 01 02 01 01 01 01 01 01 01 01 01
 *    01 01 01 01 01 90 00
 *
 */
#define COMPRESSED_LOGGING_STYLE 0

/* Set this to 1 if you want colored logs with GCC based compilers */
#define USE_COLORED_LOGS 1

#if NX_LOG_SHORT_PREFIX
static const char *szLevel[] = {"E", "W", "I", "D"};
#else
static const char *szLevel[] = {"ERROR", "WARN ", "INFO ", "DEBUG"};
#endif

#if AX_EMBEDDED
#define TAB_SEPRATOR "\t"
#else
#define TAB_SEPRATOR "   "
#endif

#if defined(SMCOM_JRCP_V2)
#include "smCom.h"
#endif

#if USE_RTOS
static SemaphoreHandle_t gLogginglock;
#elif (__GNUC__ && !AX_EMBEDDED && !__MBED__)
#include<pthread.h>
/* Only for base session with os */
static pthread_mutex_t gLogginglock;
#elif __MBED__
#include "cmsis_os2.h"
#include "mbed_rtos_storage.h"
    static osSemaphoreId_t gLogginglock;
    static mbed_rtos_storage_semaphore_t gLogginglock_mem;
#endif
static void nLog_AcquireLock();
static void nLog_ReleaseLock();
#if USE_LOCK
static uint8_t lockInitialised = false;
#endif
static void nLog_AcquireLock()
{
#if USE_LOCK
    if (lockInitialised) {
#if USE_RTOS
        if (xSemaphoreTake(gLogginglock, portMAX_DELAY) != pdTRUE) {
            PRINTF("Acquiring logging semaphore failed");
        }
#elif (__GNUC__ && !AX_EMBEDDED && !__MBED__)
        if (pthread_mutex_lock(&gLogginglock) != 0) {
            PRINTF("Acquiring logging mutext failed");
        }
#elif __MBED__
        if (osSemaphoreAcquire(gLogginglock, 0) != osOK) {
            PRINTF("Acquiring logging mutext failed\n");
        }
#endif
    }
#endif
}

static void nLog_ReleaseLock()
{
#if USE_LOCK
    if (lockInitialised) {
#if USE_RTOS
        if (xSemaphoreGive(gLogginglock) != pdTRUE) {
            PRINTF("Releasing logging semaphore failed");
        }
#elif (__GNUC__ && !AX_EMBEDDED && !__MBED__)
        if (pthread_mutex_unlock(&gLogginglock) != 0) {
            PRINTF("Releasing logging semaphore failed");
        }
#elif __MBED__
        if (osSemaphoreRelease(gLogginglock) != osOK) {
            PRINTF("Releasing logging semaphore failed\n");
        }
#endif
    }
#endif
}

uint8_t nLog_Init()
{
#if USE_LOCK
#if USE_RTOS
    gLogginglock = xSemaphoreCreateMutex();
    if (gLogginglock == NULL) {
        PRINTF("xSemaphoreCreateMutex failed");
        return 1;
    }
#elif (__GNUC__ && !AX_EMBEDDED && !__MBED__)
    if (pthread_mutex_init(&gLogginglock, NULL) != 0) {
        PRINTF("pthread_mutex_init failed");
        return 1;
    }
#elif __MBED__
    osSemaphoreAttr_t attr;
    attr.name = NULL;
    attr.attr_bits = 0;
    attr.cb_mem = &gLogginglock_mem;
    attr.cb_size = sizeof gLogginglock_mem;
    gLogginglock = osSemaphoreNew(1, 0, &attr);
    if (gLogginglock == NULL) {
        PRINTF("xSemaphoreCreateMutex failed");
        return 1;
    }
#endif
    lockInitialised = true;
#endif
    return 0;
}

void nLog_DeInit()
{
#if USE_LOCK
#if USE_RTOS
    if (gLogginglock != NULL) {
    	vSemaphoreDelete(gLogginglock);
        gLogginglock = NULL;
    }
#elif (__GNUC__ && !AX_EMBEDDED && !__MBED__)
    pthread_mutex_destroy(&gLogginglock);
#elif __MBED__
    if (gLogginglock != NULL) {
        osSemaphoreRelease(gLogginglock);
        gLogginglock = NULL;
    }
#endif
    lockInitialised = false;
#endif
}

/* Used for scenarios other than LPC55S_NS */
void nLog(const char *comp, int level, const char *format, ...)
{
    nLog_AcquireLock();
    setColor(level);
    PRINTF("%-6s:%s:", comp, szLevel[level-1]);
    if (format == NULL) {
        /* Nothing */
#ifdef SMCOM_JRCP_V2
        smCom_Echo(NULL, comp, szLevel[level-1], "");
#endif // SMCOM_JRCP_V2
    }
    else if (format[0] == '\0') {
        /* Nothing */
#ifdef SMCOM_JRCP_V2
        smCom_Echo(NULL, comp, szLevel[level-1], "");
#endif // SMCOM_JRCP_V2
    }
    else {
        char buffer[256];
        size_t size_buff = sizeof(buffer) / sizeof(buffer[0]) - 1;
        va_list vArgs;
        va_start(vArgs, format);
        vsnprintf(buffer, size_buff, format, vArgs);
        va_end(vArgs);
        PRINTF("%s", buffer);
#ifdef SMCOM_JRCP_V2
        smCom_Echo(NULL, comp, szLevel[level-1], buffer);
#endif // SMCOM_JRCP_V2
    }
    reSetColor();
    PRINTF(szEOL);
    nLog_ReleaseLock();
}

void nLog_au8(const char *comp, int level, const char *message, const unsigned char *array, size_t array_len)
{
    size_t i;
    nLog_AcquireLock();
    setColor(level);
    PRINTF("%-6s:%s:%s (Len=%" PRId32 ")", comp, szLevel[level-1], message, (int32_t)array_len);
    for (i = 0; i < array_len; i++) {
        if (0 == (i % 16)) {
            PRINTF(szEOL);
            if (0 == i) {
#if COMPRESSED_LOGGING_STYLE
                PRINTF("=>");
#endif
                PRINTF(TAB_SEPRATOR);
            }
            else {
                PRINTF(TAB_SEPRATOR);
            }
        }
#if !COMPRESSED_LOGGING_STYLE
        if (0 == (i % 4)) {
            PRINTF(TAB_SEPRATOR);
        }
#endif
        PRINTF("%02X ", array[i]);
    }
    reSetColor();
    PRINTF(szEOL);
    nLog_ReleaseLock();
}

static void setColor(int level)
{
#if defined(_MSC_VER)
    msvc_setColor(level);
#endif
#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
    ansi_setColor(level);
#endif
}

static void reSetColor(void)
{
#if defined(_MSC_VER)
    msvc_reSetColor();
#endif
#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
    ansi_reSetColor();
#endif
}

#if defined(_MSC_VER) && USE_COLORED_LOGS
static void msvc_setColor(int level)
{
#if USE_COLORED_LOGS
    WORD wAttributes = 0;
    if (sStdOutConsoleHandle == INVALID_HANDLE_VALUE) {
        sStdOutConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    switch (level) {
    case NX_LEVEL_ERROR:
        wAttributes = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    case NX_LEVEL_WARN:
        wAttributes = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    case NX_LEVEL_INFO:
        wAttributes = FOREGROUND_GREEN;
        break;
    case NX_LEVEL_DEBUG:
        /* As of now put color here. All normal printfs would be in WHITE
             * Later, remove this color.
             */
        wAttributes = FOREGROUND_RED | FOREGROUND_GREEN;
        break;
    default:
        wAttributes = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
    }
    SetConsoleTextAttribute(sStdOutConsoleHandle, wAttributes);
#endif // USE_COLORED_LOGS
}

static void msvc_reSetColor()
{
#if USE_COLORED_LOGS
    msvc_setColor(-1 /* default */);
#endif // USE_COLORED_LOGS
}
#endif

#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
static void ansi_setColor(int level)
{
#if USE_COLORED_LOGS
#if !AX_EMBEDDED
    if (!isatty(fileno(stdout))) {
        return;
    }
#endif

    switch (level) {
    case NX_LEVEL_ERROR:
        PRINTF(COLOR_RED);
        break;
    case NX_LEVEL_WARN:
        PRINTF(COLOR_YELLOW);
        break;
    case NX_LEVEL_INFO:
        PRINTF(COLOR_BLUE);
        break;
    case NX_LEVEL_DEBUG:
        /* As of now put color here. All normal printfs would be in WHITE
             * Later, remove this color.
             */
        PRINTF(COLOR_GREEN);
        break;
    default:
        PRINTF(COLOR_RESET);
    }
#endif // USE_COLORED_LOGS
}

static void ansi_reSetColor()
{
#if USE_COLORED_LOGS
#if !AX_EMBEDDED
    if (!isatty(fileno(stdout))) {
        return;
    }
#endif
    PRINTF(COLOR_RESET);
#endif // USE_COLORED_LOGS
}
#endif

#ifdef __cplusplus
}
#endif
