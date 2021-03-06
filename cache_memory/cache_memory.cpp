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

#ifdef CACHE_DEBUG
    #define CACHE_DEBUG_PRINTF(...) DEBUG_PRINTF(__VA_ARGS__);
#else
    #define CACHE_DEBUG_PRINTF(...)
#endif

// ============================================================================
cache_memory_t::cache_memory_t() {
    this->set_type_component(COMPONENT_CACHE_MEMORY);

    this->prefetcher = NULL;
    this->line_usage_predictor = NULL;

    this->cache_id = 0;
    this->bank_number = 0;
    this->total_banks = 0;
    this->address_mask_type = CACHE_MASK_TAG_INDEX_OFFSET;

    this->hierarchy_level = 0;
    this->line_size = 0;
    this->line_number = 0;
    this->associativity = 0;
    this->replacement_policy = REPLACEMENT_LRU;

    this->penalty_read = 0;
    this->penalty_write = 0;

    this->mshr_request_buffer_size = 0;
    this->mshr_prefetch_buffer_size = 0;
    this->mshr_write_buffer_size = 0;
    this->mshr_eviction_buffer_size = 0;

    this->mshr_tokens_request = NULL;
    this->mshr_tokens_write = NULL;
    this->mshr_tokens_prefetch = NULL;

    this->got_request_token = false;
    this->got_prefetch_token = false;
    this->got_write_token = false;

    this->total_sets = 0;
    this->sets = NULL;

    this->mshr_buffer = NULL;  /// Buffer of Missed Requests
    this->mshr_buffer_size = 0;

    this->lower_level_cache = new container_ptr_cache_memory_t;
    this->higher_level_cache = new container_ptr_cache_memory_t;

    this->send_ans_ready_cycle = 0;
    this->send_rqst_ready_cycle = 0;

    this->recv_ans_ready_cycle = 0;
    this->recv_rqst_read_ready_cycle = 0;
    this->recv_rqst_write_ready_cycle = 0;
};

// ============================================================================
cache_memory_t::~cache_memory_t() {
    /// De-Allocate memory to prevent memory leak
    utils_t::template_delete_variable<prefetch_t>(prefetcher);
    utils_t::template_delete_variable<line_usage_predictor_t>(line_usage_predictor);

    utils_t::template_delete_array<cache_set_t>(sets);
    utils_t::template_delete_array<memory_package_t>(mshr_buffer);

    utils_t::template_delete_array<int32_t>(mshr_tokens_request);
    utils_t::template_delete_array<int32_t>(mshr_tokens_prefetch);
    utils_t::template_delete_array<int32_t>(mshr_tokens_write);

    utils_t::template_delete_variable<container_ptr_cache_memory_t>(lower_level_cache);
    utils_t::template_delete_variable<container_ptr_cache_memory_t>(higher_level_cache);
};

// ============================================================================
void cache_memory_t::allocate() {
    ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(this->get_line_number() / this->get_associativity()), "Wrong line number(%u) or associativity(%u).\n", this->get_line_number(), this->get_associativity());
    ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(this->get_line_size()), "Wrong line size.\n");

    this->set_total_sets(this->get_line_number() / this->get_associativity());
    this->set_masks();


    this->sets = utils_t::template_allocate_array<cache_set_t>(this->get_total_sets());
    for (uint32_t i = 0; i < this->get_total_sets(); i++) {
        this->sets[i].ways = utils_t::template_allocate_array<cache_line_t>(this->get_associativity());
        /// Generate a fake but valid address for each cache line
        for (uint32_t j = 0; j < this->get_associativity(); j++) {
            this->sets[i].ways[j].tag = this->get_fake_address(i, j);
        }
    }

    ERROR_ASSERT_PRINTF(mshr_request_buffer_size > 0, "mshr_request_buffer_size should be bigger than zero.\n");
    ERROR_ASSERT_PRINTF(mshr_prefetch_buffer_size > 0, "mshr_prefetch_buffer_size should be bigger than zero.\n");
    ERROR_ASSERT_PRINTF(mshr_write_buffer_size > 0, "mshr_write_buffer_size should be bigger than zero.\n");
    ERROR_ASSERT_PRINTF(mshr_eviction_buffer_size > 0, "mshr_eviction_buffer_size should be bigger than zero.\n");

    /// MSHR = [    REQUEST    | PREFETCH | WRITE | EVICT ]
    this->mshr_buffer_size = this->mshr_request_buffer_size +
                                this->mshr_prefetch_buffer_size +
                                this->mshr_write_buffer_size +
                                this->mshr_eviction_buffer_size;
    this->mshr_buffer = utils_t::template_allocate_array<memory_package_t>(this->get_mshr_buffer_size());
    this->mshr_born_ordered.reserve(this->mshr_buffer_size);

    this->mshr_tokens_request = utils_t::template_allocate_initialize_array<int32_t>(sinuca_engine.get_interconnection_interface_array_size(), -1);
    this->mshr_tokens_prefetch = utils_t::template_allocate_initialize_array<int32_t>(sinuca_engine.get_interconnection_interface_array_size(), -1);
    this->mshr_tokens_write = utils_t::template_allocate_initialize_array<int32_t>(sinuca_engine.get_interconnection_interface_array_size(), -1);


    ERROR_ASSERT_PRINTF(this->get_total_banks() == 1 || this->prefetcher->get_prefetcher_type() == PREFETCHER_DISABLE, "Cannot use a multibanked cache with prefetch. (Some requests may be generated in the wrong bank)\n");

    // ================================================================================
    /// PREFETCH
    // ================================================================================
    char label[50] = "";
    sprintf(label, "%s_PREFETCH", this->get_label());
    this->prefetcher->set_label(label);
    this->prefetcher->allocate();

    // ================================================================================
    /// LINE_USAGE_PREDICTOR
    // ================================================================================
    sprintf(label, "%s_LINE_USAGE_PREDICTOR", this->get_label());
    this->line_usage_predictor->set_label(label);
    this->line_usage_predictor->allocate();

    #ifdef CACHE_DEBUG
        this->print_configuration();
    #endif
};

// ============================================================================
void cache_memory_t::set_tokens() {

    uint32_t higher_components = 0;

    /// If L1, create tokens for the Processors
    if (this->higher_level_cache[0].empty()) {


        for (uint32_t i = 0; i < sinuca_engine.get_processor_array_size(); i++) {
            if (sinuca_engine.processor_array[i]->get_data_cache()->get_id() == this->get_id() ||
            sinuca_engine.processor_array[i]->get_inst_cache()->get_id() == this->get_id() ) {

                higher_components++;
                uint32_t id = sinuca_engine.processor_array[i]->get_id();

                this->mshr_tokens_request[id]   = this->higher_level_request_tokens;
                this->mshr_tokens_prefetch[id]  = this->higher_level_prefetch_tokens;
                this->mshr_tokens_write[id]     = this->higher_level_write_tokens;
            }
        }
    }
    /// Other cache levels
    else {
        higher_components = this->higher_level_cache[0].size();
        for (uint32_t i = 0; i < this->higher_level_cache[0].size(); i++) {
            uint32_t id = this->higher_level_cache[0][i]->get_id();

            this->mshr_tokens_request[id]   = this->higher_level_request_tokens;
            this->mshr_tokens_prefetch[id]  = this->higher_level_prefetch_tokens;
            this->mshr_tokens_write[id]     = this->higher_level_write_tokens;
        }
    }

    ERROR_ASSERT_PRINTF(this->higher_level_request_tokens * higher_components <= this->mshr_request_buffer_size,
                        "%s Allocating more REQUEST tokens than MSHR positions.\n", this->get_label())

    ERROR_ASSERT_PRINTF(this->higher_level_prefetch_tokens * higher_components <= this->mshr_prefetch_buffer_size,
                        "%s Allocating more PREFETCH tokens than MSHR positions.\n", this->get_label())

    ERROR_ASSERT_PRINTF(this->higher_level_write_tokens * higher_components <= this->mshr_write_buffer_size,
                        "%s Allocating more WRITE tokens than MSHR positions.\n", this->get_label())

};

// ============================================================================
uint64_t cache_memory_t::get_fake_address(uint32_t index, uint32_t way){

    uint64_t final_address = 0;
    switch (this->get_address_mask_type()) {
        case CACHE_MASK_TAG_INDEX_BANK_OFFSET:
            final_address = (way << this->tag_bits_shift);
            final_address += (index << this->index_bits_shift);
            final_address += (this->get_bank_number() << this->bank_bits_shift);
        break;

        case CACHE_MASK_TAG_BANK_INDEX_OFFSET:
            final_address = (way << this->tag_bits_shift);
            final_address += (this->get_bank_number() << this->bank_bits_shift);
            final_address += (index << this->index_bits_shift);
        break;

        case CACHE_MASK_TAG_INDEX_OFFSET:
            final_address = (way << this->tag_bits_shift);
            final_address += (index << this->index_bits_shift);
        break;
    }
    ERROR_ASSERT_PRINTF(index == get_index(final_address), "Wrong Index into the Fake Address.\n")
    ERROR_ASSERT_PRINTF(this->get_bank_number() == get_bank(final_address), "Wrong Bank into the Fake Address.\n")
    return final_address;

};

// ============================================================================
void cache_memory_t::set_masks() {
    ERROR_ASSERT_PRINTF(this->get_total_banks() > this->get_bank_number(), "Wrong number of banks (%u/%u).\n", this->get_bank_number(), this->get_total_banks());
    ERROR_ASSERT_PRINTF(this->get_total_sets() > 0, "Wrong number of sets (%u).\n", this->get_total_sets());

    uint64_t i;
    this->offset_bits_mask = 0;
    this->bank_bits_mask = 0;
    this->index_bits_mask = 0;
    this->tag_bits_mask = 0;

    switch (this->get_address_mask_type()) {
        case CACHE_MASK_TAG_INDEX_BANK_OFFSET:
            ERROR_ASSERT_PRINTF(this->get_total_banks() > 1 && utils_t::check_if_power_of_two(this->get_total_banks()), "Wrong number of banks (%u).\n", this->get_total_banks());

            this->offset_bits_shift = 0;
            this->bank_bits_shift = utils_t::get_power_of_two(this->get_line_size());
            this->index_bits_shift = bank_bits_shift + utils_t::get_power_of_two(this->get_total_banks());
            this->tag_bits_shift = index_bits_shift + utils_t::get_power_of_two(this->get_total_sets());

            /// OFFSET MASK
            for (i = 0; i < utils_t::get_power_of_two(this->get_line_size()); i++) {
                this->offset_bits_mask |= 1 << i;
            }
            this->not_offset_bits_mask = ~offset_bits_mask;

            /// BANK MASK
            for (i = 0; i < utils_t::get_power_of_two(this->get_total_banks()); i++) {
                this->bank_bits_mask |= 1 << (i + bank_bits_shift);
            }

            /// INDEX MASK
            for (i = 0; i < utils_t::get_power_of_two(this->get_total_sets()); i++) {
                this->index_bits_mask |= 1 << (i + index_bits_shift);
            }

            /// TAG MASK
            for (i = tag_bits_shift; i < utils_t::get_power_of_two((uint64_t)INT64_MAX+1); i++) {
                this->tag_bits_mask |= 1 << i;
            }
        break;

        case CACHE_MASK_TAG_BANK_INDEX_OFFSET:
            ERROR_ASSERT_PRINTF(this->get_total_banks() > 1 && utils_t::check_if_power_of_two(this->get_total_banks()), "Wrong number of banks (%u).\n", this->get_total_banks());

            this->offset_bits_shift = 0;
            this->index_bits_shift = utils_t::get_power_of_two(this->get_line_size());
            this->bank_bits_shift = index_bits_shift + utils_t::get_power_of_two(this->get_total_sets());
            this->tag_bits_shift = bank_bits_shift + utils_t::get_power_of_two(this->get_total_banks());

            /// OFFSET MASK
            for (i = 0; i < utils_t::get_power_of_two(this->get_line_size()); i++) {
                this->offset_bits_mask |= 1 << i;
            }
            this->not_offset_bits_mask = ~offset_bits_mask;

            /// INDEX MASK
            for (i = 0; i < utils_t::get_power_of_two(this->get_total_sets()); i++) {
                this->index_bits_mask |= 1 << (i + index_bits_shift);
            }

            /// BANK MASK
            for (i = 0; i < utils_t::get_power_of_two(this->get_total_banks()); i++) {
                this->bank_bits_mask |= 1 << (i + bank_bits_shift);
            }

            /// TAG MASK
            for (i = tag_bits_shift; i < utils_t::get_power_of_two((uint64_t)INT64_MAX+1); i++) {
                this->tag_bits_mask |= 1 << i;
            }
        break;

        case CACHE_MASK_TAG_INDEX_OFFSET:
            ERROR_ASSERT_PRINTF(this->get_total_banks() == 1 && this->get_bank_number() == 0, "Wrong number of banks (%u).\n", this->get_total_banks());

            this->offset_bits_shift = 0;
            this->bank_bits_shift = 0;
            this->index_bits_shift = utils_t::get_power_of_two(this->get_line_size());
            this->tag_bits_shift = index_bits_shift + utils_t::get_power_of_two(this->get_total_sets());

            /// OFFSET MASK
            for (i = 0; i < utils_t::get_power_of_two(this->get_line_size()); i++) {
                this->offset_bits_mask |= 1 << i;
            }
            this->not_offset_bits_mask = ~offset_bits_mask;

            /// INDEX MASK
            for (i = 0; i < utils_t::get_power_of_two(this->get_total_sets()); i++) {
                this->index_bits_mask |= 1 << (i + index_bits_shift);
            }

            /// TAG MASK
            for (i = tag_bits_shift; i < utils_t::get_power_of_two((uint64_t)INT64_MAX+1); i++) {
                this->tag_bits_mask |= 1 << i;
            }
        break;

    }
};

// ============================================================================
void cache_memory_t::clock(uint32_t subcycle) {
    (void) subcycle;
    CACHE_DEBUG_PRINTF("==================== ID(%u) ", this->get_id());
    CACHE_DEBUG_PRINTF("====================\n");
    CACHE_DEBUG_PRINTF("cycle() \n");

    /// Nothing to be done this cycle. -- Improve the performance
    if (this->mshr_born_ordered.empty() &&
    this->prefetcher->request_buffer.is_empty()) return;

    // =================================================================
    /// MSHR_BUFFER - TRANSMISSION
    // =================================================================
    /// ANSWERS
    for (uint32_t i = 0; i < this->mshr_born_ordered.size(); i++){
        if (mshr_born_ordered[i]->state == PACKAGE_STATE_TRANSMIT &&
        this->mshr_born_ordered[i]->is_answer == true &&
        this->mshr_born_ordered[i]->ready_cycle <= sinuca_engine.get_global_cycle()) {

            CACHE_DEBUG_PRINTF("\t Send ANSWER this->mshr_born_ordered[%d] %s\n", i, this->mshr_born_ordered[i]->content_to_string().c_str());
            int32_t transmission_latency = send_package(mshr_born_ordered[i]);
            if (transmission_latency != POSITION_FAIL) {
                /// PACKAGE READY !
                this->push_token_credit(this->mshr_born_ordered[i]->id_dst, this->mshr_born_ordered[i]->memory_operation);
                this->cache_wait(this->mshr_born_ordered[i]);
                this->mshr_born_ordered[i]->package_clean();
                this->mshr_born_ordered.erase(this->mshr_born_ordered.begin() + i);
            }

            break;
        }
    }

    /// REQUESTS
    for (uint32_t i = 0; i < this->mshr_born_ordered.size(); i++){
        if (mshr_born_ordered[i]->state == PACKAGE_STATE_TRANSMIT &&
        this->mshr_born_ordered[i]->is_answer == false &&
        this->mshr_born_ordered[i]->ready_cycle <= sinuca_engine.get_global_cycle()) {

            CACHE_DEBUG_PRINTF("\t Send REQUEST this->mshr_born_ordered[%d] %s\n", i, this->mshr_born_ordered[i]->content_to_string().c_str());
            int32_t transmission_latency = send_package(mshr_born_ordered[i]);
            if (transmission_latency != POSITION_FAIL) {
                this->mshr_born_ordered[i]->state = sinuca_engine.directory_controller->treat_cache_request_sent(this->get_cache_id(), this->mshr_born_ordered[i]);
                ERROR_ASSERT_PRINTF(this->mshr_born_ordered[i]->state != PACKAGE_STATE_FREE, "Must not receive back a FREE, should receive READY")
                /// Normal READ/INST/PREFETCH Request
                if (this->mshr_born_ordered[i]->state == PACKAGE_STATE_WAIT) {
                    this->mshr_born_ordered[i]->package_wait(transmission_latency);
                }
                /// Normal WRITE-BACK Request (Will free the position)
                else if (this->mshr_born_ordered[i]->state == PACKAGE_STATE_READY) {
                    /// PACKAGE READY !
                    this->cache_wait(this->mshr_born_ordered[i]);
                    this->mshr_born_ordered[i]->package_clean();
                    this->mshr_born_ordered.erase(this->mshr_born_ordered.begin() + i);
                }
            }
            break;
        }
    }


    // =================================================================
    /// MSHR_BUFFER - UNTREATED
    // =================================================================
    /// ANSWERS
    for (uint32_t i = 0; i < this->mshr_born_ordered.size(); i++){
        if (mshr_born_ordered[i]->state == PACKAGE_STATE_UNTREATED &&
        this->mshr_born_ordered[i]->is_answer == true &&
        this->mshr_born_ordered[i]->ready_cycle <= sinuca_engine.get_global_cycle()) {

            this->mshr_born_ordered[i]->state = sinuca_engine.directory_controller->treat_cache_answer(this->get_cache_id(), this->mshr_born_ordered[i]);
            ERROR_ASSERT_PRINTF(this->mshr_born_ordered[i]->state != PACKAGE_STATE_FREE, "Must not receive back a FREE, should receive READY")
            /// Could not treat, then restart born_cycle (change priority)
            if (this->mshr_born_ordered[i]->state == PACKAGE_STATE_UNTREATED) {
                this->mshr_born_ordered[i]->born_cycle = sinuca_engine.get_global_cycle();
                memory_package_t *package = this->mshr_born_ordered[i];
                this->mshr_born_ordered.erase(this->mshr_born_ordered.begin() + i);
                this->insert_mshr_born_ordered(package);
            }
            else if (this->mshr_born_ordered[i]->state == PACKAGE_STATE_READY) {
                /// PACKAGE READY !
                if (this->mshr_born_ordered[i]->id_owner != this->get_id()) {
                    this->push_token_credit(this->mshr_born_ordered[i]->id_dst, this->mshr_born_ordered[i]->memory_operation);
                }
                this->cache_wait(this->mshr_born_ordered[i]);
                this->mshr_born_ordered[i]->package_clean();
                this->mshr_born_ordered.erase(this->mshr_born_ordered.begin() + i);
            }
            break;
        }
    }

    /// REQUESTS
    for (uint32_t i = 0; i < this->mshr_born_ordered.size(); i++){
        if (mshr_born_ordered[i]->state == PACKAGE_STATE_UNTREATED &&
        this->mshr_born_ordered[i]->is_answer == false &&
        this->mshr_born_ordered[i]->ready_cycle <= sinuca_engine.get_global_cycle()) {

            this->mshr_born_ordered[i]->state = sinuca_engine.directory_controller->treat_cache_request(this->get_cache_id(), this->mshr_born_ordered[i]);
            ERROR_ASSERT_PRINTF(this->mshr_born_ordered[i]->state != PACKAGE_STATE_FREE, "Must not receive back a FREE, should receive READY")
            /// Could not treat, then restart born_cycle (change priority)
            /// If (is_answer == true) means that higher level had the cache line.
            if (this->mshr_born_ordered[i]->state == PACKAGE_STATE_UNTREATED &&
            this->mshr_born_ordered[i]->is_answer == false) {
                this->mshr_born_ordered[i]->born_cycle = sinuca_engine.get_global_cycle();
                memory_package_t *package = this->mshr_born_ordered[i];
                this->mshr_born_ordered.erase(this->mshr_born_ordered.begin() + i);
                this->insert_mshr_born_ordered(package);
            }
            else if (this->mshr_born_ordered[i]->state == PACKAGE_STATE_READY) {
                /// PACKAGE READY !
                if (this->mshr_born_ordered[i]->id_owner != this->get_id()) {
                    this->push_token_credit(this->mshr_born_ordered[i]->id_src, this->mshr_born_ordered[i]->memory_operation);
                }
                this->cache_wait(this->mshr_born_ordered[i]);
                this->mshr_born_ordered[i]->package_clean();
                this->mshr_born_ordered.erase(this->mshr_born_ordered.begin() + i);
            }

            break;
        }
    }

    // =================================================================
    /// GET FROM PREFETCHER
    // =================================================================
    if (!this->prefetcher->request_buffer.is_empty()) {
        CACHE_DEBUG_PRINTF("\t Has New Prefetch.\n");

        int32_t slot = this->allocate_prefetch(this->prefetcher->request_buffer.front());
        if (slot != POSITION_FAIL) {
            CACHE_DEBUG_PRINTF("\t\t Allocating PREFETCH into MSHR\n");

            /// Add Identity information
            this->mshr_buffer[slot].id_owner = this->get_id();
            this->mshr_buffer[slot].id_src = this->get_id();
            this->mshr_buffer[slot].id_dst = this->get_id();
            this->mshr_buffer[slot].package_untreated(0);

            this->prefetcher->request_buffer.pop_front();
        }
    }

    this->prefetcher->clock(subcycle);
};


// ============================================================================
void cache_memory_t::insert_mshr_born_ordered(memory_package_t* package){
    /// this->mshr_born_ordered            = [OLDER --------> NEWER]
    /// this->mshr_born_ordered.born_cycle = [SMALLER -----> BIGGER]

    /// Most of the insertions are made in the end !!!
    for (int32_t i = this->mshr_born_ordered.size() - 1; i >= 0 ; i--){
        if (this->mshr_born_ordered[i]->born_cycle <= package->born_cycle) {
            this->mshr_born_ordered.insert(this->mshr_born_ordered.begin() + i + 1, package);
            return;
        }
    }
    /// Could not find a older package to insert or it is just empty
    this->mshr_born_ordered.insert(this->mshr_born_ordered.begin(), package);

    /// Check the MSHR BORN ORDERED
    #ifdef CACHE_DEBUG
        uint64_t test_order = 0;
        for (uint32_t i = 0; i < this->mshr_born_ordered.size(); i++){
            if (test_order > this->mshr_born_ordered[i]->born_cycle) {
                for (uint32_t j = 0; j < this->mshr_born_ordered.size(); j++){
                    CACHE_DEBUG_PRINTF("%" PRIu64 " ", this->mshr_born_ordered[j]->born_cycle);
                }
                ERROR_ASSERT_PRINTF(test_order > this->mshr_born_ordered[i]->born_cycle, "Wrong order when inserting (%" PRIu64 ")\n", package->born_cycle);
            }
            test_order = this->mshr_born_ordered[i]->born_cycle;
        }
    #endif
};

// ============================================================================
int32_t cache_memory_t::allocate_request(memory_package_t* package){

    int32_t slot = memory_package_t::find_free(this->mshr_buffer, this->mshr_request_buffer_size);
    if (slot != POSITION_FAIL) {
        CACHE_DEBUG_PRINTF("\t NEW REQUEST\n");
        this->mshr_buffer[slot] = *package;
        this->insert_mshr_born_ordered(&this->mshr_buffer[slot]);    /// Insert into a parallel and well organized MSHR structure
    }
    else {
        add_stat_full_mshr_request_buffer();
    }
    return slot;
};


// ============================================================================
int32_t cache_memory_t::allocate_prefetch(memory_package_t* package){

    int32_t slot = memory_package_t::find_free(this->mshr_buffer +
                                                this->mshr_request_buffer_size,
                                                this->mshr_prefetch_buffer_size);
    if (slot != POSITION_FAIL) {
        slot += this->mshr_request_buffer_size;
        CACHE_DEBUG_PRINTF("\t NEW PREFETCH\n");
        this->mshr_buffer[slot] = *package;
        this->insert_mshr_born_ordered(&this->mshr_buffer[slot]);    /// Insert into a parallel and well organized MSHR structure
    }
    else {
        add_stat_full_mshr_prefetch_buffer();
    }
    return slot;
};


// ============================================================================
int32_t cache_memory_t::allocate_write(memory_package_t* package){

    int32_t slot = memory_package_t::find_free(this->mshr_buffer +
                                                this->mshr_request_buffer_size +
                                                this->mshr_prefetch_buffer_size,
                                                this->mshr_write_buffer_size);
    if (slot != POSITION_FAIL) {
        slot += this->mshr_request_buffer_size + this->mshr_prefetch_buffer_size;
        CACHE_DEBUG_PRINTF("\t NEW WRITEBACK\n");
        this->mshr_buffer[slot] = *package;
        this->insert_mshr_born_ordered(&this->mshr_buffer[slot]);    /// Insert into a parallel and well organized MSHR structure
    }
    else {
        add_stat_full_mshr_write_buffer();
    }
    return slot;
};

// ============================================================================
int32_t cache_memory_t::allocate_eviction(memory_package_t* package){

    int32_t slot = memory_package_t::find_free(this->mshr_buffer +
                                                this->mshr_request_buffer_size +
                                                this->mshr_prefetch_buffer_size +
                                                this->mshr_write_buffer_size,
                                                this->mshr_eviction_buffer_size);
    if (slot != POSITION_FAIL) {
        slot += this->mshr_request_buffer_size + this->mshr_prefetch_buffer_size + this->mshr_write_buffer_size;
        CACHE_DEBUG_PRINTF("\t NEW WRITEBACK\n");
        this->mshr_buffer[slot] = *package;
        this->insert_mshr_born_ordered(&this->mshr_buffer[slot]);    /// Insert into a parallel and well organized MSHR structure
    }
    else {
        add_stat_full_mshr_write_buffer();
    }
    return slot;
};

// ============================================================================
int32_t cache_memory_t::send_package(memory_package_t *package) {
    CACHE_DEBUG_PRINTF("send_package() package:%s\n", package->content_to_string().c_str());
    // ~ ERROR_ASSERT_PRINTF(package->memory_address != 0, "Wrong memory address.\n%s\n", package->content_to_string().c_str());

    /// NEW ANSWER
    if (package->is_answer) {
        ///Answer never checks for tokens
        /// Control Parallel Requests
        if (this->send_ans_ready_cycle <= sinuca_engine.get_global_cycle()) {
            sinuca_engine.interconnection_controller->find_package_route(package);
            ERROR_ASSERT_PRINTF(package->hop_count != POSITION_FAIL, "Achieved the end of the route\n");
            uint32_t output_port = package->hops[package->hop_count];  /// Where to send the package ?
            ERROR_ASSERT_PRINTF(output_port < this->get_max_ports(), "Output Port does not exist\n");
            package->hop_count--;  /// Consume its own port

            uint32_t transmission_latency = sinuca_engine.interconnection_controller->find_package_route_latency(package, this, this->get_interface_output_component(output_port));
            bool sent = this->get_interface_output_component(output_port)->receive_package(package, this->get_ports_output_component(output_port), transmission_latency);
            if (sent) {
                CACHE_DEBUG_PRINTF("\tSEND OK\n");
                this->send_ans_ready_cycle = sinuca_engine.get_global_cycle() + transmission_latency;
                return transmission_latency;
            }
            else {
                CACHE_DEBUG_PRINTF("\tSEND FAIL\n");
                package->hop_count++;  /// Do not Consume its own port
                return POSITION_FAIL;
            }
        }
        CACHE_DEBUG_PRINTF("\tSEND ANS DATA FAIL (BUSY)\n");
        return POSITION_FAIL;
    }
    /// NEW REQUEST
    else {
        /// Control Parallel Requests
        if (this->send_rqst_ready_cycle <= sinuca_engine.get_global_cycle()) {

            switch (package->memory_operation) {
                // HMC -> READ buffer
                case MEMORY_OPERATION_HMC_ALU:
                case MEMORY_OPERATION_HMC_ALUR:

                case MEMORY_OPERATION_READ:
                case MEMORY_OPERATION_INST:
                    if (!this->got_request_token)
                        this->got_request_token = sinuca_engine.interconnection_interface_array[package->id_dst]->pop_token_credit(this->get_id(), package->memory_operation);
                    if (!this->got_request_token) {
                        CACHE_DEBUG_PRINTF("\tSEND FAIL (NO TOKENS)\n");
                        return POSITION_FAIL;
                    }
                break;

                case MEMORY_OPERATION_PREFETCH:
                    if (!this->got_prefetch_token)
                        this->got_prefetch_token = sinuca_engine.interconnection_interface_array[package->id_dst]->pop_token_credit(this->get_id(), package->memory_operation);
                    if (!this->got_prefetch_token) {
                        CACHE_DEBUG_PRINTF("\tSEND FAIL (NO TOKENS)\n");
                        return POSITION_FAIL;
                    }

                break;

                case MEMORY_OPERATION_WRITEBACK:
                case MEMORY_OPERATION_WRITE:
                    if (!this->got_write_token)
                        this->got_write_token = sinuca_engine.interconnection_interface_array[package->id_dst]->pop_token_credit(this->get_id(), package->memory_operation);
                    if (!this->got_write_token) {
                        CACHE_DEBUG_PRINTF("\tSEND FAIL (NO TOKENS)\n");
                        return POSITION_FAIL;
                    }
                break;
            }

            sinuca_engine.interconnection_controller->find_package_route(package);
            ERROR_ASSERT_PRINTF(package->hop_count != POSITION_FAIL, "Achieved the end of the route\n");
            uint32_t output_port = package->hops[package->hop_count];  /// Where to send the package ?
            ERROR_ASSERT_PRINTF(output_port < this->get_max_ports(), "Output Port does not exist\n");
            package->hop_count--;  /// Consume its own port

            uint32_t transmission_latency = sinuca_engine.interconnection_controller->find_package_route_latency(package, this, this->get_interface_output_component(output_port));
            bool sent = this->get_interface_output_component(output_port)->receive_package(package, this->get_ports_output_component(output_port), transmission_latency);
            if (sent) {
                CACHE_DEBUG_PRINTF("\tSEND OK\n");
                switch (package->memory_operation) {
                    // HMC -> READ buffer
                    case MEMORY_OPERATION_HMC_ALU:
                    case MEMORY_OPERATION_HMC_ALUR:

                    case MEMORY_OPERATION_READ:
                    case MEMORY_OPERATION_INST:
                        this->got_request_token = false;
                    break;

                    case MEMORY_OPERATION_PREFETCH:
                        this->got_prefetch_token = false;
                    break;

                    case MEMORY_OPERATION_WRITEBACK:
                    case MEMORY_OPERATION_WRITE:
                        this->got_write_token = false;
                    break;
                }

                this->send_rqst_ready_cycle = sinuca_engine.get_global_cycle() + transmission_latency;
                return transmission_latency;
            }
            else {
                CACHE_DEBUG_PRINTF("\tSEND FAIL\n");
                package->hop_count++;  /// Do not Consume its own port
                return POSITION_FAIL;
            }
        }
        CACHE_DEBUG_PRINTF("\tSEND RQST DATA FAIL (BUSY)\n");
        return POSITION_FAIL;
    }

    CACHE_DEBUG_PRINTF("\tSEND FAIL (BUSY)\n");
    return POSITION_FAIL;
};


// ============================================================================
bool cache_memory_t::receive_package(memory_package_t *package, uint32_t input_port, uint32_t transmission_latency) {
    CACHE_DEBUG_PRINTF("receive_package() port:%u, package:%s\n", input_port, package->content_to_string().c_str());

    // ~ ERROR_ASSERT_PRINTF(package->memory_address != 0, "Wrong memory address.\n%s\n", package->content_to_string().c_str());
    ERROR_ASSERT_PRINTF(get_bank(package->memory_address) == this->get_bank_number(), "Wrong bank.\n%s\n", package->content_to_string().c_str());
    ERROR_ASSERT_PRINTF(package->id_dst == this->get_id() && package->hop_count == POSITION_FAIL, "Received some package for a different id_dst(%d).\n%s\n", this->get_id(), package->content_to_string().c_str());
    ERROR_ASSERT_PRINTF(input_port < this->get_max_ports(), "Received a wrong input_port\n%s\n", package->content_to_string().c_str());

    /// NEW ANSWER
    if (package->is_answer) {
        /// Control Parallel Requests
        if (this->recv_ans_ready_cycle <= sinuca_engine.get_global_cycle()) {
            for (uint32_t i = 0 ; i < this->mshr_buffer_size; i++) {
                /// Find the correct REQUEST which matches with the ANSWER
                if (this->mshr_buffer[i].opcode_number == package->opcode_number &&
                this->mshr_buffer[i].uop_number == package->uop_number &&
                this->cmp_tag_index_bank(this->mshr_buffer[i].memory_address, package->memory_address) &&
                this->mshr_buffer[i].state == PACKAGE_STATE_WAIT &&
                this->mshr_buffer[i].id_owner == package->id_owner) {
                    CACHE_DEBUG_PRINTF("\t RECEIVED ANSWER package WANTED\n");
                    this->mshr_buffer[i].is_answer = package->is_answer;
                    this->mshr_buffer[i].memory_size = package->memory_size;
                    this->mshr_buffer[i].package_set_src_dst(package->id_src, package->id_dst);
                    /// Consider The critical word (word originally requested by the processor) is requested first.
                    this->mshr_buffer[i].package_untreated(1);
                    this->recv_ans_ready_cycle = transmission_latency + sinuca_engine.get_global_cycle();  /// Ready to receive from HIGHER_PORT
                    return OK;
                }
            }
            ERROR_PRINTF("Receive a NOT WANTED package %s.\n", package->content_to_string().c_str())
        }
        CACHE_DEBUG_PRINTF("\tRECV DATA FAIL (BUSY)\n");
        return FAIL;
    }
    /// NEW REQUEST
    else {
        int32_t slot = POSITION_FAIL;
        switch (package->memory_operation) {

            case MEMORY_OPERATION_READ:
            case MEMORY_OPERATION_INST:
            {
                /// Control Parallel Requests
                if (this->recv_rqst_read_ready_cycle <= sinuca_engine.get_global_cycle()) {
                    slot = this->allocate_request(package);
                    if (slot != POSITION_FAIL) {
                        CACHE_DEBUG_PRINTF("\t RECEIVED READ/INST REQUEST\n");
                        this->mshr_buffer[slot].package_untreated(0);
                        this->recv_rqst_read_ready_cycle = transmission_latency + sinuca_engine.get_global_cycle();  /// Ready to receive from HIGHER_PORT

                        // =============================================================
                        /// SEND TO PREFETCH
                        // =============================================================
                        CACHE_DEBUG_PRINTF("\t Treat REQUEST this->mshr_born_ordered[%d] %s\n", slot, this->mshr_buffer[slot].content_to_string().c_str());
                        this->prefetcher->treat_prefetch(this->mshr_buffer + slot);

                        return OK;
                    }
                }
                CACHE_DEBUG_PRINTF("\tRECV DATA FAIL (BUSY)\n");
                return FAIL;
            }
            break;

            case MEMORY_OPERATION_PREFETCH:
            {
                /// Control Parallel Requests
                if (this->recv_rqst_read_ready_cycle <= sinuca_engine.get_global_cycle()) {
                    slot = this->allocate_prefetch(package);
                    if (slot != POSITION_FAIL) {
                        CACHE_DEBUG_PRINTF("\t RECEIVED PFTCH REQUEST\n");
                        this->mshr_buffer[slot].package_untreated(0);
                        this->recv_rqst_read_ready_cycle = transmission_latency + sinuca_engine.get_global_cycle();  /// Ready to receive from HIGHER_PORT

                        // =============================================================
                        /// SEND TO PREFETCH
                        // =============================================================
                        CACHE_DEBUG_PRINTF("\t Treat REQUEST this->mshr_born_ordered[%d] %s\n", slot, this->mshr_buffer[slot].content_to_string().c_str());
                        this->prefetcher->treat_prefetch(this->mshr_buffer + slot);

                        return OK;
                    }
                }
                CACHE_DEBUG_PRINTF("\tRECV DATA FAIL (BUSY)\n");
                return FAIL;
            }
            break;

            case MEMORY_OPERATION_WRITE:
            case MEMORY_OPERATION_WRITEBACK:
            {
                /// Control Parallel Requests
                if (this->recv_rqst_write_ready_cycle <= sinuca_engine.get_global_cycle()) {
                    slot = this->allocate_write(package);
                    if (slot != POSITION_FAIL) {
                        CACHE_DEBUG_PRINTF("\t RECEIVED WRITE REQUEST\n");
                        this->mshr_buffer[slot].package_untreated(0);
                        this->recv_rqst_write_ready_cycle = transmission_latency + sinuca_engine.get_global_cycle();  /// Ready to receive from HIGHER_PORT

                        // =============================================================
                        /// SEND TO PREFETCH
                        // =============================================================
                        CACHE_DEBUG_PRINTF("\t Treat REQUEST this->mshr_born_ordered[%d] %s\n", slot, this->mshr_buffer[slot].content_to_string().c_str());
                        if (this->mshr_buffer[slot].memory_operation != MEMORY_OPERATION_WRITEBACK) {
                            this->prefetcher->treat_prefetch(this->mshr_buffer + slot);
                        }

                        return OK;
                    }
                }
                CACHE_DEBUG_PRINTF("\tRECV DATA FAIL (BUSY)\n");
                return FAIL;
            }
            break;

            // HMC
            case MEMORY_OPERATION_HMC_ALU:
            case MEMORY_OPERATION_HMC_ALUR:
            {
                /// Control Parallel Requests
                if (this->recv_rqst_read_ready_cycle <= sinuca_engine.get_global_cycle()) {
                    slot = this->allocate_request(package);
                    if (slot != POSITION_FAIL) {
                        CACHE_DEBUG_PRINTF("\t RECEIVED HMC REQUEST\n");
                        this->mshr_buffer[slot].package_untreated(0);
                        this->recv_rqst_read_ready_cycle = transmission_latency + sinuca_engine.get_global_cycle();  /// Ready to receive from HIGHER_PORT

                        return OK;
                    }
                }
                CACHE_DEBUG_PRINTF("\tRECV DATA FAIL (BUSY)\n");
                return FAIL;
            }
            break;
        }
    }
    return FAIL;
};

// ============================================================================
/// Token Controller Methods
// ============================================================================
bool cache_memory_t::pop_token_credit(uint32_t src_id, memory_operation_t memory_operation) {

    CACHE_DEBUG_PRINTF("Popping a token to id (%" PRIu32 ")\n", src_id)

    switch (memory_operation) {
        case MEMORY_OPERATION_READ:
        case MEMORY_OPERATION_INST:
        // HMC -> READ buffer
        case MEMORY_OPERATION_HMC_ALU:
        case MEMORY_OPERATION_HMC_ALUR:
            if (this->mshr_tokens_request[src_id] > 0) {
                this->mshr_tokens_request[src_id]--;
                CACHE_DEBUG_PRINTF("Found a REQUEST token. (%" PRIu32 " left)\n", this->mshr_tokens_request[src_id]);
                return OK;
            }
            else {
                CACHE_DEBUG_PRINTF("Must wait, no REQUEST tokens left\n");
                this->add_stat_full_mshr_request_buffer();
                return FAIL;
            }
        break;

        case MEMORY_OPERATION_PREFETCH:
            if (this->mshr_tokens_prefetch[src_id] > 0) {
                this->mshr_tokens_prefetch[src_id]--;
                CACHE_DEBUG_PRINTF("Found a PREFETCH token. (%" PRIu32 " left)\n", this->mshr_tokens_prefetch[src_id]);
                return OK;
            }
            else {
                CACHE_DEBUG_PRINTF("Must wait, no PREFETCH tokens left\n");
                this->add_stat_full_mshr_prefetch_buffer();
                return FAIL;
            }
        break;

        case MEMORY_OPERATION_WRITE:
        case MEMORY_OPERATION_WRITEBACK:
            if (this->mshr_tokens_write[src_id] > 0) {
                this->mshr_tokens_write[src_id]--;
                CACHE_DEBUG_PRINTF("Found a WRITE token. (%" PRIu32 " left)\n", this->mshr_tokens_write[src_id]);
                return OK;
            }
            else {
                CACHE_DEBUG_PRINTF("Must wait, no WRITE tokens left\n");
                this->add_stat_full_mshr_write_buffer();
                return FAIL;
            }
        break;
    }

    ERROR_PRINTF("Could not treat the token\n")
    return FAIL;
};

// ============================================================================
void cache_memory_t::push_token_credit(uint32_t src_id, memory_operation_t memory_operation) {

    CACHE_DEBUG_PRINTF("Pushing a token to id %" PRIu32 "\n", src_id)
    ERROR_ASSERT_PRINTF(this->get_id() != src_id, "Should not be pushing a token to this.\n")

    switch (memory_operation) {
        case MEMORY_OPERATION_READ:
        case MEMORY_OPERATION_INST:
        // HMC -> READ buffer
        case MEMORY_OPERATION_HMC_ALU:
        case MEMORY_OPERATION_HMC_ALUR:
            ERROR_ASSERT_PRINTF(this->mshr_tokens_request[src_id] <= (int32_t)this->higher_level_request_tokens &&
                                this->mshr_tokens_request[src_id] >= 0,
                                "Found an component with more tokens than permitted\n");
            this->mshr_tokens_request[src_id]++;
        break;

        case MEMORY_OPERATION_PREFETCH:
            ERROR_ASSERT_PRINTF(this->mshr_tokens_prefetch[src_id] <= (int32_t)this->higher_level_prefetch_tokens &&
                                this->mshr_tokens_prefetch[src_id] >= 0,
                                "Found an component with more tokens than permitted\n");
            this->mshr_tokens_prefetch[src_id]++;
        break;

        case MEMORY_OPERATION_WRITE:
        case MEMORY_OPERATION_WRITEBACK:
            ERROR_ASSERT_PRINTF(this->mshr_tokens_write[src_id] <= (int32_t)this->higher_level_write_tokens &&
                                this->mshr_tokens_write[src_id] >= 0,
                                "Found an component with more tokens than permitted\n");
            this->mshr_tokens_write[src_id]++;
        break;
    }

    return;
};

// ============================================================================
/// Methods called by the directory to add statistics and others
// ============================================================================
void cache_memory_t::cache_stats(memory_operation_t memory_operation, bool is_hit) {

    this->add_stat_accesses();

    if (is_hit){
        switch (memory_operation) {
            case MEMORY_OPERATION_READ:
                    this->add_stat_read_hit();
            break;

            case MEMORY_OPERATION_INST:
                this->add_stat_instruction_hit();
            break;

            case MEMORY_OPERATION_WRITE:
                this->add_stat_write_hit();
            break;

            case MEMORY_OPERATION_PREFETCH:
                this->add_stat_prefetch_hit();
            break;

            case MEMORY_OPERATION_WRITEBACK:
                this->add_stat_writeback_recv();
            break;

            // =============================================================
            // Receiving a wrong HMC
            case MEMORY_OPERATION_HMC_ALU:
            case MEMORY_OPERATION_HMC_ALUR:
                ERROR_PRINTF("Entering at cache_stats() for a HMC instruction");
            break;

        }
    }
    else {
        switch (memory_operation) {
            case MEMORY_OPERATION_READ:
                this->add_stat_read_miss();
            break;

            case MEMORY_OPERATION_INST:
                this->add_stat_instruction_miss();
            break;

            case MEMORY_OPERATION_WRITE:
                this->add_stat_write_miss();
            break;

            case MEMORY_OPERATION_PREFETCH:
                this->add_stat_prefetch_miss();
            break;

            case MEMORY_OPERATION_WRITEBACK:
                this->add_stat_writeback_send();
            break;

            // =============================================================
            // Receiving a wrong HMC
            case MEMORY_OPERATION_HMC_ALU:
            case MEMORY_OPERATION_HMC_ALUR:
                ERROR_PRINTF("Entering at cache_stats() for a HMC instruction");
            break;

        }
    }
};

// ============================================================================
void cache_memory_t::cache_wait(memory_package_t *package) {

    switch (package->memory_operation) {
        case MEMORY_OPERATION_READ:
            this->add_stat_read_wait(package->born_cycle);
        break;

        case MEMORY_OPERATION_INST:
            this->add_stat_instruction_wait(package->born_cycle);
        break;

        case MEMORY_OPERATION_WRITE:
            this->add_stat_write_wait(package->born_cycle);
        break;

        case MEMORY_OPERATION_PREFETCH:
            this->add_stat_prefetch_wait(package->born_cycle);
        break;

        case MEMORY_OPERATION_WRITEBACK:
            this->add_stat_writeback_wait(package->born_cycle);
        break;

        // HMC
        case MEMORY_OPERATION_HMC_ALU:
        case MEMORY_OPERATION_HMC_ALUR:
            this->add_stat_hmc_wait(package->born_cycle);
        break;

    }
};

// ============================================================================
// Input: memory_address
// Output: cache_line_t*, &index, &way
cache_line_t* cache_memory_t::get_line(uint32_t index, uint32_t way) {
    ERROR_ASSERT_PRINTF(index < this->index_bits_mask, "Wrong index number\n")
    ERROR_ASSERT_PRINTF(way < this->get_associativity(), "Wrong way number\n")

    return &this->sets[index].ways[way];
}


// ============================================================================
// Input: memory_address
// Output: cache_line_t*, &index, &way
cache_line_t* cache_memory_t::find_line(uint64_t memory_address, uint32_t& index, uint32_t& choosen_way) {
    index = get_index(memory_address);

    for (uint32_t way = 0; way < this->get_associativity(); way++) {
        if (cmp_tag_index_bank(this->sets[index].ways[way].tag, memory_address)) {
            choosen_way = way;
            return &this->sets[index].ways[way];
        }
    }
    return NULL;
}

// ============================================================================
// Input: memory_address
// Output: cache_line_t*, &index, &way
cache_line_t* cache_memory_t::evict_address(uint64_t memory_address, uint32_t& index, uint32_t& choosen_way) {
    cache_line_t *choosen_line = NULL;

    index = get_index(memory_address);

    switch (this->replacement_policy) {
        case REPLACEMENT_LRU: {
            uint64_t last_access = std::numeric_limits<uint64_t>::max();
            for (uint32_t way = 0; way < this->get_associativity(); way++) {
                /// The line is not locked by directory
                if (sinuca_engine.directory_controller->is_locked(this->sets[index].ways[way].tag) == FAIL) {
                    /// If the line is LRU && the line is not locked by directory
                    if (this->sets[index].ways[way].last_access <= last_access) {
                        choosen_line = &this->sets[index].ways[way];
                        last_access = this->sets[index].ways[way].last_access;
                        choosen_way = way;
                    }
                }
                else {
                    ERROR_ASSERT_PRINTF(cmp_tag_index_bank(memory_address, this->sets[index].ways[way].tag) == FAIL, "Trying to find one line to evict, but tag == address\n")
                }
            }
        }
        break;

        case REPLACEMENT_DEAD_OR_LRU: {
            uint64_t last_access = std::numeric_limits<uint64_t>::max();

            cache_line_t *dead_choosen_line = NULL;
            uint32_t dead_choosen_way;
            for (uint32_t way = 0; way < this->get_associativity(); way++) {
                /// The line is not locked by directory
                if (sinuca_engine.directory_controller->is_locked(this->sets[index].ways[way].tag) == FAIL) {
                    /// If the line is LRU && the line is not locked by directory
                    if (this->sets[index].ways[way].last_access <= last_access) {
                        choosen_line = &this->sets[index].ways[way];
                        last_access = this->sets[index].ways[way].last_access;
                        choosen_way = way;

                        /// If is_dead
                        if (this->line_usage_predictor->check_line_is_last_access(NULL, NULL, index, way)) {
                            dead_choosen_line = &this->sets[index].ways[way];
                            dead_choosen_way = way;
                        }
                    }
                }
                else {
                    ERROR_ASSERT_PRINTF(cmp_tag_index_bank(memory_address, this->sets[index].ways[way].tag) == FAIL, "Trying to find one line to evict, but tag == address\n")
                }
            }
            /// Give priority to the LRU dead line
            if (dead_choosen_line != NULL) {
                choosen_line = dead_choosen_line;
                choosen_way = dead_choosen_way;
            }
        }
        break;

        case REPLACEMENT_INVALID_OR_LRU: {
            uint64_t last_access = std::numeric_limits<uint64_t>::max();
            for (uint32_t way = 0; way < this->get_associativity(); way++) {
                /// The line is not locked by directory
                if (sinuca_engine.directory_controller->is_locked(this->sets[index].ways[way].tag) == FAIL) {
                    /// If there is free space
                    if (this->sets[index].ways[way].status == PROTOCOL_STATUS_I) {
                        choosen_line = &this->sets[index].ways[way];
                        choosen_way = way;
                        break;
                    }
                    /// If the line is LRU && the line is not locked by directory
                    else if (this->sets[index].ways[way].last_access <= last_access) {
                        choosen_line = &this->sets[index].ways[way];
                        last_access = this->sets[index].ways[way].last_access;
                        choosen_way = way;
                    }
                }
                else {
                    ERROR_ASSERT_PRINTF(cmp_tag_index_bank(memory_address, this->sets[index].ways[way].tag) == FAIL, "Trying to find one line to evict, but tag == address\n")
                }
            }
        }
        break;

        case REPLACEMENT_RANDOM: {
            /// Initialize random seed
            unsigned int seed = time(NULL);
            /// Generate random number
            uint32_t way = (rand_r(&seed) % this->get_associativity());
            /// Check if the line is not locked by directory
            if (sinuca_engine.directory_controller->is_locked(this->sets[index].ways[way].tag) == FAIL) {
                choosen_line = &this->sets[index].ways[way];
                choosen_way = way;
            }
        }
        break;


        case REPLACEMENT_FIFO:
        case REPLACEMENT_LRF:
            ERROR_PRINTF("Replacement Policy: %s not implemented.\n",  get_enum_replacement_char(this->replacement_policy));
        break;

    }
    if (choosen_line == NULL) {
        WARNING_PRINTF("Could not evict a cache line in this set.\n")
    }
    return choosen_line;
};


// ============================================================================
void cache_memory_t::change_address(cache_line_t *line, uint64_t new_memory_address) {
    ERROR_ASSERT_PRINTF(line != NULL, "Cannot change the tag address of a NULL line.\n")
    line->tag = new_memory_address;
    return;
};


// ============================================================================
void cache_memory_t::change_status(cache_line_t *line, protocol_status_t status) {
    ERROR_ASSERT_PRINTF(line != NULL, "Cannot change the status of a NULL line.\n")
    line->status = status;
    return;
};

// ============================================================================
void cache_memory_t::update_last_access(cache_line_t *line) {
    ERROR_ASSERT_PRINTF(line != NULL, "Cannot change the last_access of a NULL line.\n")
    line->last_access = sinuca_engine.get_global_cycle();
    return;
};



// ============================================================================
void cache_memory_t::print_structures() {
    SINUCA_PRINTF("%s MSHR_BUFFER:\n%s", this->get_label(), memory_package_t::print_all(this->mshr_buffer, this->mshr_buffer_size).c_str())

    SINUCA_PRINTF("%s MSHR_TOKENS_REQUEST:\n", this->get_label())
    for (uint32_t i = 0; i < sinuca_engine.get_interconnection_interface_array_size(); i++) {
        uint32_t src_id = sinuca_engine.interconnection_interface_array[i]->get_id();
        if (this->mshr_tokens_request[src_id] != -1) {
            SINUCA_PRINTF("%s => %d tokens\n", sinuca_engine.interconnection_interface_array[i]->get_label(), this->mshr_tokens_request[src_id])
        }
    }

    SINUCA_PRINTF("%s MSHR_TOKENS_PREFETCH:\n", this->get_label())
    for (uint32_t i = 0; i < sinuca_engine.get_interconnection_interface_array_size(); i++) {
        uint32_t src_id = sinuca_engine.interconnection_interface_array[i]->get_id();
        if (this->mshr_tokens_prefetch[src_id] != -1) {
            SINUCA_PRINTF("%s => %d tokens\n", sinuca_engine.interconnection_interface_array[i]->get_label(), this->mshr_tokens_prefetch[src_id])
        }
    }

    SINUCA_PRINTF("%s MSHR_TOKENS_WRITE:\n", this->get_label())
    for (uint32_t i = 0; i < sinuca_engine.get_interconnection_interface_array_size(); i++) {
        uint32_t src_id = sinuca_engine.interconnection_interface_array[i]->get_id();
        if (this->mshr_tokens_write[src_id] != -1) {
            SINUCA_PRINTF("%s => %d tokens\n", sinuca_engine.interconnection_interface_array[i]->get_label(), this->mshr_tokens_write[src_id])
        }
    }
};

// ============================================================================
void cache_memory_t::panic() {
    this->print_structures();
    this->prefetcher->panic();
    this->line_usage_predictor->panic();
};

// ============================================================================
void cache_memory_t::periodic_check(){
    #ifdef CACHE_DEBUG
        CACHE_DEBUG_PRINTF("\n");
        this->print_structures();
    #endif
    ERROR_ASSERT_PRINTF(memory_package_t::check_age(this->mshr_buffer, this->mshr_buffer_size) == OK, "Check_age failed.\n");
    this->prefetcher->periodic_check();
    this->line_usage_predictor->periodic_check();
};


// ============================================================================
/// STATISTICS
// ============================================================================
void cache_memory_t::reset_statistics() {
    this->set_stat_accesses(0);
    this->set_stat_invalidation(0);
    this->set_stat_eviction(0);
    this->set_stat_writeback(0);

    this->set_stat_final_eviction(0);
    this->set_stat_final_writeback(0);

    this->set_stat_instruction_hit(0);
    this->set_stat_read_hit(0);
    this->set_stat_prefetch_hit(0);
    this->set_stat_write_hit(0);
    this->set_stat_writeback_recv(0);

    this->set_stat_instruction_miss(0);
    this->set_stat_read_miss(0);
    this->set_stat_prefetch_miss(0);
    this->set_stat_write_miss(0);
    this->set_stat_writeback_send(0);

    this->stat_min_instruction_wait_time = 0;
    this->stat_max_instruction_wait_time = 0;
    this->stat_accumulated_instruction_wait_time = 0;

    this->stat_min_read_wait_time = 0;
    this->stat_max_read_wait_time = 0;
    this->stat_accumulated_read_wait_time = 0;

    this->stat_min_prefetch_wait_time = 0;
    this->stat_max_prefetch_wait_time = 0;
    this->stat_accumulated_prefetch_wait_time = 0;

    this->stat_min_write_wait_time = 0;
    this->stat_max_write_wait_time = 0;
    this->stat_accumulated_write_wait_time = 0;

    this->stat_min_writeback_wait_time = 0;
    this->stat_max_writeback_wait_time = 0;
    this->stat_accumulated_writeback_wait_time = 0;

    this->stat_full_mshr_request_buffer = 0;
    this->stat_full_mshr_prefetch_buffer = 0;
    this->stat_full_mshr_write_buffer = 0;
    this->stat_full_mshr_eviction_buffer = 0;

    this->prefetcher->reset_statistics();
    this->line_usage_predictor->reset_statistics();
};

// ============================================================================
void cache_memory_t::print_statistics() {
    char title[100] = "";
    snprintf(title, sizeof(title), "Statistics of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_accesses", stat_accesses);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_invalidation", stat_invalidation);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_eviction", stat_eviction);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_writeback", stat_writeback);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_final_eviction", stat_final_eviction);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_final_writeback", stat_final_writeback);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sum_read", stat_instruction_hit + stat_read_hit + stat_prefetch_hit +
                                                                                                    stat_instruction_miss + stat_read_miss + stat_prefetch_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sum_read_hit", stat_instruction_hit + stat_read_hit + stat_prefetch_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sum_read_miss", stat_instruction_miss + stat_read_miss + stat_prefetch_miss);

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sum_write", stat_write_hit + stat_writeback_recv + stat_write_miss + stat_writeback_send);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sum_write_hit", stat_write_hit + stat_writeback_recv);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_sum_write_miss", stat_write_miss + stat_writeback_send);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_instruction_hit", stat_instruction_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_read_hit", stat_read_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_prefetch_hit", stat_prefetch_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_write_hit", stat_write_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_writeback_recv", stat_writeback_recv);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_instruction_miss", stat_instruction_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_read_miss", stat_read_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_prefetch_miss", stat_prefetch_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_write_miss", stat_write_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_writeback_send", stat_writeback_send);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_instruction_miss_percentage", stat_instruction_miss, stat_instruction_miss + stat_instruction_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_read_miss_percentage", stat_read_miss, stat_read_miss + stat_read_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_prefetch_miss_percentage", stat_prefetch_miss, stat_prefetch_miss + stat_prefetch_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_write_miss_percentage", stat_write_miss, stat_write_miss + stat_write_hit);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_instruction_wait_time", stat_min_instruction_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_read_wait_time", stat_min_read_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_prefetch_wait_time", stat_min_prefetch_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_write_wait_time", stat_min_write_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_writeback_wait_time", stat_min_writeback_wait_time);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_instruction_wait_time", stat_max_instruction_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_read_wait_time", stat_max_read_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_prefetch_wait_time", stat_max_prefetch_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_write_wait_time", stat_max_write_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_writeback_wait_time", stat_max_writeback_wait_time);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_accumulated_instruction_wait_time_ratio", stat_accumulated_instruction_wait_time, stat_accesses);
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_accumulated_read_wait_time_ratio", stat_accumulated_read_wait_time, stat_read_miss);
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_accumulated_prefetch_wait_time_ratio", stat_accumulated_prefetch_wait_time, stat_prefetch_miss);
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_accumulated_write_wait_time_ratio", stat_accumulated_write_wait_time, stat_write_miss);
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_accumulated_writeback_wait_time_ratio", stat_accumulated_writeback_wait_time, stat_writeback_send);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_full_mshr_request_buffer", stat_full_mshr_request_buffer);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_full_mshr_write_buffer", stat_full_mshr_write_buffer);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_full_mshr_prefetch_buffer", stat_full_mshr_prefetch_buffer);


    this->prefetcher->print_statistics();
    this->line_usage_predictor->print_statistics();
};

// ============================================================================
// ============================================================================
void cache_memory_t::print_configuration() {
    char title[100] = "";
    snprintf(title, sizeof(title), "Configuration of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "interconnection_latency", this->get_interconnection_latency());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "interconnection_width", this->get_interconnection_width());

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "cache_id", cache_id);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "bank_number", bank_number);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "total_banks", total_banks);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "hierarchy_level", hierarchy_level);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "line_size", line_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "line_number", line_number);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "associativity", associativity);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "replacement_policy", get_enum_replacement_char(replacement_policy));

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "penalty_read", penalty_read);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "penalty_write", penalty_write);

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_buffer_size", mshr_buffer_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_request_buffer_size", mshr_request_buffer_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_prefetch_buffer_size", mshr_prefetch_buffer_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_write_buffer_size", mshr_write_buffer_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_eviction_buffer_size", mshr_eviction_buffer_size);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "higher_level_request_tokens", higher_level_request_tokens);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "higher_level_prefetch_tokens", higher_level_prefetch_tokens);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "higher_level_write_tokens", higher_level_write_tokens);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "address_mask_type", get_enum_cache_mask_char(address_mask_type));
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "tag_bits_mask", utils_t::address_to_binary(this->tag_bits_mask).c_str());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "index_bits_mask", utils_t::address_to_binary(this->index_bits_mask).c_str());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "bank_bits_mask", utils_t::address_to_binary(this->bank_bits_mask).c_str());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "offset_bits_mask", utils_t::address_to_binary(this->offset_bits_mask).c_str());

    this->prefetcher->print_configuration();
    this->line_usage_predictor->print_configuration();
};
