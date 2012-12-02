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
class line_usage_predictor_dlec_t : public line_usage_predictor_t {
    private:
        /// ====================================================================
        /// Set by sinuca_configurator
        /// ====================================================================
        uint32_t access_counter_bits;

        bool early_eviction;
        bool early_copyback;
        bool turnoff_dead_lines;

        /// metadata
        uint32_t metadata_line_number;          /// Cache Metadata
        uint32_t metadata_associativity;        /// Cache Metadata

        /// aht - misses
        uint32_t ahtm_line_number;
        uint32_t ahtm_associativity;
        replacement_t ahtm_replacement_policy;

        /// aht - cpyback
        uint32_t ahtc_line_number;
        uint32_t ahtc_associativity;
        replacement_t ahtc_replacement_policy;

        /// ====================================================================
        /// Set by this->allocate()
        /// ====================================================================
        uint32_t access_counter_max;

        /// metadata
        dlec_metadata_set_t *metadata_sets;
        uint32_t metadata_total_sets;

        /// aht - misses
        aht_set_t *ahtm_sets;
        uint32_t ahtm_total_sets;
        uint64_t ahtm_index_bits_mask;

        /// aht - copyback
        aht_set_t *ahtc_sets;
        uint32_t ahtc_total_sets;
        uint64_t ahtc_index_bits_mask;

         /// ====================================================================
        /// Statistics related
        /// ====================================================================
        uint64_t stat_line_hit;
        uint64_t stat_line_miss;
        uint64_t stat_sub_block_miss;
        uint64_t stat_send_copyback;
        uint64_t stat_recv_copyback;
        uint64_t stat_eviction;
        uint64_t stat_invalidation;

        uint64_t stat_ahtm_access;
        uint64_t stat_ahtm_hit;
        uint64_t stat_ahtm_miss;

        uint64_t stat_ahtc_access;
        uint64_t stat_ahtc_hit;
        uint64_t stat_ahtc_miss;

        /// prediction accuracy
        uint64_t stat_line_miss_learn;
        uint64_t stat_line_miss_normal_over;
        uint64_t stat_line_miss_normal_correct;
        uint64_t stat_line_miss_disable_correct;
        uint64_t stat_line_miss_disable_under;

        uint64_t stat_line_copyback_learn;
        uint64_t stat_line_copyback_normal_over;
        uint64_t stat_line_copyback_normal_correct;
        uint64_t stat_line_copyback_disable_correct;
        uint64_t stat_line_copyback_disable_under;

        uint64_t stat_is_last_access_over;
        uint64_t stat_is_last_access_correct;
        uint64_t stat_is_last_access_under;

        uint64_t stat_is_last_write_over;
        uint64_t stat_is_last_write_correct;
        uint64_t stat_is_last_write_under;

        /// general statistics
        uint64_t stat_line_access_0;
        uint64_t stat_line_access_1;
        uint64_t stat_line_access_2_3;
        uint64_t stat_line_access_4_7;
        uint64_t stat_line_access_8_15;
        uint64_t stat_line_access_16_127;
        uint64_t stat_line_access_128_bigger;

        uint64_t stat_line_write_0;
        uint64_t stat_line_write_1;
        uint64_t stat_line_write_2_3;
        uint64_t stat_line_write_4_7;
        uint64_t stat_line_write_8_15;
        uint64_t stat_line_write_16_127;
        uint64_t stat_line_write_128_bigger;

        uint64_t cycles_last_write_to_last_access;
        uint64_t cycles_last_write_to_eviction;
        uint64_t cycles_last_access_to_eviction;

        uint64_t dead_cycles;
        uint64_t alive_cycles;


    public:
        /// ====================================================================
        /// Methods
        /// ====================================================================
        line_usage_predictor_dlec_t();
        ~line_usage_predictor_dlec_t();

        /// ====================================================================
        /// Inheritance from interconnection_interface_t
        /// ====================================================================
        /// Basic Methods
        void allocate();
        void clock(uint32_t sub_cycle);
        // ~ int32_t send_package(memory_package_t *package);
        // ~ bool receive_package(memory_package_t *package, uint32_t input_port, uint32_t transmission_latency);
        /// Token Controller Methods
        // ~ void allocate_token_list();
        // ~ bool check_token_list(memory_package_t *package);
        // ~ uint32_t check_token_space(memory_package_t *package);
        // ~ void remove_token_list(memory_package_t *package);
        /// Debug Methods
        void periodic_check();
        void print_structures();
        void panic();
        /// Statistics Methods
        void reset_statistics();
        void print_statistics();
        void print_configuration();
        /// ====================================================================

        /// ====================================================================
        /// Inheritance from line_usage_predictor_t
        /// ====================================================================
        /// Inspections
        void fill_package_sub_blocks(memory_package_t *package);
        void line_sub_blocks_to_package(memory_package_t *package, uint32_t index, uint32_t way);
        bool check_sub_block_is_hit(memory_package_t *package, uint64_t index, uint32_t way);
        bool check_line_is_last_access(uint32_t index, uint32_t way);
        bool check_line_is_last_write(uint32_t index, uint32_t way);

        /// Cache Operations
        void line_hit(memory_package_t *package, uint32_t index, uint32_t way);
        void line_miss(memory_package_t *package, uint32_t index, uint32_t way);
        void sub_block_miss(memory_package_t *package, uint32_t index, uint32_t way);
        void line_send_copyback(memory_package_t *package, uint32_t index, uint32_t way);
        void line_recv_copyback(memory_package_t *package, uint32_t index, uint32_t way);
        void line_eviction(uint32_t index, uint32_t way);
        void line_invalidation(uint32_t index, uint32_t way);
        /// ====================================================================

        INSTANTIATE_GET_SET(uint32_t, access_counter_bits);
        INSTANTIATE_GET_SET(uint32_t, access_counter_max);

        INSTANTIATE_GET_SET(bool, early_eviction);
        INSTANTIATE_GET_SET(bool, early_copyback);
        INSTANTIATE_GET_SET(bool, turnoff_dead_lines);

        /// metadata
        INSTANTIATE_GET_SET(dlec_metadata_set_t*, metadata_sets);
        INSTANTIATE_GET_SET(uint32_t, metadata_line_number);
        INSTANTIATE_GET_SET(uint32_t, metadata_associativity);
        INSTANTIATE_GET_SET(uint32_t, metadata_total_sets);

        /// aht misses
        aht_line_t* ahtm_find_line(uint64_t opcode_address, uint64_t memory_address);
        aht_line_t* ahtm_evict_address(uint64_t opcode_address, uint64_t memory_address);

        INSTANTIATE_GET_SET(uint32_t, ahtm_line_number);
        INSTANTIATE_GET_SET(uint32_t, ahtm_associativity);
        INSTANTIATE_GET_SET(replacement_t, ahtm_replacement_policy);
        INSTANTIATE_GET_SET(uint32_t, ahtm_total_sets);

        /// aht copyback
        aht_line_t* ahtc_find_line(uint64_t opcode_address, uint64_t memory_address);
        aht_line_t* ahtc_evict_address(uint64_t opcode_address, uint64_t memory_address);

        INSTANTIATE_GET_SET(uint32_t, ahtc_line_number);
        INSTANTIATE_GET_SET(uint32_t, ahtc_associativity);
        INSTANTIATE_GET_SET(replacement_t, ahtc_replacement_policy);
        INSTANTIATE_GET_SET(uint32_t, ahtc_total_sets);

        /// ====================================================================
        /// Statistics related
        /// ====================================================================
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_hit);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_miss);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_sub_block_miss);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_send_copyback);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_recv_copyback);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_eviction);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_invalidation);

        INSTANTIATE_GET_SET_ADD(uint64_t, stat_ahtm_access);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_ahtm_hit);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_ahtm_miss);

        INSTANTIATE_GET_SET_ADD(uint64_t, stat_ahtc_access);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_ahtc_hit);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_ahtc_miss);

        /// Prediction Accuracy
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_miss_learn);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_miss_normal_over);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_miss_normal_correct);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_miss_disable_correct);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_miss_disable_under);

        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_copyback_learn);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_copyback_normal_over);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_copyback_normal_correct);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_copyback_disable_correct);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_copyback_disable_under);

        INSTANTIATE_GET_SET_ADD(uint64_t, stat_is_last_access_over);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_is_last_access_correct);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_is_last_access_under);

        INSTANTIATE_GET_SET_ADD(uint64_t, stat_is_last_write_over);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_is_last_write_correct);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_is_last_write_under);

        /// General statistics
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_access_0);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_access_1);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_access_2_3);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_access_4_7);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_access_8_15);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_access_16_127);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_access_128_bigger);

        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_write_0);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_write_1);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_write_2_3);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_write_4_7);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_write_8_15);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_write_16_127);
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_line_write_128_bigger);

        INSTANTIATE_GET_SET_ADD(uint64_t, cycles_last_write_to_last_access);
        INSTANTIATE_GET_SET_ADD(uint64_t, cycles_last_write_to_eviction);
        INSTANTIATE_GET_SET_ADD(uint64_t, cycles_last_access_to_eviction);

        INSTANTIATE_GET_SET_ADD(uint64_t, dead_cycles);
        INSTANTIATE_GET_SET_ADD(uint64_t, alive_cycles);
};