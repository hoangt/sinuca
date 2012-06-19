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

    this->DSBP_sub_block_size = 0;
    this->DSBP_sub_block_total = 0;
    this->DSBP_usage_counter_bits = 0;


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

            this->DSBP_usage_counter_max = pow(2, this->DSBP_usage_counter_bits);

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("Allocate %s DSBP %d(lines) / %d(assoc) = %d (sets) (%d (sub-blocks))\n", this->get_label(), this->get_DSBP_line_number(), this->get_DSBP_associativity(), this->get_DSBP_total_sets(), this->get_DSBP_sub_block_total());
            this->DSBP_sets = utils_t::template_allocate_array<DSBP_metadata_sets_t>(this->get_DSBP_total_sets());
            for (uint32_t i = 0; i < this->get_DSBP_total_sets(); i++) {
                this->DSBP_sets[i].ways = utils_t::template_allocate_array<DSBP_metadata_line_t>(this->get_DSBP_associativity());

                for (uint32_t j = 0; j < this->get_DSBP_associativity(); j++) {
                    this->DSBP_sets[i].ways[j].valid_sub_blocks = utils_t::template_allocate_initialize_array<line_sub_block_t>(sinuca_engine.get_global_line_size(), LINE_SUB_BLOCK_DISABLE);
                    this->DSBP_sets[i].ways[j].real_usage_counter = utils_t::template_allocate_initialize_array<uint32_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_sets[i].ways[j].usage_counter = utils_t::template_allocate_initialize_array<uint32_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_sets[i].ways[j].overflow = utils_t::template_allocate_initialize_array<bool>(sinuca_engine.get_global_line_size(), false);
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

                    this->DSBP_PHT_sets[i].ways[j].usage_counter = utils_t::template_allocate_initialize_array<uint32_t>(sinuca_engine.get_global_line_size(), 0);
                    this->DSBP_PHT_sets[i].ways[j].overflow = utils_t::template_allocate_initialize_array<bool>(sinuca_engine.get_global_line_size(), false);
                }
            }

            /// ================================================================
            /// Statistics
            /// ================================================================


        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
            ERROR_PRINTF("Invalid line usage predictor strategy %s.\n", get_enum_line_usage_predictor_policy_char(this->get_line_usage_predictor_type()));
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
        break;
    }
};

//==============================================================================
void line_usage_predictor_t::clock(uint32_t subcycle) {
    if (subcycle != 0) return;
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("==================== ");
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("====================\n");
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("cycle() \n");

    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP:
        break;

        case LINE_USAGE_PREDICTOR_POLICY_SPP:
        break;

        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:
        break;
    }
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
    this->DBPP_stat_line_sub_block_disable_always = 0;
    this->DBPP_stat_line_sub_block_disable_turnoff = 0;
    this->DBPP_stat_line_sub_block_normal_correct = 0;
    this->DBPP_stat_line_sub_block_normal_over = 0;
    this->DBPP_stat_line_sub_block_learn = 0;
    this->DBPP_stat_line_sub_block_wrong_first = 0;
};

// =============================================================================
void line_usage_predictor_t::print_statistics() {
    char title[100] = "";
    sprintf(title, "Statistics of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DBPP_stat_line_sub_block_disable_always", DBPP_stat_line_sub_block_disable_always);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DBPP_stat_line_sub_block_disable_turnoff", DBPP_stat_line_sub_block_disable_turnoff);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DBPP_stat_line_sub_block_normal_correct", DBPP_stat_line_sub_block_normal_correct);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DBPP_stat_line_sub_block_normal_over", DBPP_stat_line_sub_block_normal_over);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DBPP_stat_line_sub_block_learn", DBPP_stat_line_sub_block_learn);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DBPP_stat_line_sub_block_wrong_first", DBPP_stat_line_sub_block_wrong_first);

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
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_sub_block_size", DSBP_sub_block_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_sub_block_total", DSBP_sub_block_total);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "DSBP_usage_counter_bits", DSBP_usage_counter_bits);

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
// METHODS
// =============================================================================
// Input:   base_address, size
// Output:  start_sub_block, end_Sub_block
void line_usage_predictor_t::get_start_end_sub_blocks(uint64_t base_address, uint32_t size, uint32_t& sub_block_ini, uint32_t& sub_block_end) {
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
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            /// Compute the START and END sub_blocks
            uint32_t sub_block_ini, sub_block_end;
            this->get_start_end_sub_blocks(package->memory_address, package->memory_size, sub_block_ini, sub_block_end);

            /// Generates the Request Vector
            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t 0x%"PRIu64"(%"PRIu64"/%u) sub_blocks[", package->memory_address, package->memory_address & sinuca_engine.get_global_offset_bits_mask(), package->memory_size)
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                #ifdef LINE_USAGE_PREDICTOR_DEBUG
                    if (i % this->DSBP_sub_block_size == 0) SINUCA_PRINTF("|")
                    SINUCA_PRINTF("%u ", ( i >= sub_block_ini && i < sub_block_end ))

                #endif
                package->sub_blocks[i] = ( i >= sub_block_ini && i < sub_block_end );
            }
            #ifdef LINE_USAGE_PREDICTOR_DEBUG
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

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t valid_sub_blocks[")
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                #ifdef LINE_USAGE_PREDICTOR_DEBUG
                    if (i % this->DSBP_sub_block_size == 0) SINUCA_PRINTF("|")
                    SINUCA_PRINTF("%u ", this->DSBP_sets[index].ways[way].valid_sub_blocks[i])
                #endif

                if (package->sub_blocks[i] ==  true &&
                this->DSBP_sets[index].ways[way].valid_sub_blocks[i] == LINE_SUB_BLOCK_DISABLE) {
                    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t sub_blocks MISS\n")
                    return false;
                }
            }
            #ifdef LINE_USAGE_PREDICTOR_DEBUG
                SINUCA_PRINTF("]\n")
            #endif

            LINE_USAGE_PREDICTOR_DEBUG_PRINTF("\t sub_blocks HIT\n")
            return true;
        }
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

            // Update the METADATA real_usage_counter
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                this->DSBP_sets[index].ways[way].real_usage_counter[i] += package->sub_blocks[i];
            }

            // METADATA Learn Mode
            if (this->DSBP_sets[index].ways[way].learn_mode == true) {
                // Has PHT pointer
                if (this->DSBP_sets[index].ways[way].PHT_pointer != NULL) {
                    // Update the PHT
                    this->DSBP_sets[index].ways[way].PHT_pointer->last_access = sinuca_engine.get_global_cycle();
                    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                        // Check the PHT Overflow
                        if (this->DSBP_sets[index].ways[way].PHT_pointer->usage_counter[i] >= this->DSBP_usage_counter_max) {
                            this->DSBP_sets[index].ways[way].PHT_pointer->overflow[i] = true;
                        }
                        // Update the PHT usage counter
                        else {
                            this->DSBP_sets[index].ways[way].PHT_pointer->usage_counter[i]++;
                        }
                    }
                }
            }
            // METADATA Not Learn Mode
            else {
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    // METADATA Not Overflow + METADATA Used Predicted Number of Times
                    if (this->DSBP_sets[index].ways[way].overflow[i] == false &&
                    this->DSBP_sets[index].ways[way].real_usage_counter[i] >= this->DSBP_sets[index].ways[way].usage_counter[i]) {
                        // METADATA Turn off the sub_blocks
                        this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_DISABLE;
                    }
                }
            }
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
// Collateral Effect: Change the package->sub_blocks[]
void line_usage_predictor_t::line_miss(memory_package_t *package, uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("line_miss() package:%s\n", package->memory_to_string().c_str())
    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            DSBP_PHT_line_t *PHT_line = this->DSBP_PHT_find_line(package->opcode_address, package->memory_address);
            // PHT HIT
            if (PHT_line != NULL) {
                // Disable Learn_mode
                this->DSBP_sets[index].ways[way].learn_mode = false;
                // If no PHT_pointer
                if (PHT_line->pointer == false) {
                    // Create a PHT pointer
                    PHT_line->pointer = true;
                    this->DSBP_sets[index].ways[way].PHT_pointer = PHT_line;
                }
                // Copy the PHT usage prediction
                package->memory_size = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    // Erase the real_usage_counter
                    this->DSBP_sets[index].ways[way].real_usage_counter[i] = 0;
                    // Copy the PHT prediction
                    this->DSBP_sets[index].ways[way].usage_counter[i] = PHT_line->usage_counter[i];
                    this->DSBP_sets[index].ways[way].overflow[i] = PHT_line->overflow[i];
                    // Modify the package->sub_blocks (request prediction)
                    package->sub_blocks[i] |= PHT_line->usage_counter[i] != 0;
                    // Enable Valid Sub_blocks
                    if (package->sub_blocks[i]) {
                        // Fix the package size
                        package->memory_size++;
                        this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_NORMAL;
                    }
                }
            }
            // PHT MISS
            else {
                // New PHT entry
                PHT_line = DSBP_PHT_evict_address(package->opcode_address, package->memory_address);
                // Clean the PHT entry
                PHT_line->pc = package->opcode_address;
                PHT_line->offset = package->memory_address & sinuca_engine.get_global_offset_bits_mask();
                PHT_line->last_access = sinuca_engine.get_global_cycle();
                PHT_line->pointer = true;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    PHT_line->usage_counter[i] = 0;
                    PHT_line->overflow[i] = false;
                }

                // Enable Learn_mode
                this->DSBP_sets[index].ways[way].learn_mode = true;
                this->DSBP_sets[index].ways[way].PHT_pointer = PHT_line;
                // Clean the metadata entry
                package->memory_size = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    this->DSBP_sets[index].ways[way].real_usage_counter[i] = 0;
                    this->DSBP_sets[index].ways[way].usage_counter[i] = 0;
                    this->DSBP_sets[index].ways[way].overflow[i] = false;
                    // Modify the package->sub_blocks (request all)
                    package->sub_blocks[i] = true;
                    // Fix the package size
                    package->memory_size++;
                    // Enable Valid Sub_blocks
                    this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_LEARN;
                }
            }

            // Add the last access information
            this->line_hit(package, index, way);
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
// Collateral Effect: Change the package->sub_blocks[]
void line_usage_predictor_t::sub_block_miss(memory_package_t *package, uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("sub_block_miss() package:%s\n", package->memory_to_string().c_str())
    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            DSBP_PHT_line_t *PHT_line = this->DSBP_sets[index].ways[way].PHT_pointer;
            // PHT HIT
            if (PHT_line != NULL) {
                // Enable Learn_mode
                this->DSBP_sets[index].ways[way].learn_mode = true;
                PHT_line->pointer = true;

                package->memory_size = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    // Update the PHT entry
                    if (this->DSBP_sets[index].ways[way].real_usage_counter[i] < this->DSBP_usage_counter_max) {
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
                    // Enable Valid Sub_blocks
                    if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] == LINE_SUB_BLOCK_DISABLE) {
                        // Modify the package->sub_blocks (request all missing)
                        package->sub_blocks[i] = true;
                        // Fix the package size
                        package->memory_size++;
                        this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_WRONG_FIRST;
                    }
                    else {
                        package->sub_blocks[i] = false;
                    }
                }
            }
            // PHT MISS
            else {
                // Enable Learn_mode
                this->DSBP_sets[index].ways[way].learn_mode = true;
                // Enable all sub_blocks
                package->memory_size = 0;
                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    this->DSBP_sets[index].ways[way].usage_counter[i] = 0;
                    this->DSBP_sets[index].ways[way].overflow[i] = true;
                    // Enable Valid Sub_blocks
                    if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] == LINE_SUB_BLOCK_DISABLE) {
                        // Modify the package->sub_blocks (request all missing)
                        package->sub_blocks[i] = true;
                        // Fix the package size
                        package->memory_size++;
                        this->DSBP_sets[index].ways[way].valid_sub_blocks[i] = LINE_SUB_BLOCK_WRONG_FIRST;
                    }
                    else {
                        package->sub_blocks[i] = false;
                    }
                }
            }

            // Add the last access information
            this->line_hit(package, index, way);
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
void line_usage_predictor_t::line_copy_back(memory_package_t *package, uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("line_copy_back() package:%s\n", package->memory_to_string().c_str())
    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            // Modify the package->sub_blocks (valid_sub_blocks)
            package->memory_size = 0;
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                if (this->DSBP_sets[index].ways[way].valid_sub_blocks[i] != LINE_SUB_BLOCK_DISABLE) {
                    package->sub_blocks[i] = true;
                    // Fix the package size
                    package->memory_size++;
                }
                else {
                    package->sub_blocks[i] = false;
                }
            }
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
void line_usage_predictor_t::line_eviction(uint32_t index, uint32_t way) {
    LINE_USAGE_PREDICTOR_DEBUG_PRINTF("line_eviction()\n")
    switch (this->get_line_usage_predictor_type()) {
        case LINE_USAGE_PREDICTOR_POLICY_DSBP: {
            ERROR_ASSERT_PRINTF(index < this->DSBP_total_sets, "Wrong index %d > total_sets %d", index, this->DSBP_total_sets);
            ERROR_ASSERT_PRINTF(way < this->DSBP_associativity, "Wrong way %d > associativity %d", way, this->DSBP_associativity);

            DSBP_PHT_line_t *PHT_line = this->DSBP_sets[index].ways[way].PHT_pointer;
            // PHT HIT
            if (PHT_line != NULL) {
                PHT_line->pointer = true;

                for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                    // Update the PHT entry
                    if (this->DSBP_sets[index].ways[way].real_usage_counter[i] < this->DSBP_usage_counter_max) {
                        PHT_line->usage_counter[i] = this->DSBP_sets[index].ways[way].real_usage_counter[i];
                        PHT_line->overflow[i] = false;
                    }
                    else {
                        PHT_line->usage_counter[i] = this->DSBP_usage_counter_max;
                        PHT_line->overflow[i] = true;
                    }
                }
            }

            // Statistics
            for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
                switch (this->DSBP_sets[index].ways[way].valid_sub_blocks[i]) {
                    case LINE_SUB_BLOCK_DISABLE:
                        if (this->DSBP_sets[index].ways[way].usage_counter[i] == 0) {
                            this->DBPP_stat_line_sub_block_disable_always++;
                        }
                        else {
                            this->DBPP_stat_line_sub_block_disable_turnoff++;
                        }
                    break;

                    case LINE_SUB_BLOCK_NORMAL:
                        if (this->DSBP_sets[index].ways[way].overflow[i] == 1) {
                            this->DBPP_stat_line_sub_block_normal_correct++;
                        }
                        else {
                            this->DBPP_stat_line_sub_block_normal_over++;
                        }
                    break;

                    case LINE_SUB_BLOCK_LEARN:
                        this->DBPP_stat_line_sub_block_learn++;
                    break;

                    case LINE_SUB_BLOCK_WRONG_FIRST:
                        this->DBPP_stat_line_sub_block_wrong_first++;
                    break;
                }
            }
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
// DSBP - PHT
// =============================================================================
DSBP_PHT_line_t* line_usage_predictor_t::DSBP_PHT_find_line(uint64_t pc, uint64_t memory_address) {
    uint32_t PHT_offset = memory_address & sinuca_engine.get_global_offset_bits_mask();
    uint32_t PHT_index = pc & this->DSBP_PHT_index_bits_mask;

    ERROR_ASSERT_PRINTF(PHT_offset < sinuca_engine.get_global_line_size(), "Wrong offset %d > line_size %d", PHT_offset, sinuca_engine.get_global_line_size());
    ERROR_ASSERT_PRINTF(PHT_index < this->DSBP_PHT_total_sets, "Wrong index %d > total_sets %d", PHT_index, this->DSBP_PHT_total_sets);


    for (uint32_t PHT_way = 0; PHT_way < this->get_DSBP_PHT_associativity(); PHT_way++) {
        if (this->DSBP_PHT_sets[PHT_index].ways[PHT_way].pc == pc && this->DSBP_PHT_sets[PHT_index].ways[PHT_way].offset == PHT_offset) {
            return &this->DSBP_PHT_sets[PHT_index].ways[PHT_way];
        }
    }
    return NULL;
}

// =============================================================================
DSBP_PHT_line_t* line_usage_predictor_t::DSBP_PHT_evict_address(uint64_t pc, uint64_t memory_address) {
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
    }

    return choosen_line;
};
