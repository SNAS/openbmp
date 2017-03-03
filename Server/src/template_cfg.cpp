/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#include <fstream>
#include <cstring>
#include "template_cfg.h"

namespace template_cfg {
/**
 * Constructor for class
 *
 * \details Handles bgp update messages
 *
 */
    Template_cfg::Template_cfg(Logger *logPtr, bool enable_debug)
            : logger(logPtr),
              debug(enable_debug){}

    Template_cfg::~Template_cfg(){
    }

    void Template_cfg::load(const char *template_filename, std::map<template_cfg::TEMPLATE_TOPICS, template_cfg::Template_cfg> &template_map) {
        /*
         * Read the top level template file and populate top level template map
         */
        std::ifstream in(template_filename);
        std::string contents((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        std::cout << "Input file is " << contents.c_str() << std::endl;

        char tmp[contents.length() + 1];
        std::strcpy(tmp, contents.c_str());

        char *bpos = tmp;
        char *epos = tmp;
        std::string prepend_string;
        size_t remaining_length = strlen(bpos);
        size_t read = 0;

        /*
         * Append to a prepend string
         */
        while (remaining_length > 0) {
            epos = strstr(bpos, "{{");
            if (!epos) {
                cout << "Done loading" << endl;
                return;
            }

            prepend_string.append(bpos, epos - bpos);
            std::cout << "Prepend string is now " << prepend_string << std::endl;

            remaining_length -= epos - bpos + 2;
            bpos += epos - bpos + 2; epos = bpos;
            std::cout << "load bpos: " << bpos << std::endl;
            std::cout << "load epos: " << epos << std::endl;


            if (strncmp(bpos, "/*", strlen("/*")) == 0) {
                std::cout << "Found a comment, skipping" << std::endl;
                epos = strstr(bpos, "}}");
                remaining_length -= epos - bpos + 2;
                bpos += epos - bpos + 2; epos = bpos;
            } else if (strncmp(bpos, "#", strlen("#")) == 0) {
                std::cout << "Found a special type" << std::endl;
                remaining_length -= 1;
                bpos += 1; epos = bpos;
                if (strncmp(bpos, "loop", strlen("loop")) == 0) {
                    std::cout << "Error: Found a loop type at top level" << std::endl;
                    return;
                } else if (strncmp(bpos, "if_", strlen("if_")) == 0) {
                    std::cout << "Error: Found a if type at top level" << std::endl;
                    return;
                } else {
                    std::cout << "Found container type" << std::endl;
                    template_cfg::Template_cfg template_cfg(logger, debug);
                    if (strncmp(bpos, "unicast_prefix", strlen("unicast_prefix")) == 0) {
                        epos = strstr(bpos, "}}");
                        remaining_length -= epos - bpos + 2;
                        bpos += epos - bpos + 2;
                        epos = bpos;

                        read = template_cfg.create_container_loop(CONTAINER, UNICAST_PREFIX, bpos, prepend_string);
                        /*
                         * Clear the prepend string
                         */
                        prepend_string.clear();
                        if (!read) {
                            std::cout << "Error creating container" << std::endl;
                            return;
                        }

                        remaining_length -= read;
                        bpos += read; epos = bpos;
                        template_map.insert(std::pair<template_cfg::TEMPLATE_TOPICS, template_cfg::Template_cfg>(UNICAST_PREFIX,
                                                                                                     template_cfg));
                    }
                }
            }
        }
    }


    size_t Template_cfg::create_container_loop(TEMPLATE_TYPES type, TEMPLATE_TOPICS topic, char *buf, std::string &in_prepend_string) {
        this->prepend_string.append(in_prepend_string);
        this->type = type;
        this->topic = topic;

        char *bpos = buf;
        char *epos = buf;
        std::string prepend_string;
        size_t remaining_length = strlen(bpos);
        size_t read, total_read = 0;

        /*
       * Append to a prepend string
       */
        while (remaining_length > 0) {
            epos = strstr(bpos, "{{");

            prepend_string.append(bpos, epos - bpos);
            std::cout << "Prepend string is now " << prepend_string << std::endl;

            std::cout << "container bpos: " << bpos << std::endl;
            std::cout << "container epos: " << epos << std::endl;

            remaining_length -= epos - bpos + 2; total_read += epos - bpos + 2;
            bpos += epos - bpos + 2; epos = bpos;

            if (strncmp(bpos, "/*", strlen("/*")) == 0) {
                std::cout << "Found a comment, skipping" << std::endl;
                epos = strstr(bpos, "}}");
                std::cout << "container bpos: " << bpos << std::endl;
                std::cout << "container epos: " << epos << std::endl;
                remaining_length -= epos - bpos + 2; total_read += epos - bpos + 2;
                bpos += epos - bpos + 2; epos = bpos;
            } else if (strncmp(bpos, "#", strlen("#")) == 0) {
                std::cout << "Found a special type" << std::endl;
                remaining_length -= 1; total_read += 1;
                bpos += 1; epos = bpos;
                if (strncmp(bpos, "loop", strlen("loop")) == 0) {
                    std::cout << " Found a loop type inside type" << type << std::endl;
                    template_cfg::Template_cfg template_cfg(logger, debug);
                    if (type == LOOP) {
                        std::cout << " Error: Found a loop type inside type" << type << std::endl;
                        return (0);
                    }
                    epos = strstr(bpos, "}}");
                    std::cout << "container bpos: " << bpos << std::endl;
                    std::cout << "container epos: " << epos << std::endl;
                    remaining_length -= epos - bpos + 2; total_read += epos - bpos + 2;
                    bpos += epos - bpos + 2; epos = bpos;

                    read = template_cfg.create_container_loop(LOOP, topic, bpos, prepend_string);
                    /*
                     * Clear the prepend string
                     */
                    prepend_string.clear();

                    if (!read) {
                        std::cout << "Error creating container" << std::endl;
                        return(0);
                    }

                    remaining_length -= read; total_read += read;
                    bpos += read; epos = bpos;
                    this->template_children.push_back(template_cfg);
                } else if (strncmp(bpos, "if_", strlen("if_")) == 0) {
                    std::cout << "Found a if type inside container" << std::endl;
                } else if (strncmp(bpos, "end", strlen("end")) == 0) {
                    std::cout << "Found a end type inside container or loop" << std::endl;
                    epos = strstr(bpos, "}}");
                    std::cout << "container bpos: " << bpos << std::endl;
                    std::cout << "container epos: " << epos << std::endl;
                    remaining_length -= epos - bpos + 2; total_read += epos - bpos + 2;
                    cout << "container/loop type" << type << " read is " << total_read << endl;
                    return(total_read);
                } else {
                    std::cout << "Error: Found container type in container" << std::endl;
                    return (0);
                }
            } else {
                std::cout << "Found replacement type inside type " << type << std::endl;
                template_cfg::Template_cfg template_cfg(logger, debug);
                read = template_cfg.create_replacement(bpos, prepend_string);
                /*
                 * Clear the prepend string
                 */
                prepend_string.clear();

                if (!read) {
                    std::cout << "Error creating replacement" << std::endl;
                    return(0);
                }

                remaining_length -= read; total_read += read;
                bpos += read; epos = bpos;
                this->template_children.push_back(template_cfg);
            }
        }
        cout << "container/loop type" << type << " read is " << total_read << endl;

        return(total_read);
    }

    std::map<std::string, int> Template_cfg::lookup_map;

    size_t Template_cfg::create_replacement(char *buf, std::string &in_prepend_string) {
        this->prepend_string.append(in_prepend_string);
        this->type = REPLACE;

        char *bpos = buf;
        char *epos = buf;
        size_t read = 0;

        std::cout << "replacement bpos1: " << bpos << std::endl;
        std::cout << "replacement epos1: " << epos << std::endl;

        if (template_cfg::Template_cfg::lookup_map.empty()) {
            /*
             * Populate the lookup map
             */

            std::cout << "Creating the lookup map" << std::endl;
            for (int i = 0; i < parse_bgp_lib::LIB_ATTR_MAX; i++) {
                template_cfg::Template_cfg::lookup_map.insert(
                        std::pair<std::string, int>(parse_bgp_lib::parse_bgp_lib_attr_names[i], i));
            }

            for (int i = 0; i < parse_bgp_lib::LIB_NLRI_MAX; i++) {
                template_cfg::Template_cfg::lookup_map.insert(
                        std::pair<std::string, int>(parse_bgp_lib::parse_bgp_lib_nlri_names[i], i));
            }

            //TODO: Remove, test the map
            for (std::map<std::string, int>::iterator it = template_cfg::Template_cfg::lookup_map.begin();
                 it != template_cfg::Template_cfg::lookup_map.end();
                 it++) {
                std::cout << " lookup map string: " << it->first << ", value: " << it->second << std::endl;
            }
        }

        std::cout << "Prepend string for replace is now " << this->prepend_string << std::endl;
        epos = strstr(bpos, ".");
        std::cout << "replacement bpo2: " << bpos << std::endl;
        std::cout << "replacement epos2: " << epos << std::endl;
        if (!epos) {
            std::cout << "Error replacement variable is empty" << std::endl;
            return(0);
        }
        std::cout << "Replacement variable list is " << epos << std::endl;

        if (strncmp(bpos, "nlri", strlen("nlri")) == 0) {
            this->replacement_list_type = NLRI;
       } else if (strncmp(bpos, "attr", strlen("attr")) == 0) {
            this->replacement_list_type = ATTR;
        } else {
            cout << "Invalid replacement type " << epos << std::endl;
        }

        read += epos - bpos + 1;
        bpos += epos - bpos + 1; epos = bpos;

        epos = strstr(bpos, "}}");
        std::cout << "replacement bpos3: " << bpos << std::endl;
        std::cout << "replacement epos3: " << epos << std::endl;
        if (!epos) {
            std::cout << "Error replacement variable is not closed" << std::endl;
            return(0);
        }
        std::string lookup_string(bpos, epos - bpos);
        this->replacement_var = template_cfg::Template_cfg::lookup_map.find(lookup_string)->second;
        read += epos - bpos + 2;
        cout << "Created replacement var list type " << this->replacement_list_type << ", var is " << this->replacement_var << std::endl;
        cout << "replacement read is " << read << endl;

        return(read);
    }

}
