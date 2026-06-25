/*
 * Copyright (C) 2002,2009,2010 Red Hat, Inc.
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
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

/* The interfaces in this file are subject to change at any time. */

#pragma once

#include <gio/gio.h>
#include <vte/vte.h>

#include "vterowdata.hh"
#include "vtestream.h"

#include <type_traits>

typedef struct _VteVisualPosition {
	long row, col;
} VteVisualPosition;

namespace vte {

namespace base {

/*
 * Ring:
 *
 * A scrollback buffer ring.
 */
class Ring {
public:
        typedef guint32 hyperlink_idx_t;
        // FIXME make this size_t (or off_t?)
        typedef gulong row_t;
        typedef glong column_t;

        static const row_t kDefaultMaxRows = VTE_SCROLLBACK_INIT;

        Ring(row_t max_rows = kDefaultMaxRows,
             bool has_streams = false);
        ~Ring();

        // prevent accidents
        Ring(Ring& o) = delete;
        Ring(Ring const& o) = delete;
        Ring(Ring&& o) = delete;
        Ring& operator= (Ring& o) = delete;
        Ring& operator= (Ring const& o) = delete;
        Ring& operator= (Ring&& o) = delete;

        inline bool contains(row_t position) const {
                return (position >= m_start && position < m_end);
        }

        inline row_t delta() const { return m_start; }
        inline row_t length() const { return m_end - m_start; }
        inline row_t next() const { return m_end; }

        //FIXMEchpe rename this to at()
        //FIXMEchpe use references not pointers
        VteRowData const* index(row_t position); /* const? */
        VteRowData* index_writable(row_t position);
        bool is_soft_wrapped(row_t position);

        void hyperlink_maybe_gc(row_t increment);
        hyperlink_idx_t get_hyperlink_idx(char const* hyperlink);
        hyperlink_idx_t get_hyperlink_at_position(row_t position,
                                                  column_t col,
                                                  bool update_hover_idx,
                                                  char const** hyperlink);

        row_t reset();
        void resize(row_t max_rows = kDefaultMaxRows);
        void shrink(row_t max_len = kDefaultMaxRows);
        VteRowData* insert(row_t position, guint8 bidi_flags);
        VteRowData* append(guint8 bidi_flags);
        void remove(row_t position);
        void drop_scrollback(row_t position);
        void set_visible_rows(row_t rows);
        void rewrap(column_t columns,
                    VteVisualPosition** markers);
        bool write_contents(GOutputStream* stream,
                            VteWriteFlags flags,
                            GCancellable* cancellable,
                            GError** error);

private:

        #ifdef VTE_DEBUG
        void validate() const;
        #endif

        inline GString* hyperlink_get(hyperlink_idx_t idx) const { return (GString*)g_ptr_array_index(m_hyperlinks, idx); }

        inline VteRowData* get_writable_index(row_t position) const { return &m_array[position & m_mask]; }

        void hyperlink_gc();
        hyperlink_idx_t get_hyperlink_idx_no_update_current(char const* hyperlink);

        typedef struct _CellAttrChange {
                gsize text_end_offset;  /* offset of first character no longer using this attr */
                VteStreamCellAttr attr;
        } CellAttrChange;

        typedef struct _RowRecord {
                size_t text_start_offset;  /* offset where text of this row begins */
                size_t attr_start_offset;  /* offset of the first character's attributes */
                int soft_wrapped: 1;      /* end of line is not '\n' */
                int is_ascii: 1;          /* for rewrapping speedup: guarantees that line contains 32..126 bytes only. Can be 0 even when ascii only. */
                guint8 bidi_flags: 4;
        } RowRecord;

        static_assert(std::is_standard_layout_v<RowRecord> && std::is_trivial_v<RowRecord>, "Ring::RowRecord is not POD");

        /* Represents a cell position, see ../doc/rewrap.txt */
        typedef struct _CellTextOffset {
                size_t text_offset;    /* byte offset in text_stream (or perhaps beyond) */
                int fragment_cells;  /* extra number of cells to walk within a multicell character */
                int eol_cells;       /* -1 if over a character, >=0 if at EOL or beyond */
        } CellTextOffset;

        static_assert(std::is_standard_layout_v<CellTextOffset> && std::is_trivial_v<CellTextOffset>, "Ring::CellTextOffset is not POD");

        inline bool read_row_record(RowRecord* record /* out */,
                                    row_t position)
        {
                return _vte_stream_read(m_row_stream,
                                        position * sizeof(*record),
                                        (char*)record,
                                        sizeof(*record));
        }

        inline void append_row_record(RowRecord const* record,
                                      row_t position)
        {
                _vte_stream_append(m_row_stream,
                                   (char const*)record,
                                   sizeof(*record));
        }

        bool frozen_row_column_to_text_offset(row_t position,
                                              column_t column,
                                              CellTextOffset* offset);
        bool frozen_row_text_offset_to_column(row_t position,
                                              CellTextOffset const* offset,
                                              column_t* column);

        bool write_row(GOutputStream* stream,
                       VteRowData* row,
                       VteWriteFlags flags,
                       GCancellable* cancellable,
                       GError** error);

        void ensure_writable(row_t position);
        void ensure_writable_room();

        void freeze_one_row();
        void maybe_freeze_one_row();
        void thaw_one_row();
        void discard_one_row();
        void maybe_discard_one_row();

        void freeze_row(row_t position,
                        VteRowData const* row);
        void thaw_row(row_t position,
                      VteRowData* row,
                      bool do_truncate,
                      int hyperlink_column,
                      char const** hyperlink);
        void reset_streams(row_t position);

	row_t m_max;
	row_t m_start{0};
        row_t m_end{0};

	/* Writable */
	row_t m_writable{0};
        row_t m_mask{31};
	VteRowData *m_array;

        /* Storage:
         *
         * row_stream contains records of VteRowRecord for each physical row.
         * (This stream is regenerated when the contents rewrap on resize.)
         *
         * text_stream is the text in UTF-8.
         *
         * attr_stream contains entries that consist of:
         *  - a VteCellAttrChange.
         *  - a string of attr.hyperlink_length length containing the (typically empty) hyperlink data.
         *    As far as the ring is concerned, this hyperlink data is opaque. Only the caller cares that
         *    if nonempty, it actually contains the ID and URI separated with a semicolon. Not NUL terminated.
         *  - 2 bytes repeating attr.hyperlink_length so that we can walk backwards.
         */
	bool m_has_streams;
	VteStream *m_attr_stream, *m_text_stream, *m_row_stream;
	size_t m_last_attr_text_start_offset{0};
	VteCellAttr m_last_attr;
	GString *m_utf8_buffer;

	VteRowData m_cached_row;
	row_t m_cached_row_num{(row_t)-1};

        row_t m_visible_rows{0};  /* to keep at least a screenful of lines in memory, bug 646098 comment 12 */

        GPtrArray *m_hyperlinks;  /* The hyperlink pool. Contains GString* items.
                                   [0] points to an empty GString, [1] to [VTE_HYPERLINK_COUNT_MAX] contain the id;uri pairs. */
        char m_hyperlink_buf[VTE_HYPERLINK_TOTAL_LENGTH_MAX + 1];  /* One more hyperlink buffer to get the value if it's not placed in the pool. */
        hyperlink_idx_t m_hyperlink_highest_used_idx{0};  /* 0 if no hyperlinks at all in the pool. */
        hyperlink_idx_t m_hyperlink_current_idx{0};  /* The hyperlink idx used for newly created cells.
                                                   Must not be GC'd even if doesn't occur onscreen. */
        hyperlink_idx_t m_hyperlink_hover_idx{0};  /* The hyperlink idx of the hovered cell.
                                                 An idx is allocated on hover even if the cell is scrolled out to the streams. */
        row_t m_hyperlink_maybe_gc_counter{0};  /* Do a GC when it reaches 65536. */
};

}; /* namespace base */

}; /* namespace vte */

G_BEGIN_DECLS

/* temp compat API */

typedef vte::base::Ring VteRing;

static inline bool _vte_ring_contains(VteRing *ring, gulong position) { return ring->contains(position); }
static inline glong _vte_ring_delta(VteRing *ring) { return ring->delta(); }
static inline glong _vte_ring_length(VteRing *ring) { return ring->length(); }
static inline glong _vte_ring_next(VteRing *ring) { return ring->next(); }
static inline const VteRowData *_vte_ring_index (VteRing *ring, gulong position) { return ring->index(position); }
static inline VteRowData *_vte_ring_index_writable (VteRing *ring, gulong position) { return ring->index_writable(position); }
static inline void _vte_ring_hyperlink_maybe_gc (VteRing *ring, gulong increment) { ring->hyperlink_maybe_gc(increment); }
static inline auto _vte_ring_get_hyperlink_idx (VteRing *ring, const char *hyperlink) { return ring->get_hyperlink_idx(hyperlink); }
static inline auto _vte_ring_get_hyperlink_at_position (VteRing *ring, gulong position, int col, bool update_hover_idx, const char **hyperlink) { return ring->get_hyperlink_at_position(position, col, update_hover_idx, hyperlink); }
static inline long _vte_ring_reset (VteRing *ring) { return ring->reset(); }
static inline void _vte_ring_resize (VteRing *ring, gulong max_rows) { ring->resize(max_rows); }
static inline void _vte_ring_shrink (VteRing *ring, gulong max_len) { ring->shrink(max_len); }
static inline VteRowData *_vte_ring_insert (VteRing *ring, gulong position, guint8 bidi_flags) { return ring->insert(position, bidi_flags); }
static inline VteRowData *_vte_ring_append (VteRing *ring, guint8 bidi_flags) { return ring->append(bidi_flags); }
static inline void _vte_ring_remove (VteRing *ring, gulong position) { ring->remove(position); }
static inline void _vte_ring_drop_scrollback (VteRing *ring, gulong position) { ring->drop_scrollback(position); }
static inline void _vte_ring_set_visible_rows (VteRing *ring, gulong rows) { ring->set_visible_rows(rows); }
static inline void _vte_ring_rewrap (VteRing *ring, glong columns, VteVisualPosition **markers) { ring->rewrap(columns, markers); }
static inline gboolean _vte_ring_write_contents (VteRing *ring,
                                                 GOutputStream *stream,
                                                 VteWriteFlags flags,
                                                 GCancellable *cancellable,
                                                 GError **error) { return ring->write_contents(stream, flags, cancellable, error); }

G_END_DECLS
