/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2017, 2018 Christian Persch
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

#include "config.h"

#include "parser.hh"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>

#include <glib.h>

#include "parser-charset-tables.hh"

#ifdef PARSER_INCLUDE_NOP
#define _VTE_NOQ(...) _VTE_SEQ(__VA_ARGS__)
#else
#define _VTE_NOQ(...)
#endif

/*
 * Terminal Parser
 * This file contains a bunch of UTF-8 helpers and the main ctlseq-parser. The
 * parser is a simple state-machine that correctly parses all CSI, DCS, OSC, ST
 * control sequences and generic escape sequences.
 * The parser itself does not perform any actions but lets the caller react to
 * detected sequences.
 *
 * This parser is mostly DEC VT100+ compatible; known differences are:
 *
 * * DEC only recognises up to 16 parameters; vte up to 32 (and that can be easily
 *   extended)
 *
 * * DEC's parameter values range is 0..16384; vte supports 0..65535 (16-bit range).
 *
 * * When the number of parameter exceeds that number, DEC executes the function
 *   with these parameters, ignoring the excessive parameters; vte ignores the
 *   whole function instead.
 *
 * * DEC ignores CSI sequences with colon-separated parameters; vte implements colon-
 *   separated parameters as subparameters (this is an extension taken from ITU-T T.416).
 *
 * * DEC executes format effector controls in CSI, OSC, DCS sequences as if the
 *   control was received before the control sequence; vte only does this for CSI
 *   sequences and ignores all controls except ESC and BEL in OSC control strings,
 *   and passes all controls except ESC through to the control string in DCS sequences.
 *
 * * DEC only allows ST (either C0 or C1) to terminate OSC strings; vte allows
 *   OSC to be terminated by BEL (this is a deprecated xterm extension).
 *
 * * DEC parses ESC Z as DECID, a deprecated function equivalent to DA1; vte
 *   implements ECMA-48's SCI (single character introducer) instead.
 */

/*
 * Command Parser
 * The ctl-seq parser "vte_parser" only detects whole sequences, it does not
 * detect the specific command. Once a sequence is parsed, the command-parsers
 * are used to figure out their meaning.
 */

/*
 * Intermediates (and, for CSI/DCS, the optional parameter character) are
 * stored efficiently in an unsigned int. Intermediates can be 2/00..2/15,
 * plus one value for 'no intermediate'; together that fits into 5 bits.
 * Parameter character can be 'no parameter character', or one from
 * 3/12..3/15; that fits into 3 bits.
 *
 * In @seq.intermediates, the nth intermediates is stored with shift n * 5,
 * plus (for CSI/DCS) an additional shift of 3 for the parameter character
 * which is stored at bits 0..2.
 *
 * VTE_SEQ_PARAMETER(u) extracts the parameter character
 *   of a CSI or DCS sequence
 * VTE_SEQ_REMOVE_PARAMETER(u) extracts the intermediates
 *   of a CSI or DCS sequence
 * VTE_SEQ_INTERMEDIATE(u) extracts the first intermediate from an
 *   intermediates value (for CSI/DCS, that must be without parameter
 *   character, see VTE_SEQ_REMOVE_PARAMETER)
 * VTE_SEQ_REMOVE_INTERMEDIATE(u) extracts the remaining intermediates
 *   after the first one; use VTE_SEQ_INTERMEDIATE on its return value
 *   to extract the 2nd intermediate, and so on
 */

#define VTE_SEQ_PARAMETER_BITS         (3)
#define VTE_SEQ_INTERMEDIATE_BITS      (5)
#define VTE_SEQ_INTERMEDIATE_MASK      ((1U << VTE_SEQ_INTERMEDIATE_BITS) - 1U)
#define VTE_SEQ_PARAMETER_MASK         ((1U << VTE_SEQ_PARAMETER_BITS) - 1U)
#define VTE_SEQ_PARAMETER(u)           ((u) & VTE_SEQ_PARAMETER_MASK)
#define VTE_SEQ_REMOVE_PARAMETER(u)    ((u) >> VTE_SEQ_PARAMETER_BITS)
#define VTE_SEQ_INTERMEDIATE(u)        ((u) & VTE_SEQ_INTERMEDIATE_MASK)
#define VTE_SEQ_REMOVE_INTERMEDIATE(u) ((u) >> VTE_SEQ_INTERMEDIATE_BITS)
#define VTE_MAKE_CHARSET(c,s)          ((c) | ((s) << VTE_CHARSET_SLOT_OFFSET))

/*
 * _VTE_SEQ_CODE_ESC(final, intermediates):
 *
 * Make a value combining the final character and the intermediates,
 * to be used to match a sequence against known sequences.
 *
 * Since this is only used with NONE or HASH as first intermediate,
 * we can reduce the size of the lookup table by slashing the least
 * significant bit off.
 *
 * Final characters is 3/0..7/14, needing 7 bits.
 */
#define _VTE_SEQ_CODE_ESC(f,i) (((f) - 0x30) | ((i) >> 1) << 7)

/*
 * _VTE_SEQ_CODE_COMBINE(parameter, intermediates)
 *
 * Combines intermediates and the parameter character into one
 * value to be used when matching a sequence against known sequences.
 */
#define _VTE_SEQ_CODE_COMBINE(p,i) ((p) | ((i) << VTE_SEQ_PARAMETER_BITS))

/*
 * _VTE_SEQ_CODE(final, intermediates):
 *
 * Make a value combining the final character and the intermediates,
 * to be used to match a sequence against known sequences. Used for
 * CSI and DCS sequences; use _VTE_SEQ_CODE_COMBINE to combine
 * parameter and intermediates into one to pass as 2nd argument here.
 *
 * Final character is 4/0..7/14, needing 6 bits.
 */
#define _VTE_SEQ_CODE(f,i) (((f) - 0x40) | ((i) << 6))

/*
 * @introducer: either a C1 control, or the final in the equivalent ESC F sequence
 * @terminator: either a C1 control, or the final in the equivalent ESC F sequence
 *
 * Checks whether the OSC/DCS @introducer and the ST @terminator
 * are from the same control set, i.e. both C0 or both C1.
 *
 * For OSC, this check allows C0 OSC with BEL-as-ST to pass, too.
 */
static inline bool
parser_check_matching_controls(uint32_t introducer,
                               uint32_t terminator)
{
        return ((introducer ^ terminator) & 0x80) == 0;
}

static unsigned int
vte_parse_host_control(vte_seq_t const* seq)
{
        switch (seq->terminator) {
#define _VTE_SEQ(cmd,type,f,pi,ni,i0,flags) case f: return VTE_CMD_##cmd;
#include "parser-c01.hh"
#undef _VTE_SEQ
        default: return VTE_CMD_NONE;
        }
}

/* ECMA-35 § 14.1 specifies that the final character 7/14 always identifies
 * an empty set. Note that that this does not apply for DRCS sets (§ 14.4),
 * since § 13.3.3 says that all the Ft (4/0..7/14) bytes are private-use.
 */
static inline constexpr unsigned int
charset_empty_or_none(uint32_t raw)
{
        return raw == 0x7e ? VTE_CHARSET_EMPTY : VTE_CHARSET_NONE;
}

static unsigned int
vte_parse_charset_94(uint32_t raw,
                     unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94)))
                        return charset_graphic_94[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;

        case VTE_SEQ_INTERMEDIATE_BANG:
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && (raw < 0x40 + G_N_ELEMENTS(charset_graphic_94_with_2_1)))
                        return charset_graphic_94_with_2_1[raw - 0x40];
                break;

        case VTE_SEQ_INTERMEDIATE_DQUOTE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_2)))
                        return charset_graphic_94_with_2_2[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_HASH:
        case VTE_SEQ_INTERMEDIATE_CASH:
                break;

        case VTE_SEQ_INTERMEDIATE_PERCENT:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_5)))
                        return charset_graphic_94_with_2_5[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_AND:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_6)))
                        return charset_graphic_94_with_2_6[raw - 0x30];
                break;
        }

        return charset_empty_or_none(raw);
}

static unsigned int
vte_parse_charset_94_n(uint32_t raw,
                       unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_n)))
                        return charset_graphic_94_n[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;

        case VTE_SEQ_INTERMEDIATE_BANG:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_n_with_2_1)))
                        return charset_graphic_94_n_with_2_1[raw - 0x30];
                break;
        }

        return charset_empty_or_none(raw);
}

static unsigned int
vte_parse_charset_96(uint32_t raw,
                     unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_96)))
                        return charset_graphic_96[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;
        }

        return charset_empty_or_none(raw);
}

static unsigned int
vte_parse_charset_96_n(uint32_t raw,
                       unsigned int intermediates)
{
        if (VTE_SEQ_INTERMEDIATE(intermediates) == VTE_SEQ_INTERMEDIATE_SPACE)
                return VTE_CHARSET_DRCS;

        return charset_empty_or_none(raw);
}

static unsigned int
vte_parse_charset_ocs(uint32_t raw,
                      unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:  /* OCS with standard return */
                if (remaining_intermediates == 0 &&
                    raw >= 0x30 && raw < (0x30 + G_N_ELEMENTS(charset_ocs)))
                        return charset_ocs[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE: /* OCS with standard return */
                if (remaining_intermediates == 0 &&
                    raw >= 0x30 && raw < (0x30 + G_N_ELEMENTS(charset_ocs_with_2_0)))
                        return charset_ocs_with_2_0[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_BANG ... VTE_SEQ_INTERMEDIATE_DOT: /* OCS with standard return */
                break;

        case VTE_SEQ_INTERMEDIATE_SLASH: /* OCS without standard return */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_ocs_with_2_15)))
                        return charset_ocs_with_2_15[raw - 0x40];
                break;
        }

        return VTE_CHARSET_NONE;
}

static unsigned int
vte_parse_charset_control(uint32_t raw,
                          unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_BANG: /* C0 controls */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_control_c0)))
                        return charset_control_c0[raw - 0x40];
                break;

        case VTE_SEQ_INTERMEDIATE_DQUOTE: /* C1 controls */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_control_c1)))
                        return charset_control_c1[raw - 0x40];
                break;
        }

        return charset_empty_or_none(raw);
}

static unsigned int
vte_parse_host_escape(vte_seq_t const* seq,
                      unsigned int *cs_out)
{
        unsigned int intermediates = seq->intermediates;
        unsigned int intermediate0 = VTE_SEQ_INTERMEDIATE(intermediates);

        /* Switch on the first intermediate */
        switch (intermediate0) {
        case VTE_SEQ_INTERMEDIATE_NONE:
        case VTE_SEQ_INTERMEDIATE_HASH: {  /* Single control functions */
                switch (_VTE_SEQ_CODE_ESC(seq->terminator, intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
                        case _VTE_SEQ_CODE_ESC(f, VTE_SEQ_INTERMEDIATE_##i): return VTE_CMD_##cmd;
#include "parser-esc.hh"
#undef _VTE_SEQ
                default: return VTE_CMD_NONE;
                }
                break;
        }

        case VTE_SEQ_INTERMEDIATE_SPACE:   /* Announce code structure */
                if (VTE_SEQ_REMOVE_INTERMEDIATE(intermediates) == 0)
                        return VTE_CMD_ACS;
                break;

        case VTE_SEQ_INTERMEDIATE_BANG:    /* C0-designate */
        case VTE_SEQ_INTERMEDIATE_DQUOTE:  /* C1-designate */
                *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_control(seq->terminator, intermediates),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_BANG);
                return VTE_CMD_CnD;

        case VTE_SEQ_INTERMEDIATE_CASH: {  /* Designate multi-byte character sets */
                unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);
                unsigned int intermediate1 = VTE_SEQ_INTERMEDIATE(remaining_intermediates);
                remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(remaining_intermediates);

                /* Check the 2nd intermediate */
                switch (intermediate1) {
                case VTE_SEQ_INTERMEDIATE_NONE:
                        /* For compatibility with an earlier version of ISO-2022,
                         * ESC 2/4 4/0, ESC 2/4 4/1 and ESC 2/4 4/2 designate G0
                         * sets (i.e., without the 2/8 as 2nd intermediate byte).
                         */
                        switch (seq->terminator) {
                        case '@':
                        case 'A':
                        case 'B': /* G0-designate multibyte charset */
                                *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_94_n(seq->terminator,
                                                                                  remaining_intermediates),
                                                           0);
                                return VTE_CMD_GnDMm;
                        }
                        break;

                case VTE_SEQ_INTERMEDIATE_POPEN:  /* G0-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_PCLOSE: /* G1-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_MULT:   /* G2-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_PLUS:   /* G3-designate 94^n-set */
                        *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_94_n(seq->terminator,
                                                                          remaining_intermediates),
                                                   intermediate1 - VTE_SEQ_INTERMEDIATE_POPEN);
                        return VTE_CMD_GnDMm;

                case VTE_SEQ_INTERMEDIATE_COMMA:  /* Reserved for future standardisation */
                        break;

                case VTE_SEQ_INTERMEDIATE_MINUS:  /* G1-designate 96^n-set */
                case VTE_SEQ_INTERMEDIATE_DOT:    /* G2-designate 96^n-set */
                case VTE_SEQ_INTERMEDIATE_SLASH:  /* G3-designate 96^n-set */
                        *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_96_n(seq->terminator,
                                                                          remaining_intermediates),
                                                   intermediate1 - VTE_SEQ_INTERMEDIATE_COMMA);
                        return VTE_CMD_GnDMm;
                }
                break;
        }

        case VTE_SEQ_INTERMEDIATE_PERCENT: /* Designate other coding system */
                *cs_out = vte_parse_charset_ocs(seq->terminator,
                                                VTE_SEQ_REMOVE_INTERMEDIATE(intermediates));
                return VTE_CMD_DOCS;

        case VTE_SEQ_INTERMEDIATE_AND:     /* Identify revised registration */
                if (VTE_SEQ_REMOVE_INTERMEDIATE(intermediates) == 0)
                        return VTE_CMD_IRR;
                break;

        case VTE_SEQ_INTERMEDIATE_SQUOTE:  /* Reserved for future standardisation */
                break;

        case VTE_SEQ_INTERMEDIATE_POPEN:   /* G0-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_PCLOSE:  /* G1-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_MULT:    /* G2-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_PLUS:    /* G3-designate 94-set */
                *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_94(seq->terminator,
                                                                VTE_SEQ_REMOVE_INTERMEDIATE(intermediates)),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_POPEN);
                return VTE_CMD_GnDm;

        case VTE_SEQ_INTERMEDIATE_COMMA:   /* Reserved for future standardisation */
                break;

        case VTE_SEQ_INTERMEDIATE_MINUS:   /* G1-designate 96-set */
        case VTE_SEQ_INTERMEDIATE_DOT:     /* G2-designate 96-set */
        case VTE_SEQ_INTERMEDIATE_SLASH:   /* G3-designate 96-set */
                *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_96(seq->terminator,
                                                                VTE_SEQ_REMOVE_INTERMEDIATE(intermediates)),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_COMMA);
                return VTE_CMD_GnDm;
        }

        return VTE_CMD_NONE;
}

static unsigned int
vte_parse_host_csi(vte_seq_t const* seq)
{
        switch (_VTE_SEQ_CODE(seq->terminator, seq->intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
                case _VTE_SEQ_CODE(f, _VTE_SEQ_CODE_COMBINE(VTE_SEQ_PARAMETER_##p, VTE_SEQ_INTERMEDIATE_##i)): return VTE_CMD_##cmd;
#include "parser-csi.hh"
#undef _VTE_SEQ
        default: return VTE_CMD_NONE;
        }
}

static unsigned int
vte_parse_host_dcs(vte_seq_t const* seq,
                   unsigned int* flagsptr)
{
        switch (_VTE_SEQ_CODE(seq->terminator, seq->intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
                case _VTE_SEQ_CODE(f, _VTE_SEQ_CODE_COMBINE(VTE_SEQ_PARAMETER_##p, VTE_SEQ_INTERMEDIATE_##i)): *flagsptr = flags; return VTE_CMD_##cmd;
#include "parser-dcs.hh"
#undef _VTE_SEQ
        default: return VTE_CMD_NONE;
        }
}

static unsigned int
vte_parse_host_sci(vte_seq_t const* seq)
{
        switch (_VTE_SEQ_CODE(seq->terminator, 0)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
                case _VTE_SEQ_CODE(f, 0): return VTE_CMD_##cmd;
#include "parser-sci.hh"
#undef _VTE_SEQ
        default: return VTE_CMD_NONE;
        }
}

/*
 * State Machine
 * This parser controls the parser-state and returns any detected sequence to
 * the caller. The parser is based on this state-diagram from Paul Williams:
 *   https://vt100.net/emu/
 * It was written from scratch and extended where needed.
 * This parser is fully compatible up to the vt500 series. We expect UCS-4 as
 * input. It's the callers responsibility to do any UTF-8 parsing.
 */

enum parser_state_t {
        STATE_GROUND,           /* initial state and ground */
        STATE_DCS_PASS_ESC,     /* ESC after DCS which may be ESC \ aka C0 ST */
        STATE_OSC_STRING_ESC,   /* ESC after OSC which may be ESC \ aka C0 ST */
        STATE_ESC,              /* ESC sequence was started */
        STATE_ESC_INT,          /* intermediate escape characters */
        STATE_CSI_ENTRY,        /* starting CSI sequence */
        STATE_CSI_PARAM,        /* CSI parameters */
        STATE_CSI_INT,          /* intermediate CSI characters */
        STATE_CSI_IGNORE,       /* CSI error; ignore this CSI sequence */
        STATE_DCS_ENTRY,        /* starting DCS sequence */
        STATE_DCS_PARAM,        /* DCS parameters */
        STATE_DCS_INT,          /* intermediate DCS characters */
        STATE_DCS_PASS,         /* DCS data passthrough */
        STATE_DCS_IGNORE,       /* DCS error; ignore this DCS sequence */
        STATE_OSC_STRING,       /* parsing OSC sequence */
        STATE_ST_IGNORE,        /* unimplemented seq; ignore until ST */
        STATE_SCI,              /* single character introducer sequence was started */

        STATE_N,
};

/* Parser state transitioning */

typedef int (* parser_action_func)(vte_parser_t* parser, uint32_t raw);

// FIXMEchpe: I get weird performance results here from
// either not inlining, inlining these function or the
// macros below. Sometimes (after a recompile) one is
// (as much as 50%!) slower, sometimes the other one etc. ‽

#if 1 // (inline) functions

// #define PTINLINE inline
#define PTINLINE

/* nop */
static PTINLINE int
parser_nop(vte_parser_t* parser,
           uint32_t raw)
{
        return VTE_SEQ_NONE;
}
/* dispatch related actions */
static PTINLINE int
parser_action(vte_parser_t* parser,
              uint32_t raw,
              parser_action_func action)
{
        return action(parser, raw);
}

/* perform state transition */
static PTINLINE int
parser_transition_no_action(vte_parser_t* parser,
                            uint32_t raw,
                            unsigned int state)
{
        parser->state = state;
        return VTE_SEQ_NONE;
}

/* perform state transition and dispatch related actions */
static PTINLINE int
parser_transition(vte_parser_t* parser,
                  uint32_t raw,
                  unsigned int state,
                  parser_action_func action)
{
        parser->state = state;

        return action(parser, raw);
}

#undef PTINLINE

#else // macros

/* nop */
#define parser_nop(parser,raw) \
        ({ VTE_SEQ_NONE; })

/* dispatch related actions */
#define parser_action(p,r,a) \
        ({ \
                a((p), (r)); \
        })

/* perform state transition */
#define parser_transition_no_action(p,r,s) \
        ({ \
                parser->state = s; \
                VTE_SEQ_NONE; \
        })

/* perform state transition and dispatch related actions */
#define parser_transition(p,r,s,a) \
        ({ \
                (p)->state = s; \
                a((p), (r)); \
        })

#endif // (inline) functions or macros

/*
 * vte_parser_init:
 * @parser:
 */
void
vte_parser_init(vte_parser_t* parser)
{
        memset(parser, 0, sizeof(*parser));
        vte_seq_string_init(&parser->seq.arg_str);
}

/**
 * vte_parser_deinit:
 * @parser:
 */
void
vte_parser_deinit(vte_parser_t* parser)
{
        vte_seq_string_free(&parser->seq.arg_str);
}

static inline int
parser_clear(vte_parser_t* parser,
             uint32_t raw)
{
        /* seq.command is set when the sequence is executed,
         * seq.terminator is set when the final character is received,
         * and seq.introducer is set when the introducer is received,
         * and all this happens before the sequence is dispatched.
         * Therefore these fiedls need not be cleared in any case.
         */
        return VTE_SEQ_NONE;
}

static inline int
parser_clear_int(vte_parser_t* parser,
                 uint32_t raw)
{
        parser->seq.intermediates = 0;
        parser->seq.n_intermediates = 0;

        return parser_clear(parser, raw);
}

static inline int
parser_clear_params(vte_parser_t* parser,
                    uint32_t raw)
{
        /* The (n_args+1)th parameter may have been started but not
         * finialised, so it needs cleaning too. All further params
         * have not been touched, so need not be cleaned.
         */
        unsigned int n_args = G_UNLIKELY(parser->seq.n_args >= VTE_PARSER_ARG_MAX)
                ? VTE_PARSER_ARG_MAX
                : parser->seq.n_args + 1;
        memset(parser->seq.args, 0, n_args * sizeof(parser->seq.args[0]));
#ifdef PARSER_EXTRA_CLEAN
        /* Assert that the assumed-clean params are actually clean. */
        for (unsigned int n = n_args; n < VTE_PARSER_ARG_MAX; ++n)
                g_assert_cmpuint(parser->seq.args[n], ==, VTE_SEQ_ARG_INIT_DEFAULT);
#endif

        parser->seq.n_args = 0;
        parser->seq.n_final_args = 0;

        return VTE_SEQ_NONE;
}

static inline int
parser_clear_int_and_params(vte_parser_t* parser,
                            uint32_t raw)
{
        parser_clear_int(parser, raw);
        return parser_clear_params(parser, raw);
}

static int
parser_ignore(vte_parser_t* parser,
              uint32_t raw)
{
        parser->seq.type = VTE_SEQ_IGNORE;
        parser->seq.command = VTE_CMD_NONE;
        parser->seq.terminator = raw;

        return parser->seq.type;
}

static int
parser_print(vte_parser_t* parser,
             uint32_t raw)
{
        parser->seq.type = VTE_SEQ_GRAPHIC;
        parser->seq.command = VTE_CMD_GRAPHIC;
        parser->seq.terminator = raw;

        return parser->seq.type;
}

static int
parser_execute(vte_parser_t* parser,
               uint32_t raw)
{
        parser->seq.type = VTE_SEQ_CONTROL;
        parser->seq.terminator = raw;
        parser->seq.command = vte_parse_host_control(&parser->seq);

        return parser->seq.type;
}

static int
parser_collect_esc(vte_parser_t* parser,
                   uint32_t raw)
{
        assert(raw >= 0x20 && raw <= 0x2f);

        /* ESCAPE sequences only have intermediates or 2/0..2/15, so there's no
         * need for the extra shift as below for CSI/DCS sequences
         */
        parser->seq.intermediates |= (VTE_SEQ_MAKE_INTERMEDIATE(raw) << (VTE_SEQ_INTERMEDIATE_BITS * parser->seq.n_intermediates++));

        return VTE_SEQ_NONE;
}

static int
parser_collect_csi(vte_parser_t* parser,
                   uint32_t raw)
{
        assert(raw >= 0x20 && raw <= 0x2f);

        /* In addition to 2/0..2/15 intermediates, CSI/DCS sequence
         * can also have one parameter byte 3/12..3/15 at the
         * start of the parameters (see parser_collect_parameter below);
         * that's what the extra shift is for.
         */
        parser->seq.intermediates |= (VTE_SEQ_MAKE_INTERMEDIATE(raw) << (VTE_SEQ_PARAMETER_BITS +
                                                                         VTE_SEQ_INTERMEDIATE_BITS * parser->seq.n_intermediates++));

        return VTE_SEQ_NONE;
}

static int
parser_collect_parameter(vte_parser_t* parser,
                         uint32_t raw)
{
        assert(raw >= 0x3c && raw <= 0x3f);

        /* CSI/DCS may optionally have one parameter byte from 3/12..3/15
         * at the start of the parameters; we put that into the lowest
         * part of @seq.intermediates.
         * Note that there can only be *one* such byte; the state machine
         * already enforces that, so we do not need any additional checks
         * here.
         */
        parser->seq.intermediates |= VTE_SEQ_MAKE_PARAMETER(raw);

        return VTE_SEQ_NONE;
}

static void
parser_params_overflow(vte_parser_t* parser,
                       uint32_t raw)
{
        /* An overflow of the parameter number can only happen in
         * STATE_{CSI,DCS}_PARAM, and it occurs when
         * seq.n_arg == VTE_PARSER_ARG_MAX, and either an 0…9
         * is encountered, starting the next param, or an
         * explicit ':' or ';' terminating a (defaulted) (sub)param,
         * or when the intermediates/final character(s) occur
         * after a defaulted (sub)param.
         *
         * Transition to STATE_{CSI,DCS}_IGNORE to ignore the
         * whole sequence.
         */
        parser_transition_no_action(parser,
                                    raw,
                                    parser->state == STATE_CSI_PARAM ?
                                    STATE_CSI_IGNORE : STATE_DCS_IGNORE);
}

/* The next two functions are only called when encountering a ';' or ':',
 * so if there's already MAX-1 parameters, the ';' or ':' would finish
 * the MAXth parameter and there would be a default or non-default
 * MAX+1th parameter following it.
 */
static int
parser_finish_param(vte_parser_t* parser,
                    uint32_t raw)
{
        if (G_LIKELY(parser->seq.n_args < VTE_PARSER_ARG_MAX - 1)) {
                vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], false);
                ++parser->seq.n_args;
                ++parser->seq.n_final_args;
        } else
                parser_params_overflow(parser, raw);

        return VTE_SEQ_NONE;
}

static int
parser_finish_subparam(vte_parser_t* parser,
                       uint32_t raw)
{
        if (G_LIKELY(parser->seq.n_args < VTE_PARSER_ARG_MAX - 1)) {
                vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], true);
                ++parser->seq.n_args;
        } else
                parser_params_overflow(parser, raw);

        return VTE_SEQ_NONE;
}

static int
parser_param(vte_parser_t* parser,
             uint32_t raw)
{
        /* assert(raw >= '0' && raw <= '9'); */

        if (G_LIKELY(parser->seq.n_args < VTE_PARSER_ARG_MAX))
                vte_seq_arg_push(&parser->seq.args[parser->seq.n_args], raw);
        else
                parser_params_overflow(parser, raw);

        return VTE_SEQ_NONE;
}

static inline int
parser_osc_start(vte_parser_t* parser,
                 uint32_t raw)
{
        parser_clear(parser, raw);

        vte_seq_string_reset(&parser->seq.arg_str);

        parser->seq.introducer = raw;
        return VTE_SEQ_NONE;
}

static int
parser_osc_collect(vte_parser_t* parser,
                   uint32_t raw)
{
        /*
         * Only characters from 0x20..0x7e and >= 0xa0 are allowed here.
         * Our state-machine already verifies those restrictions.
         */

        if (G_UNLIKELY(!vte_seq_string_push(&parser->seq.arg_str, raw)))
                parser->state = STATE_ST_IGNORE;

        return VTE_SEQ_NONE;
}

static int
parser_dcs_start(vte_parser_t* parser,
                 uint32_t raw)
{
        parser_clear_int_and_params(parser, raw);

        vte_seq_string_reset(&parser->seq.arg_str);

        parser->seq.introducer = raw;
        return VTE_SEQ_NONE;
}

static int
parser_dcs_consume(vte_parser_t* parser,
                   uint32_t raw)
{
        /* parser->seq is cleared during DCS-START state, thus there's no need
         * to clear invalid fields here. */

        if (G_LIKELY(parser->seq.n_args < VTE_PARSER_ARG_MAX)) {
                if (parser->seq.n_args > 0 ||
                    vte_seq_arg_started(parser->seq.args[parser->seq.n_args])) {
                        vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], false);
                        ++parser->seq.n_args;
                        ++parser->seq.n_final_args;
                }
        }

        parser->seq.type = VTE_SEQ_DCS;
        parser->seq.terminator = raw;
        parser->seq.st = 0;

        auto flags = unsigned{};
        parser->seq.command = vte_parse_host_dcs(&parser->seq, &flags);

        return (flags & VTE_DISPATCH_UNRIPE) && parser->dispatch_unripe ? VTE_SEQ_DCS : VTE_SEQ_NONE;
}

static int
parser_dcs_collect(vte_parser_t* parser,
                   uint32_t raw)
{
        if (G_UNLIKELY(!vte_seq_string_push(&parser->seq.arg_str, raw)))
                parser->state = STATE_DCS_IGNORE;

        return VTE_SEQ_NONE;
}

static int
parser_esc(vte_parser_t* parser,
           uint32_t raw)
{
        parser->seq.type = VTE_SEQ_ESCAPE;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_escape(&parser->seq,
                                                    &parser->seq.charset);

        return parser->seq.type;
}

static int
parser_csi(vte_parser_t* parser,
           uint32_t raw)
{
        /* parser->seq is cleared during CSI-ENTER state, thus there's no need
         * to clear invalid fields here. */

        if (G_LIKELY(parser->seq.n_args < VTE_PARSER_ARG_MAX)) {
                if (parser->seq.n_args > 0 ||
                    vte_seq_arg_started(parser->seq.args[parser->seq.n_args])) {
                        vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], false);
                        ++parser->seq.n_args;
                        ++parser->seq.n_final_args;
                }
        }

        parser->seq.type = VTE_SEQ_CSI;
        parser->seq.terminator = raw;
        parser->seq.command = vte_parse_host_csi(&parser->seq);

        return parser->seq.type;
}

static int
parser_osc(vte_parser_t* parser,
           uint32_t raw)
{
        /* parser->seq is cleared during OSC_START state, thus there's no need
         * to clear invalid fields here. */

        vte_seq_string_finish(&parser->seq.arg_str);

        /* We only dispatch a DCS if the introducer and string
         * terminator are from the same control set, i.e. both
         * C0 or both C1; we discard sequences with mixed controls.
         */
        if (!parser_check_matching_controls(parser->seq.introducer, raw))
                return VTE_SEQ_IGNORE;

        parser->seq.type = VTE_SEQ_OSC;
        parser->seq.command = VTE_CMD_OSC;
        parser->seq.st = raw;

        return parser->seq.type;
}

static int
parser_dcs(vte_parser_t* parser,
           uint32_t raw)
{
        /* Most of parser->seq was already filled in parser_dcs_consume() */
        parser->seq.st = raw;

        vte_seq_string_finish(&parser->seq.arg_str);

        /* We only dispatch a DCS if the introducer and string
         * terminator are from the same control set, i.e. both
         * C0 or both C1; we discard sequences with mixed controls.
         */
        if (!parser_check_matching_controls(parser->seq.introducer, raw))
                return VTE_SEQ_IGNORE;

        return parser->seq.type;
}

static int
parser_sci(vte_parser_t* parser,
           uint32_t raw)
{
        parser->seq.type = VTE_SEQ_SCI;
        parser->seq.terminator = raw;
        parser->seq.command = vte_parse_host_sci(&parser->seq);

        return parser->seq.type;
}

#define ACTION_CLEAR parser_clear
#define ACTION_CLEAR_INT parser_clear_int
#define ACTION_CLEAR_INT_AND_PARAMS parser_clear_int_and_params
#define ACTION_CLEAR_PARAMS_ONLY parser_clear_params
#define ACTION_IGNORE parser_ignore
#define ACTION_PRINT parser_print
#define ACTION_EXECUTE parser_execute
#define ACTION_COLLECT_ESC parser_collect_esc
#define ACTION_COLLECT_CSI parser_collect_csi
#define ACTION_COLLECT_DCS ACTION_COLLECT_CSI
#define ACTION_COLLECT_PARAMETER parser_collect_parameter
#define ACTION_PARAM parser_param
#define ACTION_FINISH_PARAM parser_finish_param
#define ACTION_FINISH_SUBPARAM parser_finish_subparam
#define ACTION_ESC_DISPATCH parser_esc
#define ACTION_CSI_DISPATCH parser_csi
#define ACTION_DCS_START parser_dcs_start
#define ACTION_DCS_CONSUME parser_dcs_consume
#define ACTION_DCS_COLLECT parser_dcs_collect
#define ACTION_DCS_DISPATCH parser_dcs
#define ACTION_OSC_START parser_osc_start
#define ACTION_OSC_COLLECT parser_osc_collect
#define ACTION_OSC_DISPATCH parser_osc
#define ACTION_SCI_DISPATCH parser_sci

static int
parser_feed_to_state(vte_parser_t* parser,
                     uint32_t raw)
{
        switch (parser->state) {
        case STATE_GROUND:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                case 0x80 ... 0x9f:        /* C1 */
                        return parser_action(parser, raw,
                                             ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                }

                return parser_action(parser, raw,
                                     ACTION_PRINT);

        case STATE_DCS_PASS_ESC:
        case STATE_OSC_STRING_ESC:
                if (raw == 0x5c /* '\' */) {
                        switch (parser->state) {
                        case STATE_DCS_PASS_ESC:
                                return parser_transition(parser, raw, STATE_GROUND,
                                                         ACTION_DCS_DISPATCH);
                        case STATE_OSC_STRING_ESC:
                                return parser_transition(parser, raw, STATE_GROUND,
                                                         ACTION_OSC_DISPATCH);
                        }
                }

                /* Do the deferred clear and fallthrough to STATE_ESC */
                parser_transition(parser, 0x1b /* ESC */, STATE_ESC,
                                  ACTION_CLEAR_INT);

                [[fallthrough]];
        case STATE_ESC:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                                 ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_ESC_INT,
                                                 ACTION_COLLECT_ESC);
                case 0x30 ... 0x4f:        /* ['0' - '~'] \ */
                case 0x51 ... 0x57:        /* { 'P', 'X', 'Z' '[', ']', '^', '_' } */
                case 0x59:
                case 0x5c:
                case 0x60 ... 0x7e:
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_ESC_DISPATCH);
                case 0x50:                /* 'P' */
                        return parser_transition(parser, raw, STATE_DCS_ENTRY,
                                                 ACTION_DCS_START);
                case 0x5a:                /* 'Z' */
                        return parser_transition(parser, raw, STATE_SCI,
                                                 ACTION_CLEAR);
                case 0x5b:                /* '[' */
                        return parser_transition(parser, raw, STATE_CSI_ENTRY,
                                                 ACTION_CLEAR_PARAMS_ONLY
                                                 /* rest already cleaned on ESC state entry */);
                case 0x5d:                /* ']' */
                        return parser_transition(parser, raw, STATE_OSC_STRING,
                                                 ACTION_OSC_START);
                case 0x58:                /* 'X' */
                case 0x5e:                /* '^' */
                case 0x5f:                /* '_' */
                        return parser_transition_no_action(parser, raw, STATE_ST_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw, STATE_GROUND,
                                         ACTION_IGNORE);
        case STATE_ESC_INT:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                             ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_action(parser, raw,
                                             ACTION_COLLECT_ESC);
                case 0x30 ... 0x7e:        /* ['0' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_ESC_DISPATCH);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw, STATE_GROUND,
                                         ACTION_IGNORE);
        case STATE_CSI_ENTRY:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                             ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_CSI_INT,
                                                 ACTION_COLLECT_CSI);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_PARAM);
                case 0x3a:                 /* ':' */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_FINISH_SUBPARAM);
                case 0x3b:                 /* ';' */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_FINISH_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_COLLECT_PARAMETER);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition_no_action(parser, raw, STATE_CSI_IGNORE);
        case STATE_CSI_PARAM:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                             ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_CSI_INT,
                                                 ACTION_COLLECT_CSI);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                        return parser_action(parser, raw,
                                             ACTION_PARAM);
                case 0x3a:                 /* ':' */
                        return parser_action(parser, raw,
                                             ACTION_FINISH_SUBPARAM);
                case 0x3b:                 /* ';' */
                        return parser_action(parser, raw,
                                             ACTION_FINISH_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition_no_action(parser, raw, STATE_CSI_IGNORE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition_no_action(parser, raw, STATE_CSI_IGNORE);
        case STATE_CSI_INT:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                             ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_action(parser, raw,
                                             ACTION_COLLECT_CSI);
                case 0x30 ... 0x3f:        /* ['0' - '?'] */
                        return parser_transition_no_action(parser, raw, STATE_CSI_IGNORE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition_no_action(parser, raw, STATE_CSI_IGNORE);
        case STATE_CSI_IGNORE:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                             ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x3f:        /* [' ' - '?'] */
                        return parser_nop(parser, raw);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition_no_action(parser, raw, STATE_GROUND);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_nop(parser, raw);
        case STATE_DCS_ENTRY:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ ESC */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                             ACTION_IGNORE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_DCS_INT,
                                                 ACTION_COLLECT_DCS);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_PARAM);
                case 0x3a:                 /* ':' */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_FINISH_SUBPARAM);
                case 0x3b:                 /* ';' */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_FINISH_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_COLLECT_PARAMETER);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_PARAM:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                             ACTION_IGNORE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_DCS_INT,
                                                 ACTION_COLLECT_DCS);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                        return parser_action(parser, raw,
                                                 ACTION_PARAM);
                case 0x3a:                 /* ':' */
                        return parser_action(parser, raw,
                                             ACTION_FINISH_SUBPARAM);
                case 0x3b:                 /* ';' */
                        return parser_action(parser, raw,
                                             ACTION_FINISH_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition_no_action(parser, raw, STATE_DCS_IGNORE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_INT:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_action(parser, raw,
                                             ACTION_IGNORE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_action(parser, raw,
                                             ACTION_COLLECT_DCS);
                case 0x30 ... 0x3f:        /* ['0' - '?'] */
                        return parser_transition_no_action(parser, raw, STATE_DCS_IGNORE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_PASS:
                switch (raw) {
                case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                case 0x1c ... 0x7f:
                        return parser_action(parser, raw,
                                             ACTION_DCS_COLLECT);
                case 0x1b:                /* ESC */
                        return parser_transition_no_action(parser, raw, STATE_DCS_PASS_ESC);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_DCS_DISPATCH);
                }

                return parser_action(parser, raw,
                                     ACTION_DCS_COLLECT);
        case STATE_DCS_IGNORE:
                switch (raw) {
                case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                case 0x1c ... 0x7f:
                        return parser_nop(parser, raw);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x9c:                /* ST */
                        return parser_transition_no_action(parser, raw, STATE_GROUND);
                }

                return parser_nop(parser, raw);
        case STATE_OSC_STRING:
                switch (raw) {
                case 0x00 ... 0x06:        /* C0 \ { BEL, ESC } */
                case 0x08 ... 0x1a:
                case 0x1c ... 0x1f:
                        return parser_nop(parser, raw);
                case 0x1b:                /* ESC */
                        return parser_transition_no_action(parser, raw, STATE_OSC_STRING_ESC);
                case 0x20 ... 0x7f:        /* [' ' - DEL] */
                        return parser_action(parser, raw,
                                             ACTION_OSC_COLLECT);
                case 0x07:                /* BEL */
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_OSC_DISPATCH);
                }

                return parser_action(parser, raw,
                                     ACTION_OSC_COLLECT);
        case STATE_ST_IGNORE:
                switch (raw) {
                case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                case 0x1c ... 0x7f:
                        return parser_nop(parser, raw);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR_INT);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw,
                                                 STATE_GROUND, ACTION_IGNORE);
                }

                return parser_nop(parser, raw);
        case STATE_SCI:
                switch (raw) {
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw,
                                                 STATE_ESC, ACTION_CLEAR_INT);
                case 0x08 ... 0x0d:        /* BS, HT, LF, VT, FF, CR */
                case 0x20 ... 0x7e:        /* [' ' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_SCI_DISPATCH);
                }

                return parser_transition(parser, raw, STATE_GROUND,
                                         ACTION_IGNORE);
        }

        g_assert_not_reached();
        return VTE_SEQ_NONE;
}

int
vte_parser_feed(vte_parser_t* parser,
                uint32_t raw)
{
        /*
         * Notes:
         *  * DEC treats GR codes as GL. We don't do that as we require UTF-8
         *    as charset and, thus, it doesn't make sense to treat GR special.
         *  * During control sequences, unexpected C1 codes cancel the sequence
         *    and immediately start a new one. C0 codes, however, may or may not
         *    be ignored/executed depending on the sequence.
         */

        switch (raw) {
        case 0x18:                /* CAN */
                return parser_transition(parser, raw,
                                         STATE_GROUND, ACTION_IGNORE);
        case 0x1a:                /* SUB */
                return parser_transition(parser, raw,
                                         STATE_GROUND, ACTION_EXECUTE);
        case 0x7f:                 /* DEL */
                return parser_nop(parser, raw);
        case 0x80 ... 0x8f:        /* C1 \ {DCS, SOS, SCI, CSI, ST, OSC, PM, APC} */
        case 0x91 ... 0x97:
        case 0x99:
                return parser_transition(parser, raw,
                                         STATE_GROUND, ACTION_EXECUTE);
        case 0x98:                /* SOS */
        case 0x9e:                /* PM */
        case 0x9f:                /* APC */
                return parser_transition_no_action(parser, raw, STATE_ST_IGNORE);
                // FIXMEchpe shouldn't this use ACTION_CLEAR?
        case 0x90:                /* DCS */
                return parser_transition(parser, raw,
                                         STATE_DCS_ENTRY, ACTION_DCS_START);
        case 0x9a:                /* SCI */
                return parser_transition(parser, raw,
                                         STATE_SCI, ACTION_CLEAR);
        case 0x9d:                /* OSC */
                return parser_transition(parser, raw,
                                         STATE_OSC_STRING, ACTION_OSC_START);
        case 0x9b:                /* CSI */
                return parser_transition(parser, raw,
                                         STATE_CSI_ENTRY, ACTION_CLEAR_INT_AND_PARAMS);
        default:
                return parser_feed_to_state(parser, raw);
        }
}

void
vte_parser_reset(vte_parser_t* parser)
{
        parser_transition(parser, 0, STATE_GROUND, ACTION_IGNORE);
}

/*
 * vte_parser_set_dispatch_unripe:
 * @parser: a #vte_parser_t
 * @enable:
 *
 * Enables or disables dispatch of unripe DCS sequences.
 * If enabled, known DCS sequences with the %VTE_DISPATCH_UNRIPE
 * flag will be dispatched when the Final character is received,
 * instead of when the control string terminator (ST) is received.
 * The application handling the unripe DCS sequence may then
 * either
 * * do nothing; in this case the DCS sequence will be dispatched
 *   again when the control string was fully received. Ripe and
 *   unripe sequences can be distinguished by the value of
 *   parser.seq.st which will be 0 for an unripe sequence and
 *   either 0x5c (C0 ST) or 0x9c (C1 ST) for a ripe sequence. Or
 * * call vte_parser_ignore_until_st(); in this case the DCS
 *   sequence will be ignored until after the ST (or an other
 *   character that aborts the control string) has been
 *   received; or
 * * switch to a different parser (e.g. DECSIXEL) to parse the
 *   control string directly on-the-fly. Note that in this case,
 *   the subparser should take care to handle C0 and C1 controls
 *   the same way as this parser would.
 */
void
vte_parser_set_dispatch_unripe(vte_parser_t* parser,
                               bool enable)
{
        parser->dispatch_unripe = enable;
}

/*
 * vte_parser_ignore_until_st:
 * @parser: a #vte_parser_t
 *
 * When used on an unrip %VTE_SEQ_DCS sequence, makes the
 * parser ignore everything until the ST is received (or
 * the DCS is aborted by the usual other means).
 *
 * Note that there is some inconsistencies here:
 *
 * * SUB aborts the DCS in our parser, but e.g. a DECSIXEL
 *   parser will handle it as if 3/15 was received.
 *
 * * the ST terminating the DCS will be dispatched as an ST
 *   sequence, instead of producing an IGNORE sequence
 *   (this is easily fixable but would slightly complicate
 *   the parser for no actual gain).
 */
void
vte_parser_ignore_until_st(vte_parser_t* parser)
{
        switch (parser->state) {
        case STATE_DCS_PASS:
                parser_transition_no_action(parser, 0, STATE_DCS_IGNORE);
                break;
        default:
                g_assert_not_reached();
                break;
        }
}
