//==============================================================================
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
//==============================================================================
#include "../sinuca.hpp"

#ifdef LINE_USAGE_PREDICTOR_DEBUG
    #define LINE_USAGE_PREDICTOR_DEBUG_PRINTF(...) DEBUG_PRINTF(__VA_ARGS__);
#else
    #define LINE_USAGE_PREDICTOR_DEBUG_PRINTF(...)
#endif

//==============================================================================
line_usage_predictor_t::line_usage_predictor_t() {
    this->line_usage_predictor_type = LINE_USAGE_PREDICTOR_POLICY_DISABLE;

    /// ========================================================================
    /// DSBP
    /// ========================================================================
    this->DSBP_sets = NULL;
    this->DSBP_line_number = 0;
    this->DSBP_associativity = 0;
    this->DSBP_total_sets = 0;

    this->DSBP_sub_block_size = 0;
    this->DSBP_sub_block_total = 0;

    this->DSBP_usage_counter_bits = 0;
    this->DSBP_usage_counter_max = 0;

    /// PHT
    this->DSBP_PHT_sets = NULL;
    this->DSBP_PHT_line_number = 0;
    this->DSBP_PHT_associativity = 0;
    this->DSBP_PHT_total_sets = 0;
};

//==============================================================================
line_usage_predictor_t::~line_usage_predictor_t() {
    /// De-Allocate memory to prevent memory leak
    utils_t::template_delete_array<DSBP_metadata_sets_t>(DSBP_sets);
    utils_t::template_delete_array<DSBP_PHT_sets_t>(DSBP_PHT_sets);
};

//==============================================================================
void line_usage_predictor_t::allocate() {
    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP:

            ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(sinuca_engine.get_global_line_size() / this->get_DSBP_sub_block_size()), "Wrong line_size(%u) or sub_block_size(%u).\n", this->get_DSBP_line_number(), this->get_DSBP_associativity());
            this->set_DSBP_sub_block_total(sinuca_engine.get_global_line_size() / this->get_DSBP_sub_block_size());

            ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(this->get_DSBP_line_number() / this->get_DSBP_associativity()), "Wrong line_number(%u) or associativity(%u).\n", this->get_DSBP_line_number(), this->get_DSBP_associativity());
            this->set_DSBP_total_sets(this->get_DSBP_line_number() / this->get_DSBP_associativity());

            this->DSBP_usage_counter_max = pow(2, this->DSBP_usage_counter_bits) - 1;

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("Allocate %s DSBP %d(lines) / %d(assoc) = %d (sets) (%d (sub-blocks))\n", this->get_label(), this->get_DSBP_line_number(), this->get_DSBP_associativity(), this->get_DSBP_total_sets(), this->get_DSBP_sub_block_total());
            this->DSBP_sets = utils_t::template_allocate_array<DSBP_metadata_sets_t>(this->get_DSBP_total_sets());
            for (uint32_t i = 0; i < this->get_DSBP_total_sets(); i++) {
                this->DSBP_sets[i].ways = utils_t::template_allocate_array<DSBP_metadata_line_t>(this->get_DSBP_associativity());

                for (uint32_t j = 0; j < this->get_DSBP_associativity(); j++) {
                    this->DSBP_sets[i].ways[j].valid_sub_blocks = utils_t::template_allocate_initialize_array<line_sub_block_t>(sinuca_engine.get_global_line_size(), LINE_SUB_BLOCK_DISABLE);
                    this->DSBP_sets[i].ways[j].real_usage_counter = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_sets[i].ways[j].usage_counter = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_sets[i].ways[j].overflow = utils_t::template_allocate_initialize_array<bool>(sinuca_engine.get_global_line_size(), false);

                    this->DSBP_sets[i].ways[j].clock_become_alive = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_sets[i].ways[j].clock_become_dead = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);

                    this->DSBP_sets[i].ways[j].active_sub_blocks = 0;
                    this->DSBP_sets[i].ways[j].is_dead = true;
                }
            }

            ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(this->get_DSBP_PHT_line_number() / this->get_DSBP_PHT_associativity()), "Wrong line number(%u) or associativity(%u).\n", this->get_DSBP_PHT_line_number(), this->get_DSBP_PHT_associativity());
            this->set_DSBP_PHT_total_sets(this->get_DSBP_PHT_line_number() / this->get_DSBP_PHT_associativity());
            this->DSBP_PHT_sets = utils_t::template_allocate_array<DSBP_PHT_sets_t>(this->get_DSBP_PHT_total_sets());
            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("Allocate %s DSBP_PHT %d(lines) / %d(assoc) = %d (sets) (%d (sub-blocks))\n", this->get_label(), this->get_DSBP_PHT_line_number(), this->get_DSBP_PHT_associativity(), this->get_DSBP_PHT_total_sets(), this->get_DSBP_sub_block_total());

            /// INDEX MASK
            this->DSBP_PHT_index_bits_mask = 0;
            for (uint32_t i = 0; i < utils_t::get_power_of_two(this->get_DSBP_PHT_total_sets()); i++) {
                this->DSBP_PHT_index_bits_mask |= 1 << (i);
            }

            for (uint32_t i = 0; i < this->get_DSBP_PHT_total_sets(); i++) {
                this->DSBP_PHT_sets[i].ways = utils_t::template_allocate_array<DSBP_PHT_line_t>(this->get_DSBP_PHT_associativity());

                for (uint32_t j = 0; j < this->get_DSBP_PHT_associativity(); j++) {
                    this->DSBP_PHT_sets[i].ways[j].pc = 0;
                    this->DSBP_PHT_sets[i].ways[j].offset = 0;
                    this->DSBP_PHT_sets[i].ways[j].pointer = false;

                    this->DSBP_PHT_sets[i].ways[j].usage_counter = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_PHT_sets[i].ways[j].overflow = utils_t::template_allocate_initialize_array<bool>(sinuca_engine.get_global_line_size(), false);
                }
            }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
            // Cache Metadata
            ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(sinuca_engine.get_global_line_size() / this->get_DSBP_sub_block_size()), "Wrong line_size(%u) or sub_block_size(%u).\n", this->get_DSBP_line_number(), this->get_DSBP_associativity());
            this->set_DSBP_sub_block_total(sinuca_engine.get_global_line_size() / this->get_DSBP_sub_block_size());

            ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(this->get_DSBP_line_number() / this->get_DSBP_associativity()), "Wrong line_number(%u) or associativity(%u).\n", this->get_DSBP_line_number(), this->get_DSBP_associativity());
            this->set_DSBP_total_sets(this->get_DSBP_line_number() / this->get_DSBP_associativity());

            this->DSBP_usage_counter_max = pow(2, this->DSBP_usage_counter_bits) -1;

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("Allocate %s DSBP %d(lines) / %d(assoc) = %d (sets) (%d (sub-blocks))\n", this->get_label(), this->get_DSBP_line_number(), this->get_DSBP_associativity(), this->get_DSBP_total_sets(), this->get_DSBP_sub_block_total());
            this->DSBP_sets = utils_t::template_allocate_array<DSBP_metadata_sets_t>(this->get_DSBP_total_sets());
            for (uint32_t i = 0; i < this->get_DSBP_total_sets(); i++) {
                this->DSBP_sets[i].ways = utils_t::template_allocate_array<DSBP_metadata_line_t>(this->get_DSBP_associativity());

                for (uint32_t j = 0; j < this->get_DSBP_associativity(); j++) {
                    this->DSBP_sets[i].ways[j].valid_sub_blocks = utils_t::template_allocate_initialize_array<line_sub_block_t>(sinuca_engine.get_global_line_size(), LINE_SUB_BLOCK_DISABLE);
                    this->DSBP_sets[i].ways[j].real_usage_counter = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_sets[i].ways[j].usage_counter = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_sets[i].ways[j].overflow = utils_t::template_allocate_initialize_array<bool>(sinuca_engine.get_global_line_size(), false);

                    this->DSBP_sets[i].ways[j].clock_become_alive = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_sets[i].ways[j].clock_become_dead = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size(), 0);

                    this->DSBP_sets[i].ways[j].active_sub_blocks = 0;
                    this->DSBP_sets[i].ways[j].is_dead = true;
                }
            }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
        break;
    }

    /// ================================================================
    /// Statistics
    /// ================================================================
    this->stat_accessed_sub_block = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size() + 1, 0);
    this->stat_active_sub_block_per_access = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size() + 1, 0);
    this->stat_active_sub_block_per_cycle = utils_t::template_allocate_initialize_array<uint64_t>(sinuca_engine.get_global_line_size() + 1, 0);


};

//==============================================================================
void line_usage_predictor_t::clock(uint32_t subcycle) {
    if (subcycle != 0) return;
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("==================== ");
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("====================\n");
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("cycle() \n");

};


//==============================================================================
bool line_usage_predictor_t::receive_package(memory_package_t *package, uint32_t input_port) {
    ERROR_PRINTF("Received package %s into the input_port %u.\n", package->memory_to_string().c_str(), input_port);
    return FAIL;
};

//==============================================================================
void line_usage_predictor_t::print_structures() {
};

// =============================================================================
void line_usage_predictor_t::panic() {
    this->print_structures();
};

//==============================================================================
void line_usage_predictor_t::periodic_check(){
    #ifdef PREFETCHER_DEBUG
        this->print_structures();
    #endif
};

// =============================================================================
// STATISTICS
// =============================================================================
void line_usage_predictor_t::reset_statistics() {
    this->stat_DSBP_line_sub_block_disable_always = 0;
    this->stat_DSBP_line_sub_block_disable_turnoff = 0;
    this->stat_DSBP_line_sub_block_normal_correct = 0;
    this->stat_DSBP_line_sub_block_normal_over = 0;
    this->stat_DSBP_line_sub_block_learn = 0;
    this->stat_DSBP_line_sub_block_wrong_first = 0;
    this->stat_DSBP_line_sub_block_copyback = 0;

    this->stat_line_miss = 0;
    this->stat_sub_block_miss = 0;
    this->stat_copyback = 0;
    this->stat_eviction = 0;

    this->stat_DSBP_PHT_access = 0;
    this->stat_DSBP_PHT_hit = 0;
    this->stat_DSBP_PHT_miss = 0;

    this->stat_sub_block_touch_0 = 0;
    this->stat_sub_block_touch_1 = 0;
    this->stat_sub_block_touch_2_3 = 0;
    this->stat_sub_block_touch_4_7 = 0;
    this->stat_sub_block_touch_8_15 = 0;
    this->stat_sub_block_touch_16_127 = 0;
    this->stat_sub_block_touch_128_bigger = 0;

    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size() + 1; i++) {
        stat_accessed_sub_block[i] = 0;
    }
};

// =============================================================================
void line_usage_predictor_t::print_statistics() {
    char title[100] = "";
    sprintf(title, "Statistics of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

    for (uint32_t i = 0; i < this->get_DSBP_total_sets(); i++) {
        for (uint32_t j = 0; j < this->get_DSBP_associativity(); j++) {
            this->line_eviction(i, j);
        }
    }

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_line_sub_block_disable_always", stat_DSBP_line_sub_block_disable_always);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_line_sub_block_disable_turnoff", stat_DSBP_line_sub_block_disable_turnoff);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_line_sub_block_normal_correct", stat_DSBP_line_sub_block_normal_correct);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_line_sub_block_normal_over", stat_DSBP_line_sub_block_normal_over);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_line_sub_block_learn", stat_DSBP_line_sub_block_learn);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_line_sub_block_wrong_first", stat_DSBP_line_sub_block_wrong_first);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_line_sub_block_copyback", stat_DSBP_line_sub_block_copyback);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_line_miss", stat_line_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sub_block_miss", stat_sub_block_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_copyback", stat_copyback);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_eviction", stat_eviction);
    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_PHT_access", stat_DSBP_PHT_access);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_PHT_hit", stat_DSBP_PHT_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_DSBP_PHT_miss", stat_DSBP_PHT_miss);


    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sub_block_touch_0", stat_sub_block_touch_0);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sub_block_touch_1", stat_sub_block_touch_1);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sub_block_touch_2_3", stat_sub_block_touch_2_3);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sub_block_touch_4_7", stat_sub_block_touch_4_7);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sub_block_touch_8_15", stat_sub_block_touch_8_15);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sub_block_touch_16_127", stat_sub_block_touch_16_127);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sub_block_touch_128_bigger", stat_sub_block_touch_128_bigger);

    char name[100];
    sinuca_engine.write_statistics_small_separator();
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size() + 1; i++) {
        sprintf(name, "stat_accessed_sub_block_%u", i);
        sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), name, stat_accessed_sub_block[i]);
    }

    sinuca_engine.write_statistics_small_separator();
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size() + 1; i++) {
        sprintf(name, "stat_active_sub_block_per_access_%u", i);
        sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), name, stat_active_sub_block_per_access[i]);
    }

    sinuca_engine.write_statistics_small_separator();
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size() + 1; i++) {
        sprintf(name, "stat_active_sub_block_per_cycle_%u", i);
        sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), name, stat_active_sub_block_per_cycle[i]);
    }
};

// =============================================================================
void line_usage_predictor_t::print_configuration() {
    char title[100] = "";
    sprintf(title, "Configuration of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "line_usage_predictor_type", get_enum_line_usage_predictor_policy_char(line_usage_predictor_type));

    /// ====================================================================
    /// DSBP
    /// ====================================================================
    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_usage_counter_bits", DSBP_usage_counter_bits);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_usage_counter_max", DSBP_usage_counter_max);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_sub_block_size", DSBP_sub_block_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_sub_block_total", DSBP_sub_block_total);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_line_number", DSBP_line_number);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_associativity", DSBP_associativity);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_total_sets", DSBP_total_sets);


    /// PHT
    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_PHT_line_number", DSBP_PHT_line_number);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_PHT_associativity", DSBP_PHT_associativity);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_PHT_total_sets", DSBP_PHT_total_sets);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_PHT_replacement_policy", get_enum_replacement_char(DSBP_PHT_replacement_policy));
};


// =============================================================================
// Input:   base_address, size
// Output:  start_sub_block, end_Sub_block
void line_usage_predictor_t::get_start_end_sub_blocks(uint64_t base_address, uint32_t size, uint32_t& sub_block_ini, uint32_t& sub_block_end) {
    ERROR_ASSERT_PRINTF(size > 0, "Received a request with invalid size.\n")
    uint64_t address_offset = base_address & sinuca_engine.get_global_offset_bits_mask();
    uint32_t address_size = address_offset + size;
    if (address_size >= sinuca_engine.get_global_line_size()){
        address_size = sinuca_engine.get_global_line_size();
    }

    sub_block_ini = uint32_t(address_offset / this->DSBP_sub_block_size) * this->DSBP_sub_block_size;
    sub_block_end = uint32_t( (address_size / this->DSBP_sub_block_size) +
                                        uint32_t((address_size % this->DSBP_sub_block_size) != 0) ) * this->DSBP_sub_block_size;
}

// =============================================================================
void line_usage_predictor_t::fill_package_sub_blocks(memory_package_t *package) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("fill_package_sub_blocks() package:%s\n", package->memory_to_string().c_str())

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP:
        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
        {
            /// Compute the START and END sub_blocks
            uint32_t sub_block_ini, sub_block_end;
            this->get_start_end_sub_blocks(package->memory_address, package->memory_size, sub_block_ini, sub_block_end);

            /// Generates the Request Vector
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                package->sub_blocks[i] = ( i >= sub_block_ini && i < sub_block_end );
            }

            #ifdef LINE_USAGE_PREDICTOR_DEBUG
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t package->sub_blocks[")
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    if (i % this->DSBP_sub_block_size == 0) {
                        SINUCA_PRINTF("|");
                    }
                    SINUCA_PRINTF("%u ", package->sub_blocks[i])
                }
                SINUCA_PRINTF("]\n")
            #endif
        }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
        break;
    }
};

// =============================================================================
bool line_usage_predictor_t::check_sub_block_is_hit(memory_package_t *package, uint64_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("check_sub_block_is_hit() package:%s\n", package->memory_to_string().c_str())

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {

            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                if (package->sub_blocks[i] == true &&
                this->DSBP_sets[index].ways[way].valid_sub_blocks[i] == LINE_SUB_BLOCK_DISABLE) {
                    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t sub_blocks MISS\n")
                    return false;
                }
            }

            #ifdef LINE_USAGE_PREDICTOR_DEBUG
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t valid_sub_blocks[")
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    if (i % this->DSBP_sub_block_size == 0) { SINUCA_PRINTF("|"); }
                    SINUCA_PRINTF("%u ", this->DSBP_sets[index].ways[way].valid_sub_blocks[i])
                }
                SINUCA_PRINTF("]\n")
            #endif

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t sub_blocks HIT\n")
            return true;
        }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
        break;
    }
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t sub_blocks HIT\n")
    return true;
};

// =============================================================================
// Mechanism Operations
// =============================================================================
void line_usage_predictor_t::line_hit(memory_package_t *package, uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("line_hit() package:%s\n", package->memory_to_string().c_str())

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            #ifdef LINE_USAGE_PREDICTOR_DEBUG
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t package->sub_blocks[")
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    if (i % this->DSBP_sub_block_size == 0) { SINUCA_PRINTF("|"); }
                    SINUCA_PRINTF("%u ", package->sub_blocks[i])
                }
                SINUCA_PRINTF("]\n")
            #endif

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t Before Hit %s", DSBP_metadata_line_to_string(&this->DSBP_sets[index].ways[way]).c_str())

            // =================================================================
            // Statistics for Dynamic Energy
            uint32_t number_sub_blocks = this->DSBP_sets[index].ways[way].active_sub_blocks;
            ERROR_ASSERT_PRINTF(number_sub_blocks > 0, "No active_sub_blocks during an access.\n");
            ERROR_ASSERT_PRINTF(number_sub_blocks <= sinuca_engine.get_global_line_size(), "More active_sub_blocks than the line_size.\n");
            this->stat_active_sub_block_per_access[number_sub_blocks]++;
            // =================================================================

            // Update the METADATA real_usage_counter
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                // Package Requested
                if (package->sub_blocks[i] == true) {
                    this->DSBP_sets[index].ways[way].real_usage_counter[i]++;
                    ERROR_ASSERT_PRINTF(this->DSBP_sets[index].ways[way].valid_sub_blocks[i] != LINE_SUB_BLOCK_DISABLE, "Line hit into a DISABLED sub_block.\n");
                }
            }

            /// ================================================================
            /// METADATA Learn Mode
            /// ================================================================
            if (this->DSBP_sets[index].ways[way].learn_mode == true) {
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t LEARN MODE ON\n");
                // Has PHT pointer
                if (this->DSBP_sets[index].ways[way].PHT_pointer != NULL) {
                    // Update the PHT
                    this->add_stat_DSBP_PHT_access();
                    this->DSBP_sets[index].ways[way].PHT_pointer->last_access = sinuca_engine.get_global_cycle();
                    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                        // Package Requested
                        if (package->sub_blocks[i] == true) {
                            // Check the PHT Overflow
                            if (this->DSBP_sets[index].ways[way].PHT_pointer->usage_counter[i] >= this->DSBP_usage_counter_max) {
                                this->DSBP_sets[index].ways[way].PHT_pointer->overflow[i] = true;
                            }
                            else {
                                this->DSBP_sets[index].ways[way].PHT_pointer->usage_counter[i]++;
                            }
                        }
                    }
                }
            }

            /// ================================================================
            /// METADATA Not Learn Mode
            /// ================================================================
            else {
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t LEARN MODE OFF\n");
                // Turn off dead sub-blocks
                if (this->DSBP_sets[index].ways[way].active_sub_blocks > 0) {
                    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                        // METADATA Not Overflow + METADATA Used Predicted Number of Times
                        if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] != LINE_SUB_BLOCK_DISABLE &&
                        this->DSBP_sets[index].ways[way].overflow[i] == false &&
                        this->DSBP_sets[index].ways[way].real_usage_counter[i] == this->DSBP_sets[index].ways[way].usage_counter[i]) {
                            // METADATA Turn off the sub_blocks
                            this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_DISABLE;
                            this->DSBP_sets[index].ways[way].active_sub_blocks--;
                            this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
                        }
                    }
                }
            }

            // Enable/Disable IS-DEAD flag
            if (this->DSBP_sets[index].ways[way].active_sub_blocks > 0) {
                this->DSBP_sets[index].ways[way].is_dead = false;
            }
            else {
                this->DSBP_sets[index].ways[way].is_dead = true;
            }

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t After Hit %s", DSBP_metadata_line_to_string(&this->DSBP_sets[index].ways[way]).c_str())
        }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
            // Update the METADATA real_usage_counter
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                this->DSBP_sets[index].ways[way].real_usage_counter[i] += package->sub_blocks[i];
            }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
        break;
    }
};


// =============================================================================
void line_usage_predictor_t::compute_static_energy(uint32_t index, uint32_t way) {
    // Statistics for Static Energy
    bool aux_computed_sub_blocks[sinuca_engine.get_global_line_size()];
    uint32_t aux_computed_number = sinuca_engine.get_global_line_size();

    // Add the clock_become_dead for the never turned-off sub_blocks
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] != LINE_SUB_BLOCK_DISABLE) {
            this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
        }
        aux_computed_sub_blocks[i] = false;
    }
    uint64_t computed_cycles = 0;
    uint64_t old_sub_block_clock = sinuca_engine.get_global_cycle() + 1;
    uint64_t old_sub_block_position = 0;
    uint32_t sub_blocks_become_dead = 1;
    while (aux_computed_number > 0 ) {
        old_sub_block_clock = sinuca_engine.get_global_cycle();
        old_sub_block_position = 0;
        sub_blocks_become_dead = 1;

        // Find the (Not computed yet) and (become dead first)
        for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
            if (aux_computed_sub_blocks[i] == false) {
                if (this->DSBP_sets[index].ways[way].clock_become_dead[i] < old_sub_block_clock) {
                    old_sub_block_clock = this->DSBP_sets[index].ways[way].clock_become_dead[i];
                    old_sub_block_position = i;
                    sub_blocks_become_dead = 1;
                }
                else if (this->DSBP_sets[index].ways[way].clock_become_dead[i] == old_sub_block_clock) {
                    sub_blocks_become_dead++;
                }
            }
        }

        // Compute the sub_block time of life
        uint64_t time_of_life = 0;
        if (computed_cycles == 0) {
            time_of_life = this->DSBP_sets[index].ways[way].clock_become_dead[old_sub_block_position] - this->DSBP_sets[index].ways[way].clock_become_alive[old_sub_block_position];
        }
        else {
            time_of_life = this->DSBP_sets[index].ways[way].clock_become_dead[old_sub_block_position] - computed_cycles;
        }
        this->stat_active_sub_block_per_cycle[aux_computed_number] += time_of_life;

        // Update the cycles already computed.
        computed_cycles = this->DSBP_sets[index].ways[way].clock_become_dead[old_sub_block_position];

        // Reduce the number of sub_blocks to compute
        // If only one sub_block was turned off on the same cycle
        if (sub_blocks_become_dead == 1) {
            aux_computed_sub_blocks[old_sub_block_position] = true;
            aux_computed_number--;
        }
        // if multiple sub_blocks were turned off on the same cycle
        else {
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                if (this->DSBP_sets[index].ways[way].clock_become_dead[i] == old_sub_block_clock) {
                    aux_computed_sub_blocks[i] = true;
                    aux_computed_number--;
                }
            }
        }
    }

    // Compute the sub_block time of life (Zero sub_blocks turned on)
    uint64_t time_of_life = 0;
    // ~ if (computed_cycles == 0) {
        // ~ time_of_life = sinuca_engine.get_global_cycle() - this->DSBP_sets[index].ways[way].clock_become_alive[old_sub_block_position];
    // ~ }
    // ~ else {
        time_of_life = sinuca_engine.get_global_cycle() - computed_cycles;
    // ~ }
    this->stat_active_sub_block_per_cycle[0] += time_of_life;
};

// =============================================================================
// Collateral Effect: Change the package->sub_blocks[]
void line_usage_predictor_t::line_miss(memory_package_t *package, uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("line_miss() package:%s\n", package->memory_to_string().c_str())
    this->add_stat_line_miss();

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            DSBP_PHT_line_t *PHT_line = this->DSBP_PHT_find_line(package->opcode_address, package->memory_address);
            ///=================================================================
            /// PHT HIT
            ///=================================================================
            if (PHT_line != NULL) {
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t PHT HIT\n")
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t %s", DSBP_PHT_line_to_string(PHT_line).c_str())

                this->add_stat_DSBP_PHT_hit();

                // Update the PHT entry
                this->add_stat_DSBP_PHT_access();
                PHT_line->last_access = sinuca_engine.get_global_cycle();

                // If no PHT_pointer
                if (PHT_line->pointer == false) {
                    // Create a PHT pointer
                    PHT_line->pointer = true;
                    this->DSBP_sets[index].ways[way].PHT_pointer = PHT_line;
                }
                else {
                    this->DSBP_sets[index].ways[way].PHT_pointer = NULL;
                }

                // Clean the metadata entry
                this->DSBP_sets[index].ways[way].learn_mode = false;
                this->DSBP_sets[index].ways[way].active_sub_blocks = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    // Copy the PHT prediction
                    this->DSBP_sets[index].ways[way].usage_counter[i] = PHT_line->usage_counter[i];
                    this->DSBP_sets[index].ways[way].overflow[i] = PHT_line->overflow[i];

                    this->DSBP_sets[index].ways[way].real_usage_counter[i] = 0;
                    this->DSBP_sets[index].ways[way].clock_become_alive[i] = sinuca_engine.get_global_cycle();
                    this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
                    this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_DISABLE;
                    if (package->sub_blocks[i] == true || PHT_line->usage_counter[i] > 0) {
                        this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_NORMAL;
                        this->DSBP_sets[index].ways[way].active_sub_blocks++;
                    }
                }

                // Add the last access information
                this->line_hit(package, index, way);

                // Modify the package->sub_blocks (next level request)
                package->memory_size = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] != LINE_SUB_BLOCK_DISABLE ||
                    this->DSBP_sets[index].ways[way].real_usage_counter[i] != 0) { //already turned off due to this access (dead on arrival)
                        package->sub_blocks[i] = true;
                        package->memory_size++;
                    }
                    else {
                        package->sub_blocks[i] = false;
                    }

                }
            }

            ///=================================================================
            /// PHT MISS
            ///=================================================================
            else {
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t PHT MISS\n")
                this->add_stat_DSBP_PHT_miss();
                // New PHT entry
                PHT_line = DSBP_PHT_evict_address(package->opcode_address, package->memory_address);
                // Clean the PHT entry
                this->add_stat_DSBP_PHT_access();
                PHT_line->last_access = sinuca_engine.get_global_cycle();
                PHT_line->pc = package->opcode_address;
                PHT_line->offset = package->memory_address & sinuca_engine.get_global_offset_bits_mask();
                PHT_line->last_access = sinuca_engine.get_global_cycle();
                PHT_line->pointer = true;
                this->DSBP_sets[index].ways[way].PHT_pointer = PHT_line;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    PHT_line->usage_counter[i] = 0;
                    PHT_line->overflow[i] = false;
                }

                // Clean the metadata entry
                this->DSBP_sets[index].ways[way].learn_mode = true;
                this->DSBP_sets[index].ways[way].active_sub_blocks = sinuca_engine.get_global_line_size();
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    this->DSBP_sets[index].ways[way].real_usage_counter[i] = 0;
                    this->DSBP_sets[index].ways[way].usage_counter[i] = 0;
                    this->DSBP_sets[index].ways[way].overflow[i] = false;
                    this->DSBP_sets[index].ways[way].clock_become_alive[i] = sinuca_engine.get_global_cycle();
                    this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
                    this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_LEARN;
                }

                // Add the last access information
                this->line_hit(package, index, way);

                // Modify the package->sub_blocks (next level request)
                package->memory_size = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    package->sub_blocks[i] = true;
                    package->memory_size++;
                }
            }

        }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
            package->memory_size = sinuca_engine.get_global_line_size();

            // Clean the metadata entry
            this->DSBP_sets[index].ways[way].learn_mode = true;
            this->DSBP_sets[index].ways[way].active_sub_blocks = sinuca_engine.get_global_line_size();
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                this->DSBP_sets[index].ways[way].real_usage_counter[i] = 0;
                this->DSBP_sets[index].ways[way].usage_counter[i] = 0;
                this->DSBP_sets[index].ways[way].overflow[i] = true;
                this->DSBP_sets[index].ways[way].clock_become_alive[i] = sinuca_engine.get_global_cycle();
                this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
                this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_NORMAL;
            }

            // Add the last access information
            this->line_hit(package, index, way);

            // Modify the package->sub_blocks (next level request)
            package->memory_size = sinuca_engine.get_global_line_size();
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
            package->memory_size = sinuca_engine.get_global_line_size();
        break;
    }
};


// =============================================================================
// Collateral Effect: Change the package->sub_blocks[]
void line_usage_predictor_t::sub_block_miss(memory_package_t *package, uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("sub_block_miss() package:%s\n", package->memory_to_string().c_str())
    this->add_stat_sub_block_miss();

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            // =================================================================
            // Statistics for Static Energy
            this->compute_static_energy(index, way);
            // =================================================================

            DSBP_PHT_line_t *PHT_line = this->DSBP_sets[index].ways[way].PHT_pointer;
            ///=================================================================
            /// PHT HIT
            ///=================================================================
            if (PHT_line != NULL && PHT_line->pointer == true) {
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t PHT HIT\n")
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t Before Update %s", DSBP_metadata_line_to_string(&this->DSBP_sets[index].ways[way]).c_str())
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t Before Update %s", DSBP_PHT_line_to_string(PHT_line).c_str())

                // Enable Learn_mode
                this->DSBP_sets[index].ways[way].learn_mode = true;
                this->DSBP_sets[index].ways[way].active_sub_blocks = sinuca_engine.get_global_line_size();

                // Update the PHT entry
                this->add_stat_DSBP_PHT_access();
                PHT_line->last_access = sinuca_engine.get_global_cycle();
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    if (this->DSBP_sets[index].ways[way].real_usage_counter[i] <= this->DSBP_usage_counter_max) {
                        PHT_line->usage_counter[i] = this->DSBP_sets[index].ways[way].real_usage_counter[i];
                        PHT_line->overflow[i] = false;
                    }
                    else {
                        PHT_line->usage_counter[i] = this->DSBP_usage_counter_max;
                        PHT_line->overflow[i] = true;
                    }

                    // Enable all sub_blocks
                    this->DSBP_sets[index].ways[way].usage_counter[i] = 0;
                    this->DSBP_sets[index].ways[way].overflow[i] = true;
                    this->DSBP_sets[index].ways[way].clock_become_alive[i] = sinuca_engine.get_global_cycle();
                    this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
                    // Enable Valid Sub_blocks
                    if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] == LINE_SUB_BLOCK_DISABLE) {
                        this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_WRONG_FIRST;
                    }
                    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t After Update %s", DSBP_PHT_line_to_string(PHT_line).c_str())
                }

                // Add the last access information
                this->line_hit(package, index, way);

                // Enable all sub_blocks missing
                package->memory_size = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] == LINE_SUB_BLOCK_WRONG_FIRST) {
                        package->sub_blocks[i] = true;
                        package->memory_size++;
                    }
                    else {
                        package->sub_blocks[i] = false;
                    }
                }
            }

            ///=================================================================
            /// PHT MISS
            ///=================================================================
            else {
                // Enable Learn_mode
                this->DSBP_sets[index].ways[way].learn_mode = true;
                this->DSBP_sets[index].ways[way].active_sub_blocks = sinuca_engine.get_global_line_size();

                // Enable all sub_blocks
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    this->DSBP_sets[index].ways[way].usage_counter[i] = 0;
                    this->DSBP_sets[index].ways[way].overflow[i] = true;
                    this->DSBP_sets[index].ways[way].clock_become_alive[i] = sinuca_engine.get_global_cycle();
                    this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
                    // Enable Valid Sub_blocks
                    if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] == LINE_SUB_BLOCK_DISABLE) {
                        this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_WRONG_FIRST;
                    }
                }

                // Add the last access information
                this->line_hit(package, index, way);

                // Enable all sub_blocks missing
                package->memory_size = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] == LINE_SUB_BLOCK_WRONG_FIRST) {
                        package->sub_blocks[i] = true;
                        package->memory_size++;
                    }
                    else {
                        package->sub_blocks[i] = false;
                    }
                }
            }
        }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
        break;
    }
};

// =============================================================================
// Collateral Effect: Change the package->sub_blocks[]
void line_usage_predictor_t::line_insert_copyback(memory_package_t *package, uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("line_miss() package:%s\n", package->memory_to_string().c_str())
    this->add_stat_line_miss();

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            // Clean the metadata entry
            this->DSBP_sets[index].ways[way].learn_mode = true;
            this->DSBP_sets[index].ways[way].active_sub_blocks = 0;
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                this->DSBP_sets[index].ways[way].real_usage_counter[i] = 0;
                this->DSBP_sets[index].ways[way].usage_counter[i] = 0;
                this->DSBP_sets[index].ways[way].overflow[i] = false;
                this->DSBP_sets[index].ways[way].clock_become_alive[i] = sinuca_engine.get_global_cycle();
                this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
                this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_DISABLE;
                if (package->sub_blocks[i] == true) {
                    this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_COPYBACK;
                    this->DSBP_sets[index].ways[way].active_sub_blocks++;
                }
            }

            package->memory_size = 1;
        }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
            // Clean the metadata entry
            this->DSBP_sets[index].ways[way].learn_mode = true;
            this->DSBP_sets[index].ways[way].active_sub_blocks = sinuca_engine.get_global_line_size();
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                this->DSBP_sets[index].ways[way].real_usage_counter[i] = 0;
                this->DSBP_sets[index].ways[way].usage_counter[i] = 0;
                this->DSBP_sets[index].ways[way].overflow[i] = true;
                this->DSBP_sets[index].ways[way].clock_become_alive[i] = sinuca_engine.get_global_cycle();
                this->DSBP_sets[index].ways[way].clock_become_dead[i] = sinuca_engine.get_global_cycle();
                this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_NORMAL;
            }

            // Modify the package->sub_blocks (next level request)
            package->memory_size = 1;
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
            package->memory_size = 1;
        break;
    }
};


// =============================================================================
void line_usage_predictor_t::line_get_copyback(memory_package_t *package, uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("line_copy_back() package:%s\n", package->memory_to_string().c_str())

    this->add_stat_copyback();

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            // Modify the package->sub_blocks (valid_sub_blocks)
            package->memory_size = 0;
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] != LINE_SUB_BLOCK_DISABLE) {
                    package->sub_blocks[i] = true;
                    package->memory_size++;
                }
                else {
                    package->sub_blocks[i] = false;
                }
            }
        }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
            package->memory_size = sinuca_engine.get_global_line_size();
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
            package->memory_size = sinuca_engine.get_global_line_size();
        break;
    }
};

// =============================================================================
void line_usage_predictor_t::line_eviction(uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("line_eviction()\n")

    this->add_stat_eviction();

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP:
            // =================================================================
            // Statistics for Static Energy
            this->compute_static_energy(index, way);
            // =================================================================

        case LINE_USAGE_PREDICTOR_POLICY_DSBP_DISABLE:
        {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);


            DSBP_PHT_line_t *PHT_line = this->DSBP_sets[index].ways[way].PHT_pointer;
            // PHT HIT
            if (PHT_line != NULL) {
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t PHT HIT\n")
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t Before Update %s", DSBP_metadata_line_to_string(&this->DSBP_sets[index].ways[way]).c_str())
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t Before Update %s", DSBP_PHT_line_to_string(PHT_line).c_str())
                // Update the PHT entry
                this->add_stat_DSBP_PHT_access();
                PHT_line->last_access = sinuca_engine.get_global_cycle();
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    if (this->DSBP_sets[index].ways[way].real_usage_counter[i] <= this->DSBP_usage_counter_max) {
                        PHT_line->usage_counter[i] = this->DSBP_sets[index].ways[way].real_usage_counter[i];
                        PHT_line->overflow[i] = false;
                    }
                    else {
                        PHT_line->usage_counter[i] = this->DSBP_usage_counter_max;
                        PHT_line->overflow[i] = true;
                    }
                }
                PHT_line->pointer = false;
                this->DSBP_sets[index].ways[way].PHT_pointer = NULL;
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t After Update %s", DSBP_PHT_line_to_string(PHT_line).c_str())
            }
            else {
                LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t PHT MISS\n")
            }


            //==================================================================
            // Statistics
            uint32_t sub_blocks_touched = 0;
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                // Prediction Accuracy
                switch (this->DSBP_sets[index].ways[way].valid_sub_blocks[i]) {
                    case LINE_SUB_BLOCK_DISABLE:
                        if (this->DSBP_sets[index].ways[way].real_usage_counter[i] == 0) {
                            this->stat_DSBP_line_sub_block_disable_always++;
                        }
                        else {
                            this->stat_DSBP_line_sub_block_disable_turnoff++;
                        }
                    break;

                    case LINE_SUB_BLOCK_NORMAL:
                        if (this->DSBP_sets[index].ways[way].overflow[i] == 1) {
                            this->stat_DSBP_line_sub_block_normal_correct++;
                        }
                        else {
                            this->stat_DSBP_line_sub_block_normal_over++;
                        }
                    break;

                    case LINE_SUB_BLOCK_LEARN:
                        this->stat_DSBP_line_sub_block_learn++;
                    break;

                    case LINE_SUB_BLOCK_WRONG_FIRST:
                        this->stat_DSBP_line_sub_block_wrong_first++;
                    break;

                    case LINE_SUB_BLOCK_COPYBACK:
                        this->stat_DSBP_line_sub_block_copyback++;
                    break;

                }

                // Touches before eviction
                if (this->DSBP_sets[index].ways[way].real_usage_counter[i] == 0) {
                    this->add_stat_sub_block_touch_0();
                }
                else if (this->DSBP_sets[index].ways[way].real_usage_counter[i] == 1) {
                    this->add_stat_sub_block_touch_1();
                }
                else if (this->DSBP_sets[index].ways[way].real_usage_counter[i] >= 2 && this->DSBP_sets[index].ways[way].real_usage_counter[i] <= 3) {
                    this->add_stat_sub_block_touch_2_3();
                }
                else if (this->DSBP_sets[index].ways[way].real_usage_counter[i] >= 4 && this->DSBP_sets[index].ways[way].real_usage_counter[i] <= 7) {
                    this->add_stat_sub_block_touch_4_7();
                }
                else if (this->DSBP_sets[index].ways[way].real_usage_counter[i] >= 8 && this->DSBP_sets[index].ways[way].real_usage_counter[i] <= 15) {
                    this->add_stat_sub_block_touch_8_15();
                }
                else if (this->DSBP_sets[index].ways[way].real_usage_counter[i] >= 16 && this->DSBP_sets[index].ways[way].real_usage_counter[i] <= 127) {
                    this->add_stat_sub_block_touch_16_127();
                }
                else if (this->DSBP_sets[index].ways[way].real_usage_counter[i] >=128) {
                    this->add_stat_sub_block_touch_128_bigger();
                }

                // Sub-Blocks accessed before eviction
                if (this->DSBP_sets[index].ways[way].real_usage_counter[i] != 0) {
                    sub_blocks_touched++;
                }
            }
            this->stat_accessed_sub_block[sub_blocks_touched]++;

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("Stats %u %"PRIu64"\n", sub_blocks_touched, this->stat_accessed_sub_block[sub_blocks_touched] )
        }
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
        break;
    }
};


// =============================================================================
std::string line_usage_predictor_t::DSBP_metadata_line_to_string(DSBP_metadata_line_t *DSBP_metadata_line) {
    std::string PackageString;
    PackageString = "";

    PackageString = PackageString + "DSBP_LINE Learn:" + utils_t::uint32_to_char(DSBP_metadata_line->learn_mode);
    PackageString = PackageString + " Dead:" + utils_t::uint32_to_char(DSBP_metadata_line->is_dead);
    PackageString = PackageString + " PHT Ptr:" + utils_t::uint32_to_char(DSBP_metadata_line->PHT_pointer != NULL);

    PackageString = PackageString + "\n\t Valid Sub-Blocks      [";
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        if (i % this->DSBP_sub_block_size == 0) {
            PackageString = PackageString + "|";
        }
        PackageString = PackageString + " " + utils_t::uint32_to_char(DSBP_metadata_line->valid_sub_blocks[i]);
    }
    PackageString = PackageString + "]\n";

    PackageString = PackageString + "\t Real Usage Counter    [";
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        if (i % this->DSBP_sub_block_size == 0) {
            PackageString = PackageString + "|";
        }
        PackageString = PackageString + " " + utils_t::uint32_to_char(DSBP_metadata_line->real_usage_counter[i]);
    }
    PackageString = PackageString + "]\n";

    PackageString = PackageString + "\t Usage Counter         [";
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        if (i % this->DSBP_sub_block_size == 0) {
            PackageString = PackageString + "|";
        }
        PackageString = PackageString + " " + utils_t::uint32_to_char(DSBP_metadata_line->usage_counter[i]);
    }
    PackageString = PackageString + "]\n";

    PackageString = PackageString + "\t Overflow              [";
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        if (i % this->DSBP_sub_block_size == 0) {
            PackageString = PackageString + "|";
        }
        PackageString = PackageString + " " + utils_t::uint32_to_char(DSBP_metadata_line->overflow[i]);
    }

    PackageString = PackageString + "]\n";
    return PackageString;
};



// =============================================================================
// DSBP - PHT
// =============================================================================
DSBP_PHT_line_t* line_usage_predictor_t::DSBP_PHT_find_line(uint64_t pc, uint64_t memory_address) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("DSBP_PHT_find_line()\n")
    uint32_t PHT_offset = memory_address & sinuca_engine.get_global_offset_bits_mask();
    uint32_t PHT_index = pc & this->DSBP_PHT_index_bits_mask;

    ERROR_ASSERT_PRINTF(PHT_offset < sinuca_engine.get_global_line_size(), "Wrong offset %d > line_size %d", PHT_offset, sinuca_engine.get_global_line_size());
    ERROR_ASSERT_PRINTF(PHT_index < this->DSBP_PHT_total_sets, "Wrong index %d > total_sets %d", PHT_index, this->DSBP_PHT_total_sets);

    for (uint32_t PHT_way = 0; PHT_way < this->get_DSBP_PHT_associativity(); PHT_way++) {
        if (this->DSBP_PHT_sets[PHT_index].ways[PHT_way].pc == pc && this->DSBP_PHT_sets[PHT_index].ways[PHT_way].offset == PHT_offset) {
            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t Found PHT Index %u - Way %u\n", PHT_index, PHT_way )
            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t PHT PC %"PRIu64" - Offset %"PRIu64" - Last Access %"PRIu64"\n", this->DSBP_PHT_sets[PHT_index].ways[PHT_way].pc,
                                                                                                    this->DSBP_PHT_sets[PHT_index].ways[PHT_way].offset,
                                                                                                    this->DSBP_PHT_sets[PHT_index].ways[PHT_way].last_access )
            return &this->DSBP_PHT_sets[PHT_index].ways[PHT_way];
        }
    }
    return NULL;
}

// =============================================================================
DSBP_PHT_line_t* line_usage_predictor_t::DSBP_PHT_evict_address(uint64_t pc, uint64_t memory_address) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("DSBP_PHT_evict_address()\n")
    uint32_t PHT_offset = memory_address & sinuca_engine.get_global_offset_bits_mask();
    uint32_t PHT_index = pc & this->DSBP_PHT_index_bits_mask;

    ERROR_ASSERT_PRINTF(PHT_offset < sinuca_engine.get_global_line_size(), "Wrong offset %d > line_size %d", PHT_offset, sinuca_engine.get_global_line_size());
    ERROR_ASSERT_PRINTF(PHT_index < this->DSBP_PHT_total_sets, "Wrong index %d > total_sets %d", PHT_index, this->DSBP_PHT_total_sets);

    DSBP_PHT_line_t *choosen_line = NULL;

    switch (this->DSBP_PHT_replacement_policy) {
        case REPLACEMENT_LRU: {
            uint64_t last_access = sinuca_engine.get_global_cycle() + 1;
            for (uint32_t PHT_way = 0; PHT_way < this->get_DSBP_PHT_associativity(); PHT_way++) {
                /// If the line is LRU
                if (this->DSBP_PHT_sets[PHT_index].ways[PHT_way].last_access <= last_access) {
                    choosen_line = &this->DSBP_PHT_sets[PHT_index].ways[PHT_way];
                    last_access = this->DSBP_PHT_sets[PHT_index].ways[PHT_way].last_access;
                }
            }
        }
        break;

        case REPLACEMENT_RANDOM: {
            /// Initialize random seed
            unsigned int seed = time(NULL);
            /// Generate random number
            uint32_t PHT_way = (rand_r(&seed) % this->get_DSBP_PHT_associativity());
            choosen_line = &this->DSBP_PHT_sets[PHT_index].ways[PHT_way];
        }
        break;

        case REPLACEMENT_FIFO:
            ERROR_PRINTF("Replacement Policy: REPLACEMENT_POLICY_FIFO not implemented.\n");
        break;

        case REPLACEMENT_LRF:
            ERROR_PRINTF("Replacement Policy: REPLACEMENT_POLICY_LRF not implemented.\n");
        break;

        case REPLACEMENT_LRU_DSBP:
            ERROR_PRINTF("Replacement Policy: REPLACEMENT_POLICY_LRU_DSBP should not use for line_usage_predictor.\n");
        break;
    }

    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t PHT PC %"PRIu64" - Offset %"PRIu64" - Last Access %"PRIu64"\n", choosen_line->pc,
                                                                                                        choosen_line->offset,
                                                                                                        choosen_line->last_access )
    return choosen_line;
};

// =============================================================================
std::string line_usage_predictor_t::DSBP_PHT_line_to_string(DSBP_PHT_line_t *PHT_line) {
    std::string PackageString;
    PackageString = "";

    PackageString = PackageString + "PHT_LINE PC:" + utils_t::uint64_to_char(PHT_line->pc);
    PackageString = PackageString + " Offset:" + utils_t::uint32_to_char(PHT_line->offset);
    PackageString = PackageString + " Last Access:" + utils_t::uint64_to_char(PHT_line->last_access);
    PackageString = PackageString + " Has Pointer:" + utils_t::uint32_to_char(PHT_line->pointer);

    PackageString = PackageString + "\n\t Usage Counter [";
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        if (i % this->DSBP_sub_block_size == 0) {
            PackageString = PackageString + "|";
        }
        PackageString = PackageString + " " + utils_t::uint32_to_char(PHT_line->usage_counter[i]);
    }
    PackageString = PackageString + "]\n";

    PackageString = PackageString + "\t Overflow      [";
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        if (i % this->DSBP_sub_block_size == 0) {
            PackageString = PackageString + "|";
        }
        PackageString = PackageString + " " + utils_t::uint32_to_char(PHT_line->overflow[i]);
    }
    PackageString = PackageString + "]\n";
    return PackageString;
};


