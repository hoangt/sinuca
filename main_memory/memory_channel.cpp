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

#ifdef MEMORY_CONTROLLER_DEBUG
    #define MEMORY_CONTROLLER_DEBUG_PRINTF(...) DEBUG_PRINTF(__VA_ARGS__);
#else
    #define MEMORY_CONTROLLER_DEBUG_PRINTF(...)
#endif

/// ============================================================================
memory_channel_t::memory_channel_t() {
    this->bank_per_channel = 0;
    this->bank_buffer_size = 0;
    this->bank_selection_policy = SELECTION_ROUND_ROBIN;
    this->request_priority_policy = REQUEST_PRIORITY_ROW_BUFFER_HITS_FIRST;
    this->write_priority_policy = WRITE_PRIORITY_DRAIN_WHEN_FULL;


    this->column_bits_mask = 0;
    this->not_column_bits_mask = 0;
    this->column_bits_shift = 0;

    this->row_bits_mask = 0;
    this->row_bits_shift = 0;

    this->bank_bits_mask = 0;
    this->bank_bits_shift = 0;


    this->bank_buffer = NULL;
    this->bank_buffer_actual_position = NULL;

    this->bank_is_drain_write = NULL;
    this->bank_open_row_address = NULL;

    this->bank_last_command = NULL;
    this->bank_last_command_cycle = NULL;
    this->channel_last_command_cycle = NULL;

    this->last_bank_selected = 0;
};

/// ============================================================================
memory_channel_t::~memory_channel_t() {
    utils_t::template_delete_array<container_ptr_memory_package_t>(bank_buffer);
    utils_t::template_delete_array<int32_t>(bank_buffer_actual_position);

    utils_t::template_delete_array<bool>(bank_is_drain_write);
    utils_t::template_delete_array<uint64_t>(bank_open_row_address);

    utils_t::template_delete_array<memory_controller_command_t>(bank_last_command);
    utils_t::template_delete_matrix<uint64_t>(bank_last_command_cycle, this->get_bank_per_channel());
    utils_t::template_delete_array<uint64_t>(channel_last_command_cycle);
};

/// ============================================================================
void memory_channel_t::allocate() {

    this->bank_buffer = utils_t::template_allocate_array<container_ptr_memory_package_t>(this->get_bank_per_channel());
    this->bank_buffer_actual_position = utils_t::template_allocate_initialize_array<int32_t>(this->get_bank_per_channel(), -1);

    this->bank_is_drain_write = utils_t::template_allocate_initialize_array<bool>(this->get_bank_per_channel(), false);
    this->bank_open_row_address = utils_t::template_allocate_initialize_array<uint64_t>(this->get_bank_per_channel(), 0);

    this->bank_last_command = utils_t::template_allocate_initialize_array<memory_controller_command_t>(this->get_bank_per_channel(), MEMORY_CONTROLLER_COMMAND_PRECHARGE);
    this->bank_last_command_cycle = utils_t::template_allocate_initialize_matrix<uint64_t>(this->get_bank_per_channel(), MEMORY_CONTROLLER_COMMAND_NUMBER, 0);
    this->channel_last_command_cycle = utils_t::template_allocate_initialize_array<uint64_t>(MEMORY_CONTROLLER_COMMAND_NUMBER, 0);
};


/// ============================================================================
int32_t memory_channel_t::find_next_read_operation(uint32_t bank) {
    ERROR_ASSERT_PRINTF(this->bank_buffer[bank].size() > 0, "Calling find_next_operation with empty buffer\n")

    int32_t slot = POSITION_FAIL;
    uint32_t i;

    switch (this->request_priority_policy) {
        case REQUEST_PRIORITY_ROW_BUFFER_HITS_FIRST:
            /// Try to find OPERATION in the same OPEN_ROW
            for (i = 0; i < this->bank_buffer[bank].size(); i++) {
                if ((this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_READ ||
                this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_INST ||
                this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_PREFETCH)
                &&
                this->cmp_row_bank_channel(this->bank_buffer[bank][i]->memory_address, this->bank_open_row_address[bank])) {
                    slot = i;
                    break;
                }
            }
            /// If could not find, Try to find OLDER OPERATION
            if (slot == POSITION_FAIL) {
                for (i = 0; i < this->bank_buffer[bank].size(); i++) {
                    if (this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_READ ||
                    this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_INST ||
                    this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_PREFETCH) {
                        slot = i;
                        break;
                    }
                }
            }
        break;

        case REQUEST_PRIORITY_FIRST_COME_FIRST_SERVE:
            /// Try to find OLDER OPERATION
            for (i = 0; i < this->bank_buffer[bank].size(); i++) {
                if (this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_READ ||
                this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_INST ||
                this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_PREFETCH) {
                    slot = i;
                    break;
                }
            }
        break;
    }
    return slot;
};

/// ============================================================================
int32_t memory_channel_t::find_next_write_operation(uint32_t bank) {
    ERROR_ASSERT_PRINTF(this->bank_buffer[bank].size() > 0, "Calling find_next_operation with empty buffer\n")

    int32_t slot = POSITION_FAIL;
    uint32_t i;

    switch (this->request_priority_policy) {
        case REQUEST_PRIORITY_ROW_BUFFER_HITS_FIRST:
            /// Try to find OPERATION in the same OPEN_ROW
            for (i = 0; i < this->bank_buffer[bank].size(); i++) {
                if ((this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_COPYBACK ||
                this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_WRITE)
                &&
                this->cmp_row_bank_channel(this->bank_buffer[bank][i]->memory_address, this->bank_open_row_address[bank])) {
                    slot = i;
                    break;
                }
            }
            /// If could not find, Try to find OLDER OPERATION
            if (slot == POSITION_FAIL) {
                for (i = 0; i < this->bank_buffer[bank].size(); i++) {
                    if (this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_COPYBACK ||
                    this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_WRITE) {
                        slot = i;
                        break;
                    }
                }
            }
        break;

        case REQUEST_PRIORITY_FIRST_COME_FIRST_SERVE:
            /// Try to find OLDER OPERATION
            for (i = 0; i < this->bank_buffer[bank].size(); i++) {
                if (this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_COPYBACK ||
                this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_WRITE) {
                    slot = i;
                    break;
                }
            }
        break;
    }
    return slot;
};


/// ============================================================================
int32_t memory_channel_t::find_next_package(uint32_t bank) {

    if (this->bank_buffer[bank].size() == 0) {
        return POSITION_FAIL;
    }

    int32_t slot = POSITION_FAIL;

    /// If it is in drain_write mode
    if (this->bank_is_drain_write[bank]) {
        slot = find_next_write_operation(bank);
        if (slot == POSITION_FAIL) {
            this->bank_is_drain_write[bank] = false;
            slot = find_next_read_operation(bank);
        }
    }
    else {
        switch (this->write_priority_policy) {
            case WRITE_PRIORITY_DRAIN_WHEN_FULL:
                slot = find_next_read_operation(bank);
                /// Could not find READ, but buffer is FULL
                if (slot == POSITION_FAIL && this->bank_buffer[bank].size() == this->bank_buffer_size) {
                    this->bank_is_drain_write[bank] = true;
                    slot = find_next_write_operation(bank);
                }
            break;

            case WRITE_PRIORITY_SERVICE_AT_NO_READ:
                slot = find_next_read_operation(bank);
                /// Could not find READ
                if (slot == POSITION_FAIL) {
                    /// If buffer is full
                    if (this->bank_buffer[bank].size() == this->bank_buffer_size) {
                        this->bank_is_drain_write[bank] = true;
                    }
                    slot = find_next_write_operation(bank);
                }
            break;
        }
    }
    return slot;
};


/// ============================================================================
bool memory_channel_t::check_if_minimum_latency(uint32_t bank, memory_controller_command_t next_command) {
    uint64_t actual_cycle = sinuca_engine.get_global_cycle();

    switch (next_command){
        case MEMORY_CONTROLLER_COMMAND_PRECHARGE:
            /// ================================================================
            /// Same Bank
            if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_ROW_ACCESS] + this->timing_ras)
            > actual_cycle)
                return false;
            else if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_READ] + this->timing_al + this->timing_burst + this->timing_rtp - this->timing_ccd)
            > actual_cycle)
                return false;
            else if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] + this->timing_al + this->timing_cwd + this->timing_burst + this->timing_wr)
            > actual_cycle)
                return false;
            /// ================================================================
            else
                return true;
        break;

        case MEMORY_CONTROLLER_COMMAND_ROW_ACCESS:
            /// ================================================================
            /// Same Bank
            if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_ROW_ACCESS] + this->timing_rc)
            > actual_cycle)
                return false;
            /// ================================================================
            /// Same Channel
            else if ((this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_ROW_ACCESS] + this->timing_rrd)
            > actual_cycle)
                return false;
            /// ================================================================
            else
                return true;
        break;

        case MEMORY_CONTROLLER_COMMAND_COLUMN_READ:
            /// ================================================================
            /// Same Bank
            if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_ROW_ACCESS] + this->timing_rcd - this->timing_al)
            > actual_cycle)
                return false;
            else if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_READ] + this->timing_burst)
            > actual_cycle)
                return false;
            else if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_READ] + this->timing_ccd)
            > actual_cycle)
                return false;
            else if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] + this->timing_cwd + this->timing_burst + this->timing_wtr)
            > actual_cycle)
                return false;
            /// ================================================================
            /// Same channel
            else if ((this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_READ] + this->timing_burst)
            > actual_cycle)
                return false;
            else if ((this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_READ] + this->timing_ccd)
            > actual_cycle)
                return false;
            else if ((this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] + this->timing_cwd + this->timing_burst + this->timing_wtr)
            > actual_cycle)
                return false;
            /// ================================================================
            else
                return true;
        break;

        case MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE:
            /// ================================================================
            /// Same Bank
            if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_ROW_ACCESS] + this->timing_rcd - this->timing_al)
            > actual_cycle)
                return false;
            /// Consider that tRTRS - tCWL is equal to ZERO. (Both are parallel)
            else if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_READ] + this->timing_cas + this->timing_burst)
            > actual_cycle)
                return false;
            else if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] + this->timing_burst)
            > actual_cycle)
                return false;
            else if ((this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] + this->timing_ccd)
            > actual_cycle)
                return false;
            /// ================================================================
            /// Same channel
            else if ((this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_READ] + this->timing_cas + this->timing_burst)
            > actual_cycle)
                return false;
            else if ((this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] + this->timing_burst)
            > actual_cycle)
                return false;
            else if ((this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] + this->timing_ccd)
            > actual_cycle)
                return false;
            /// ================================================================
            else
                return true;
        break;

        case MEMORY_CONTROLLER_COMMAND_NUMBER:
            ERROR_ASSERT_PRINTF(false, "Should not receive COMMAND_NUMBER\n")
        break;
    }

    ERROR_ASSERT_PRINTF(false, "Could not check the minimum latency\n")
    return false;
};

/// ============================================================================
package_state_t memory_channel_t::treat_memory_request(memory_package_t *package) {
    uint32_t bank = get_bank(package->memory_address);
    uint32_t i;

    switch (package->memory_operation) {
        case MEMORY_OPERATION_READ:
        case MEMORY_OPERATION_INST:
        case MEMORY_OPERATION_PREFETCH:
            /// Check for data forward
            for (i = 0; i < this->bank_buffer[bank].size(); i++) {
                if ((this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_COPYBACK ||
                this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_WRITE)
                && sinuca_engine.global_cmp_tag_index_bank(this->bank_buffer[bank][i]->memory_address, package->memory_address)) {
                    /// Prepare for answer later
                    package->memory_size = sinuca_engine.get_global_line_size();
                    package->is_answer = true;
                    package->package_transmit(1);
                    this->add_stat_read_forward();
                    return PACKAGE_STATE_TRANSMIT;
                }
            }
        break;

        case MEMORY_OPERATION_COPYBACK:
        case MEMORY_OPERATION_WRITE:
            // ~ /// Check for older write on same cache line
            // ~ for (i = 0; i < this->bank_buffer[bank].size(); i++) {
                // ~ if ((this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_COPYBACK ||
                // ~ this->bank_buffer[bank][i]->memory_operation == MEMORY_OPERATION_WRITE)
                // ~ && sinuca_engine.global_cmp_tag_index_bank(this->bank_buffer[bank][i]->memory_address, package->memory_address)) {
                    // ~ /// Prepare for answer later
                    // ~ this->bank_buffer[bank][i]->memory_size = sinuca_engine.get_global_line_size();
                    // ~ this->bank_buffer[bank][i]->is_answer = true;
                    // ~ this->bank_buffer[bank][i]->package_ready(1);
                    // ~ this->add_stat_write_forward();
                    // ~ break;
                // ~ }
            // ~ }
        break;
    }

    /// Try to insert into bank buffer
    if (this->bank_buffer[bank].size() < this->bank_buffer_size) {
        this->bank_buffer[bank].push_back(package);
        return PACKAGE_STATE_WAIT;
    }
    return PACKAGE_STATE_UNTREATED;
};


/// ============================================================================
void memory_channel_t::clock(uint32_t subcycle) {
    if (subcycle != 0) return;
    MEMORY_CONTROLLER_DEBUG_PRINTF("==================== ID(%u) ",this->get_id());
    MEMORY_CONTROLLER_DEBUG_PRINTF("====================\n");
    MEMORY_CONTROLLER_DEBUG_PRINTF("cycle() \n");

    uint32_t bank = 0;
    switch (this->get_bank_selection_policy()) {
        case SELECTION_ROUND_ROBIN:
            bank = this->selection_bank_round_robin();
        break;

        case SELECTION_RANDOM:
            bank = this->selection_bank_random();
        break;

        case SELECTION_BUFFER_LEVEL:
            ERROR_PRINTF("Selection Policy: SELECTION_BUFFER_LEVEL not implemented.\n");
        break;
    }

    /// Select package to be treated
    if (this->bank_buffer_actual_position[bank] == -1) {
        this->bank_buffer_actual_position[bank] = find_next_package(bank);
        if (this->bank_buffer_actual_position[bank] == POSITION_FAIL) {
            return;
        }
    }

    memory_package_t *package = this->bank_buffer[bank][this->bank_buffer_actual_position[bank]];

    /// Considering the last command, do the next
    switch(this->bank_last_command[bank]) {
        //======================================================================
        case MEMORY_CONTROLLER_COMMAND_PRECHARGE:
            /// Respect Min. Latency
            if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_ROW_ACCESS)) {
                return;
            }

            this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_ROW_ACCESS;
            this->bank_open_row_address[bank] = package->memory_address;
            this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_ROW_ACCESS] = sinuca_engine.get_global_cycle();
            this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_ROW_ACCESS] = sinuca_engine.get_global_cycle();
        break;

        //======================================================================
        case MEMORY_CONTROLLER_COMMAND_ROW_ACCESS:
            ERROR_ASSERT_PRINTF(cmp_row_bank_channel(this->bank_open_row_address[bank], package->memory_address), "Sending READ/WRITE to wrong row.")
            switch (package->memory_operation) {
                case MEMORY_OPERATION_READ:
                case MEMORY_OPERATION_INST:
                case MEMORY_OPERATION_PREFETCH:
                    /// Respect Min. Latency
                    if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_COLUMN_READ)) {
                        return;
                    }

                    this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_COLUMN_READ;
                    this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_READ] = sinuca_engine.get_global_cycle();
                    this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_READ] = sinuca_engine.get_global_cycle();
                    /// Prepare for answer later
                    package->memory_size = sinuca_engine.get_global_line_size();
                    package->is_answer = true;
                    package->package_transmit(this->timing_rcd - this->timing_al);
                break;

                case MEMORY_OPERATION_COPYBACK:
                case MEMORY_OPERATION_WRITE:
                    /// Respect Min. Latency
                    if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE)) {
                        return;
                    }

                    this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE;
                    this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] = sinuca_engine.get_global_cycle();
                    this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] = sinuca_engine.get_global_cycle();
                    /// Prepare for answer later
                    package->memory_size = 1;
                    package->is_answer = true;
                    package->package_ready(this->timing_rcd - this->timing_al);
                break;
            }

            this->bank_buffer[bank].erase(this->bank_buffer[bank].begin() + this->bank_buffer_actual_position[bank]);
            this->bank_buffer_actual_position[bank] = -1;
        break;

        //======================================================================
        case MEMORY_CONTROLLER_COMMAND_COLUMN_READ:
            if (cmp_row_bank_channel(this->bank_open_row_address[bank], package->memory_address)) {
                switch (package->memory_operation) {
                    case MEMORY_OPERATION_READ:
                    case MEMORY_OPERATION_INST:
                    case MEMORY_OPERATION_PREFETCH:
                        /// Respect Min. Latency
                        if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_COLUMN_READ)) {
                            return;
                        }

                        this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_COLUMN_READ;
                        this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_READ] = sinuca_engine.get_global_cycle();
                        this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_READ] = sinuca_engine.get_global_cycle();
                        package->is_answer = true;
                        package->package_transmit(this->timing_burst);
                    break;

                    case MEMORY_OPERATION_COPYBACK:
                    case MEMORY_OPERATION_WRITE:
                        /// Respect Min. Latency
                        if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE)) {
                            return;
                        }

                        this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE;
                        this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] = sinuca_engine.get_global_cycle();
                        this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] = sinuca_engine.get_global_cycle();
                        /// Consider that tRTRS - tCWL is equal to ZERO. (Both are parallel)
                        package->is_answer = true;
                        package->package_ready(this->timing_cas + this->timing_burst);
                    break;
                }

                this->bank_buffer[bank].erase(this->bank_buffer[bank].begin() + this->bank_buffer_actual_position[bank]);
                this->bank_buffer_actual_position[bank] = -1;
            }
            else {
                /// Respect Min. Latency
                if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_PRECHARGE)) {
                    return;
                }

                this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_PRECHARGE;
                this->bank_open_row_address[bank] = package->memory_address;
                this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_PRECHARGE] = sinuca_engine.get_global_cycle();
                this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_PRECHARGE] = sinuca_engine.get_global_cycle();
            }
        break;

        //======================================================================
        case MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE:
            if (cmp_row_bank_channel(this->bank_open_row_address[bank], package->memory_address)) {
                switch (package->memory_operation) {
                    case MEMORY_OPERATION_READ:
                    case MEMORY_OPERATION_INST:
                    case MEMORY_OPERATION_PREFETCH:
                        /// Respect Min. Latency
                        if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_COLUMN_READ)) {
                            return;
                        }

                        this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_COLUMN_READ;
                        this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_READ] = sinuca_engine.get_global_cycle();
                        this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_READ] = sinuca_engine.get_global_cycle();
                        package->is_answer = true;
                        package->package_transmit(this->timing_cwd + this->timing_burst + this->timing_wtr);
                    break;

                    case MEMORY_OPERATION_COPYBACK:
                    case MEMORY_OPERATION_WRITE:
                        /// Respect Min. Latency
                        if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE)) {
                            return;
                        }

                        this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE;
                        this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] = sinuca_engine.get_global_cycle();
                        this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE] = sinuca_engine.get_global_cycle();
                        /// Max(tBurst, tCCD)
                        package->is_answer = true;
                        package->package_ready(this->timing_burst > this->timing_ccd ? this->timing_burst : this->timing_ccd);
                    break;
                }

                this->bank_buffer[bank].erase(this->bank_buffer[bank].begin() + this->bank_buffer_actual_position[bank]);
                this->bank_buffer_actual_position[bank] = -1;
            }
            else {
                /// Respect Min. Latency
                if (!check_if_minimum_latency(bank, MEMORY_CONTROLLER_COMMAND_PRECHARGE)) {
                    return;
                }

                this->bank_last_command[bank] = MEMORY_CONTROLLER_COMMAND_PRECHARGE;
                this->bank_open_row_address[bank] = package->memory_address;
                this->bank_last_command_cycle[bank][MEMORY_CONTROLLER_COMMAND_PRECHARGE] = sinuca_engine.get_global_cycle();
                this->channel_last_command_cycle[MEMORY_CONTROLLER_COMMAND_PRECHARGE] = sinuca_engine.get_global_cycle();
            }
        break;

        //======================================================================
        case MEMORY_CONTROLLER_COMMAND_NUMBER:
            ERROR_ASSERT_PRINTF(false, "Should not receive COMMAND_NUMBER\n")
        break;

    }
};



/// ============================================================================
/// Bank Selection Strategies
/// ============================================================================
/// Selection strategy: Random
uint32_t memory_channel_t::selection_bank_random() {
    unsigned int seed = sinuca_engine.get_global_cycle() % 1000;
    uint32_t selected = (rand_r(&seed) % this->get_bank_per_channel());
    return selected;
};

/// ============================================================================
/// Selection strategy: Round Robin
uint32_t memory_channel_t::selection_bank_round_robin() {
    this->last_bank_selected++;
    if (this->last_bank_selected >= this->get_bank_per_channel()) {
        this->last_bank_selected = 0;
    }
    return this->last_bank_selected;
};


/// ============================================================================
int32_t memory_channel_t::send_package(memory_package_t *package) {
    ERROR_PRINTF("Send package %s.\n", package->content_to_string().c_str());
    return POSITION_FAIL;
};

/// ============================================================================
bool memory_channel_t::receive_package(memory_package_t *package, uint32_t input_port, uint32_t transmission_latency) {
    ERROR_PRINTF("Received package %s into the input_port %u, latency %u.\n", package->content_to_string().c_str(), input_port, transmission_latency);
    return FAIL;
};

/// ============================================================================
/// Token Controller Methods
/// ============================================================================
void memory_channel_t::allocate_token_list() {
    MEMORY_CONTROLLER_DEBUG_PRINTF("allocate_token_list()\n");
};

/// ============================================================================
bool memory_channel_t::check_token_list(memory_package_t *package) {
    ERROR_PRINTF("check_token_list %s.\n", get_enum_memory_operation_char(package->memory_operation))
    return FAIL;
};

/// ============================================================================
uint32_t memory_channel_t::check_token_space(memory_package_t *package) {
    ERROR_PRINTF("check_token_space %s.\n", get_enum_memory_operation_char(package->memory_operation))
    return FAIL;
};

/// ============================================================================
void memory_channel_t::remove_token_list(memory_package_t *package) {
    ERROR_PRINTF("remove_token_list %s.\n", get_enum_memory_operation_char(package->memory_operation))
};

/// ============================================================================
void memory_channel_t::print_structures() {

};

/// ============================================================================
void memory_channel_t::panic() {
    this->print_structures();
};

/// ============================================================================
void memory_channel_t::periodic_check(){
    #ifdef MEMORY_CONTROLLER_DEBUG
        this->print_structures();
    #endif
};

/// ============================================================================
/// STATISTICS
/// ============================================================================
void memory_channel_t::reset_statistics() {
    this->stat_read_forward = 0;
    this->stat_write_forward = 0;
};

/// ============================================================================
void memory_channel_t::print_statistics() {
     char title[100] = "";
    sprintf(title, "Statistics of %s", this->get_label());
    sinuca_engine.write_statistics_big_separator();
    sinuca_engine.write_statistics_comments(title);
    sinuca_engine.write_statistics_big_separator();

    sinuca_engine.write_statistics_small_separator();
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_read_forward", stat_read_forward);
    sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_write_forward", stat_write_forward);

    // ~ sinuca_engine.write_statistics_small_separator();
    // ~ sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_row_buffer_miss", stat_open_new_row);
    // ~ sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "stat_row_buffer_hit", stat_accesses - stat_open_new_row);


};

/// ============================================================================
void memory_channel_t::print_configuration() {
    // ~ char title[100] = "";
    // ~ sprintf(title, "Configuration of %s", this->get_label());
    // ~ sinuca_engine.write_statistics_big_separator();
    // ~ sinuca_engine.write_statistics_comments(title);
    // ~ sinuca_engine.write_statistics_big_separator();
// ~
    // ~ sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "prefetcher_type", get_enum_prefetch_policy_char(prefetcher_type));
    // ~ sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "full_buffer_type", get_enum_full_buffer_char(full_buffer_type));
    // ~ sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "request_buffer_size", request_buffer_size);
// ~
    // ~ sinuca_engine.write_statistics_small_separator();
    // ~ sinuca_engine.write_statistics_value(get_type_component_label(), get_label(), "offset_bits_mask", utils_t::address_to_binary(this->offset_bits_mask).c_str());
};