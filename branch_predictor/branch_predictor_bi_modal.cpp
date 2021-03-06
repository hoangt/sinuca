/*
 * Copyright (C) 2010~2014  Marco Antonio Zanata Alves
 *                          (mazalves at inf.ufrgs.br)
 *                          GPPD - Parallel and Distributed Processing Group
 *                          Universidade Federal do Rio Grande do Sul
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../sinuca.hpp"

#ifdef BRANCH_PREDICTOR_DEBUG
    #define BRANCH_PREDICTOR_DEBUG_PRINTF(...) DEBUG_PRINTF(__VA_ARGS__);
#else
    #define BRANCH_PREDICTOR_DEBUG_PRINTF(...)
#endif

// ============================================================================
branch_predictor_bi_modal_t::branch_predictor_bi_modal_t() {
    this->branch_predictor_bi_modal_type = BRANCH_PREDICTOR_BI_MODAL;

    this->btb = NULL;
    this->btb_line_number = 0;
    this->btb_associativity = 0;
    this->btb_total_sets = 0;
    this->btb_replacement_policy = REPLACEMENT_LRU;

    this->btb_index_bits_mask = 0;
    this->btb_tag_bits_mask = 0;

    this->bht = NULL;
    this->bht_line_number = 0;

    this->fsm_bits = 0;
    this->fsm_max_counter = 0;
    this->fsm_taken_threshold = 0;
};

// ============================================================================
branch_predictor_bi_modal_t::~branch_predictor_bi_modal_t() {
    /// De-Allocate memory to prevent memory leak
    utils_t::template_delete_array<branch_target_buffer_set_t>(btb);
    utils_t::template_delete_array<uint32_t>(bht);
};

// ============================================================================
void branch_predictor_bi_modal_t::allocate() {
    branch_predictor_t::allocate();

    ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(this->get_btb_line_number() / this->get_btb_associativity()), "BTB Wrong line number(%u) or associativity(%u).\n", this->get_btb_line_number(), this->get_btb_associativity());
    ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(this->get_bht_line_number()), "BHT Wrong line number(%u).\n", this->get_bht_line_number());

    uint32_t i;


    this->set_btb_total_sets(this->get_btb_line_number() / this->get_btb_associativity());

    this->btb = utils_t::template_allocate_array<branch_target_buffer_set_t>(this->get_btb_total_sets());
    for (i = 0; i < this->get_btb_total_sets(); i++) {
        this->btb[i].ways = utils_t::template_allocate_array<branch_target_buffer_line_t>(this->get_btb_associativity());
    }

    /// BTB INDEX MASK
    this->btb_index_bits_shift = 0;
    for (i = 0; i < utils_t::get_power_of_two(this->get_btb_total_sets()); i++) {
        this->btb_index_bits_mask |= 1 << (i + btb_index_bits_shift);
    }

    /// BTB TAG MASK
    this->btb_tag_bits_shift = btb_index_bits_shift + utils_t::get_power_of_two(this->get_btb_total_sets());
    for (i = btb_tag_bits_shift; i < utils_t::get_power_of_two((uint64_t)INT64_MAX+1); i++) {
        this->btb_tag_bits_mask |= 1 << i;
    }

    // ========================================================================
    /// BHT FSM MASK
    for (i = 0; i < this->get_fsm_bits(); i++) {
        this->fsm_max_counter |= 1 << i;
    }
    this->fsm_taken_threshold = this->get_fsm_max_counter() / 2;



    /// BHT INDEX MASK
    for (i = 0; i < utils_t::get_power_of_two(this->get_btb_line_number()); i++) {
        this->bht_index_bits_mask |= 1 << i;
    }

    /// Allocate and initialize the BHT with WEAKLY TAKEN
    this->bht = utils_t::template_allocate_initialize_array<uint32_t>(this->get_btb_line_number(), this->get_fsm_taken_threshold());
};


// ============================================================================
uint32_t branch_predictor_bi_modal_t::btb_evict_address(uint64_t opcode_address) {
    uint64_t index = btb_get_index(opcode_address >> 2);
    uint32_t way = 0;
    uint32_t selected = 0;

    switch (this->btb_replacement_policy) {
        case REPLACEMENT_LRU: {
            uint64_t last_access = UINT64_MAX;
            for (way = 0; way < this->get_btb_associativity(); way++) {
                /// If there is free space
                if (this->btb[index].ways[way].last_access <= last_access) {
                    selected = way;
                    last_access = this->btb[index].ways[way].last_access;
                }
            }
            break;
        }
        case REPLACEMENT_RANDOM: {
            // initialize random seed
            unsigned int seed = time(NULL);
            // generate random number
            selected = (rand_r(&seed) % this->get_btb_associativity());
        }
        break;

        case REPLACEMENT_INVALID_OR_LRU:
            ERROR_PRINTF("Replacement Policy: REPLACEMENT_INVALID_OR_LRU not implemented.\n");
        break;

        case REPLACEMENT_FIFO:
            ERROR_PRINTF("Replacement Policy: REPLACEMENT_POLICY_FIFO not implemented.\n");
        break;

        case REPLACEMENT_LRF:
            ERROR_PRINTF("Replacement Policy: REPLACEMENT_POLICY_LRF not implemented.\n");
        break;

        case REPLACEMENT_DEAD_OR_LRU:
            ERROR_PRINTF("Replacement Policy: REPLACEMENT_DEAD_OR_LRU should not use for branch_prediction.\n");
        break;

    }

    return selected;
};


// ============================================================================
bool branch_predictor_bi_modal_t::btb_find_update_address(uint64_t opcode_address) {
    uint64_t index = btb_get_index(opcode_address >> 2);
    uint64_t tag = btb_get_tag(opcode_address >> 2);
    uint32_t way = 0;

    this->add_stat_btb_accesses();

    ERROR_ASSERT_PRINTF(index < this->get_btb_total_sets(), "Index >= btb_total_sets \n")
    for (way = 0 ; way < this->get_btb_associativity() ; way++) {
        /// BTB HIT
        if (this->btb[index].ways[way].tag == tag) {
            this->btb[index].ways[way].last_access = sinuca_engine.get_global_cycle();
            this->btb[index].ways[way].usage_counter++;
            this->add_stat_btb_hit();
            return OK;
        }
    }

    /// BTB MISS
    way = this->btb_evict_address(opcode_address);
    this->btb[index].ways[way].tag = tag;
    this->btb[index].ways[way].last_access = sinuca_engine.get_global_cycle();
    this->btb[index].ways[way].usage_counter = 1;
    this->add_stat_btb_miss();
    return FAIL;
};

// ============================================================================
bool branch_predictor_bi_modal_t::bht_find_update_prediction(const opcode_package_t& actual_opcode, const opcode_package_t& next_opcode) {
    /// Get the prediction
    uint32_t bht_index = this->bht_index_bits_mask & actual_opcode.opcode_address >> 2;
    bool bht_taken = false;
    if (this->bht[bht_index] >= this->fsm_taken_threshold) {   /// Get the BHT prediction
        bht_taken = true;
    }

    /// Update the prediction
    uint64_t next_sequential_address = actual_opcode.opcode_address + actual_opcode.opcode_size;
    bool is_taken = (next_sequential_address != next_opcode.opcode_address);

    if (is_taken && this->bht[bht_index] < this->fsm_max_counter) {     /// Update the BHT prediction (TAKEN)
        this->bht[bht_index]++;
    }
    if (!is_taken && int32_t(this->bht[bht_index]) > 0) {                 /// Update the BHT prediction (NOT TAKEN)
        this->bht[bht_index]--;
    }

    return bht_taken;
};

// ============================================================================
/// 1st. Predict if it is a branch or normal instruction
/// 2nd. Predict the target address
// =================================
/// CASE 1: Branch (Not Predicted as Branch)    - Static on Decode (Not Predict the target)     - STALL UNTIL EXECUTE
/// CASE 2: Branch (Not Predicted as Branch)    - Static on Decode (Predict the target)         - STALL UNTIL DECODE

/// CASE 3: Branch (Predicted as Branch)        - Two Level Predictor (Not Predict the target)  - STALL UNTIL EXECUTE
/// CASE 4: Branch (Predicted as Branch)        - Two Level Predictor (Predict the target)      - OK

/// CASE 5: Not Branch (Not Predicted as Branch)                                                - OK
/// CASE 6: Not Branch (Predicted as Branch)                                                    - NEVER HAPPENS
processor_stage_t branch_predictor_bi_modal_t::predict_branch(const opcode_package_t& actual_opcode, const opcode_package_t& next_opcode) {
    processor_stage_t solve_stage = PROCESSOR_STAGE_FETCH;

    if (actual_opcode.opcode_operation != INSTRUCTION_OPERATION_BRANCH) {
        solve_stage = PROCESSOR_STAGE_FETCH;
    }
    else {
        uint64_t next_sequential_address = actual_opcode.opcode_address + actual_opcode.opcode_size;
        bool is_taken = (next_sequential_address != next_opcode.opcode_address);
        bool is_taken_bht = is_taken;

        BRANCH_PREDICTOR_DEBUG_PRINTF("BRANCH OPCODE FOUND TAKEN?(%d) - ", is_taken);

        /// Checks for Conditional and Unconditional Branches
        bool is_btb_hit = this->btb_find_update_address(actual_opcode.opcode_address);

        /// Only update the BHT for Conditional Branches
        if (actual_opcode.is_conditional) {
            is_taken_bht = this->bht_find_update_prediction(actual_opcode, next_opcode);
            add_stat_branch_predictor_conditional();
        }
        else {
            add_stat_branch_predictor_unconditional();
        }

        if (is_btb_hit) {
            BRANCH_PREDICTOR_DEBUG_PRINTF("BTB FOUND - ");

            if (!actual_opcode.is_conditional || is_taken_bht == is_taken) {
                BRANCH_PREDICTOR_DEBUG_PRINTF("CORRECT PREDICTED => PROCESSOR_STAGE_FETCH\n");
                solve_stage = PROCESSOR_STAGE_FETCH;
            }
            else {
                BRANCH_PREDICTOR_DEBUG_PRINTF("MISS PREDICTED => PROCESSOR_STAGE_EXECUTION\n");
                solve_stage = PROCESSOR_STAGE_EXECUTION;
            }
        }
        else {
            BRANCH_PREDICTOR_DEBUG_PRINTF("BTB NOT FOUND => PROCESSOR_STAGE_EXECUTION\n");
            /// If NOT TAKEN, it will not generate extra latency
            if (!is_taken){
                solve_stage = PROCESSOR_STAGE_FETCH;
            }
            else {
                solve_stage = PROCESSOR_STAGE_EXECUTION;
            }
        }

        /// Increment the statistics
        this->add_stat_branch_predictor_operation();
        if (solve_stage == PROCESSOR_STAGE_FETCH) {
            this->add_stat_branch_predictor_hit();
        }
        else {
            this->add_stat_branch_predictor_miss();
        }
    }

    return solve_stage;
};

// ============================================================================
void branch_predictor_bi_modal_t::print_structures() {
    branch_predictor_t::print_structures();
};

// ============================================================================
void branch_predictor_bi_modal_t::panic() {
    branch_predictor_t::panic();

    this->print_structures();
};

// ============================================================================
void branch_predictor_bi_modal_t::periodic_check(){
    branch_predictor_t::periodic_check();

    #ifdef BRANCH_PREDICTOR_DEBUG
        this->print_structures();
    #endif
};

// ============================================================================
/// STATISTICS
// ============================================================================
void branch_predictor_bi_modal_t::reset_statistics() {
    branch_predictor_t::reset_statistics();

    this->set_stat_btb_accesses(0);
    this->set_stat_btb_hit(0);
    this->set_stat_btb_miss(0);
};

// ============================================================================
void branch_predictor_bi_modal_t::print_statistics() {
    branch_predictor_t::print_statistics();

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_btb_accesses", stat_btb_accesses);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_btb_hit", stat_btb_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_btb_miss", stat_btb_miss);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_btb_miss_ratio", stat_btb_miss, stat_btb_accesses);

};

// ============================================================================
// ============================================================================
void branch_predictor_bi_modal_t::print_configuration() {
    branch_predictor_t::print_configuration();

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "btb_line_number", btb_line_number);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "btb_associativity", btb_associativity);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "btb_total_sets", btb_total_sets);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "btb_replacement_policy", get_enum_replacement_char(btb_replacement_policy));
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "btb_tag_bits_mask", utils_t::address_to_binary(btb_tag_bits_mask).c_str());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "btb_index_bits_mask", utils_t::address_to_binary(btb_index_bits_mask).c_str());

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "bht_line_number", bht_line_number);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "bht_index_bits_mask", utils_t::address_to_binary(bht_index_bits_mask).c_str());

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "fsm_bits", fsm_bits);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "fsm_max_counter", utils_t::address_to_binary(fsm_max_counter).c_str());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "fsm_taken_threshold", utils_t::address_to_binary(fsm_taken_threshold).c_str());
};
