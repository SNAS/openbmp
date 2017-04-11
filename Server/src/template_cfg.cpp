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
        template_cfg::TEMPLATE_FORMAT_TYPE format = template_cfg::RAW;

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
                    } else if (key.compare("router") == 0) {
                        topic = template_cfg::BMP_ROUTER;
                    } else if (key.compare("collector") == 0) {
                        topic = template_cfg::BMP_COLLECTOR;
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
                                    return (false);
                                }

                            } catch (YAML::TypedBadConversion<std::string> err) {
                                cout << "template type is not of type string" << endl;
                                return (false);
                            }
                        } else if (key2.compare("schema") == 0) {
                            std::string value;

                            if (format == template_cfg::RAW) {
                                try {
                                    value = node2.as<std::string>();
//                                cout << "Loaded schema is: " << value.c_str();
                                    read = template_cfg.create_container_loop(template_cfg::CONTAINER, topic,
                                                                              (char *) value.c_str(), prepend_string);
                                    if (!read) {
                                        std::cout << "Error creating container" << std::endl;
                                        return (false);
                                    }
                                } catch (YAML::TypedBadConversion<std::string> err) {
                                    cout << "template type is not of type string" << endl;
                                    return (false);
                                }
                            } else if (format == template_cfg::TSV) {
                                if (!template_cfg.create_container_loop_tsv(template_cfg::CONTAINER, topic, node2)) {
                                    std::cout << "Error creating container" << std::endl;
                                    return (false);
                                }
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
                    cout << "template should only have maps at the root/base level" << endl;
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
                                         parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                         parse_bgp_lib::parseBgpLib::peer_map &peer,
                                         parse_bgp_lib::parseBgpLib::router_map &router,
                                         parse_bgp_lib::parseBgpLib::collector_map &collector,
                                         parse_bgp_lib::parseBgpLib::header_map &header) {

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
            case template_cfg::NLRI : {
                replace_string = map_string(nlri.nlri[static_cast<parse_bgp_lib::BGP_LIB_NLRI>(this->replacement_var)].value);
                break;
            }
            case template_cfg::PEER : {
                replace_string = map_string(peer[static_cast<parse_bgp_lib::BGP_LIB_PEER>(this->replacement_var)].value);
                break;
            }
            case template_cfg::ROUTER : {
                replace_string = map_string(router[static_cast<parse_bgp_lib::BGP_LIB_ROUTER>(this->replacement_var)].value);
                break;
            }
            case template_cfg::COLLECTOR : {
                replace_string = map_string(collector[static_cast<parse_bgp_lib::BGP_LIB_COLLECTOR>(this->replacement_var)].value);
                break;
            }
            case template_cfg::HEADER : {
                replace_string = map_string(header[static_cast<parse_bgp_lib::BGP_LIB_HEADER>(this->replacement_var)].value);
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
                                      parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                      parse_bgp_lib::parseBgpLib::peer_map &peer,
                                      parse_bgp_lib::parseBgpLib::router_map &router,
                                      parse_bgp_lib::parseBgpLib::collector_map &collector,
                                      parse_bgp_lib::parseBgpLib::header_map &header, uint64_t &sequence) {

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
            if ((i != 0) and (this->format == template_cfg::RAW)) {
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
                        if ((it->replacement_list_type == template_cfg::HEADER) and
                                (it->replacement_var == parse_bgp_lib::LIB_HEADER_SEQUENCE_NUMBER)) {
                            strncpy(buf2, it->prepend_string.c_str(), it->prepend_string.length());
                            written = it->prepend_string.length();
                            if ((remaining_len - written) <= 0) {
                                return (max_buf_length - remaining_len);
                            }
                            strncpy(buf, buf2, written);
                            buf += written; remaining_len -= written;

                            written = snprintf(buf, remaining_len, "%" PRIu64, sequence);
                            if ((remaining_len - written) <= 0) {
                                return (max_buf_length - remaining_len);
                            }
                            buf += written; remaining_len -= written;
                        } else {
                            written = it->execute_replace(buf, remaining_len, rib_list[i], attrs, peer, router,
                                                          collector, header);
                            if ((remaining_len - written) <= 0) {
                                return (max_buf_length - remaining_len);
                            }
                            buf += written; remaining_len -= written;
                        }
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
            ++sequence;
      }

        return (max_buf_length - remaining_len);
    }

    size_t Template_cfg::execute_container(char *buf, size_t max_buf_length,
                                           std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                           parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                           parse_bgp_lib::parseBgpLib::peer_map &peer,
                                           parse_bgp_lib::parseBgpLib::router_map &router,
                                           parse_bgp_lib::parseBgpLib::collector_map &collector,
                                           parse_bgp_lib::parseBgpLib::header_map &header) {

        char buf2[80000] = {0}; // Second working buffer

        size_t  remaining_len = max_buf_length, written = 0;
        bool noLoop = true;

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
                    noLoop = false;
                    written = it->execute_loop(buf, remaining_len, rib_list, attrs, peer, router, collector, header, seq);
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
                    if ((it->replacement_list_type == template_cfg::HEADER) and
                        (it->replacement_var == parse_bgp_lib::LIB_HEADER_SEQUENCE_NUMBER)) {
                        strncpy(buf2, it->prepend_string.c_str(), it->prepend_string.length());
                        written = it->prepend_string.length();
                        if ((remaining_len - written) <= 0) {
                            return (max_buf_length - remaining_len);
                        }
                        strncpy(buf, buf2, written);
                        buf += written; remaining_len -= written;

                        written = snprintf(buf, remaining_len, "%" PRIu64, seq);
                        if ((remaining_len - written) <= 0) {
                            return (max_buf_length - remaining_len);
                        }
                        buf += written; remaining_len -= written;
                    } else {
                        written = it->execute_replace(buf, remaining_len,
                                                      *(parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri *) NULL,
                                                      attrs, peer, router, collector, header);
                        if ((remaining_len - written) <= 0) {
                            return (max_buf_length - remaining_len);
                        }
                        buf += written; remaining_len -= written;
                    }
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
        if (noLoop)
            ++seq;

       return (max_buf_length - remaining_len);
    }

    size_t Template_cfg::create_container_loop(TEMPLATE_TYPES type, TEMPLATE_TOPICS topic, char *buf, std::string &in_prepend_string) {
        this->prepend_string.append(in_prepend_string);
        this->type = type;
        this->topic = topic;
        this->format = template_cfg::RAW;
        this->seq = 0;

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
//                cout << "Craft end object for container" << endl;

                template_cfg::Template_cfg template_cfg(logger, debug);
                template_cfg.type = template_cfg::END;
                strip_last_newline(prepend_string);
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
//                std::cout << "Found a comment, skipping" << std::endl;

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
//                    std::cout << "Found a if type inside container" << std::endl;
                } else if (strncmp(bpos, "end", strlen("end")) == 0) {
//                    std::cout << "Found a end type inside container or loop" << std::endl;

                    epos = strstr(bpos, "}}");
//                    std::cout << "container bpos: " << bpos << std::endl;
//                    std::cout << "container epos: " << epos << std::endl;
                    template_cfg::Template_cfg template_cfg(logger, debug);
                    template_cfg.type = template_cfg::END;
                    strip_last_newline(prepend_string);
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
//                std::cout << "Found replacement type inside type " << type << std::endl;
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
//        cout << "container/loop type" << type << " read is " << total_read << endl;

        return(total_read);
    }

    bool Template_cfg::create_container_loop_tsv(TEMPLATE_TYPES type, TEMPLATE_TOPICS topic, const YAML::Node &node) {
        this->type = type;
        this->topic = topic;
        this->format = template_cfg::TSV;
        bool headers = false;
        this->seq = 0;

        size_t read;
        std::string prepend_string_null = std::string("");
        std::string prepend_string_tab = std::string("\t");


        if (node.Type() == YAML::NodeType::Map) {
            template_cfg::Template_cfg template_cfg_loop(logger, debug);

            for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
                const YAML::Node &node2 = it->second;
                const std::string &key2 = it->first.Scalar();

                if (node2.Type() != YAML::NodeType::Sequence) {
                    std::cout << "template header/loop type is not of type sequence" << std::endl;
                    return (false);
                }
                std::string value;
                if (key2.compare("header") == 0) {
                    headers = true;
//                    std::cout << " Found a header type inside type" << type << std::endl;
                    for (std::size_t i = 0; i < node2.size(); i++) {
                        try {
                            value = node2[i].as<std::string>();
                            template_cfg::Template_cfg template_cfg(logger, debug);
                            template_cfg.format = template_cfg::TSV;
                            if (i == 0) {
                                read = template_cfg.create_replacement(((char *) value.c_str()),
                                                                       prepend_string_null);
                            } else {
                                read = template_cfg.create_replacement(((char *) value.c_str()),
                                                                       prepend_string_tab);
                            }

                            if (!read) {
                                std::cout << "Error creating replacement" << std::endl;
                                return (false);
                            }

                            this->template_children.push_back(template_cfg);
                        } catch (YAML::TypedBadConversion<std::string> err) {
                            printWarning("template type is not of type string", node2[i]);
                            return (false);
                        }
                    }
                } else if (key2.compare("loop") == 0) {
                    std::cout << " Found a loop type inside type" << type << std::endl;
                    template_cfg_loop.type = template_cfg::LOOP;
                    template_cfg_loop.topic = topic;
                    template_cfg_loop.format = template_cfg::TSV;
                    if (headers)
                        template_cfg_loop.prepend_string.assign(std::string("\n"));

                   for (std::size_t i = 0; i < node2.size(); i++) {
                       try {
                            value = node2[i].as<std::string>();
                            template_cfg::Template_cfg template_cfg_replace(logger, debug);
                            template_cfg_replace.format = template_cfg::TSV;

                            if (i == 0) {
                                read = template_cfg_replace.create_replacement(((char *) value.c_str()),
                                                                       prepend_string_null);
                            } else {
                                read = template_cfg_replace.create_replacement(((char *) value.c_str()),
                                                                       prepend_string_tab);
                            }

                            if (!read) {
                                std::cout << "Error creating replacement" << std::endl;
                                return(false);
                            }

                            template_cfg_loop.template_children.push_back(template_cfg_replace);
                        } catch (YAML::TypedBadConversion<std::string> err) {
                            printWarning("template type is not of type string", node2[i]);
                            return (false);
                        }
                    }
                    std::cout << "Craft end object for loop" << std::endl;

                    template_cfg::Template_cfg template_cfg_end(logger, debug);
                    template_cfg_end.type = template_cfg::END;
                    template_cfg_end.prepend_string.append(std::string("\n"));

                    template_cfg_loop.template_children.push_back(template_cfg_end);
                    this->template_children.push_back(template_cfg_loop);
                } else {
                    std::cout << "Unknown template container type" << std::endl;
                    return (false);
                }
            }
        } else {
            cout << "Container/schema should only have maps at the root/base level" << endl;
        }

        cout << "Craft end object for container" << endl;

        template_cfg::Template_cfg template_cfg(logger, debug);
        template_cfg.type = template_cfg::END;
//        template_cfg.prepend_string.append(std::string("\n"));

        this->template_children.push_back(template_cfg);

        return (true);
    }

    std::map<std::string, int> Template_cfg::lookup_map;

    size_t Template_cfg::create_replacement(char *buf, std::string &in_prepend_string) {
        this->prepend_string.append(in_prepend_string);
        this->type = REPLACE;

        char *bpos = buf;
        char *epos = buf;
        size_t read = 0;

        if (template_cfg::Template_cfg::lookup_map.empty()) {
            /*
             * Populate the lookup map
             */
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

            for (int i = 0; i < parse_bgp_lib::LIB_ROUTER_MAX; i++) {
                template_cfg::Template_cfg::lookup_map.insert(
                        std::pair<std::string, int>(parse_bgp_lib::parse_bgp_lib_router_names[i], i));
            }

            for (int i = 0; i < parse_bgp_lib::LIB_COLLECTOR_MAX; i++) {
                template_cfg::Template_cfg::lookup_map.insert(
                        std::pair<std::string, int>(parse_bgp_lib::parse_bgp_lib_collector_names[i], i));
            }

            for (int i = 0; i < parse_bgp_lib::LIB_HEADER_MAX; i++) {
                template_cfg::Template_cfg::lookup_map.insert(
                        std::pair<std::string, int>(parse_bgp_lib::parse_bgp_lib_header_names[i], i));
            }

        }

//        std::cout << "Prepend string for replace is now " << this->prepend_string << std::endl;
        epos = strstr(bpos, ".");
        if (!epos) {
            std::cout << "Error replacement variable is empty" << std::endl;
            return(0);
        }
//        std::cout << "Replacement variable list is " << epos << std::endl;

        if (strncmp(bpos, "nlri", strlen("nlri")) == 0) {
            this->replacement_list_type = NLRI;
       } else if (strncmp(bpos, "attr", strlen("attr")) == 0) {
            this->replacement_list_type = ATTR;
        } else if (strncmp(bpos, "peer", strlen("peer")) == 0) {
            this->replacement_list_type = PEER;
        } else if (strncmp(bpos, "router", strlen("router")) == 0) {
            this->replacement_list_type = ROUTER;
        } else if (strncmp(bpos, "collector", strlen("collector")) == 0) {
            this->replacement_list_type = COLLECTOR;
        } else if (strncmp(bpos, "header", strlen("header")) == 0) {
            this->replacement_list_type = HEADER;
        } else {
            cout << "Invalid replacement type " << epos << std::endl;
            return (0);
        }

        read += epos - bpos + 1;
        bpos += epos - bpos + 1; epos = bpos;

        epos = strstr(bpos, "}}");
       if (!epos) {
            if (this->format == template_cfg::RAW) {
                std::cout << "Error replacement variable is not closed" << std::endl;
                return (0);
            } else {
                epos = buf + strlen(buf);
            }
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

    /**
 * print warning message for parsing node
 *
 * \param [in] msg      Warning message
 * \param [in] node     Offending node that caused the warning
 */
    void Template_cfg::printWarning(const std::string msg, const YAML::Node &node) {
        std::string type;

        switch (node.Type()) {
            case YAML::NodeType::Null:
                type = "Null";
                break;
            case YAML::NodeType::Scalar:
                type = "Scalar";
                break;
            case YAML::NodeType::Sequence:
                type = "Sequence";
                break;
            default:
                type = "Unknown";
                break;
        }
        std::cout << "WARN: " << msg << " : " << type << " = " << node.Scalar() << std::endl;
    }

}
