/// ============================================================================
//
// Copyright (C) 2010, 2011, 2012
// Marco Antonio Zanata Alves
// Eduardo Henrique Molina da Cruz
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
#include "./../sinuca.hpp"
#include <string>

/// ============================================================================
memory_package_t::memory_package_t() {
    ERROR_ASSERT_PRINTF(sinuca_engine.get_global_line_size() > 0, "Allocating 0 positions.\n")
    this->sub_blocks = utils_t::template_allocate_initialize_array<bool>(sinuca_engine.get_global_line_size(), false);

    this->package_clean();
};

/// ============================================================================
memory_package_t::~memory_package_t() {
    utils_t::template_delete_array<bool>(sub_blocks);
};

/// ============================================================================
memory_package_t &memory_package_t::operator=(const memory_package_t &package) {

    this->id_owner = package.id_owner;
    this->opcode_number = package.opcode_number;
    this->opcode_address = package.opcode_address;
    this->uop_number = package.uop_number;
    this->memory_address = package.memory_address;
    this->memory_size = package.memory_size;

    this->state = package.state;
    this->ready_cycle = sinuca_engine.get_global_cycle();
    this->born_cycle = sinuca_engine.get_global_cycle();

    this->memory_operation = package.memory_operation;
    this->is_answer = package.is_answer;

    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        this->sub_blocks[i] = package.sub_blocks[i];
    }

    /// Routing Control
    this->id_src = package.id_src;
    this->id_dst = package.id_dst;
    this->hops = package.hops;
    this->hop_count = package.hop_count;

    return *this;
};

/// ============================================================================
void memory_package_t::package_clean() {
    this->id_owner = 0;
    this->opcode_number = 0;
    this->opcode_address = 0;
    this->uop_number = 0;
    this->memory_address = 0;
    this->memory_size = 0;

    this->state = PACKAGE_STATE_FREE;
    this->ready_cycle = sinuca_engine.get_global_cycle();
    this->born_cycle = sinuca_engine.get_global_cycle();

    this->memory_operation = MEMORY_OPERATION_INST;
    this->is_answer = false;

    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        this->sub_blocks[i] = false;
    }

    /// Routing Control
    this->id_src = 0;
    this->id_dst = 0;
    this->hops = NULL;
    this->hop_count = POSITION_FAIL;
};

/// ============================================================================
void memory_package_t::package_set_src_dst(uint32_t id_src, uint32_t id_dst) {
    this->id_src = id_src;
    this->id_dst = id_dst;
    this->hops = NULL;
    this->hop_count = POSITION_FAIL;
};

/// ============================================================================
void memory_package_t::package_untreated(uint32_t stall_time) {
    this->ready_cycle = sinuca_engine.get_global_cycle() + stall_time;
    this->state = PACKAGE_STATE_UNTREATED;
};

/// ============================================================================
void memory_package_t::package_wait(uint32_t stall_time) {
    this->ready_cycle = sinuca_engine.get_global_cycle() + stall_time;
    this->state = PACKAGE_STATE_WAIT;
};

/// ============================================================================
void memory_package_t::package_ready(uint32_t stall_time) {
    this->ready_cycle = sinuca_engine.get_global_cycle() + stall_time;
    this->state = PACKAGE_STATE_READY;
};

/// ============================================================================
void memory_package_t::package_transmit(uint32_t stall_time) {
    this->ready_cycle = sinuca_engine.get_global_cycle() + stall_time;
    this->state = PACKAGE_STATE_TRANSMIT;
};

/// ============================================================================
void memory_package_t::packager(uint32_t id_owner, uint64_t opcode_number, uint64_t opcode_address, uint64_t uop_number,
                                uint64_t memory_address, uint32_t memory_size,
                                package_state_t state, uint32_t stall_time,
                                memory_operation_t memory_operation, bool is_answer,
                                uint32_t id_src, uint32_t id_dst, uint32_t *hops, uint32_t hop_count) {

    ERROR_ASSERT_PRINTF(this->state == PACKAGE_STATE_FREE, "Wrong package state.\n");
    ERROR_ASSERT_PRINTF(stall_time < MAX_ALIVE_TIME, "Stall time should be less than MAX_ALIVE_TIME.\n");
    ERROR_ASSERT_PRINTF(memory_size > 0, "Memory size should be bigger than 0.\n");

    this->id_owner = id_owner;
    this->opcode_number = opcode_number;
    this->opcode_address = opcode_address;
    this->uop_number = uop_number;

    this->memory_address = memory_address;
    this->memory_size = memory_size;

    this->state = state;
    this->ready_cycle = stall_time + sinuca_engine.get_global_cycle();
    this->born_cycle = sinuca_engine.get_global_cycle();

    this->memory_operation = memory_operation;
    this->is_answer = is_answer;

    /// Routing Control
    this->id_src = id_src;
    this->id_dst = id_dst;
    this->hops = hops;
    this->hop_count = hop_count;
};

/// ============================================================================
std::string memory_package_t::content_to_string() {
    std::string content_string;
    content_string = "";

    #ifndef SHOW_FREE_PACKAGE
        if (this->state == PACKAGE_STATE_FREE) {
            return content_string;
        }
    #endif
    content_string = content_string + " MEM: Owner:" + utils_t::uint32_to_char(this->id_owner);
    content_string = content_string + " OPCode#" + utils_t::uint64_to_char(this->opcode_number);
    content_string = content_string + " 0x" + utils_t::uint64_to_char(this->opcode_address);
    content_string = content_string + " UOP#" + utils_t::uint64_to_char(this->uop_number);

    content_string = content_string + " | " + get_enum_memory_operation_char(this->memory_operation);
    if (this->is_answer) {
        content_string = content_string + " ANS ";
    }
    else {
        content_string = content_string + " RQST";
    }

    content_string = content_string + " 0x" + utils_t::uint64_to_char(this->memory_address);
    content_string = content_string + " Size:" + utils_t::uint32_to_char(this->memory_size);

    content_string = content_string + " | " + get_enum_package_state_char(this->state);
    content_string = content_string + " Ready:" + utils_t::uint64_to_char(this->ready_cycle);
    content_string = content_string + " Born:" + utils_t::uint64_to_char(this->born_cycle);


    content_string = content_string + " | SRC:" + utils_t::uint32_to_char(this->id_src);
    content_string = content_string + " => DST:" + utils_t::uint32_to_char(this->id_dst);
    content_string = content_string + " HOPS:" + utils_t::int32_to_char(this->hop_count);

    if (this->hops != NULL) {
        for (int32_t i = 0; i <= this->hop_count; i++) {
            content_string = content_string + " [" + utils_t::uint32_to_char(this->hops[i]) + "]";
        }
    }
    return content_string;
};

/// ============================================================================
std::string memory_package_t::sub_blocks_to_string() {
    std::string content_string;
    content_string = "";

    #ifndef SHOW_FREE_PACKAGE
        if (this->state == PACKAGE_STATE_FREE) {
            return content_string;
        }
    #endif

    content_string = content_string + " SUB_BLOCKS:[";
    for (uint32_t i = 0; i < sinuca_engine.get_global_line_size(); i++) {
        if (i % 4 == 0) {
            content_string = content_string + "|";
        }
        content_string = content_string + utils_t::uint32_to_char(this->sub_blocks[i]);
    }
    content_string = content_string + "]\n";
    return content_string;
};

/// ============================================================================
int32_t memory_package_t::find_free(memory_package_t *input_array, uint32_t size_array) {
    for (uint32_t i = 0; i < size_array ; i++) {
        if (input_array[i].state == PACKAGE_STATE_FREE)
            return i;
    }
    return POSITION_FAIL;
};

uint32_t memory_package_t::count_free(memory_package_t *input_array, uint32_t size_array) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < size_array ; i++) {
        if (input_array[i].state == PACKAGE_STATE_FREE)
            count++;
    }
    return count;
};


/// ============================================================================
int32_t memory_package_t::find_old_request_state_ready(memory_package_t *input_array, uint32_t size_array, package_state_t state) {
    int32_t old_pos = POSITION_FAIL;
    uint64_t old_cycle = sinuca_engine.get_global_cycle() + 1;

    for (uint32_t i = 0; i < size_array ; i++) {
        if (input_array[i].state == state &&
        input_array[i].is_answer == false &&
        old_cycle > input_array[i].born_cycle &&
        input_array[i].ready_cycle <= sinuca_engine.get_global_cycle()) {
            old_cycle = input_array[i].born_cycle;
            old_pos = i;
        }
    }
    return old_pos;
};


/// ============================================================================
int32_t memory_package_t::find_old_answer_state_ready(memory_package_t *input_array, uint32_t size_array, package_state_t state) {
    int32_t old_pos = POSITION_FAIL;
    uint64_t old_cycle = sinuca_engine.get_global_cycle() + 1;

    for (uint32_t i = 0; i < size_array ; i++) {
        if (input_array[i].state == state &&
        input_array[i].is_answer == true &&
        old_cycle > input_array[i].born_cycle &&
        input_array[i].ready_cycle <= sinuca_engine.get_global_cycle()) {
            old_cycle = input_array[i].born_cycle;
            old_pos = i;
        }
    }
    return old_pos;
};

/// ============================================================================
int32_t memory_package_t::find_state_mem_address(memory_package_t *input_array, uint32_t size_array, package_state_t state, uint64_t uop_number, uint64_t address) {

    for (uint32_t i = 0; i < size_array ; i++) {
        if (input_array[i].state == state &&
        input_array[i].ready_cycle <= sinuca_engine.get_global_cycle() &&
        input_array[i].uop_number == uop_number &&
        input_array[i].memory_address == address) {
            return i;
        }
    }
    return POSITION_FAIL;
};


/// ============================================================================
std::string memory_package_t::print_all(memory_package_t *input_array, uint32_t size_array) {
    std::string content_string;
    std::string final_string;

    final_string = "";
    for (uint32_t i = 0; i < size_array ; i++) {
        content_string = "";
        content_string = input_array[i].content_to_string();
        if (content_string.size() > 1) {
            final_string = final_string + "[" + utils_t::uint32_to_string(i) + "] " + content_string + "\n";
        }
    }
    return final_string;
};

/// ============================================================================
std::string memory_package_t::print_all(memory_package_t **input_matrix, uint32_t size_x_matrix, uint32_t size_y_matrix) {
    std::string content_string;
    std::string ColumnString;
    std::string final_string;

    final_string = "";
    for (uint32_t i = 0; i < size_x_matrix ; i++) {
        ColumnString = "";
        for (uint32_t j = 0; j < size_y_matrix ; j++) {
            content_string = "";
            content_string = input_matrix[i][j].content_to_string();
            if (content_string.size() > 1) {
                ColumnString = ColumnString +
                                "[" + utils_t::uint32_to_string(i) + "]" +
                                "[" + utils_t::uint32_to_string(j) + "] " + content_string + "\n";
            }
        }
        if (ColumnString.size() > 1) {
            final_string = final_string + ColumnString + "\n";
        }
    }
    return final_string;
};


/// ============================================================================
bool memory_package_t::check_age(memory_package_t *input_array, uint32_t size_array) {
    uint64_t min_cycle = 0;
    if (sinuca_engine.get_global_cycle() > MAX_ALIVE_TIME) {
        min_cycle = sinuca_engine.get_global_cycle() - MAX_ALIVE_TIME;
    }

    for (uint32_t i = 0; i < size_array ; i++) {
        if (input_array[i].state != PACKAGE_STATE_FREE) {
            if (input_array[i].born_cycle < min_cycle) {
                WARNING_PRINTF("CHECK AGE FAIL: %s\n", input_array[i].content_to_string().c_str())
                return FAIL;
            }
            /// Statistics of the oldest memory package
            if (sinuca_engine.get_global_cycle() - input_array[i].born_cycle > sinuca_engine.get_stat_old_memory_package()) {
                sinuca_engine.set_stat_old_memory_package(sinuca_engine.get_global_cycle() - input_array[i].born_cycle);
            }
        }
    }
    return OK;
};

/// ============================================================================
bool memory_package_t::check_age(memory_package_t **input_matrix, uint32_t size_x_matrix, uint32_t size_y_matrix) {
    uint64_t min_cycle = 0;
    if (sinuca_engine.get_global_cycle() > MAX_ALIVE_TIME) {
        min_cycle = sinuca_engine.get_global_cycle() - MAX_ALIVE_TIME;
    }

    for (uint32_t i = 0; i < size_x_matrix ; i++) {
        for (uint32_t j = 0; j < size_y_matrix ; j++) {
            if (input_matrix[i][j].state != PACKAGE_STATE_FREE) {
                if (input_matrix[i][j].born_cycle < min_cycle) {
                    WARNING_PRINTF("CHECK AGE FAIL: %s\n", input_matrix[i][j].content_to_string().c_str())
                    return FAIL;
                }
                /// Statistics of the oldest memory package
                if (sinuca_engine.get_global_cycle() - input_matrix[i][j].born_cycle > sinuca_engine.get_stat_old_memory_package()) {
                    sinuca_engine.set_stat_old_memory_package(sinuca_engine.get_global_cycle() - input_matrix[i][j].born_cycle);
                }
            }
        }
    }
    return OK;
};