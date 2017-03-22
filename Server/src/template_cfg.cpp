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
#include "MsgBusInterface.hpp"
#include <yaml-cpp/yaml.h>
#include <boost/xpressive/xpressive.hpp>
#include <boost/exception/all.hpp>

/**
 * Constructor for class
 *
 * \details Handles bgp update messages
 *
 */
    Template_map::Template_map(Logger *logPtr, bool enable_debug)
            : logger(logPtr),
              debug(enable_debug){}

    Template_map::~Template_map(){
    }

    bool Template_map::load(const char *template_filename) {
        template_cfg::TEMPLATE_TOPICS topic;
        template_cfg::TEMPLATE_FORMAT_TYPE format;

        std::string prepend_string;
        size_t read = 0;

        if (debug)
            std::cout << "---| Loading template file |----------------------------- " << std::endl;

        try {
            YAML::Node root = YAML::LoadFile(template_filename);

            /*
             * Iterate through the root node objects
             */

            if (root.Type() == YAML::NodeType::Map) {
                for (YAML::const_iterator it = root.begin(); it != root.end(); ++it) {
                    const YAML::Node &node = it->second;
                    const std::string &key = it->first.Scalar();
                    template_cfg::Template_cfg template_cfg(logger, debug);
                    if (key.compare("unicast_prefix") == 0) {
                        topic = template_cfg::UNICAST_PREFIX;
                    } else if (key.compare("ls_nodes") == 0) {
                        topic = template_cfg::LS_NODES;
                    } else if (key.compare("ls_links") == 0) {
                        topic = template_cfg::LS_LINKS;
                    } else if (key.compare("ls_prefixes") == 0) {
                        topic = template_cfg::LS_PREFIXES;
                    } else if (key.compare("l3vpn") == 0) {
                        topic = template_cfg::L3_VPN;
                    } else if (key.compare("evpn") == 0) {
                        topic = template_cfg::EVPN;
                    } else {
                        std::cout << "Unknown template topic" << std::endl;
                        return (false);
                    }

                    for (YAML::const_iterator it2 = node.begin(); it2 != node.end(); ++it2) {
                        const YAML::Node &node2 = it2->second;
                        const std::string &key2 = it2->first.Scalar();
                        if (key2.compare("format") == 0) {
                            std::string value;

                            try {
                                value = node2.as<std::string>();

                                if (value.compare("raw") == 0) {
                                    format = template_cfg::RAW;
                                } else if (value.compare("tsv") == 0) {
                                    format = template_cfg::TSV;
                                } else {
                                    cout << "Format should only be raw or tsv" << endl;
                                }

                            } catch (YAML::TypedBadConversion<std::string> err) {
                                cout << "template type is not of type string" << endl;
                                return (false);
                            }
                        } else if (key2.compare("schema") == 0) {
                            std::string value;

                            try {
                                value = node2.as<std::string>();
                                cout << "Loaded schema is: " << value.c_str();
                                read = template_cfg.create_container_loop(template_cfg::CONTAINER, topic, (char *)value.c_str(), prepend_string);
                                if (!read) {
                                    std::cout << "Error creating container" << std::endl;
                                    return (false);
                                }

                            } catch (YAML::TypedBadConversion<std::string> err) {
                                cout << "template type is not of type string" << endl;
                                return (false);
                            }
                        } else {
                            std::cout << "Unknown template container type" << std::endl;
                            return (false);
                        }
                    }
                    this->template_map.insert(
                            std::pair<template_cfg::TEMPLATE_TOPICS, template_cfg::Template_cfg>(topic,
                                                                                                 template_cfg));

                    if (debug)
                        std::cout << "   Template: Key " << key << " Type " << node.Type() << std::endl;

                }

         } else {
                    cout << "template should only have maps at the root/base level found" << endl;
            }

        } catch (YAML::BadFile err) {
            throw err.what();
        } catch (YAML::ParserException err) {
            throw err.what();
        } catch (YAML::InvalidNode err) {
            throw err.what();
        }

        if (debug)
            std::cout << "---| Done Loading template file |------------------------- " << std::endl;
        return (true);
    }

namespace template_cfg {


    Template_cfg::Template_cfg(Logger *logPtr, bool enable_debug)
            : logger(logPtr),
              debug(enable_debug){}

    Template_cfg::~Template_cfg(){
    }

    size_t Template_cfg::execute_replace(char *buf, size_t max_buf_length,
                                           parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri &nlri,
                                           parse_bgp_lib::parseBgpLib::attr_map &attrs, parse_bgp_lib::parseBgpLib::peer_map &peer) {

        char buf2[80000] = {0}; // Second working buffer

        size_t  remaining_len = max_buf_length, written = 0;
        std::string replace_string;

        strncpy(buf2, this->prepend_string.c_str(), this->prepend_string.length());
        written = this->prepend_string.length();
        if ((remaining_len - written) <= 0) {
            return (max_buf_length - remaining_len);
        }
        strncpy(buf, buf2, written);
        buf += written; remaining_len -= written;

        switch (this->replacement_list_type) {
            case template_cfg::ATTR : {
                replace_string = map_string(attrs[static_cast<parse_bgp_lib::BGP_LIB_ATTRS>(this->replacement_var)].value);
                break;
            }
            case template_cfg:: NLRI : {
                replace_string = map_string(nlri.nlri[static_cast<parse_bgp_lib::BGP_LIB_NLRI>(this->replacement_var)].value);
                break;
            }
            case template_cfg:: PEER : {
                replace_string = map_string(peer[static_cast<parse_bgp_lib::BGP_LIB_PEER>(this->replacement_var)].value);
                break;
            }
                default:
                break;
        }

        strncpy(buf2, replace_string.c_str(), replace_string.length());
        written = replace_string.length();
        if ((remaining_len - written) <= 0) {
            return (max_buf_length - remaining_len);
        }
        strncpy(buf, buf2, written);
        remaining_len -= written;

        return (max_buf_length - remaining_len);
    }

    size_t Template_cfg::execute_loop(char *buf, size_t max_buf_length,
                                           std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                           parse_bgp_lib::parseBgpLib::attr_map &attrs, parse_bgp_lib::parseBgpLib::peer_map &peer) {

        char buf2[80000] = {0}; // Second working buffer

        size_t  remaining_len = max_buf_length, written = 0;

        strncpy(buf2, this->prepend_string.c_str(), this->prepend_string.length());
        written = this->prepend_string.length();
        if ((remaining_len - written) <= 0) {
            return (max_buf_length - remaining_len);
        }
        strncpy(buf, buf2, written);
        buf += written; remaining_len -= written;

        for (size_t i = 0; i < rib_list.size(); i++) {
            if (i != 0) {
                //TODO: change this to a {{#more, ', '}} type object
                strncpy(buf2, ",", strlen(","));
                written = strlen(",");
                if ((remaining_len - written) <= 0) {
                    return (max_buf_length - remaining_len);
                }
                strncpy(buf, buf2, written);
                buf += written; remaining_len -= written;
            }

            for (std::list<template_cfg::Template_cfg>::iterator it = this->template_children.begin();
                 it != this->template_children.end(); it++) {
                switch (it->type) {
                    case template_cfg::CONTAINER : {
                        cout << "Error: container found inside loop" << endl;
                        return (0);
                    }
                    case template_cfg::LOOP : {
                        cout << "Error: loop found inside loop" << endl;
                        return (0);
                    }
                    case template_cfg::REPLACE : {
                        written = it->execute_replace(buf, remaining_len, rib_list[i], attrs, peer);
                        if ((remaining_len - written) <= 0) {
                            return (max_buf_length - remaining_len);
                        }
                        buf += written; remaining_len -= written;
                        break;
                    }
                    case template_cfg::END : {
                        strncpy(buf2, it->prepend_string.c_str(), it->prepend_string.length());
                        written = it->prepend_string.length();
                        if ((remaining_len - written) <= 0) {
                            return (max_buf_length - remaining_len);
                        }
                        strncpy(buf, buf2, written);
                        buf+= written; remaining_len -= written;
                    }
                    default:
                        break;
                }
            }
      }

        return (max_buf_length - remaining_len);
    }

    size_t Template_cfg::execute_container(char *buf, size_t max_buf_length,
                                         std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                         parse_bgp_lib::parseBgpLib::attr_map &attrs, parse_bgp_lib::parseBgpLib::peer_map &peer) {

        char buf2[80000] = {0}; // Second working buffer

        size_t  remaining_len = max_buf_length, written = 0;

        strncpy(buf2, this->prepend_string.c_str(), this->prepend_string.length());
        written = this->prepend_string.length();
        if ((remaining_len - written) <= 0) {
            return (max_buf_length - remaining_len);
        }
        strncpy(buf, buf2, written);
        buf += written; remaining_len -= written;

        for (std::list<template_cfg::Template_cfg>::iterator it = this->template_children.begin();
             it != this->template_children.end(); it++) {
            switch (it->type) {
                case template_cfg::CONTAINER : {
                    cout << "Error: container found inside container" << endl;
                    return (0);
                }
                case template_cfg:: LOOP : {
                    written = it->execute_loop(buf, remaining_len, rib_list, attrs, peer);
                    if ((remaining_len - written) <= 0) {
                        return (max_buf_length - remaining_len);
                    }
                    buf += written; remaining_len -= written;
                    break;
                }
                case template_cfg:: REPLACE : {
                    if (it->replacement_list_type == NLRI) {
                        cout << "Error: Replacement type inside container cannot be NLRI" << endl;
                        return (0);
                    }
                    written = it->execute_replace(buf, remaining_len, rib_list[0], attrs, peer);
                    if ((remaining_len - written) <= 0) {
                        return (max_buf_length - remaining_len);
                    }
                    buf += written; remaining_len -= written;
                    break;
                }
                case template_cfg:: END : {
                    strncpy(buf2, it->prepend_string.c_str(), it->prepend_string.length());
                    written = it->prepend_string.length();
                    if ((remaining_len - written) <= 0) {
                        return (max_buf_length - remaining_len);
                    }
                    strncpy(buf, buf2, written);
                    buf+= written; remaining_len -= written;
                }
                default:
                    break;
            }
        }

       return (max_buf_length - remaining_len);
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
            if (!epos) {

                if (this->type == template_cfg::LOOP) {
                    cout << "Error: Non-closed Loop" << endl;
                    return (0);
                }
                cout << "Craft end object for container" << endl;

                template_cfg::Template_cfg template_cfg(logger, debug);
                template_cfg.type = template_cfg::END;
                template_cfg.prepend_string.append(bpos, remaining_length);

                this->template_children.push_back(template_cfg);
                total_read += remaining_length;
                return(total_read);
            }

            prepend_string.append(bpos, epos - bpos);
//            std::cout << "create_container Prepend string is now " << prepend_string << std::endl;

//            std::cout << "container bpos: " << bpos << std::endl;
//            std::cout << "container epos: " << epos << std::endl;

            remaining_length -= epos - bpos + 2; total_read += epos - bpos + 2;
            bpos += epos - bpos + 2; epos = bpos;

            if (strncmp(bpos, "/*", strlen("/*")) == 0) {
                std::cout << "Found a comment, skipping" << std::endl;

                //Strip the newline
                strip_last_newline(prepend_string);

                epos = strstr(bpos, "}}");
 //               std::cout << "container bpos: " << bpos << std::endl;
//                std::cout << "container epos: " << epos << std::endl;
                remaining_length -= epos - bpos + 2; total_read += epos - bpos + 2;
                bpos += epos - bpos + 2; epos = bpos;
            } else if (strncmp(bpos, "#", strlen("#")) == 0) {
                std::cout << "Found a special type" << std::endl;
                strip_last_newline(prepend_string);

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
//                    std::cout << "container bpos: " << bpos << std::endl;
//                    std::cout << "container epos: " << epos << std::endl;
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
//                    std::cout << "container bpos: " << bpos << std::endl;
//                    std::cout << "container epos: " << epos << std::endl;
                    template_cfg::Template_cfg template_cfg(logger, debug);
                    template_cfg.type = template_cfg::END;
                    template_cfg.prepend_string.append(prepend_string);
                    prepend_string.clear();

                    this->template_children.push_back(template_cfg);
                    remaining_length -= epos - bpos + 2; total_read += epos - bpos + 2;
//                    cout << "container/loop type" << type << " read is " << total_read << endl;
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

//        std::cout << "replacement bpos1: " << bpos << std::endl;
//        std::cout << "replacement epos1: " << epos << std::endl;

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

            for (int i = 0; i < parse_bgp_lib::LIB_PEER_MAX; i++) {
                template_cfg::Template_cfg::lookup_map.insert(
                        std::pair<std::string, int>(parse_bgp_lib::parse_bgp_lib_peer_names[i], i));
            }

        }

//        std::cout << "Prepend string for replace is now " << this->prepend_string << std::endl;
        epos = strstr(bpos, ".");
//        std::cout << "replacement bpo2: " << bpos << std::endl;
//        std::cout << "replacement epos2: " << epos << std::endl;
        if (!epos) {
            std::cout << "Error replacement variable is empty" << std::endl;
            return(0);
        }
        std::cout << "Replacement variable list is " << epos << std::endl;

        if (strncmp(bpos, "nlri", strlen("nlri")) == 0) {
            this->replacement_list_type = NLRI;
       } else if (strncmp(bpos, "attr", strlen("attr")) == 0) {
            this->replacement_list_type = ATTR;
        } else if (strncmp(bpos, "peer", strlen("peer")) == 0) {
            this->replacement_list_type = PEER;
        } else {
            cout << "Invalid replacement type " << epos << std::endl;
        }

        read += epos - bpos + 1;
        bpos += epos - bpos + 1; epos = bpos;

        epos = strstr(bpos, "}}");
//        std::cout << "replacement bpos3: " << bpos << std::endl;
//        std::cout << "replacement epos3: " << epos << std::endl;
        if (!epos) {
            std::cout << "Error replacement variable is not closed" << std::endl;
            return(0);
        }
        std::string lookup_string(bpos, epos - bpos);
        std::map<std::string, int>::iterator it = template_cfg::Template_cfg::lookup_map.find(lookup_string);
        if (it == template_cfg::Template_cfg::lookup_map.end()) {
            /*
             * Couldnt find
             */
            cout << "Error in replacement variable name" << endl;
            return(0);
        }

        this->replacement_var = it->second;
        read += epos - bpos + 2;
        cout << "Created replacement var list type " << this->replacement_list_type << ", var is " << this->replacement_var << std::endl;
        cout << "replacement read is " << read << endl;

        return(read);
    }

}
