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

#ifdef DIRECTORY_CTRL_DEBUG
    #define DIRECTORY_CTRL_DEBUG_PRINTF(...) DEBUG_PRINTF(__VA_ARGS__);
#else
    #define DIRECTORY_CTRL_DEBUG_PRINTF(...)
#endif

// ============================================================================
directory_controller_t::directory_controller_t() {
    this->set_type_component(COMPONENT_DIRECTORY_CONTROLLER);

    this->coherence_protocol_type = COHERENCE_PROTOCOL_MOESI;
    this->inclusiveness_type = INCLUSIVENESS_NON_INCLUSIVE;

    ERROR_ASSERT_PRINTF(utils_t::check_if_power_of_two(sinuca_engine.get_global_line_size()), "Wrong line_size.\n");
    this->not_offset_bits_mask = ~utils_t::fill_bit(0, utils_t::get_power_of_two(sinuca_engine.get_global_line_size()) - 1);

    this->generate_llc_writeback = true;
    this->generate_non_llc_writeback = true;
    this->final_writeback_all = true;

    this->max_cache_level = 0;
};

// ============================================================================
directory_controller_t::~directory_controller_t() {
    /// De-Allocate memory to prevent memory leak
    this->directory_lines.clear();
    for (uint32_t i = 0; i < this->directory_lines.size(); i++) {
        WARNING_PRINTF("Directory was not empty.\n")
        directory_line_t *directory_line = this->directory_lines[i];
        utils_t::template_delete_variable<directory_line_t>(directory_line);
    }
};

// ============================================================================
void directory_controller_t::allocate() {
    if (this->generate_non_llc_writeback == false && this->generate_llc_writeback == true) {
        ERROR_PRINTF("NON-LLC writeback disabled will not generate writebacks to the LLC\n")
    }

    uint32_t sum_mshr_buffer_size = 0;
    this->max_cache_level = 0;

    /// Find all LLC
    for (uint32_t i = 0; i < sinuca_engine.get_cache_memory_array_size(); i++) {
        cache_memory_t *cache_memory = sinuca_engine.cache_memory_array[i];
        container_ptr_cache_memory_t *lower_level_cache = cache_memory->get_lower_level_cache();

        /// Found LLC
        if (lower_level_cache->empty()) {
            sum_mshr_buffer_size += cache_memory->get_mshr_buffer_size();
            this->llc_caches.push_back(cache_memory);
        }

        /// Find Max Cache Level
        if (this->max_cache_level < cache_memory->get_hierarchy_level()) {
            this->max_cache_level = cache_memory->get_hierarchy_level();
        }

    }
    this->directory_lines.reserve(sum_mshr_buffer_size);

    // Addres Mapping to Mem.Ctrl.
    this->address_mapping();

};

// ============================================================================
void directory_controller_t::address_mapping(){

    /// If user defined address mapping file, then generate hash mapping
    if (sinuca_engine.arg_map_file_name != NULL) {

        /// PAGE MASK
        this->page_bits_shift = utils_t::get_power_of_two(PAGE_SIZE);

        /// Open Map File
        gzFile gzMapFile = gzopen(sinuca_engine.arg_map_file_name, "ro");   /// Open the .gz file
        ERROR_ASSERT_PRINTF(gzMapFile != NULL, "Could not open the file.\n%s\n", sinuca_engine.arg_map_file_name);

        DIRECTORY_CTRL_DEBUG_PRINTF("Map File = %s => READY !\n", sinuca_engine.arg_map_file_name);

        gzclearerr(gzMapFile);
        gzseek(gzMapFile, 0, SEEK_SET);   /// Go to the Begin of the File
        ERROR_ASSERT_PRINTF(!gzeof(gzMapFile), "Static File Unexpected EOF.\n")

        char *line_map = utils_t::template_allocate_array<char>(TRACE_LINE_SIZE);

        /// Read map file generating Hash table
        while (!gzeof(gzMapFile)) {
            char *buffer = gzgets(gzMapFile, line_map, TRACE_LINE_SIZE);
            char *sub_string = NULL;
            char *tmp_ptr = NULL;
            uint32_t count = 0;
            uint64_t pageaddr = 0;
            uint32_t memctrl = 0;

            if (line_map[0] == '\0' || line_map[0] == '#' || buffer == NULL) {     /// If Comment, then ignore
                continue;
            }
            else {
                for (uint32_t i = 0; line_map[i] != '\0' && i < TRACE_LINE_SIZE; i++) {
                    count += (line_map[i] == ' ');
                }
                ERROR_ASSERT_PRINTF(count == 1, "Error converting Mapping file (Wrong  number of fields %d) \n%s\n", count, line_map)

                sub_string = strtok_r(line_map, " ", &tmp_ptr);
                pageaddr = utils_t::string_to_uint64(sub_string);
                ERROR_ASSERT_PRINTF(this->mapped_controller.find(pageaddr) == this->mapped_controller.end(),
                                    "Multiple Mapping to the same pageaddr %" PRIu64 "\n", pageaddr )

                sub_string = strtok_r(NULL, " ", &tmp_ptr);
                memctrl = utils_t::string_to_uint32(sub_string)-1;
                ERROR_ASSERT_PRINTF(memctrl < sinuca_engine.memory_controller_array_size,
                                    "Error converting Mapping file (Wrong  number of Mem. Ctrl. %" PRIu32 ")\n", memctrl)
                this->mapped_controller[pageaddr] = memctrl;
            }
        }
        gzclose(gzMapFile);
        utils_t::template_delete_array<char>(line_map);
    }
};

// ============================================================================
void directory_controller_t::clock(uint32_t subcycle) {
    if (subcycle != 0) return;
    DIRECTORY_CTRL_DEBUG_PRINTF("==================== ");
    DIRECTORY_CTRL_DEBUG_PRINTF("====================\n");
    DIRECTORY_CTRL_DEBUG_PRINTF("cycle() \n");
};


// ============================================================================
int32_t directory_controller_t::send_package(memory_package_t *package) {
    ERROR_PRINTF("Send package %s.\n", package->content_to_string().c_str());
    return POSITION_FAIL;
};

// ============================================================================
bool directory_controller_t::receive_package(memory_package_t *package, uint32_t input_port, uint32_t transmission_latency) {
    ERROR_PRINTF("Received package %s into the input_port %u, latency %u.\n", package->content_to_string().c_str(), input_port, transmission_latency);
    return FAIL;
};

// ============================================================================
/// Token Controller Methods
// ============================================================================
bool directory_controller_t::pop_token_credit(uint32_t src_id, memory_operation_t memory_operation) {
    ERROR_PRINTF("pop_token_credit %" PRIu32 " %s.\n", src_id, get_enum_memory_operation_char(memory_operation))
    return FAIL;
};

// ====================================================================================
/*! This method will take care about the inclusiveness and the cache coherence for all
 *  the cache operations.
 */
package_state_t directory_controller_t::treat_cache_request(uint32_t cache_id, memory_package_t *package) {
    DIRECTORY_CTRL_DEBUG_PRINTF("new_cache_request() cache_id:%u, package:%s\n", cache_id, package->content_to_string().c_str())
    ERROR_ASSERT_PRINTF(cache_id < sinuca_engine.get_cache_memory_array_size(), "Wrong cache_id.\n")


    if (package->memory_operation == MEMORY_OPERATION_HMC_ALU  ||
        package->memory_operation == MEMORY_OPERATION_HMC_ALUR ) {

        /// Get CACHE pointer
        cache_memory_t *cache = sinuca_engine.cache_memory_array[cache_id];
        directory_line_t *directory_line = NULL;
        int32_t directory_line_number = POSITION_FAIL;

        if (cache->get_hierarchy_level() != 1) {
            directory_line_number = find_directory_line(package);
            ERROR_ASSERT_PRINTF(directory_line_number != POSITION_FAIL,
                                "High level RQST must have a directory_line.\n. cache_id:%u, package:%s\n",
                                cache->get_id(), package->content_to_string().c_str())
            directory_line = this->directory_lines[directory_line_number];
        }
        /// Check for LOCK
        else {
            for (uint32_t i = 0; i < this->directory_lines.size(); i++) {
                /// Transaction on the same address was found
                if (this->cmp_index_tag(this->directory_lines[i]->initial_memory_address, package->memory_address)) {

                    // HMC lock
                    if (this->directory_lines[i]->initial_memory_operation == MEMORY_OPERATION_HMC_ALU ||
                    this->directory_lines[i]->initial_memory_operation == MEMORY_OPERATION_HMC_ALUR){
                        break;
                    }

                    ERROR_ASSERT_PRINTF(directory_lines[i]->lock_type != LOCK_FREE, "Found directory with LOCK_FREE\n");

                    /// Cannot continue right now
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED (Found incompatible LOCK)\n")
                    SINUCA_PRINTF("\t RETURN UNTREATED (Found incompatible LOCK)\n")
                    return PACKAGE_STATE_UNTREATED;
                }
            }
            ERROR_ASSERT_PRINTF(directory_line == NULL,
                                "This level RQST must not have a directory_line.\n cache_id:%u, package:%s\n",
                                cache->get_id(), package->content_to_string().c_str())
        }


        /// Get CACHE_LINE, INDEX, WAY
        uint32_t index, way;
        cache_line_t *cache_line = cache->find_line(package->memory_address, index, way);
        if (cache_line != NULL) {
            // =====================================================
            /// Takes care about INCLUSIVENESS / WRITEBACK
            // =====================================================
            if (this->inclusiveness_new_eviction(cache, cache_line, index, way, package) == FAIL) {
                /// Cannot continue right now
                DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED (Cache_line == NULL)\n")
                SINUCA_PRINTF("\t RETURN UNTREATED (Cache_line == NULL) could not evict-inclusiveness\n")
                return PACKAGE_STATE_UNTREATED;
            }
            /// Coherence Invalidate
            cache->change_status(cache_line, PROTOCOL_STATUS_I);
        }

        /// The request can be treated now !
        /// New Directory_Line + LOCK
        if (directory_line == NULL) {
            this->directory_lines.push_back(new directory_line_t());
            directory_line = this->directory_lines.back();

            directory_line->packager(package->id_owner, package->opcode_number, package->opcode_address, package->uop_number,
                                        LOCK_WRITE,
                                        package->memory_operation, package->memory_address, package->memory_size);
            DIRECTORY_CTRL_DEBUG_PRINTF("\t New Directory Line:%s\n", directory_line->directory_line_to_string().c_str())
        }
        /// Update existing Directory_Line
        directory_line->cache_request_order[cache_id] = ++directory_line->cache_requested;

        /// LATENCY = NONE
        package->ready_cycle = sinuca_engine.get_global_cycle();

        package->package_set_src_dst(cache->get_id(), this->find_next_obj_id(cache, package->memory_address));

        DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN TRANSMIT RQST (Miss)\n")
        return PACKAGE_STATE_TRANSMIT;
    }


    // NORMAL DIRECTORY OPERATION ==============================================
    /// Get CACHE pointer
    cache_memory_t *cache = sinuca_engine.cache_memory_array[cache_id];

    /// Inspect IS_READ
    bool is_read = this->coherence_is_read(package->memory_operation);

    // ================================================================================
    /// Find the existing directory line
    directory_line_t *directory_line = NULL;
    int32_t directory_line_number = POSITION_FAIL;
    /// If NOT Cache L1 (New directory line may already exist)
    if (cache->get_hierarchy_level() != 1 && cache->get_id() != package->id_owner) {
        directory_line_number = find_directory_line(package);
        ERROR_ASSERT_PRINTF(directory_line_number != POSITION_FAIL,
                            "High level RQST must have a directory_line.\n. cache_id:%u, package:%s\n",
                            cache->get_id(), package->content_to_string().c_str())
        directory_line = this->directory_lines[directory_line_number];
    }
    /// Check for LOCK
    else {
        for (uint32_t i = 0; i < this->directory_lines.size(); i++) {
            /// Transaction on the same address was found
            if (this->cmp_index_tag(this->directory_lines[i]->initial_memory_address, package->memory_address)){
                ERROR_ASSERT_PRINTF(directory_lines[i]->lock_type != LOCK_FREE, "Found directory with LOCK_FREE\n");

                /// If READ     Need LOCK_FREE or LOCK_READ    => LOCK_READ
                if (is_read && directory_lines[i]->lock_type == LOCK_READ) {
                    break;
                }
                /// If WRITE    Need LOCK_FREE or WRITE in this same cache  => LOCK_WRITE
                else if (!is_read && directory_lines[i]->lock_type == LOCK_WRITE && this->directory_lines[i]->id_owner == package->id_owner) {
                    break;
                }
                else {
                    /// Cannot continue right now
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED (Found incompatible LOCK)\n")
                    return PACKAGE_STATE_UNTREATED;
                }
            }
        }
        ERROR_ASSERT_PRINTF(directory_line == NULL,
                            "This level RQST must not have a directory_line.\n cache_id:%u, package:%s\n",
                            cache->get_id(), package->content_to_string().c_str())
    }

    /// Get CACHE_LINE, INDEX, WAY
    uint32_t index, way;
    cache_line_t *cache_line = cache->find_line(package->memory_address, index, way);

    // ================================================================================
    /// Takes care about Parallel Requests at the same Cache Level
    // ================================================================================
    /// Check for parallel requests
    for (uint32_t i = 0; i < this->directory_lines.size(); i++) {
        /// Find Parallel Request (Cannot be an Writeback operation leaving the cache)
        if (directory_lines[i]->cache_request_order[cache_id] != 0 &&
        this->cmp_index_tag(directory_lines[i]->initial_memory_address, package->memory_address)) {
            // =============================================================
            // Line Usage Prediction => Check if the subblocks were requested
            bool line_is_disabled = cache->line_usage_predictor->check_line_is_disabled(cache, cache_line, index, way);
            if (!line_is_disabled &&
            directory_lines[i]->initial_memory_operation != MEMORY_OPERATION_WRITEBACK) {
                // =============================================================
                // Line Usage Prediction => The statistics between the cache and our mechanism will be different
                cache->line_usage_predictor->line_hit(cache, cache_line, package, index, way);
            }
            else {
                /// A new request needs to be generated in the future
                return PACKAGE_STATE_UNTREATED;
            }

            /// No Directory_line yet => create
            if (directory_line == NULL) {
                this->directory_lines.push_back(new directory_line_t());
                directory_line = this->directory_lines.back();

                directory_line->packager(package->id_owner, package->opcode_number, package->opcode_address, package->uop_number,
                                            is_read ? LOCK_READ : LOCK_WRITE,
                                            package->memory_operation, package->memory_address, package->memory_size);
                DIRECTORY_CTRL_DEBUG_PRINTF("\t New Directory Line:%s\n", directory_line->directory_line_to_string().c_str())
            }
            /// Update existing Directory_Line
            directory_line->cache_request_order[cache_id] = ++directory_line->cache_requested;

            DIRECTORY_CTRL_DEBUG_PRINTF("\t Parallel Request:%s\n", directory_line->directory_line_to_string().c_str())

            if (package->memory_operation == MEMORY_OPERATION_WRITE) {
                package->memory_operation = MEMORY_OPERATION_READ;
            }

            /// Sit and wait for a answer for the same line
            DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN WAIT (Parallel Request)\n")
            return PACKAGE_STATE_WAIT;
        }
    }


    // ================================================================================
    /// Takes care about the CACHE HIT/MISS
    // ================================================================================
    switch (package->memory_operation) {
        // =====================================================================
        // Receiving a wrong HMC
        case MEMORY_OPERATION_HMC_ALU:
        case MEMORY_OPERATION_HMC_ALUR:
        {
            /// Send the message to the next level without latency
            ERROR_PRINTF("Found a HMC in a wrong part of directory_controller_t::treat_cache_request()\n");
        }


        // =====================================================================
        /// READ and WRITE
        case MEMORY_OPERATION_READ:
        case MEMORY_OPERATION_INST:
        case MEMORY_OPERATION_PREFETCH:
        case MEMORY_OPERATION_WRITE:
        {
            /// Inspect IS_HIT
            bool is_tag_hit = (cache_line != NULL && this->coherence_is_hit(cache_line->status));
            bool line_is_disabled = true;
            if (is_tag_hit) {
                // =============================================================
                // Line Usage Prediction
                line_is_disabled = cache->line_usage_predictor->check_line_is_disabled(cache, cache_line, index, way);
            }
            // =================================================================
            /// TAG hit *AND* SUB-BLOCK hit
            // =================================================================
            if (!line_is_disabled) {
                /// From this point the package is transfering a cache line size
                package->memory_size = sinuca_engine.get_global_line_size();

                // =============================================================
                // Line Usage Prediction
                cache->line_usage_predictor->line_hit(cache, cache_line, package, index, way);
                /// Update Coherence Status and Last Access Time
                this->coherence_new_operation(cache, cache_line, package, true);
                /// Update Statistics
                this->new_statistics(cache, package->memory_operation, true);

                /// Early Writeback?
                // =============================================================
                // Line Usage Prediction
                if (coherence_is_dirty(cache_line->status) && cache->line_usage_predictor->check_line_is_last_write(cache, cache_line, index, way)) {

                    memory_package_t *writeback_package = this->create_cache_writeback(cache, cache_line);

                    if (writeback_package != NULL) {
                        DIRECTORY_CTRL_DEBUG_PRINTF("\t Early Writeback cache_id:%u, package:%s\n", cache->get_id(), writeback_package->content_to_string().c_str())
                        /// Add statistics to the cache
                        cache->add_stat_writeback();
                        /// Update Coherence Status and Last Access Time
                        this->coherence_new_operation(cache, cache_line, writeback_package, false);
                        /// Update Statistics
                        this->new_statistics(cache, writeback_package->memory_operation, false);
                        // =============================================================
                        // Line Usage Prediction
                        cache->line_usage_predictor->line_send_writeback(cache, cache_line, writeback_package, index, way);
                    }
                }

                /// THIS cache level started the request (PREFETCH)
                if (package->id_owner == cache->get_id()) {
                    /// Add Latency
                    package->ready_cycle = sinuca_engine.get_global_cycle() + cache->get_penalty_read();
                    /// Erase the answer
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN FREE (Requester = This)\n")
                    return PACKAGE_STATE_READY;
                }
                /// Need to send answer to higher level
                else {
                    /// WRITES never sends answer
                    if (package->memory_operation == MEMORY_OPERATION_WRITE) {
                        /// Add Latency
                        package->ready_cycle = sinuca_engine.get_global_cycle() + cache->get_penalty_write();
                        /// Erase the package
                        DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN FREE (WRITE Done)\n")
                        return PACKAGE_STATE_READY;
                    }
                    else {
                        /// Add Latency
                        package->ready_cycle = sinuca_engine.get_global_cycle() + cache->get_penalty_read();
                        /// Send ANSWER
                        package->is_answer = true;
                        package->package_set_src_dst(cache->get_id(), package->id_src);

                        DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN TRANSMIT (Hit)\n")
                        return PACKAGE_STATE_TRANSMIT;
                    }
                }
            }
            // =================================================================
            /// TAG miss *OR* SUB-BLOCK miss
            // =================================================================
            else {
                /// Tag is not allocated
                if (!is_tag_hit) {
                    // =================================================================
                    /// Make room for the new cache line
                    // =================================================================
                    /// Do not have a line with same TAG
                    if (cache_line == NULL) {
                        cache_line = cache->evict_address(package->memory_address, index, way);
                        /// Could not evict any line
                        if (cache_line == NULL) {
                            /// Cannot continue right now
                            DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED (Cache_line == NULL)\n")
                            return PACKAGE_STATE_UNTREATED;
                        }
                        /// Found a line to evict
                        // =====================================================
                        /// Takes care about INCLUSIVENESS / WRITEBACK
                        // =====================================================
                        if (this->inclusiveness_new_eviction(cache, cache_line, index, way, package) == FAIL) {
                            /// Cannot continue right now
                            DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED (Cache_line == NULL)\n")
                            return PACKAGE_STATE_UNTREATED;
                        }
                    }
                }

                /// Reserve the evicted line for the new address
                cache->change_address(cache_line, package->memory_address);
                /// Coherence Invalidate
                cache->change_status(cache_line, PROTOCOL_STATUS_I);
                /// Update the last access time
                cache->update_last_access(cache_line);

                /// The request can be treated now !
                /// New Directory_Line + LOCK
                if (directory_line == NULL) {
                    this->directory_lines.push_back(new directory_line_t());
                    directory_line = this->directory_lines.back();

                    directory_line->packager(package->id_owner, package->opcode_number, package->opcode_address, package->uop_number,
                                                is_read ? LOCK_READ : LOCK_WRITE,
                                                package->memory_operation, package->memory_address, package->memory_size);
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t New Directory Line:%s\n", directory_line->directory_line_to_string().c_str())
                }

                /// Update the Directory_Line
                directory_line->cache_request_order[cache_id] = ++directory_line->cache_requested;
                DIRECTORY_CTRL_DEBUG_PRINTF("\t Update Directory Line:%s\n", directory_line->directory_line_to_string().c_str())

                /// If line miss OR line_turned_off -> try to find on higher level
                if (!is_tag_hit ||
                cache->line_usage_predictor->check_line_is_disabled(NULL, NULL, index, way))
                {

                    /// Check if some HIGHER LEVEL has the cache line
                    uint32_t sum_latency = 0;
                    cache->change_status(cache_line, this->find_cache_line_higher_levels(&sum_latency, cache, package->memory_address, true));

                    /// Higher Level Hit
                    if (this->coherence_is_hit(cache_line->status)) {
                        this->add_stat_cache_to_cache();
                        if (coherence_is_dirty(cache_line->status)) {
                            // =============================================================
                            // Line Usage Prediction
                            cache->line_usage_predictor->line_recv_writeback(cache, cache_line, package, index, way);
                            cache->line_usage_predictor->line_hit(cache, cache_line, package, index, way);

                            /// Update Statistics
                            this->new_statistics(cache, MEMORY_OPERATION_WRITEBACK, true);

                            /// Early Writeback?
                            // =============================================================
                            // Line Usage Prediction
                            if (cache->line_usage_predictor->check_line_is_last_write(cache, cache_line, index, way)) {
                                memory_package_t *writeback_package = this->create_cache_writeback(cache, cache_line);
                                if (writeback_package != NULL) {
                                    DIRECTORY_CTRL_DEBUG_PRINTF("\t Early Writeback cache_id:%u, package:%s\n", cache->get_id(), writeback_package->content_to_string().c_str())
                                    /// Add statistics to the cache
                                    cache->add_stat_writeback();
                                    /// Update Coherence Status and Last Access Time
                                    this->coherence_new_operation(cache, cache_line, writeback_package, false);
                                    /// Update Statistics
                                    this->new_statistics(cache, writeback_package->memory_operation, false);
                                    // =============================================================
                                    // Line Usage Prediction
                                    cache->line_usage_predictor->line_send_writeback(cache, cache_line, writeback_package, index, way);
                                }
                            }
                        }
                        else {
                            // =============================================================
                            // Line Usage Prediction
                            if (is_tag_hit) {
                                cache->line_usage_predictor->sub_block_miss(cache, cache_line, package, index, way);
                            }
                            else {
                                cache->line_usage_predictor->line_miss(cache, cache_line, package, index, way);
                            }

                            cache->line_usage_predictor->line_hit(cache, cache_line, package, index, way);
                        }

                        /// LATENCY = CACHE-TO-CACHE = Level until it was found
                        package->ready_cycle = sinuca_engine.get_global_cycle() + sum_latency;
                        DIRECTORY_CTRL_DEBUG_PRINTF("REQUESTER: %s LAT: %d\n", cache->get_label(), (int)sum_latency);
                        /// Send Request to fill the cache line
                        if (package->memory_operation == MEMORY_OPERATION_WRITE) {
                            package->memory_operation = MEMORY_OPERATION_READ;
                        }

                        package->is_answer = true;
                        DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED ANSWER (Found Higher Level)\n")
                        return PACKAGE_STATE_UNTREATED;
                    }
                }
                /// Missing only some sub-block or Higher Level Miss

                // =============================================================
                // Line Usage Prediction
                if (is_tag_hit) {
                    cache->line_usage_predictor->sub_block_miss(cache, cache_line, package, index, way);
                }
                else {
                    cache->line_usage_predictor->line_miss(cache, cache_line, package, index, way);
                }
                cache->line_usage_predictor->line_hit(cache, cache_line, package, index, way);
                /// From this point the package is transfering a cache line size
                package->memory_size = sinuca_engine.get_global_line_size();


                /// Send Request to fill the cache line
                if (package->memory_operation == MEMORY_OPERATION_WRITE) {
                    package->memory_operation = MEMORY_OPERATION_READ;
                }

                /// LATENCY = READ
                package->ready_cycle = sinuca_engine.get_global_cycle() + cache->get_penalty_read();
                package->package_set_src_dst(cache->get_id(), this->find_next_obj_id(cache, package->memory_address));

                DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN TRANSMIT RQST (Miss)\n")
                return PACKAGE_STATE_TRANSMIT;
            }
        }
        break;


        // =====================================================================
        /// WRITE-BACK
        case MEMORY_OPERATION_WRITEBACK:
        {
            /// WRITEBACK from THIS cache => LOWER level
            ERROR_ASSERT_PRINTF(cache->get_id() != package->id_owner, "Copyback should be created using create_cache_writeback.\n")

            // =================================================================
            /// Make room for the new cache line
            // =================================================================
            /// Do not have a line with same TAG
            if (cache_line == NULL) {
                cache_line = cache->evict_address(package->memory_address, index, way);
                /// Could not evict any line
                if (cache_line == NULL) {
                    /// Cannot continue right now
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED (Cache_line == NULL)\n")
                    return PACKAGE_STATE_UNTREATED;
                }
                /// Found a line to evict
                // =====================================================
                /// Takes care about INCLUSIVENESS / WRITEBACK
                // =====================================================
                if (this->inclusiveness_new_eviction(cache, cache_line, index, way, package) == FAIL) {
                    /// Cannot continue right now
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED (Cache_line == NULL)\n")
                    return PACKAGE_STATE_UNTREATED;
                }
            }

            // =============================================================
            // Line Usage Prediction
            cache->line_usage_predictor->line_recv_writeback(cache, cache_line, package, index, way);

            /// Reserve the evicted line for the new address
            cache->change_address(cache_line, package->memory_address);
            /// Coherence Invalidate
            cache->change_status(cache_line, PROTOCOL_STATUS_I);
            /// Update Coherence Status and Last Access Time
            this->coherence_new_operation(cache, cache_line, package, true);
            /// Update Statistics
            this->new_statistics(cache, package->memory_operation, true);
            /// Add Latency
            package->ready_cycle = sinuca_engine.get_global_cycle() + cache->get_penalty_write();

            /// Early Writeback?
            // =============================================================
            // Line Usage Prediction
            if (cache->line_usage_predictor->check_line_is_last_write(cache, cache_line, index, way)) {
                memory_package_t *writeback_package = this->create_cache_writeback(cache, cache_line);
                if (writeback_package != NULL) {
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t Early Writeback cache_id:%u, package:%s\n", cache->get_id(), writeback_package->content_to_string().c_str())
                    /// Add statistics to the cache
                    cache->add_stat_writeback();
                    /// Update Coherence Status and Last Access Time
                    this->coherence_new_operation(cache, cache_line, writeback_package, false);
                    /// Update Statistics
                    this->new_statistics(cache, writeback_package->memory_operation, false);
                    // =============================================================
                    // Line Usage Prediction (Tag different from actual)
                    cache->line_usage_predictor->line_send_writeback(cache, cache_line, writeback_package, index, way);
                }
            }

            /// Erase the directory_entry
            DIRECTORY_CTRL_DEBUG_PRINTF("\t Erasing Directory Line:%s\n", directory_line->directory_line_to_string().c_str())
            utils_t::template_delete_variable<directory_line_t>(directory_line);
            directory_line = NULL;
            this->directory_lines.erase(this->directory_lines.begin() + directory_line_number);

            /// Erase the package
            DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN FREE (WRITE Done)\n")
            return PACKAGE_STATE_READY;
        }
        break;
    }

    ERROR_PRINTF("Could not treat the cache request\n")
    return PACKAGE_STATE_UNTREATED;
};


// ============================================================================
package_state_t directory_controller_t::treat_cache_answer(uint32_t cache_id, memory_package_t *package) {
    DIRECTORY_CTRL_DEBUG_PRINTF("new_cache_request() cache_id:%u, package:%s\n", cache_id, package->content_to_string().c_str())
    ERROR_ASSERT_PRINTF(cache_id < sinuca_engine.get_cache_memory_array_size(), "Wrong cache_id.\n")


    // HMC =====================================================================
    if (package->memory_operation == MEMORY_OPERATION_HMC_ALU) {
            /// Send the message to the next level without latency
            ERROR_PRINTF("Found a HMC_ALU in a wrong part of directory_controller_t::treat_cache_answer()\n");
    }
    else if (package->memory_operation == MEMORY_OPERATION_HMC_ALUR) {

        /// Get CACHE pointer
        cache_memory_t *cache = sinuca_engine.cache_memory_array[cache_id];

        /// Find the existing directory line.
        directory_line_t *directory_line = NULL;
        int32_t directory_line_number = find_directory_line(package);
        if (directory_line_number != POSITION_FAIL) {
            directory_line = this->directory_lines[directory_line_number];
        }
        ERROR_ASSERT_PRINTF(directory_line != NULL, "Higher level REQUEST must have a directory_line\n")

        /// Update existing Directory_Line
        ERROR_ASSERT_PRINTF(directory_line->cache_request_order[cache_id] == directory_line->cache_requested, "Wrong Cache Request Order\n")
        directory_line->cache_requested--;
        directory_line->cache_request_order[cache_id] = 0;
        DIRECTORY_CTRL_DEBUG_PRINTF("\t Update Directory Line:%s\n", directory_line->directory_line_to_string().c_str())

        // ============================================================================
        /// This is the FIRST cache to receive the request
        if (directory_line->cache_requested == 0) {
            /// Get the DST ID
            package->package_set_src_dst(cache->get_id(), directory_line->id_owner);

            package->memory_operation = directory_line->initial_memory_operation;
            package->memory_address = directory_line->initial_memory_address;
            package->memory_size = directory_line->initial_memory_size;

            /// Erase the directory_entry
            DIRECTORY_CTRL_DEBUG_PRINTF("\t Erasing Directory Line:%s\n", directory_line->directory_line_to_string().c_str())
            utils_t::template_delete_variable<directory_line_t>(directory_line);
            directory_line = NULL;
            this->directory_lines.erase(this->directory_lines.begin() + directory_line_number);

            /// Send the package answer
            DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN TRANSMIT ANS (First Cache Requested)\n")

            return PACKAGE_STATE_TRANSMIT;
        }
        // ============================================================================
        /// This is NOT the FIRST cache to receive the request
        else {
            for (uint32_t i = 0; i < sinuca_engine.cache_memory_array_size; i++) {
                /// Found the previous requester
                if (directory_line->cache_request_order[i] == directory_line->cache_requested) {
                    /// Get the DST ID
                    package->package_set_src_dst(cache->get_id(), sinuca_engine.cache_memory_array[i]->get_id());

                    /// Send the package answer
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN TRANSMIT ANS (NOT First Cache Requested)\n")

                    return PACKAGE_STATE_TRANSMIT;
                }
            }
            ERROR_PRINTF("Could not find the processor that requested a HMC.\n")
        }
    }

    /// Get CACHE pointer
    cache_memory_t *cache = sinuca_engine.cache_memory_array[cache_id];

    /// Find the existing directory line.
    directory_line_t *directory_line = NULL;
    int32_t directory_line_number = find_directory_line(package);
    if (directory_line_number != POSITION_FAIL) {
        directory_line = this->directory_lines[directory_line_number];
    }
    ERROR_ASSERT_PRINTF(directory_line != NULL, "Higher level REQUEST must have a directory_line\n")

    // ================================================================================
    /// Takes care about Parallel Requests at the same Cache Level
    // ================================================================================
    /// Get CACHE_MSHR
    memory_package_t *cache_mshr_buffer = cache->get_mshr_buffer();
    /// Get CACHE_MSHR_SIZE
    uint32_t cache_mshr_buffer_size = cache->get_mshr_buffer_size();

    /// Wake up all requests waiting in the cache_mshr
    for (uint32_t i = 0; i < cache_mshr_buffer_size; i++) {
        if (cache_mshr_buffer[i].state == PACKAGE_STATE_WAIT &&
        this->cmp_index_tag(cache_mshr_buffer[i].memory_address, package->memory_address)) {
            cache_mshr_buffer[i].is_answer = true;
            cache_mshr_buffer[i].package_untreated(0);
        }
    }

    // ================================================================================
    /// Takes care about the Coherence Update and Answer
    // ================================================================================
    /// Get CACHE_LINE
    uint32_t index, way;
    cache_line_t *cache_line = cache->find_line(package->memory_address, index, way);

    // ================================================================================
    /// THIS cache level generated the request (PREFETCH)
    if (directory_line->id_owner == cache->get_id()) {
        /// Erase the directory_entry
        DIRECTORY_CTRL_DEBUG_PRINTF("\t Erasing Directory Line:%s\n", directory_line->directory_line_to_string().c_str())
        utils_t::template_delete_variable<directory_line_t>(directory_line);
        directory_line = NULL;
        this->directory_lines.erase(this->directory_lines.begin() + directory_line_number);
        /// Update Coherence Status
        this->coherence_new_operation(cache, cache_line, package, false);
        /// Update Statistics
        this->new_statistics(cache, package->memory_operation, false);
        /// Erase the package
        DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN FREE (Requester = This)\n")
        return PACKAGE_STATE_READY;
    }
    /// Need to send answer to higher level
    else {
        /// Update existing Directory_Line
        ERROR_ASSERT_PRINTF(directory_line->cache_request_order[cache_id] == directory_line->cache_requested, "Wrong Cache Request Order\n")
        directory_line->cache_requested--;
        directory_line->cache_request_order[cache_id] = 0;
        DIRECTORY_CTRL_DEBUG_PRINTF("\t Update Directory Line:%s\n", directory_line->directory_line_to_string().c_str())

        // ============================================================================
        /// This is the FIRST cache to receive the request
        if (directory_line->cache_requested == 0) {
            /// Get the DST ID
            package->package_set_src_dst(cache->get_id(), directory_line->id_owner);

            package->memory_operation = directory_line->initial_memory_operation;
            package->memory_address = directory_line->initial_memory_address;
            package->memory_size = directory_line->initial_memory_size;
            /// Erase the directory_entry
            DIRECTORY_CTRL_DEBUG_PRINTF("\t Erasing Directory Line:%s\n", directory_line->directory_line_to_string().c_str())
            utils_t::template_delete_variable<directory_line_t>(directory_line);
            directory_line = NULL;
            this->directory_lines.erase(this->directory_lines.begin() + directory_line_number);
            /// Update Coherence Status
            this->coherence_new_operation(cache, cache_line, package, false);
            /// Update Statistics
            this->new_statistics(cache, package->memory_operation, false);

            if (package->memory_operation == MEMORY_OPERATION_WRITE) {
                /// Add Latency
                package->ready_cycle = sinuca_engine.get_global_cycle() + cache->get_penalty_write();
                /// Erase the package
                DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN FREE (WRITE Done)\n")
                return PACKAGE_STATE_READY;
            }
            else {
                /// Send the package answer
                DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN TRANSMIT ANS (First Cache Requested)\n")

                return PACKAGE_STATE_TRANSMIT;
            }
        }

        // ============================================================================
        /// This is NOT the FIRST cache to receive the request
        else {
            for (uint32_t i = 0; i < sinuca_engine.cache_memory_array_size; i++) {
                /// Found the previous requester
                if (directory_line->cache_request_order[i] == directory_line->cache_requested) {
                    /// Get the DST ID
                    package->package_set_src_dst(cache->get_id(), sinuca_engine.cache_memory_array[i]->get_id());
                    /// Update Coherence Status
                    this->coherence_new_operation(cache, cache_line, package, false);
                    /// Update Statistics
                    this->new_statistics(cache, package->memory_operation, false);
                    /// Send the package answer
                    DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN TRANSMIT ANS (NOT First Cache Requested)\n")

                    return PACKAGE_STATE_TRANSMIT;
                }
            }
        }
    }

    ERROR_PRINTF("Could not treat the cache answer\n")
    return PACKAGE_STATE_UNTREATED;
};


// ============================================================================
package_state_t directory_controller_t::treat_cache_request_sent(uint32_t cache_id, memory_package_t *package) {
    DIRECTORY_CTRL_DEBUG_PRINTF("new_cache_request() cache_id:%u, package:%s\n", cache_id, package->content_to_string().c_str())
    ERROR_ASSERT_PRINTF(cache_id < sinuca_engine.get_cache_memory_array_size(), "Wrong cache_id.\n")

    switch (package->memory_operation) {

        // =============================================================
        // HMC
        case MEMORY_OPERATION_HMC_ALUR:

        /// READ and WRITE
        case MEMORY_OPERATION_READ:
        case MEMORY_OPERATION_INST:
        case MEMORY_OPERATION_PREFETCH:
        case MEMORY_OPERATION_WRITE:
            return PACKAGE_STATE_WAIT;
        break;

        // =============================================================
        // HMC
        case MEMORY_OPERATION_HMC_ALU:
        /// WRITEBACK
        case MEMORY_OPERATION_WRITEBACK:
        {
            /// Get CACHE pointer
            cache_memory_t *cache = sinuca_engine.cache_memory_array[cache_id];

            /// Check if request sent to main_memory (memory_controller)
            container_ptr_cache_memory_t *lower_level_cache = cache->get_lower_level_cache();
            if (lower_level_cache->empty()) {
                /// Find the directory entry
                int32_t directory_line_number = find_directory_line(package);
                ERROR_ASSERT_PRINTF(directory_line_number != POSITION_FAIL, "High level RQST must have a directory_line.\n. cache_id:%u, package:%s\n",
                                    cache->get_id(), package->content_to_string().c_str())

                /// Obtain the directory entry
                directory_line_t *directory_line = this->directory_lines[directory_line_number];

                /// Erase the directory_entry
                DIRECTORY_CTRL_DEBUG_PRINTF("\t Erasing Directory Line:%s\n", directory_line->directory_line_to_string().c_str())
                utils_t::template_delete_variable<directory_line_t>(directory_line);
                directory_line = NULL;
                this->directory_lines.erase(this->directory_lines.begin() + directory_line_number);
            }
            /// Erase the package
            DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN FREE (Requester = This)\n")
            return PACKAGE_STATE_READY;
        }
        break;

    }

    ERROR_PRINTF("Could not treat the cache answer\n")
    return PACKAGE_STATE_UNTREATED;
};


// ============================================================================
int32_t directory_controller_t::find_directory_line(memory_package_t *package) {
    for (uint32_t i = 0; i < this->directory_lines.size(); i++) {
        /// Requested Address Found
        if (this->directory_lines[i]->id_owner == package->id_owner &&
        this->directory_lines[i]->opcode_number == package->opcode_number &&
        this->directory_lines[i]->uop_number == package->uop_number &&
        this->cmp_index_tag(directory_lines[i]->initial_memory_address, package->memory_address)) {
            return i;
        }
    }
    return POSITION_FAIL;
};


// ============================================================================
/*! This method should be only called if there is no directory lock for the
 * cache line being writeback.
 */
memory_package_t* directory_controller_t::create_cache_writeback(cache_memory_t *cache, cache_line_t *cache_line) {
    ERROR_ASSERT_PRINTF(cache_line->tag != 0, "Invalid memory_address.\n")

    /// Create the writeback package
    memory_package_t writeback_package;
    writeback_package.packager(
                cache->get_id(),                        /// Request Owner
                0,                                      /// Opcode. Number
                0,                                      /// Opcode. Address
                // ~ 0,                                      /// Uop. Number
                /// To identify the COPYBACK, the UOP=CYCLE
                sinuca_engine.get_global_cycle(),                                      /// Uop. Number


                cache_line->tag,                        /// Mem. Address
                sinuca_engine.get_global_line_size(),   /// Block Size

                PACKAGE_STATE_TRANSMIT,                 /// Pack. State
                cache->get_penalty_read(),              /// Ready Cycle - It takes time to read from the cache the whole data

                MEMORY_OPERATION_WRITEBACK,              /// Mem. Operation
                false,                                  /// Is Answer

                cache->get_id(),                        /// Src ID
                cache->get_id(),                        /// Dst ID
                NULL,                                   /// *Hops
                POSITION_FAIL);                         /// Hop Counter

    // =========================================================================
    /// Allocate CopyBack at the MSHR
    // =========================================================================

    int32_t slot = cache->allocate_eviction(&writeback_package);
    if (slot == POSITION_FAIL) {
        return NULL;
    }

    memory_package_t *cache_mshr_buffer = cache->get_mshr_buffer();
    memory_package_t *package = &cache_mshr_buffer[slot];
    DIRECTORY_CTRL_DEBUG_PRINTF("create_cache_writeback() cache_id:%u, package:%s\n", cache->get_id(), package->content_to_string().c_str())

    // =========================================================================
    /// Allocate CopyBack at the Directory_Line + LOCK
    // =========================================================================
    this->directory_lines.push_back(new directory_line_t());
    directory_line_t *directory_line = this->directory_lines.back();

    directory_line->packager(package->id_owner, package->opcode_number, package->opcode_address, package->uop_number,
                                LOCK_WRITE,
                                package->memory_operation, package->memory_address, package->memory_size);
    DIRECTORY_CTRL_DEBUG_PRINTF("\t New Directory Line:%s\n", directory_line->directory_line_to_string().c_str())

    /// Update the Directory_Line
    directory_line->cache_request_order[cache->get_cache_id()] = ++directory_line->cache_requested;
    DIRECTORY_CTRL_DEBUG_PRINTF("\t Update Directory Line:%s\n", directory_line->directory_line_to_string().c_str())

    /// From this point the package is transfering a cache line size
    package->memory_size = sinuca_engine.get_global_line_size();
    /// Higher Level Copy Back
    package->package_set_src_dst(cache->get_id(), this->find_next_obj_id(cache, package->memory_address));

    return package;
};


// ============================================================================
/*! This method should be only called the cache need space to install a new cache line
 */
bool directory_controller_t::inclusiveness_new_eviction(cache_memory_t *cache, cache_line_t *cache_line, uint32_t index, uint32_t way, memory_package_t *package) {
    (void)package;
    // =====================================================
    /// TAKES CARE ABOUT INCLUSIVENESS
    // =====================================================
    /// No Need CopyBack This Level
    if (! coherence_need_writeback(cache, cache_line)) {
        /// Need CopyBack Higher Level
        /// Check if some higher level has DIRTY line
        switch (this->inclusiveness_type) {
            /// Nothing need to be done
            case INCLUSIVENESS_NON_INCLUSIVE: {
            }
            break;

            /// If this is LLC => INVALIDATE Higher levels
            case INCLUSIVENESS_INCLUSIVE_LLC: {
                /// If this is LLC
                container_ptr_cache_memory_t *lower_level_cache = cache->get_lower_level_cache();
                if (lower_level_cache->empty()) {
                    /// Check if some HIGHER LEVEL has the cache line Modified
                    uint32_t sum_latency = 0;
                    ///*--------------------------*/ printf("\n EVICTING:");
                    cache->change_status(cache_line, this->find_cache_line_higher_levels(&sum_latency, cache, cache_line->tag, false));

                    /// Higher Level Hit
                    if (coherence_is_dirty(cache_line->status)) {
                        // =============================================================
                        // Line Usage Prediction
                        cache->line_usage_predictor->line_recv_writeback(cache, cache_line, package, index, way);
                        /// Update Statistics
                        this->new_statistics(cache, MEMORY_OPERATION_WRITEBACK, true);
                    }
                }
            }
            break;

            /// If this is Any Level => INVALIDATE Higher levels
            case INCLUSIVENESS_INCLUSIVE_ALL: {
                /// Check if some HIGHER LEVEL has the cache line Modified
                uint32_t sum_latency = 0;
                ///*--------------------------*/ printf("\n EVICTING:");
                cache->change_status(cache_line, this->find_cache_line_higher_levels(&sum_latency, cache, cache_line->tag, false));

                /// Higher Level Hit
                if (coherence_is_dirty(cache_line->status)) {
                    // =============================================================
                    // Line Usage Prediction
                    cache->line_usage_predictor->line_recv_writeback(cache, cache_line, package, index, way);
                    /// Update Statistics
                    this->new_statistics(cache, MEMORY_OPERATION_WRITEBACK, true);
                }
            }
            break;
        }
    }


    /// Need CopyBack After all?
    if (coherence_need_writeback(cache, cache_line)) {
        memory_package_t *writeback_package = this->create_cache_writeback(cache, cache_line);
        if (writeback_package != NULL) {
            /// Add statistics to the cache
            cache->add_stat_writeback();
            /// Update Coherence Status and Last Access Time
            this->coherence_new_operation(cache, cache_line, writeback_package, false);
            /// Update Statistics
            this->new_statistics(cache, writeback_package->memory_operation, false);

            // =============================================================
            // Line Usage Prediction (Tag different from actual)
            cache->line_usage_predictor->line_send_writeback(cache, cache_line, writeback_package, index, way);
        }
        else {
            /// Cannot continue right now
            DIRECTORY_CTRL_DEBUG_PRINTF("\t RETURN UNTREATED (Find_free MSHR == NULL)\n")
            return FAIL;
        }
    }
    else {
        /// Add statistics to the cache
        cache->add_stat_eviction();
    }


    /// Invalidate higher levels to maintain INCLUSIVINESS
    switch (this->inclusiveness_type) {
        /// Nothing need to be done
        case INCLUSIVENESS_NON_INCLUSIVE: {
            // =============================================================
            // Line Usage Prediction => Does not matter if it got writeback, it will be evicted anyway.
            cache->line_usage_predictor->line_eviction(cache, cache_line, index, way);
        }
        break;

        case INCLUSIVENESS_INCLUSIVE_LLC: {
            container_ptr_cache_memory_t *lower_level_cache = cache->get_lower_level_cache();
            /// If this is LLC => INVALIDATE Higher levels
            if (lower_level_cache->empty()) {
                this->coherence_evict_higher_levels(cache, cache_line->tag);
                // =============================================================
                // Line Usage Prediction => Does not matter if it got writeback, it will be evicted anyway.
                // Implicit call: cache_memory->line_usage_predictor->line_eviction(index, way);
            }
            else {
                // =============================================================
                // Line Usage Prediction
                cache->line_usage_predictor->line_eviction(cache, cache_line, index, way);
            }
        }
        break;

        /// If this is Any Level => INVALIDATE Higher levels
        case INCLUSIVENESS_INCLUSIVE_ALL: {
            this->coherence_evict_higher_levels(cache, cache_line->tag);
            // =============================================================
            // Line Usage Prediction => Does not matter if it got writeback, it will be evicted anyway.
            // Implicit call: cache_memory->line_usage_predictor->line_eviction(index, way);
        }
        break;
    }
    return OK;
};

// ============================================================================
void directory_controller_t::coherence_evict_higher_levels(cache_memory_t *cache_memory, uint64_t memory_address) {

    // ================================================================================
    /// Invalidate on the higher levels
    container_ptr_cache_memory_t *higher_level_cache = cache_memory->get_higher_level_cache();
    for (uint32_t i = 0; i < higher_level_cache->size(); i++) {
        cache_memory_t *higher_cache = higher_level_cache[0][i];
        this->coherence_evict_higher_levels(higher_cache, memory_address);
    }

    // ================================================================================
    /// Invalidate this level
    uint32_t index, way;
    cache_line_t *cache_line = cache_memory->find_line(memory_address, index, way);
    /// Only evict if the line has a valid state
    if (cache_line != NULL) {
        // =============================================================
        // Line Usage Prediction
        cache_memory->line_usage_predictor->line_eviction(cache_memory, cache_line, index, way);

        cache_memory->change_address(cache_line, 0);
        cache_memory->change_status(cache_line, PROTOCOL_STATUS_I);

        /// Cache Statistics
        cache_memory->add_stat_invalidation();
    }
};


// ============================================================================
protocol_status_t directory_controller_t::find_cache_line_higher_levels(uint32_t *sum_latency, cache_memory_t *cache_memory,
                                                                        uint64_t memory_address, bool check_llc) {
    ERROR_ASSERT_PRINTF(cache_memory != NULL, "Received a NULL cache_memory\n");

    cache_memory_t *higher_cache = NULL;
    cache_line_t *cache_line = NULL;
    uint32_t index, way;
    protocol_status_t return_status = PROTOCOL_STATUS_I;

    /// First, lets check at this level.
    cache_line = cache_memory->find_line(memory_address, index, way);
    if (cache_line != NULL){
        return_status = cache_line->status;
    }

    /// Second, if could not find, check on the higher levels
    if (!coherence_is_hit(return_status)){
        container_ptr_cache_memory_t *higher_level_cache = cache_memory->get_higher_level_cache();
        for (uint32_t i = 0; i < higher_level_cache->size(); i++) {
            higher_cache = higher_level_cache[0][i];
            return_status = this->find_cache_line_higher_levels(sum_latency, higher_cache, memory_address, false);
            if (coherence_is_hit(return_status)) {
                int32_t noc_latency = 0;
                /// Latency to send a request
                noc_latency = sinuca_engine.interconnection_controller->get_total_low_latency(cache_memory->get_id(), higher_cache->get_id());
                ERROR_ASSERT_PRINTF(noc_latency > 0, "Latency should be greater than zero");
                *sum_latency += noc_latency;

                /// Latency to obtain the data
                noc_latency = sinuca_engine.interconnection_controller->get_total_high_latency(cache_memory->get_id(), higher_cache->get_id());
                ERROR_ASSERT_PRINTF(noc_latency > 0, "Latency should be greater than zero");
                *sum_latency += noc_latency;
                break;
            }
        }
    }

    /// Third, if could not find, iterate over all LLC
    container_ptr_cache_memory_t *lower_level_cache = cache_memory->get_lower_level_cache();
    if (!coherence_is_hit(return_status) && lower_level_cache->empty() && check_llc) {
        for (uint32_t i = 0; i < this->llc_caches.size(); i++) {
            higher_cache = this->llc_caches[i];

            /// If bank invalid for the address (avoid multibanked to search multiple times)
            if (higher_cache->get_bank(memory_address) != higher_cache->get_bank_number()) {
                continue;
            }

            /// Propagate Higher
            return_status = this->find_cache_line_higher_levels(sum_latency, higher_cache, memory_address, false);
            if (coherence_is_hit(return_status)) {
                int32_t noc_latency = 0;
                /// Latency to send a request
                noc_latency = sinuca_engine.interconnection_controller->get_total_low_latency(cache_memory->get_id(), higher_cache->get_id());
                ERROR_ASSERT_PRINTF(noc_latency > 0, "Latency should be greater than zero");
                *sum_latency += noc_latency;

                /// Latency to obtain the data
                noc_latency = sinuca_engine.interconnection_controller->get_total_high_latency(cache_memory->get_id(), higher_cache->get_id());
                ERROR_ASSERT_PRINTF(noc_latency > 0, "Latency should be greater than zero");
                *sum_latency += noc_latency;
                break;
            }
        }
    }

    /// Allocate also at this level
    if (cache_line != NULL && coherence_is_hit(return_status)) {
        switch (this->get_coherence_protocol_type()) {
            case COHERENCE_PROTOCOL_MOESI:
                switch (cache_line->status) {
                    case PROTOCOL_STATUS_M:
                    case PROTOCOL_STATUS_O:
                        /// Lower level becomes the owner
                        return_status = PROTOCOL_STATUS_O;
                    break;

                    case PROTOCOL_STATUS_E:
                    case PROTOCOL_STATUS_S:
                    case PROTOCOL_STATUS_I:
                    break;
                }

                /// This Level stays with a normal copy
                cache_memory->change_status(cache_line, PROTOCOL_STATUS_S);

            break;
        }
    }

    /// Increment the latency if it is propagating a valid data
    if (coherence_is_hit(return_status)) {
        DIRECTORY_CTRL_DEBUG_PRINTF("C2C:%s -> ", cache_memory->get_label());
        *sum_latency += cache_memory->get_penalty_read();
    }

    return return_status;
};

// ============================================================================
void directory_controller_t::coherence_new_operation(cache_memory_t *cache, cache_line_t *cache_line,  memory_package_t *package, bool is_hit) {
    ERROR_ASSERT_PRINTF(cache != NULL, "Received a NULL cache_memory\n");

    cache->update_last_access(cache_line);
    switch (this->coherence_protocol_type) {
        case COHERENCE_PROTOCOL_MOESI:
            switch (package->memory_operation) {
                case MEMORY_OPERATION_READ:
                case MEMORY_OPERATION_INST:
                case MEMORY_OPERATION_PREFETCH:
                    /// Update the Replacemente Policy information
                    if (cache_line->status == PROTOCOL_STATUS_I) {
                        cache_line->status = PROTOCOL_STATUS_S;
                    }
                break;

                case MEMORY_OPERATION_WRITE:
                    this->coherence_invalidate_all(cache, package->memory_address);
                    /// Update the Replacemente Policy information
                    cache_line->status = PROTOCOL_STATUS_M;
                break;

                case MEMORY_OPERATION_WRITEBACK:
                    /// Received a Copy-back
                    if (is_hit) {
                        ERROR_ASSERT_PRINTF(cache_line->status == PROTOCOL_STATUS_I, "Receiving a Copyback the line should be NULL or INVALID\n")
                        ERROR_ASSERT_PRINTF(cache->get_id() != package->id_owner, "CopyBack Recv on the same cache which stated the writeback\n")
                        /// Update the Replacement Policy information
                        cache_line->status = PROTOCOL_STATUS_O;
                    }
                    /// Send an early write-back
                    else {
                        ERROR_ASSERT_PRINTF(cache_line->status != PROTOCOL_STATUS_I, "Sending a Copyback the line should be NULL or INVALID\n")
                        /// Update the Replacement Policy information
                        cache_line->status = PROTOCOL_STATUS_S;
                    }
                break;

                // Receiving a wrong HMC
                case MEMORY_OPERATION_HMC_ALU:
                case MEMORY_OPERATION_HMC_ALUR:
                    ERROR_PRINTF("Entering at coherence_new_operation() for a HMC instruction");
                break;
            }
        break;
    }
};

// ============================================================================
void directory_controller_t::coherence_invalidate_all(cache_memory_t *cache_memory, uint64_t memory_address) {
    /// Get pointers to all cache lines.
    for (uint32_t i = 0; i < sinuca_engine.get_cache_memory_array_size(); i++) {
        /// Cache different to THIS
        if (i != cache_memory->get_cache_id()) {
            uint32_t index, way;
            cache_line_t *cache_line = sinuca_engine.cache_memory_array[i]->find_line(memory_address, index, way);
            /// Only invalidate if the line has a valid state
            if (cache_line != NULL && this->coherence_is_hit(cache_line->status)) {
                // =============================================================
                // Line Usage Prediction
                sinuca_engine.cache_memory_array[i]->line_usage_predictor->line_invalidation(cache_memory, cache_line, index, way);

                sinuca_engine.cache_memory_array[i]->change_status(cache_line, PROTOCOL_STATUS_I);
                /// Cache Statistics
                sinuca_engine.cache_memory_array[i]->add_stat_invalidation();
            }
        }
    }
};

// ============================================================================
bool directory_controller_t::coherence_evict_all() {
    bool all_clean = true;

    if (final_writeback_all == false){
        return all_clean;
    }

    /// Stores the last evicted index, for each cache.
    static uint32_t *last_index = utils_t::template_allocate_initialize_array<uint32_t>(sinuca_engine.get_cache_memory_array_size(), 0);

    DIRECTORY_CTRL_DEBUG_PRINTF("\nMass Eviction...");
    this->add_stat_final_writeback_all_cycles();

    for (uint32_t level = 1; level <= this->max_cache_level; level++) {
        DIRECTORY_CTRL_DEBUG_PRINTF("Level %" PRIu32 " - ", level);
        /// Get pointers to all cache lines.
        for (uint32_t i = 0; i < sinuca_engine.get_cache_memory_array_size(); i++) {
            cache_memory_t *cache = sinuca_engine.cache_memory_array[i];
            if (cache->get_hierarchy_level() != level) {
                continue;
            }
            for (uint32_t index = last_index[i]; index < cache->get_total_sets(); index++) {
                last_index[i] = index;
                for (uint32_t way = 0; way < cache->get_associativity(); way++) {
                    cache_line_t *cache_line = cache->get_line(index, way);
                    if (this->coherence_is_dirty(cache_line->status)) {
                        all_clean = false;
                        memory_package_t *writeback_package = this->create_cache_writeback(cache, cache_line);
                        if (writeback_package != NULL) {
                            DIRECTORY_CTRL_DEBUG_PRINTF("%s Evicted... %" PRIu32 " x %" PRIu32 " = %" PRIu64 " \n", cache->get_label(), index, way , writeback_package->memory_address);
                            /// Add statistics to the cache
                            cache->add_stat_final_writeback();
                            cache_line->status = PROTOCOL_STATUS_I;
                            // ~ /// Update Statistics
                            // ~ this->new_statistics(cache, writeback_package->memory_operation, false);
                        }
                        /// Go for the next cache
                        index = cache->get_total_sets();
                        way = cache->get_associativity();
                    }
                    else {
                        /// Add statistics to the cache
                        cache->add_stat_final_eviction();
                        cache_line->status = PROTOCOL_STATUS_I;
                    }
                }
            }
        }
        /// Don't go to the next level, until finish this.
        if (all_clean != true) break;
    }

    if (all_clean) {
        utils_t::template_delete_array<uint32_t>(last_index);
    }
    return all_clean;
};


// ============================================================================
// ============================================================================
bool directory_controller_t::coherence_is_read(memory_operation_t memory_operation) {
    switch (memory_operation) {
        case MEMORY_OPERATION_READ:
        case MEMORY_OPERATION_INST:
        case MEMORY_OPERATION_PREFETCH:
            return OK;
        break;

        case MEMORY_OPERATION_WRITE:
        case MEMORY_OPERATION_WRITEBACK:
            return FAIL;
        break;

        // Receiving a wrong HMC
        case MEMORY_OPERATION_HMC_ALU:
        case MEMORY_OPERATION_HMC_ALUR:
            ERROR_PRINTF("Entering at coherence_is_read() for a HMC instruction\n");
        break;
    }
    ERROR_PRINTF("Wrong memory_operation_t\n")
    return FAIL;
};

// ============================================================================
bool directory_controller_t::coherence_is_dirty(protocol_status_t line_status) {
    switch (this->coherence_protocol_type) {
        case COHERENCE_PROTOCOL_MOESI:
            switch (line_status) {
                case PROTOCOL_STATUS_M:
                case PROTOCOL_STATUS_O:
                    return OK;
                break;

                case PROTOCOL_STATUS_E:
                case PROTOCOL_STATUS_S:
                case PROTOCOL_STATUS_I:
                    return FAIL;
                break;

            }
        break;
    }

    ERROR_PRINTF("Wrong protocol_status_t\n")
    return FAIL;
};

// ============================================================================
bool directory_controller_t::coherence_is_hit(protocol_status_t line_status) {
    switch (this->coherence_protocol_type) {
        case COHERENCE_PROTOCOL_MOESI:
            switch (line_status) {
                case PROTOCOL_STATUS_M:
                case PROTOCOL_STATUS_O:
                case PROTOCOL_STATUS_E:
                case PROTOCOL_STATUS_S:
                    return OK;
                break;

                case PROTOCOL_STATUS_I:
                    return FAIL;
                break;
            }
        break;
    }
    ERROR_PRINTF("Invalid protocol status\n");
    return FAIL;
};

// ============================================================================
bool directory_controller_t::coherence_need_writeback(cache_memory_t *cache_memory, cache_line_t *cache_line) {
    ERROR_ASSERT_PRINTF(cache_line != NULL, "Received a NULL cache_line\n");

    /// LLC
    container_ptr_cache_memory_t *lower_level_cache = cache_memory->get_lower_level_cache();
    if (lower_level_cache->empty()) {
        if (this->generate_llc_writeback && this->coherence_is_dirty(cache_line->status)) {
            return OK;
        }
    }
    /// Non LLC
    else {
        if (this->generate_non_llc_writeback && this->coherence_is_dirty(cache_line->status)) {
            return OK;
        }
    }

    return FAIL;
};


// =============================================================================
uint32_t directory_controller_t::find_next_obj_id(cache_memory_t *cache_memory, uint64_t memory_address) {
    container_ptr_cache_memory_t *lower_level_cache = cache_memory->get_lower_level_cache();

    /// Find Next Cache Level
    for (uint32_t i = 0; i < lower_level_cache->size(); i++) {
        cache_memory_t *lower_cache = lower_level_cache[0][i];
        if (lower_cache->get_bank(memory_address) == lower_cache->get_bank_number()) {
            return lower_cache->get_id();
        }
    }
    ERROR_ASSERT_PRINTF(lower_level_cache->empty(), "Could not find a valid lower_level_cache but size != 0.\n")

    /// Find Next Main Memory

    // =====================================
    // Addres Mapping to Mem.Ctrl.
    if (sinuca_engine.arg_map_file_name != NULL) {
        uint64_t pageaddr = memory_address >> this->page_bits_shift;

        /// Try to obtain a valid mapping
        if (this->mapped_controller.find(pageaddr) !=  this->mapped_controller.end()) {
            uint64_t  memctrl = this->mapped_controller[pageaddr];
            return sinuca_engine.memory_controller_array[memctrl]->get_id();
        }
        ///Otherwise obtain a normal mapping (memory mask)
        else {
            for (uint32_t i = 0; i < sinuca_engine.memory_controller_array_size; i++) {
                if (sinuca_engine.memory_controller_array[i]->get_controller(memory_address) ==
                    sinuca_engine.memory_controller_array[i]->get_controller_number()) {
                    return sinuca_engine.memory_controller_array[i]->get_id();
                }
            }
        }
    }
    else {
        for (uint32_t i = 0; i < sinuca_engine.memory_controller_array_size; i++) {
            if (sinuca_engine.memory_controller_array[i]->get_controller(memory_address) ==
                sinuca_engine.memory_controller_array[i]->get_controller_number()) {
                return sinuca_engine.memory_controller_array[i]->get_id();
            }
        }
    }


    ERROR_PRINTF("Could not find a next_level for the memory address\n")
    return FAIL;
};


// =============================================================================
bool directory_controller_t::is_locked(uint64_t memory_address) {
    /// Check for a lock to the address.
    for (uint32_t i = 0; i < this->directory_lines.size(); i++) {
        /// Same address found
        if (this->cmp_index_tag(directory_lines[i]->initial_memory_address, memory_address)) {
            /// Is Locked
            return OK;
        }
    }
    /// Is Un-Locked
    return FAIL;
};


// ============================================================================
void directory_controller_t::new_statistics(cache_memory_t *cache, memory_operation_t memory_operation, bool is_hit) {

    cache->cache_stats(memory_operation, is_hit);
    /// Statistics - HIT
    if (is_hit) {
        switch (memory_operation) {
            case MEMORY_OPERATION_READ:
                this->add_stat_read_hit();
            break;

            case MEMORY_OPERATION_INST:
                this->add_stat_instruction_hit();
            break;

            case MEMORY_OPERATION_PREFETCH:
                this->add_stat_prefetch_hit();
            break;

            case MEMORY_OPERATION_WRITE:
                this->add_stat_write_hit();
            break;

            case MEMORY_OPERATION_WRITEBACK:
                this->add_stat_writeback_recv();
            break;

            // HMC
            case MEMORY_OPERATION_HMC_ALU:
            case MEMORY_OPERATION_HMC_ALUR:
                ERROR_PRINTF("Entering at new_statistics() for a HMC instruction\n");
            break;

        }
    }

    /// Statistics - MISS
    else {
        switch (memory_operation) {
            case MEMORY_OPERATION_READ:
                this->add_stat_read_miss();
            break;

            case MEMORY_OPERATION_INST:
                this->add_stat_instruction_miss();
            break;

            case MEMORY_OPERATION_PREFETCH:
                this->add_stat_prefetch_miss();
            break;

            case MEMORY_OPERATION_WRITE:
                this->add_stat_write_miss();
            break;

            case MEMORY_OPERATION_WRITEBACK:
                this->add_stat_writeback_send();
            break;

            // HMC
            case MEMORY_OPERATION_HMC_ALU:
            case MEMORY_OPERATION_HMC_ALUR:
                ERROR_PRINTF("Entering at new_statistics() for a HMC instruction");
            break;


        }
    }

};

// ============================================================================
void directory_controller_t::print_structures() {
    SINUCA_PRINTF("DIRECTORY_LINE:\n")
    for (uint32_t i = 0; i < this->directory_lines.size(); i++) {
        SINUCA_PRINTF("[%u] %s\n", i, this->directory_lines[i]->directory_line_to_string().c_str());
    }
};

// =============================================================================
void directory_controller_t::panic() {
    this->print_structures();
};

// ============================================================================
void directory_controller_t::periodic_check(){
    #ifdef DIRECTORY_CTRL_DEBUG
        DIRECTORY_CTRL_DEBUG_PRINTF("\n");
        this->print_structures();
    #endif
    ERROR_ASSERT_PRINTF(directory_line_t::check_age(&this->directory_lines, this->directory_lines.size()) == OK, "Check_age failed.\n");
};

// ============================================================================
/// STATISTICS
// ============================================================================
void directory_controller_t::reset_statistics() {

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

    this->set_stat_cache_to_cache(0);
    this->set_stat_final_writeback_all_cycles(0);
};

// ============================================================================
void directory_controller_t::print_statistics() {
    char title[100] = "";
    snprintf(title, sizeof(title), "Statistics of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

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
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_cache_to_cache", stat_cache_to_cache);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_final_writeback_all_cycles", stat_final_writeback_all_cycles);

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_instruction_miss_percentage", stat_instruction_miss, stat_instruction_miss + stat_instruction_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_read_miss_percentage", stat_read_miss, stat_read_miss + stat_read_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_prefetch_miss_percentage", stat_prefetch_miss, stat_prefetch_miss + stat_prefetch_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_write_miss_percentage", stat_write_miss, stat_write_miss + stat_write_hit);
    sinuca_engine.write_statistics_value_percentage(get_type_component_label(), get_label(), "stat_writeback_send_percentage", stat_writeback_send, stat_writeback_send + stat_writeback_recv);
};

// ============================================================================
void directory_controller_t::print_configuration() {
    char title[100] = "";
    snprintf(title, sizeof(title), "Configuration of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "coherence_protocol_type", get_enum_coherence_protocol_char(coherence_protocol_type));
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "inclusiveness_type", get_enum_inclusiveness_char(inclusiveness_type));
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "generate_llc_writeback", generate_llc_writeback);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "not_offset_bits_mask", utils_t::address_to_binary(this->not_offset_bits_mask).c_str());
};
