/// ============================================================================
//
// Copyright (C) 2010, 2011
// Marco Antonio Zanata Alves
//
// GPPD - Parallel and Distributed Processing Group
// Universidade Federal do Rio Grande do Sul
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
/// ============================================================================
class dlec_metadata_line_t {
    public:
        line_sub_block_t valid_sub_blocks;
        uint64_t real_access_counter;
        uint64_t access_counter;
        bool overflow;
        bool learn_mode;
        aht_line_t *ahtm_pointer;
        aht_line_t *ahtc_pointer;

        /// Static Energy
        uint64_t clock_become_alive;
        uint64_t clock_become_dead;

        /// Special Flags
        bool is_dirty;
        bool need_copyback;
        bool is_dead;
        bool is_last_access;
        bool is_last_write;

        /// Statistics
        uint64_t stat_access_counter;
        uint64_t stat_write_counter;

        uint64_t stat_clock_first_read;
        uint64_t stat_clock_last_read;

        uint64_t stat_clock_first_write;
        uint64_t stat_clock_last_write;

        /// ====================================================================
        /// Methods
        /// ====================================================================
        dlec_metadata_line_t();
        ~dlec_metadata_line_t();

        void clean();
        void reset_statistics();
        std::string content_to_string();
};