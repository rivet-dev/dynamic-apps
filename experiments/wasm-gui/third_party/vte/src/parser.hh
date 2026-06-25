/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2018 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <cstdio>

#include "parser-arg.hh"
#include "parser-string.hh"

struct vte_parser_t;
struct vte_seq_t;

/*
 * Parsers
 * The vte_parser object parses control-sequences for both host and terminal
 * side. Based on this parser, there is a set of command-parsers that take a
 * vte_seq sequence and returns the command it represents. This is different
 * for host and terminal side, and so far we only provide the terminal side, as
 * host side is not used by anyone.
 */

#define VTE_PARSER_ARG_MAX (32)

enum {
        VTE_SEQ_NONE,        /* placeholder, no sequence parsed */

        VTE_SEQ_IGNORE,      /* no-op character */
        VTE_SEQ_GRAPHIC,     /* graphic character */
        VTE_SEQ_CONTROL,     /* control character */
        VTE_SEQ_ESCAPE,      /* escape sequence */
        VTE_SEQ_CSI,         /* control sequence function */
        VTE_SEQ_DCS,         /* device control string */
        VTE_SEQ_OSC,         /* operating system control */
        VTE_SEQ_SCI,         /* single character control function */
        VTE_SEQ_APC,         /* application program command */
        VTE_SEQ_PM,          /* privacy message */
        VTE_SEQ_SOS,         /* start of string */

        VTE_SEQ_N,
};

enum {
        VTE_SEQ_INTERMEDIATE_CHAR_NONE    = 0,

        VTE_SEQ_INTERMEDIATE_CHAR_SPACE   = ' ',  /* 02/00 */
        VTE_SEQ_INTERMEDIATE_CHAR_BANG    = '!',  /* 02/01 */
        VTE_SEQ_INTERMEDIATE_CHAR_DQUOTE  = '"',  /* 02/02 */
        VTE_SEQ_INTERMEDIATE_CHAR_HASH    = '#',  /* 02/03 */
        VTE_SEQ_INTERMEDIATE_CHAR_CASH    = '$',  /* 02/04 */
        VTE_SEQ_INTERMEDIATE_CHAR_PERCENT = '%',  /* 02/05 */
        VTE_SEQ_INTERMEDIATE_CHAR_AND     = '&',  /* 02/06 */
        VTE_SEQ_INTERMEDIATE_CHAR_SQUOTE  = '\'', /* 02/07 */
        VTE_SEQ_INTERMEDIATE_CHAR_POPEN   = '(',  /* 02/08 */
        VTE_SEQ_INTERMEDIATE_CHAR_PCLOSE  = ')',  /* 02/09 */
        VTE_SEQ_INTERMEDIATE_CHAR_MULT    = '*',  /* 02/10 */
        VTE_SEQ_INTERMEDIATE_CHAR_PLUS    = '+',  /* 02/11 */
        VTE_SEQ_INTERMEDIATE_CHAR_COMMA   = ',',  /* 02/12 */
        VTE_SEQ_INTERMEDIATE_CHAR_MINUS   = '-',  /* 02/13 */
        VTE_SEQ_INTERMEDIATE_CHAR_DOT     = '.',  /* 02/14 */
        VTE_SEQ_INTERMEDIATE_CHAR_SLASH   = '/',  /* 02/15 */
};

enum {
        VTE_SEQ_PARAMETER_CHAR_NONE  = 0,

        /* Numbers; not used         *  03/00..03/09 */
        /* COLON is reserved         = ':'   * 03/10 */
        /* SEMICOLON is reserved     = ';'   * 03/11 */
        VTE_SEQ_PARAMETER_CHAR_LT    = '<', /* 03/12 */
        VTE_SEQ_PARAMETER_CHAR_EQUAL = '=', /* 03/13 */
        VTE_SEQ_PARAMETER_CHAR_GT    = '>', /* 03/14 */
        VTE_SEQ_PARAMETER_CHAR_WHAT  = '?'  /* 03/15 */
};

#define VTE_SEQ_MAKE_INTERMEDIATE(c) ((c) - ' ' + 1)

enum {
        VTE_SEQ_INTERMEDIATE_NONE      = 0,

        VTE_SEQ_INTERMEDIATE_SPACE     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_SPACE  ),
        VTE_SEQ_INTERMEDIATE_BANG      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_BANG   ),
        VTE_SEQ_INTERMEDIATE_DQUOTE    = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_DQUOTE ),
        VTE_SEQ_INTERMEDIATE_HASH      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_HASH   ),
        VTE_SEQ_INTERMEDIATE_CASH      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_CASH   ),
        VTE_SEQ_INTERMEDIATE_PERCENT   = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_PERCENT),
        VTE_SEQ_INTERMEDIATE_AND       = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_AND    ),
        VTE_SEQ_INTERMEDIATE_SQUOTE    = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_SQUOTE ),
        VTE_SEQ_INTERMEDIATE_POPEN     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_POPEN  ),
        VTE_SEQ_INTERMEDIATE_PCLOSE    = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_PCLOSE ),
        VTE_SEQ_INTERMEDIATE_MULT      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_MULT   ),
        VTE_SEQ_INTERMEDIATE_PLUS      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_PLUS   ),
        VTE_SEQ_INTERMEDIATE_COMMA     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_COMMA  ),
        VTE_SEQ_INTERMEDIATE_MINUS     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_MINUS  ),
        VTE_SEQ_INTERMEDIATE_DOT       = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_DOT    ),
        VTE_SEQ_INTERMEDIATE_SLASH     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_SLASH  ),
};

#define VTE_SEQ_MAKE_PARAMETER(c) ('?' - (c) + 1)

enum {
        VTE_SEQ_PARAMETER_NONE  = 0,

        VTE_SEQ_PARAMETER_LT    = VTE_SEQ_MAKE_PARAMETER(VTE_SEQ_PARAMETER_CHAR_LT   ),
        VTE_SEQ_PARAMETER_EQUAL = VTE_SEQ_MAKE_PARAMETER(VTE_SEQ_PARAMETER_CHAR_EQUAL),
        VTE_SEQ_PARAMETER_GT    = VTE_SEQ_MAKE_PARAMETER(VTE_SEQ_PARAMETER_CHAR_GT   ),
        VTE_SEQ_PARAMETER_WHAT  = VTE_SEQ_MAKE_PARAMETER(VTE_SEQ_PARAMETER_CHAR_WHAT ),
};

enum {
#define _VTE_CMD(cmd) VTE_CMD_##cmd,
#define _VTE_NOP(cmd) VTE_CMD_##cmd,
#include "parser-cmd.hh"
#undef _VTE_CMD
#undef _VTE_NOP

        VTE_CMD_N,
        VTE_CMD_NOP_FIRST = VTE_CMD_ACK
};

enum {
#define _VTE_REPLY(cmd,type,final,pintro,intermediate,code) VTE_REPLY_##cmd,
#include "parser-reply.hh"
#undef _VTE_REPLY

        VTE_REPLY_N
};

enum {
#define _VTE_CHARSET_PASTE(name) VTE_CHARSET_##name,
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2) VTE_CHARSET_##name1 = VTE_CHARSET_##name2,
#define _VTE_CHARSET_ALIAS(name1,name2) _VTE_CHARSET_ALIAS_PASTE(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
};

enum {
#define _VTE_OSC(osc,value) VTE_OSC_##osc = value,
#include "parser-osc.hh"
#undef _VTE_OSC

        VTE_OSC_N
};

enum {
#define _VTE_SGR(name, value) VTE_SGR_##name = value,
#define _VTE_NGR(...)
#include "parser-sgr.hh"
#undef _VTE_SGR
#undef _VTE_NGR
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-declarations"
enum {
#define _VTE_SGR(name, value) VTE_DECSGR_##name = value,
#define _VTE_NGR(...)
#include "parser-decsgr.hh"
#undef _VTE_SGR
#undef _VTE_NGR
};
#pragma GCC diagnostic pop

#define VTE_CHARSET_CHARSET_MASK   ((1U << 16) - 1U)
#define VTE_CHARSET_SLOT_OFFSET    (16)
#define VTE_CHARSET_GET_CHARSET(c) ((c) & VTE_CHARSET_CHARSET_MASK)
#define VTE_CHARSET_GET_SLOT(c)    ((c) >> VTE_CHARSET_SLOT_OFFSET)

enum {
      VTE_DISPATCH_UNRIPE = 1u << 0,
};

struct vte_seq_t {
        unsigned int type;
        unsigned int command;
        uint32_t terminator;
        unsigned int intermediates;
        unsigned int n_intermediates;
        unsigned int charset;
        unsigned int n_args;
        unsigned int n_final_args;
        vte_seq_arg_t args[VTE_PARSER_ARG_MAX];
        vte_seq_string_t arg_str;
        uint32_t introducer;
        uint32_t st;
};

struct vte_parser_t {
        vte_seq_t seq;
        unsigned int state;
        bool dispatch_unripe;
};

void vte_parser_init(vte_parser_t* parser);
void vte_parser_deinit(vte_parser_t* parser);
int vte_parser_feed(vte_parser_t* parser,
                    uint32_t raw);
void vte_parser_reset(vte_parser_t* parser);
void vte_parser_set_dispatch_unripe(vte_parser_t* parser,
                                    bool enable);
void vte_parser_ignore_until_st(vte_parser_t* parser);
