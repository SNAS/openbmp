/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#ifndef LOGGER_H_
#define LOGGER_H_

#include <cstdio>
#include <iostream>
#include <cstdint>


/*
 * DEBUG is a macro for DebugPrint with FILE, LINE, FUNCTION added
 */
#define DEBUG(...) logger->DebugPrint(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define SELF_DEBUG(...) if (debug) logger->DebugPrint(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

/*
 * Below defines LOG macros for various severities
 */
#define LOG_INFO(...)    logger->Print("INFO",   __FUNCTION__, __VA_ARGS__)
#define LOG_WARN(...)    logger->Print("WARN",   __FUNCTION__, __VA_ARGS__)
#define LOG_NOTICE(...)  logger->Print("NOTICE", __FUNCTION__, __VA_ARGS__)
#define LOG_ERR(...)     logger->Print("ERROR",  __FUNCTION__, __VA_ARGS__)

/**
 * \class   Logger
 *
 * \brief   Logger class/instance that is used for common logging
 * \details
 *          Each class will need to define and implement the enableDebug()
 *          and disableDebug() methods that toggle the private variable
 *          *debug*.    Each class also needs to define the private
 *          variable *log* which is a Logger pointer.  Upon class init
 *          the log variable should be set by the provided/passed
 *          log pointer.
 *
 *          Each class can then use the macro SELF_DEBUG() to log only
 *          debug when debug has been enabled.
 *
 *          The DEBUG() macro will log when global debug has been
 *          enabled, which is the default when any debug has been toggled.
 *
 *          LOG_<sev>() macros are used for general logging, not DEBUG.
 *
 *      \code{.cpp}
 *      public:
 *      void Logger::disableDebug(void) {
 *          debug = false;
 *      }
 *
 *      void Logger::enableDebug(void) {
 *          debug = true;
 *      }
 *
 *      private:
 *      bool            debug;        ///< debug flag to indicate debugging
 *      Logger          *log;         ///< Logging class pointer
 *
 */
class Logger {
public:

    /*********************************************************************//**
     * Constructor for class
     *
     * \param [in]  log_filename  log file name or NULL to use stdout
     * \param [in]  debug_filename = debug log filename or NULL to use stdout
     *
     * \throws const char * with the error message
     ***********************************************************************/
    Logger(const char *log_filename, const char *debug_filename);

    /*********************************************************************//**
     * Destructor for class
     ***********************************************************************/
    virtual ~Logger();

    /*********************************************************************//**
     * Enables debug logging
     ***********************************************************************/
    void enableDebug(void);

    /*********************************************************************//**
     * Disables debug logging
     ***********************************************************************/
    void disableDebug(void);

    /*********************************************************************//**
     * Sets the function width for printing
     *
     * \param[in] width     Column width to use for printing
     ***********************************************************************/
    void setWidthFunction(u_char width);

    /*********************************************************************//**
     * Sets the filename width for printing
     *
     * \param[in] width     Column width to use for printing
     ***********************************************************************/
    void setWidthFilename(u_char width);


    /*********************************************************************//**
     * Prints the message
     *
     *
     * \param[in]  sev          the logging severity
     * \param[in]  func_name    function name of the calling function
     * \param[in]  msg          message to print, can contain sprintf formats
     * \param[in]  ...          Optional list of args for vfprintf
     ***********************************************************************/
    void Print(const char *sev, const char *func_name, const char *msg, ...);

    /*********************************************************************//**
     * Prints debug message if debug is enabled
     *
     * \param[in]  filename     the source file that originated the debug message
     * \param[in]  line_num     the line number from the file that originated the debug message
     * \param[in]  func_name    function name of the calling function
     * \param[in]  msg          message to print, can contain sprintf formats
     * \param[in]  ...          Optional list of args for vfprintf
     *
     ***********************************************************************/
    void DebugPrint(const char *filename, int line_num, const char *func_name, const char *msg, ...);


private:
    bool    logFile_REALFILE;           ///< Indicates if the log file is using a real file or not
    bool    debugFile_REALFILE;         ///< Indicates if the debug log file is using a real file or not
    FILE    *debugFile;                 ///< Debug log file
    FILE    *logFile;                   ///< Log file
    bool    debugEnabled;               ///< Enable Debug


    u_char  width_function;             ///< Defines the width of the function field when printed
    u_char  width_filename;             ///< Defines the width of the filename field when printed


    /**
     * Prints the message using a variable arg list
     *
     * \param [in]  sev         the logging severity
     * \param [in]  filename    the source file that originated the debug message
     *                          IF NULL, then the filename and line number will not be included
     * \param [in]  line_num    the line number from the file that originated the debug message
     * \param [in]  func_name   function name of the calling function
     * \param [in]  msg         message to print, can contain sprintf formats
     * \param [in]  args        variable list of args for vfprintf
     */
    void printV(const char *sev,
                FILE *output,
                const char *filename,
                int line_num,
                const char *func_name,
                const char *msg,
                va_list args);

};
#endif /* LOGGER_H_ */
