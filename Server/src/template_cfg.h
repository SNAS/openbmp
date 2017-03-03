/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef TEMPLATE_CFG_H_
#define TEMPLATE_CFG_H_

#include <string>
#include <list>
#include <map>
#include "Logger.h"
#include "parseBgpLib.h"

namespace template_cfg {
    /**
     * Defines the template topics
     *
     *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
     */
    enum TEMPLATE_TOPICS {
        UNICAST_PREFIX,
    };


    enum TEMPLATE_TYPES {
        CONTAINER,
        LOOP,
        REPLACE,
    };

    enum REPLACEMENT_LIST_TYPE {
        ATTR,
        NLRI
    };

    /**
     * \class   Template_cfg
     *
     * \brief   Template configuration class for openbmpd
     * \details
     *      Parses the template configuration file and loads value in this class instance.
     */
    class Template_cfg {
    public:
        /*********************************************************************//**
         * Constructor for class
         ***********************************************************************/
        Template_cfg(Logger *logPtr, bool enable_debug);

        virtual ~Template_cfg();

        static std::map<std::string, int> lookup_map;


        /*********************************************************************//**
         * Load configuration from file
         *
         * \param [in] template_filename     template filename
         ***********************************************************************/
        void load(const char *template_filename, std::map<template_cfg::TEMPLATE_TOPICS, template_cfg::Template_cfg> &template_map);
        size_t create_container_loop(TEMPLATE_TYPES type, TEMPLATE_TOPICS topic, char *buf, std::string &prepend_string);
        size_t create_replacement(char *buf, std::string &prepend_string);

        std::list<template_cfg::Template_cfg> template_children;

        TEMPLATE_TYPES type;
        TEMPLATE_TOPICS topic;

        REPLACEMENT_LIST_TYPE replacement_list_type;
        int replacement_var;


    private:
        bool debug;                             ///< debug flag to indicate debugging
        Logger *logger;                         ///< Logging class pointer
        std::string prepend_string;
    };
}

#endif /* TEMPLATE_CFG_H_ */
