/// ============================================================================
//
// Copyright (C) 2010, 2011, 2012
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
#include "../sinuca.hpp"

#ifdef CACHE_DEBUG
    #define CACHE_DEBUG_PRINTF(...) DEBUG_PRINTF(__VA_ARGS__);
#else
    #define CACHE_DEBUG_PRINTF(...)
#endif

/// ============================================================================
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

    this->mshr_buffer_request_reserved_size = 0;
    this->mshr_buffer_copyback_reserved_size = 0;
    this->mshr_buffer_prefetch_reserved_size = 0;

    this->total_sets = 0;
    this->sets = NULL;

    this->mshr_buffer = NULL;  /// Buffer of Missed Requests
    this->mshr_buffer_size = 0;

    this->lower_level_cache = new container_ptr_cache_memory_t;
    this->higher_level_cache = new container_ptr_cache_memory_t;

    this->mshr_born_ordered = new container_ptr_memory_package_t;

    this->send_ready_cycle = 0;
    this->recv_ans_ready_cycle = 0;
    this->recv_rqst_read_ready_cycle = 0;
    this->recv_rqst_write_ready_cycle = 0;
};

/// ============================================================================
cache_memory_t::~cache_memory_t() {
    /// De-Allocate memory to prevent memory leak
    utils_t::template_delete_array<cache_set_t>(sets);
    utils_t::template_delete_array<memory_package_t>(mshr_buffer);
    utils_t::template_delete_variable<container_ptr_cache_memory_t>(lower_level_cache);
    utils_t::template_delete_variable<container_ptr_cache_memory_t>(higher_level_cache);

    utils_t::template_delete_variable<container_ptr_memory_package_t>(mshr_born_ordered);
};

/// ============================================================================
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

    ERROR_ASSERT_PRINTF(mshr_buffer_request_reserved_size > 0, "mshr_buffer_request_reserved_size should be bigger than zero.\n");
    ERROR_ASSERT_PRINTF(mshr_buffer_copyback_reserved_size > 0, "mshr_buffer_copyback_reserved_size should be bigger than zero.\n");
    ERROR_ASSERT_PRINTF(mshr_buffer_prefetch_reserved_size > 0, "mshr_buffer_prefetch_reserved_size should be bigger than zero.\n");


    /// MSHR = [    REQUEST    | COPYBACK | PREFETCH ]
    this->mshr_buffer_size = this->mshr_buffer_request_reserved_size +
                                this->mshr_buffer_copyback_reserved_size +
                                this->mshr_buffer_prefetch_reserved_size;
    this->mshr_buffer = utils_t::template_allocate_array<memory_package_t>(this->get_mshr_buffer_size());




    ERROR_ASSERT_PRINTF(this->get_total_banks() == 1 || this->prefetcher->get_prefetcher_type() == PREFETCHER_DISABLE, "Cannot use a multibanked cache with prefetch. (Some requests may be generated in the wrong bank)\n");

    /// ================================================================================
    /// PREFETCH
    /// ================================================================================
    char label[50] = "";
    sprintf(label, "Prefetch_%s", this->get_label());
    this->prefetcher->set_label(label);
    this->prefetcher->allocate();

    /// ================================================================================
    /// LINE_USAGE_PREDICTOR
    /// ================================================================================
    sprintf(label, "Line_Usage_Predictor_%s", this->get_label());
    this->line_usage_predictor->set_label(label);
    this->line_usage_predictor->allocate();

    #ifdef CACHE_DEBUG
        this->print_configuration();
    #endif
};

/// ============================================================================
uint64_t cache_memory_t::get_fake_address(uint32_t index, uint32_t way){
    CACHE_DEBUG_PRINTF("index:%d way:%d\n", index, way);
    CACHE_DEBUG_PRINTF("tag:%"PRIu64" index:%"PRIu64" bank:%"PRIu64" offset:%"PRIu64"\n", tag_bits_shift, index_bits_shift, bank_bits_shift, offset_bits_shift );


    uint64_t final_address = 0;
    switch (this->get_address_mask_type()) {
        case CACHE_MASK_TAG_INDEX_BANK_OFFSET:
            CACHE_DEBUG_PRINTF("%"PRIu64" ->", final_address);

            final_address = (way << this->tag_bits_shift);
            CACHE_DEBUG_PRINTF("%"PRIu64" ->", final_address);

            final_address += (index << this->index_bits_shift);
            CACHE_DEBUG_PRINTF("%"PRIu64" ->", final_address);

            final_address += (this->get_bank_number() << this->bank_bits_shift);
            CACHE_DEBUG_PRINTF("%"PRIu64"\n", final_address);
        break;

        case CACHE_MASK_TAG_INDEX_OFFSET:
            CACHE_DEBUG_PRINTF("%"PRIu64" ->", final_address);

            final_address = (way << this->tag_bits_shift);
            CACHE_DEBUG_PRINTF("%"PRIu64" ->", final_address);

            final_address += (index << this->index_bits_shift);
            CACHE_DEBUG_PRINTF("%"PRIu64"\n", final_address);
        break;
    }
    ERROR_ASSERT_PRINTF(index == get_index(final_address), "Wrong Index into the Fake Address.\n")
    ERROR_ASSERT_PRINTF(this->get_bank_number() == get_bank(final_address), "Wrong Bank into the Fake Address.\n")
    return final_address;

};

/// ============================================================================
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

/// ============================================================================
void cache_memory_t::clock(uint32_t subcycle) {
    if (subcycle != 0) return;
    CACHE_DEBUG_PRINTF("==================== ID(%u) ",this->get_id());
    CACHE_DEBUG_PRINTF("====================\n");
    CACHE_DEBUG_PRINTF("cycle() \n");

    this->prefetcher->clock(subcycle);

    /// =================================================================
    /// MSHR_BUFFER - REMOVE THE READY PACKAGES
    /// =================================================================
    for (int32_t i = this->mshr_born_ordered->size() - 1; i >= 0 ; i--){
        if (mshr_born_ordered[0][i]->state == PACKAGE_STATE_READY &&
        this->mshr_born_ordered[0][i]->ready_cycle <= sinuca_engine.get_global_cycle()) {
                this->mshr_born_ordered[0][i]->package_clean();
                this->mshr_born_ordered->erase(this->mshr_born_ordered->begin() + i);
        }
    }


    /// =================================================================
    /// MSHR_BUFFER - TRANSMISSION
    /// =================================================================
    for (uint32_t i = 0; i < this->mshr_born_ordered->size(); i++){
        if (mshr_born_ordered[0][i]->state == PACKAGE_STATE_TRANSMIT &&
        this->mshr_born_ordered[0][i]->is_answer == true &&
        this->mshr_born_ordered[0][i]->ready_cycle <= sinuca_engine.get_global_cycle()) {

            CACHE_DEBUG_PRINTF("\t Send ANSWER this->mshr_born_ordered[%d] %s\n", i, this->mshr_born_ordered[0][i]->content_to_string().c_str());
            int32_t transmission_latency = send_package(mshr_born_ordered[0][i]);
            if (transmission_latency != POSITION_FAIL) {
                this->mshr_born_ordered[0][i]->package_ready(transmission_latency);
            }
            break;
        }
    }

    for (uint32_t i = 0; i < this->mshr_born_ordered->size(); i++){
        if (mshr_born_ordered[0][i]->state == PACKAGE_STATE_TRANSMIT &&
        this->mshr_born_ordered[0][i]->is_answer == false &&
        this->mshr_born_ordered[0][i]->ready_cycle <= sinuca_engine.get_global_cycle()) {

            CACHE_DEBUG_PRINTF("\t Send REQUEST this->mshr_born_ordered[%d] %s\n", i, this->mshr_born_ordered[0][i]->content_to_string().c_str());
            int32_t transmission_latency = send_package(mshr_born_ordered[0][i]);
            if (transmission_latency != POSITION_FAIL) {
                this->mshr_born_ordered[0][i]->state = sinuca_engine.directory_controller->treat_cache_request_sent(this->get_cache_id(), this->mshr_born_ordered[0][i]);
                /// Normal READ/INST/PREFETCH Request
                if (this->mshr_born_ordered[0][i]->state == PACKAGE_STATE_WAIT) {
                    this->mshr_born_ordered[0][i]->package_wait(transmission_latency);
                }
                /// Normal COPY-BACK Request
                else if (this->mshr_born_ordered[0][i]->state == PACKAGE_STATE_FREE) {
                    this->mshr_born_ordered[0][i]->package_clean();
                    this->mshr_born_ordered->erase(this->mshr_born_ordered->begin() + i);
                }
            }
            break;
        }
    }


    /// =================================================================
    /// MSHR_BUFFER - UNTREATED
    /// =================================================================
    for (uint32_t i = 0; i < this->mshr_born_ordered->size(); i++){
        if (mshr_born_ordered[0][i]->state == PACKAGE_STATE_UNTREATED &&
        this->mshr_born_ordered[0][i]->is_answer == true &&
        this->mshr_born_ordered[0][i]->ready_cycle <= sinuca_engine.get_global_cycle()) {

            this->mshr_born_ordered[0][i]->state = sinuca_engine.directory_controller->treat_cache_answer(this->get_cache_id(), this->mshr_born_ordered[0][i]);
            ERROR_ASSERT_PRINTF(this->mshr_born_ordered[0][i]->state != PACKAGE_STATE_FREE, "Must not receive back a FREE, should receive READY + Latency")
            /// Could not treat, then restart born_cycle (change priority)
            if (this->mshr_born_ordered[0][i]->state == PACKAGE_STATE_UNTREATED) {
                this->mshr_born_ordered[0][i]->born_cycle = sinuca_engine.get_global_cycle();

                memory_package_t *package = this->mshr_born_ordered[0][i];
                this->mshr_born_ordered->erase(this->mshr_born_ordered->begin() + i);
                this->insert_mshr_born_ordered(package);
            }
            break;
        }
    }

    for (uint32_t i = 0; i < this->mshr_born_ordered->size(); i++){
        if (mshr_born_ordered[0][i]->state == PACKAGE_STATE_UNTREATED &&
        this->mshr_born_ordered[0][i]->is_answer == false &&
        this->mshr_born_ordered[0][i]->ready_cycle <= sinuca_engine.get_global_cycle()) {

            this->mshr_born_ordered[0][i]->state = sinuca_engine.directory_controller->treat_cache_request(this->get_cache_id(), this->mshr_born_ordered[0][i]);
            ERROR_ASSERT_PRINTF(this->mshr_born_ordered[0][i]->state != PACKAGE_STATE_FREE, "Must not receive back a FREE, should receive READY + Latency")
            /// Could not treat, then restart born_cycle (change priority)
            /// If (is_answer == true) means that higher level had the cache line.
            if (this->mshr_born_ordered[0][i]->state == PACKAGE_STATE_UNTREATED &&
            this->mshr_born_ordered[0][i]->is_answer == false) {
                this->mshr_born_ordered[0][i]->born_cycle = sinuca_engine.get_global_cycle();

                memory_package_t *package = this->mshr_born_ordered[0][i];
                this->mshr_born_ordered->erase(this->mshr_born_ordered->begin() + i);
                this->insert_mshr_born_ordered(package);
            }
            else {
                /// =============================================================
                /// SEND TO PREFETCH
                /// =============================================================
                CACHE_DEBUG_PRINTF("\t Treat REQUEST this->mshr_born_ordered[%d] %s\n", i, this->mshr_born_ordered[0][i]->content_to_string().c_str());
                if (this->mshr_born_ordered[0][i]->id_owner != this->get_id() &&
                this->mshr_born_ordered[0][i]->memory_operation != MEMORY_OPERATION_COPYBACK) {
                    this->prefetcher->treat_prefetch(this->mshr_born_ordered[0][i]);
                }
            }

            break;
        }
    }

    /// =================================================================
    /// GET FROM PREFETCHER
    /// =================================================================
    memory_package_t* memory_package = this->prefetcher->request_buffer_get_older();
    if (memory_package != NULL) {
        CACHE_DEBUG_PRINTF("\t Has New Prefetch.\n");

        int32_t slot = this->allocate_prefetch(memory_package);
        if (slot != POSITION_FAIL) {
            CACHE_DEBUG_PRINTF("\t\t Allocating PREFETCH into MSHR\n");
            this->prefetcher->request_buffer_remove();
        }
    }
};


/// ============================================================================
void cache_memory_t::insert_mshr_born_ordered(memory_package_t* package){
    /// this->mshr_born_ordered            = [OLDER --------> NEWER]
    /// this->mshr_born_ordered.born_cycle = [SMALLER -----> BIGGER]

    /// Most of the insertions are made in the end !!!
    for (int32_t i = this->mshr_born_ordered->size() - 1; i >= 0 ; i--){
        if (this->mshr_born_ordered[0][i]->born_cycle <= package->born_cycle) {
            this->mshr_born_ordered->insert(this->mshr_born_ordered->begin() + i + 1, package);
            return;
        }
    }
    /// Could not find a older package to insert or it is just empty
    this->mshr_born_ordered->insert(this->mshr_born_ordered->begin(), package);

    /// Check the MSHR BORN ORDERED
    #ifdef CACHE_DEBUG
        uint64_t test_order = 0;
        for (uint32_t i = 0; i < this->mshr_born_ordered->size(); i++){
            if (test_order > this->mshr_born_ordered[0][i]->born_cycle) {
                for (uint32_t j = 0; j < this->mshr_born_ordered->size(); j++){
                    CACHE_DEBUG_PRINTF("%"PRIu64" ", this->mshr_born_ordered[0][j]->born_cycle);
                }
                ERROR_ASSERT_PRINTF(test_order > this->mshr_born_ordered[0][i]->born_cycle, "Wrong order when inserting (%"PRIu64")\n", package->born_cycle);
            }
            test_order = this->mshr_born_ordered[0][i]->born_cycle;
        }
    #endif
};

/// ============================================================================
int32_t cache_memory_t::allocate_copyback(memory_package_t* package){

    int32_t slot = memory_package_t::find_free(this->mshr_buffer + this->mshr_buffer_request_reserved_size, this->mshr_buffer_copyback_reserved_size);
    if (slot != POSITION_FAIL) {
        slot += this->mshr_buffer_request_reserved_size;
        CACHE_DEBUG_PRINTF("\t NEW COPYBACK\n");
        this->mshr_buffer[slot] = *package;
        this->insert_mshr_born_ordered(&this->mshr_buffer[slot]);    /// Insert into a parallel and well organized MSHR structure
    }
    else {
        add_stat_full_mshr_buffer_copyback();
    }
    return slot;
};

/// ============================================================================
int32_t cache_memory_t::allocate_prefetch(memory_package_t* package){

    int32_t slot = memory_package_t::find_free(this->mshr_buffer + this->mshr_buffer_request_reserved_size + this->mshr_buffer_copyback_reserved_size,
                                                this->mshr_buffer_prefetch_reserved_size);
    if (slot != POSITION_FAIL) {
        slot += this->mshr_buffer_request_reserved_size + this->mshr_buffer_copyback_reserved_size;

        CACHE_DEBUG_PRINTF("\t NEW PREFETCH\n");

        this->mshr_buffer[slot] = *package;
        /// Add Identity information
        this->mshr_buffer[slot].id_owner = this->get_id();
        this->mshr_buffer[slot].id_src = this->get_id();
        this->mshr_buffer[slot].id_dst = this->get_id();
        this->mshr_buffer[slot].package_untreated(0);

        this->insert_mshr_born_ordered(&this->mshr_buffer[slot]);    /// Insert into a parallel and well organized MSHR structure
    }
    else {
        add_stat_full_mshr_buffer_prefetch();
    }
    return slot;
};

/// ============================================================================
int32_t cache_memory_t::send_package(memory_package_t *package) {
    CACHE_DEBUG_PRINTF("send_package() package:%s\n", package->content_to_string().c_str());
    ERROR_ASSERT_PRINTF(package->memory_address != 0, "Wrong memory address.\n%s\n", package->content_to_string().c_str());

    if (this->send_ready_cycle <= sinuca_engine.get_global_cycle()) {
        /// Check if FINAL DESTINATION has FREE SPACE available.
        if (package->is_answer == false &&
            sinuca_engine.interconnection_interface_array[package->id_dst]->check_token_list(package) == false) {
            CACHE_DEBUG_PRINTF("\tSEND FAIL (NO TOKENS)\n");
            return POSITION_FAIL;
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
            this->send_ready_cycle = sinuca_engine.get_global_cycle() + transmission_latency;
            return transmission_latency;
        }
        else {
            CACHE_DEBUG_PRINTF("\tSEND FAIL\n");
            package->hop_count++;  /// Do not Consume its own port
            return POSITION_FAIL;
        }
    }
    CACHE_DEBUG_PRINTF("\tSEND FAIL (BUSY)\n");
    return POSITION_FAIL;
};


/// ============================================================================
bool cache_memory_t::receive_package(memory_package_t *package, uint32_t input_port, uint32_t transmission_latency) {
    CACHE_DEBUG_PRINTF("receive_package() port:%u, package:%s\n", input_port, package->content_to_string().c_str());

    ERROR_ASSERT_PRINTF(package->memory_address != 0, "Wrong memory address.\n%s\n", package->content_to_string().c_str());
    ERROR_ASSERT_PRINTF(get_bank(package->memory_address) == this->get_bank_number(), "Wrong bank.\n%s\n", package->content_to_string().c_str());
    ERROR_ASSERT_PRINTF(package->id_dst == this->get_id() && package->hop_count == POSITION_FAIL, "Received some package for a different id_dst.\n%s\n", package->content_to_string().c_str());
    ERROR_ASSERT_PRINTF(input_port < this->get_max_ports(), "Received a wrong input_port\n%s\n", package->content_to_string().c_str());

    /// NEW ANSWER
    if (package->is_answer) {
        if (this->recv_ans_ready_cycle <= sinuca_engine.get_global_cycle()) {
            for (uint32_t i = 0 ; i < this->mshr_buffer_size; i++) {
                /// Find the correct REQUEST which matches with the ANSWER
                if (this->mshr_buffer[i].state == PACKAGE_STATE_WAIT &&
                this->mshr_buffer[i].id_owner == package->id_owner &&
                this->mshr_buffer[i].opcode_number == package->opcode_number &&
                this->mshr_buffer[i].uop_number == package->uop_number &&
                this->cmp_tag_index_bank(this->mshr_buffer[i].memory_address, package->memory_address)) {
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
            case MEMORY_OPERATION_PREFETCH:
            {
                if (this->recv_rqst_read_ready_cycle <= sinuca_engine.get_global_cycle()) {
                    slot = memory_package_t::find_free(this->mshr_buffer, this->mshr_buffer_request_reserved_size);
                    if (slot != POSITION_FAIL) {
                        CACHE_DEBUG_PRINTF("\t RECEIVED READ REQUEST\n");
                        this->mshr_buffer[slot] = *package;
                        this->mshr_buffer[slot].package_untreated(1);
                        this->recv_rqst_read_ready_cycle = transmission_latency + sinuca_engine.get_global_cycle();  /// Ready to receive from HIGHER_PORT

                        this->insert_mshr_born_ordered(&this->mshr_buffer[slot]);    /// Insert into a parallel and well organized MSHR structure
                        this->remove_token_list(package);
                        return OK;
                    }
                    else {
                        add_stat_full_mshr_buffer_request();
                        return FAIL;
                    }
                }
                CACHE_DEBUG_PRINTF("\tRECV DATA FAIL (BUSY)\n");
                return FAIL;
            }
            break;

            case MEMORY_OPERATION_WRITE:
            case MEMORY_OPERATION_COPYBACK:
            {
                if (this->recv_rqst_write_ready_cycle <= sinuca_engine.get_global_cycle()) {
                    slot = memory_package_t::find_free(this->mshr_buffer, this->mshr_buffer_request_reserved_size);
                    if (slot != POSITION_FAIL) {
                        CACHE_DEBUG_PRINTF("\t RECEIVED WRITE REQUEST\n");
                        this->mshr_buffer[slot] = *package;
                        this->mshr_buffer[slot].package_untreated(1);
                        this->recv_rqst_write_ready_cycle = transmission_latency + sinuca_engine.get_global_cycle();  /// Ready to receive from HIGHER_PORT

                        this->insert_mshr_born_ordered(&this->mshr_buffer[slot]);    /// Insert into a parallel and well organized MSHR structure
                        this->remove_token_list(package);
                        return OK;
                    }
                    else {
                        add_stat_full_mshr_buffer_request();
                        return FAIL;
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

/// ============================================================================
/// Token Controller Methods
/// ============================================================================
void cache_memory_t::allocate_token_list() {
    CACHE_DEBUG_PRINTF("allocate_token_list()\n");

    this->set_token_list(utils_t::template_allocate_array<container_token_t>(1));
};

/// ============================================================================
bool cache_memory_t::check_token_list(memory_package_t *package) {
    ERROR_ASSERT_PRINTF(package->is_answer == false, "check_token_list received a Answer.\n")
    uint32_t token = 0;

    /// 1. Check if the name is already in the guest list.
    container_token_t *local_token_list = this->get_token_list();
    for (token = 0; token < local_token_list[0].size(); token++) {
        /// Requested Address Found
        if (local_token_list[0][token].id_owner == package->id_owner &&
        local_token_list[0][token].opcode_number == package->opcode_number &&
        local_token_list[0][token].uop_number == package->uop_number &&
        local_token_list[0][token].memory_address == package->memory_address &&
        local_token_list[0][token].memory_operation == package->memory_operation) {
            break;
        }
    }

    /// 2. Name is not in the guest list, lets add it.
    if (token == local_token_list[0].size()) {
        /// Allocate the new token
        token_t new_token;
        new_token.id_owner = package->id_owner;
        new_token.opcode_number = package->opcode_number;
        new_token.uop_number = package->uop_number;
        new_token.memory_address = package->memory_address;
        new_token.memory_operation = package->memory_operation;

        local_token_list[0].push_back(new_token);
    }

    /// 3. Check if the guest can come now, or it needs to wait for free space.
    if (token < this->check_token_space(package)) {
        /// Lets party !
        return OK;
    }
    else {
        /// Hold on !
        add_stat_full_mshr_buffer_request();
        return FAIL;
    }
};

/// ============================================================================
uint32_t cache_memory_t::check_token_space(memory_package_t *package) {
    CACHE_DEBUG_PRINTF("check_token_space() %s\n", package->content_to_string().c_str());
    ERROR_ASSERT_PRINTF(get_bank(package->memory_address) == this->get_bank_number(), "Wrong bank.\n%s\n", package->content_to_string().c_str());

    uint32_t free_space = memory_package_t::count_free(this->mshr_buffer, this->mshr_buffer_request_reserved_size);
    return free_space;
};

/// ============================================================================
void cache_memory_t::remove_token_list(memory_package_t *package) {
    container_token_t *local_token_list = this->get_token_list();
    for (uint32_t token = 0; token < local_token_list[0].size(); token++) {
        /// Requested Address Found
        if (local_token_list[0][token].id_owner == package->id_owner &&
        local_token_list[0][token].opcode_number == package->opcode_number &&
        local_token_list[0][token].uop_number == package->uop_number &&
        local_token_list[0][token].memory_address == package->memory_address &&
        local_token_list[0][token].memory_operation == package->memory_operation) {
            local_token_list[0].erase(local_token_list[0].begin() + token);
            return;
        }
    }
    ERROR_PRINTF("Could not find the previous allocated token.\n%s\n", package->content_to_string().c_str())
};

/// ============================================================================
/// Methods called by the directory to add statistics and others
/// ============================================================================
void cache_memory_t::cache_hit(memory_package_t *package) {
    switch (package->memory_operation) {
        case MEMORY_OPERATION_READ:
            this->add_stat_read_hit();
            this->add_stat_accesses();
        break;

        case MEMORY_OPERATION_INST:
            this->add_stat_instruction_hit();
            this->add_stat_accesses();
        break;

        case MEMORY_OPERATION_WRITE:
            this->add_stat_write_hit();
            this->add_stat_accesses();
        break;

        case MEMORY_OPERATION_PREFETCH:
            this->add_stat_prefetch_hit();
        break;

        case MEMORY_OPERATION_COPYBACK:
            this->add_stat_copyback_hit();
        break;
    }
};
/// ============================================================================
void cache_memory_t::cache_miss(memory_package_t *package) {
    switch (package->memory_operation) {
        case MEMORY_OPERATION_READ:
            this->add_stat_read_miss(package->born_cycle);
            this->add_stat_accesses();
        break;

        case MEMORY_OPERATION_INST:
            this->add_stat_instruction_miss(package->born_cycle);
            this->add_stat_accesses();
        break;

        case MEMORY_OPERATION_WRITE:
            this->add_stat_write_miss(package->born_cycle);
            this->add_stat_accesses();
        break;

        case MEMORY_OPERATION_PREFETCH:
            this->add_stat_prefetch_miss(package->born_cycle);
        break;

        case MEMORY_OPERATION_COPYBACK:
            this->add_stat_copyback_miss(package->born_cycle);
        break;
    }
};
/// ============================================================================
void cache_memory_t::cache_invalidate(bool is_copyback) {
    if (is_copyback) {
        this->add_stat_invalidation_copyback();
    }
    else {
        this->add_stat_invalidation();
    }
};

/// ============================================================================
void cache_memory_t::cache_evict(bool is_copyback) {
    if (is_copyback) {
        this->add_stat_eviction_copyback();
    }
    else {
        this->add_stat_eviction();
    }
};



/// ============================================================================
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

/// ============================================================================
// Input: memory_address
// Output: cache_line_t*, &index, &way
cache_line_t* cache_memory_t::evict_address(uint64_t memory_address, uint32_t& index, uint32_t& choosen_way) {
    cache_line_t *choosen_line = NULL;

    index = get_index(memory_address);

    switch (this->replacement_policy) {
        case REPLACEMENT_LRU: {
            uint64_t last_access = sinuca_engine.get_global_cycle() + 1;
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
            uint64_t last_access = sinuca_engine.get_global_cycle() + 1;
            for (uint32_t way = 0; way < this->get_associativity(); way++) {
                /// The line is not locked by directory
                if (sinuca_engine.directory_controller->is_locked(this->sets[index].ways[way].tag) == FAIL) {
                    /// If is_dead
                    if (this->line_usage_predictor->check_line_is_last_access(index, way)) {
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

        case REPLACEMENT_INVALID_OR_LRU: {
            uint64_t last_access = sinuca_engine.get_global_cycle() + 1;
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

    return choosen_line;
};


/// ============================================================================
void cache_memory_t::change_address(cache_line_t *line, uint64_t new_memory_address) {
    ERROR_ASSERT_PRINTF(line != NULL, "Cannot change the tag address of a NULL line.\n")
    line->tag = new_memory_address;
    return;
};


/// ============================================================================
void cache_memory_t::change_status(cache_line_t *line, protocol_status_t status) {
    ERROR_ASSERT_PRINTF(line != NULL, "Cannot change the status of a NULL line.\n")
    line->status = status;
    return;
};

/// ============================================================================
void cache_memory_t::update_last_access(cache_line_t *line) {
    ERROR_ASSERT_PRINTF(line != NULL, "Cannot change the last_access of a NULL line.\n")
    line->last_access = sinuca_engine.get_global_cycle();
    line->usage_counter++;
    return;
};



/// ============================================================================
void cache_memory_t::print_structures() {
    SINUCA_PRINTF("%s MSHR_BUFFER:\n%s", this->get_label(), memory_package_t::print_all(this->mshr_buffer, this->mshr_buffer_size).c_str())
};

/// ============================================================================
void cache_memory_t::panic() {
    this->print_structures();
    this->prefetcher->panic();
    this->line_usage_predictor->panic();
};

/// ============================================================================
void cache_memory_t::periodic_check(){
    #ifdef CACHE_DEBUG
        CACHE_DEBUG_PRINTF("\n");
        this->print_structures();
    #endif
    ERROR_ASSERT_PRINTF(memory_package_t::check_age(this->mshr_buffer, this->mshr_buffer_size) == OK, "Check_age failed.\n");
    this->prefetcher->periodic_check();
    this->line_usage_predictor->periodic_check();
};


/// ============================================================================
/// STATISTICS
/// ============================================================================
void cache_memory_t::reset_statistics() {
    this->set_stat_accesses(0);
    this->set_stat_invalidation(0);
    this->set_stat_invalidation_copyback(0);
    this->set_stat_eviction(0);
    this->set_stat_eviction_copyback(0);

    this->set_stat_instruction_hit(0);
    this->set_stat_read_hit(0);
    this->set_stat_prefetch_hit(0);
    this->set_stat_write_hit(0);
    this->set_stat_copyback_hit(0);

    this->set_stat_instruction_miss(0);
    this->set_stat_read_miss(0);
    this->set_stat_prefetch_miss(0);
    this->set_stat_write_miss(0);
    this->set_stat_copyback_miss(0);

    this->stat_min_instruction_wait_time = 0;
    this->stat_max_instruction_wait_time = 0;
    this->stat_acumulated_instruction_wait_time = 0;

    this->stat_min_read_wait_time = 0;
    this->stat_max_read_wait_time = 0;
    this->stat_acumulated_read_wait_time = 0;

    this->stat_min_prefetch_wait_time = 0;
    this->stat_max_prefetch_wait_time = 0;
    this->stat_acumulated_prefetch_wait_time = 0;

    this->stat_min_write_wait_time = 0;
    this->stat_max_write_wait_time = 0;
    this->stat_acumulated_write_wait_time = 0;

    this->stat_min_copyback_wait_time = 0;
    this->stat_max_copyback_wait_time = 0;
    this->stat_acumulated_copyback_wait_time = 0;

    this->stat_full_mshr_buffer_request = 0;
    this->stat_full_mshr_buffer_copyback = 0;
    this->stat_full_mshr_buffer_prefetch = 0;

    this->prefetcher->reset_statistics();
    this->line_usage_predictor->reset_statistics();
};

/// ============================================================================
void cache_memory_t::print_statistics() {
    char title[100] = "";
    sprintf(title, "Statistics of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_accesses", stat_accesses);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_invalidation", stat_invalidation);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_invalidation_copyback", stat_invalidation_copyback);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_eviction", stat_eviction);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_eviction_copyback", stat_eviction_copyback);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_instruction_hit", stat_instruction_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_read_hit", stat_read_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_prefetch_hit", stat_prefetch_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_write_hit", stat_write_hit);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_copyback_hit", stat_copyback_hit);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_instruction_miss", stat_instruction_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_read_miss", stat_read_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_prefetch_miss", stat_prefetch_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_write_miss", stat_write_miss);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_copyback_miss", stat_copyback_miss);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_instruction_miss_percentage",stat_instruction_miss, stat_instruction_miss + stat_instruction_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_read_miss_percentage",stat_read_miss, stat_read_miss + stat_read_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_prefetch_miss_percentage",stat_prefetch_miss, stat_prefetch_miss + stat_prefetch_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_write_miss_percentage",stat_write_miss, stat_write_miss + stat_write_hit);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_instruction_wait_time", stat_min_instruction_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_read_wait_time", stat_min_read_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_prefetch_wait_time", stat_min_prefetch_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_write_wait_time", stat_min_write_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_min_copyback_wait_time", stat_min_copyback_wait_time);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_instruction_wait_time", stat_max_instruction_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_read_wait_time", stat_max_read_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_prefetch_wait_time", stat_max_prefetch_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_write_wait_time", stat_max_write_wait_time);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_max_copyback_wait_time", stat_max_copyback_wait_time);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_acumulated_instruction_wait_time_ratio",stat_acumulated_instruction_wait_time, stat_instruction_miss);
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_acumulated_read_wait_time_ratio",stat_acumulated_read_wait_time, stat_read_miss);
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_acumulated_prefetch_wait_time_ratio",stat_acumulated_prefetch_wait_time, stat_prefetch_miss);
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_acumulated_write_wait_time_ratio",stat_acumulated_write_wait_time, stat_write_miss);
    sinuca_engine.write_statistics_value_ratio(get_type_component_label(), get_label(), "stat_acumulated_copyback_wait_time_ratio",stat_acumulated_copyback_wait_time, stat_copyback_miss);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_full_mshr_buffer_request", stat_full_mshr_buffer_request);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_full_mshr_buffer_copyback", stat_full_mshr_buffer_copyback);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_full_mshr_buffer_prefetch", stat_full_mshr_buffer_prefetch);


    this->prefetcher->print_statistics();
    this->line_usage_predictor->print_statistics();
};

/// ============================================================================
/// ============================================================================
void cache_memory_t::print_configuration() {
    char title[100] = "";
    sprintf(title, "Configuration of %s", this->get_label());
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

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_buffer_request_reserved_size", mshr_buffer_request_reserved_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_buffer_copyback_reserved_size", mshr_buffer_copyback_reserved_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_buffer_prefetch_reserved_size", mshr_buffer_prefetch_reserved_size);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "mshr_buffer_size", mshr_buffer_size);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "address_mask_type", get_enum_cache_mask_char(address_mask_type));
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "tag_bits_mask", utils_t::address_to_binary(this->tag_bits_mask).c_str());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "index_bits_mask", utils_t::address_to_binary(this->index_bits_mask).c_str());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "bank_bits_mask", utils_t::address_to_binary(this->bank_bits_mask).c_str());
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "offset_bits_mask", utils_t::address_to_binary(this->offset_bits_mask).c_str());

    this->prefetcher->print_configuration();
    this->line_usage_predictor->print_configuration();
};