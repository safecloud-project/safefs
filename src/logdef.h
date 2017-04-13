/*

Sfuse
  (c) 2016 2016 INESC TEC.  by J. Paulo (ADD Other authors here)

*/
#ifndef __LOGDEF_H__
#define __LOGDEF_H__

#define LOCAL_ZLOGCONFIG_PATH "zlog.conf"
#define DEFAULT_ZLOGCONFIG_PATH "/etc/safefs/zlog.conf"
/**
 * Initializes the logging facilities
 */
void LOG_INIT();

/**
 * Tears down the logging facilities
 */
void LOG_EXIT();

/**
 * Logs a debug message
 * @param format Format message
 */
void DEBUG_MSG(const char *format, ...);

/**
 * Logs an error message
 * @param format Format message
 */
void ERROR_MSG(const char *format, ...);

/**
 * Prints a message on the screen
 * @param format Format message
 */
void SCREEN_MSG(const char *format, ...);
#endif /*__LOGDEF_H__*/
