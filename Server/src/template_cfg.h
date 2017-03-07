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
        LS_NODES,
        LS_LINKS,
        LS_PREFIXES,
        L3_VPN,
        EVPN,
    };


    enum TEMPLATE_TYPES {
        CONTAINER,
        LOOP,
        REPLACE,
        END,
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
        size_t
        create_container_loop(TEMPLATE_TYPES type, TEMPLATE_TOPICS topic, char *buf, std::string &prepend_string);

        size_t create_replacement(char *buf, std::string &prepend_string);

        size_t execute_container(char *buf, size_t max_buf_length,
                               std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                               parse_bgp_lib::parseBgpLib::attr_map &attrs);

        size_t execute_loop(char *buf, size_t max_buf_length,
                                 std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                 parse_bgp_lib::parseBgpLib::attr_map &attrs);

        size_t execute_replace(char *buf, size_t max_buf_length,
                            parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri &nlri,
                            parse_bgp_lib::parseBgpLib::attr_map &attrs);

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

    class Template_map {
    public:
        /*********************************************************************//**
         * Constructor for class
         ***********************************************************************/
        Template_map(Logger *logPtr, bool enable_debug);

        virtual ~Template_map();

        std::map<template_cfg::TEMPLATE_TOPICS, template_cfg::Template_cfg> template_map;

        /*********************************************************************//**
         * Load configuration from file
         *
         * \param [in] template_filename     template filename
         ***********************************************************************/
        bool load(const char *template_filename);

    private:
        bool debug;                             ///< debug flag to indicate debugging
        Logger *logger;                         ///< Logging class pointer
    };

static void print_template (template_cfg::Template_cfg template_cfg_print, size_t iteration) {
    std::string append_str(iteration, 32);
    switch (template_cfg_print.type) {
        case template_cfg::CONTAINER : {
            cout << append_str << "Container: ";
            switch (template_cfg_print.topic) {
                case template_cfg::UNICAST_PREFIX : {
                    cout << "unicast_prefix" << endl;
                    break;
                }
                case template_cfg::LS_NODES : {
                    cout << "ls_node" << endl;
                    break;
                }
                case template_cfg::LS_LINKS: {
                    cout << "ls_links" << endl;
                    break;
                }
                case template_cfg::LS_PREFIXES : {
                    cout << "ls_prefixes" << endl;
                    break;
                }
                case template_cfg::L3_VPN : {
                    cout << "l3vpn" << endl;
                    break;
                }
                case template_cfg::EVPN : {
                    cout << "evpn" << endl;
                    break;
                }
                default:
                    break;
            }
            for (std::list<template_cfg::Template_cfg>::iterator it = template_cfg_print.template_children.begin();
                 it != template_cfg_print.template_children.end(); it++) {
                print_template(*it, iteration + 4);
            }
            break;
        }
        case template_cfg::LOOP : {
            cout << append_str.c_str() << "Loop: " << endl;
            for (std::list<template_cfg::Template_cfg>::iterator it = template_cfg_print.template_children.begin();
                 it != template_cfg_print.template_children.end(); it++) {
                print_template(*it, iteration + 4);
            }
            break;
        }
        case template_cfg::END : {
            cout << append_str.c_str() << "End " << endl;
            break;
        }
        case template_cfg::REPLACE : {
            cout << append_str.c_str() << "Replacement: List: ";
            switch (template_cfg_print.replacement_list_type) {
                case template_cfg::ATTR :
                    cout << "ATTR, replacement variable: " << template_cfg_print.replacement_var << endl;
                    break;
                case template_cfg::NLRI :
                    cout << "NLRI, replacement variable: " << template_cfg_print.replacement_var << endl;
                    break;
                default:
                    break;
            }
        }
        default:
            break;
    }
}


#endif /* TEMPLATE_CFG_H_ */
