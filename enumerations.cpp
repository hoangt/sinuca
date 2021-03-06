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

#include "./sinuca.hpp"
// ============================================================================
/// Enumerations
// ============================================================================

// ============================================================================
/// Enumerates the INSTRUCTION (Opcode and Uop) operation type
const char* get_enum_instruction_operation_char(instruction_operation_t type) {
    switch (type) {
        // ====================================================================
        /// INTEGERS
        case INSTRUCTION_OPERATION_INT_ALU:     return "OP_IN_ALU"; break;
        case INSTRUCTION_OPERATION_INT_MUL:     return "OP_IN_MUL"; break;
        case INSTRUCTION_OPERATION_INT_DIV:     return "OP_IN_DIV"; break;
        // ====================================================================
        /// FLOAT POINT
        case INSTRUCTION_OPERATION_FP_ALU:      return "OP_FP_ALU"; break;
        case INSTRUCTION_OPERATION_FP_MUL:      return "OP_FP_MUL"; break;
        case INSTRUCTION_OPERATION_FP_DIV:      return "OP_FP_DIV"; break;
        // ====================================================================
        /// BRANCHES
        case INSTRUCTION_OPERATION_BRANCH:      return "OP_BRANCH"; break;
        // ====================================================================
        /// MEMORY OPERATIONS
        case INSTRUCTION_OPERATION_MEM_LOAD:    return "OP_LOAD  "; break;
        case INSTRUCTION_OPERATION_MEM_STORE:   return "OP_STORE "; break;
        // ====================================================================
        /// NOP or NOT IDENTIFIED
        case INSTRUCTION_OPERATION_NOP:         return "OP_NOP   "; break;
        case INSTRUCTION_OPERATION_OTHER:       return "OP_OTHER "; break;
        // ====================================================================
        /// SYNCHRONIZATION
        case INSTRUCTION_OPERATION_BARRIER:     return "OP_BARRIER"; break;
        // ====================================================================
        /// HMC
        case INSTRUCTION_OPERATION_HMC_ALU:     return "OP_HMC_ALU"; break;
        case INSTRUCTION_OPERATION_HMC_ALUR:    return "OP_HMC_ALUR"; break;
    };
    ERROR_PRINTF("Wrong INSTRUCTION_OPERATION\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the MEMORY operation type
const char* get_enum_memory_operation_char(memory_operation_t type) {
    switch (type) {
        case MEMORY_OPERATION_INST:         return "INST "; break;
        case MEMORY_OPERATION_READ:         return "READ "; break;
        case MEMORY_OPERATION_PREFETCH:     return "PFTCH"; break;
        case MEMORY_OPERATION_WRITE:        return "WRITE"; break;
        case MEMORY_OPERATION_WRITEBACK:    return "WBACK"; break;

        // ====================================================================
        /// HMC
        case MEMORY_OPERATION_HMC_ALU:      return "HMC_ALU"; break;
        case MEMORY_OPERATION_HMC_ALUR:     return "HMC_ALUR"; break;
    };
    ERROR_PRINTF("Wrong MEMORY_OPERATION\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the package status when it arrives on the components
const char *get_enum_package_state_char(package_state_t type) {
    switch (type) {
        case PACKAGE_STATE_FREE:        return "FREE "; break;
        case PACKAGE_STATE_UNTREATED:   return "UNTD "; break;
        case PACKAGE_STATE_READY:       return "READY"; break;
        case PACKAGE_STATE_WAIT:        return "WAIT "; break;
        case PACKAGE_STATE_TRANSMIT:    return "XMIT "; break;
    };
    ERROR_PRINTF("Wrong PACKAGE_STATE\n");
    return "FAIL";
};

// ============================================================================
/// Enumarates the data and instruction ports inside the processor
const char *get_enum_processor_port_char(processor_port_t type) {
    switch (type) {
        case PROCESSOR_PORT_DATA_CACHE:   return "DATA_PORT "; break;
        case PROCESSOR_PORT_INST_CACHE:   return "INST_PORT "; break;
        case PROCESSOR_PORT_NUMBER:       return "TOTAL_PORTS"; break;
    };
    ERROR_PRINTF("Wrong PROCESSOR_PORT\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the coherence protocol status
const char *get_enum_protocol_status_char(protocol_status_t type) {
    switch (type) {
        case PROTOCOL_STATUS_M: return "MODF"; break;
        case PROTOCOL_STATUS_O: return "OWNR"; break;
        case PROTOCOL_STATUS_E: return "EXCL"; break;
        case PROTOCOL_STATUS_S: return "SHRD"; break;
        case PROTOCOL_STATUS_I: return "INVD"; break;
    };
    ERROR_PRINTF("Wrong PROTOCOL_STATUS\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the types of components
const char *get_enum_component_char(component_t type) {
    switch (type) {
        case COMPONENT_PROCESSOR:                   return "PROCESSOR"; break;
        case COMPONENT_CACHE_MEMORY:                return "CACHE_MEMORY"; break;
        case COMPONENT_MEMORY_CONTROLLER:           return "MEMORY_CONTROLLER"; break;
        case COMPONENT_INTERCONNECTION_ROUTER:      return "INTERCONNECTION_ROUTER"; break;
        case COMPONENT_DIRECTORY_CONTROLLER:        return "DIRECTORY_CONTROLLER"; break;
        case COMPONENT_INTERCONNECTION_CONTROLLER:  return "INTERCONNECTION_CONTROLLER"; break;
        /// LAST ELEMENT
        case COMPONENT_NUMBER: return "COMPONENT_NUMBER"; break;
    };
    ERROR_PRINTF("Wrong COMPONENT\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the types of hash function
const char *get_enum_hash_function_char(hash_function_t type)  {
    switch (type) {
        case HASH_FUNCTION_XOR_SIMPLE:  return "HASH_FUNCTION_XOR_SIMPLE"; break;
        case HASH_FUNCTION_INPUT1_ONLY: return "HASH_FUNCTION_INPUT1_ONLY"; break;
        case HASH_FUNCTION_INPUT2_ONLY: return "HASH_FUNCTION_INPUT2_ONLY"; break;
        case HASH_FUNCTION_INPUT1_2BITS: return "HASH_FUNCTION_INPUT1_2BITS"; break;
        case HASH_FUNCTION_INPUT1_4BITS: return "HASH_FUNCTION_INPUT1_4BITS"; break;
        case HASH_FUNCTION_INPUT1_8BITS: return "HASH_FUNCTION_INPUT1_8BITS"; break;
        case HASH_FUNCTION_INPUT1_16BITS: return "HASH_FUNCTION_INPUT1_16BITS"; break;
        case HASH_FUNCTION_INPUT1_32BITS: return "HASH_FUNCTION_INPUT1_32BITS"; break;
    };
    ERROR_PRINTF("Wrong HASH_FUNCTION\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the type of branch predictor
const char *get_enum_branch_predictor_policy_char(branch_predictor_policy_t type) {
    switch (type) {
        case BRANCH_PREDICTOR_TWO_LEVEL_GAG:    return "BRANCH_PREDICTOR_TWO_LEVEL_GAG"; break;
        case BRANCH_PREDICTOR_TWO_LEVEL_GAS:    return "BRANCH_PREDICTOR_TWO_LEVEL_GAS"; break;
        case BRANCH_PREDICTOR_TWO_LEVEL_PAG:    return "BRANCH_PREDICTOR_TWO_LEVEL_PAG"; break;
        case BRANCH_PREDICTOR_TWO_LEVEL_PAS:    return "BRANCH_PREDICTOR_TWO_LEVEL_PAS"; break;
        case BRANCH_PREDICTOR_BI_MODAL:         return "BRANCH_PREDICTOR_BI_MODAL"; break;
        case BRANCH_PREDICTOR_STATIC_TAKEN:     return "BRANCH_PREDICTOR_STATIC_TAKEN"; break;
        case BRANCH_PREDICTOR_PERFECT:          return "BRANCH_PREDICTOR_PERFECT"; break;
        case BRANCH_PREDICTOR_DISABLE:          return "BRANCH_PREDICTOR_DISABLE"; break;
    };
    ERROR_PRINTF("Wrong BRANCH_PREDICTOR_POLICY\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the processor stages, used to indicate when the branch will be solved
const char *get_enum_processor_stage_char(processor_stage_t type) {
    switch (type) {
        case PROCESSOR_STAGE_FETCH:     return "FETCH    "; break;
        case PROCESSOR_STAGE_DECODE:    return "DECODE   "; break;
        case PROCESSOR_STAGE_RENAME:    return "RENAME   "; break;
        case PROCESSOR_STAGE_DISPATCH:  return "DISPATCH "; break;
        case PROCESSOR_STAGE_EXECUTION: return "EXECUTION"; break;
        case PROCESSOR_STAGE_COMMIT:    return "COMMIT   "; break;
    };
    ERROR_PRINTF("Wrong PROCESSOR_STAGE\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the synchronization type required by the dynamic trace.
const char *get_enum_sync_char(sync_t type) {
    switch (type) {
        case SYNC_BARRIER:                  return "BARRIER"; break;
        case SYNC_WAIT_CRITICAL_START:      return "WAIT_CRITICAL_START"; break;
        case SYNC_CRITICAL_START:           return "CRITICAL_START"; break;
        case SYNC_CRITICAL_END:             return "CRITICAL_END"; break;
        case SYNC_FREE:                     return "SYNC_FREE"; break;
    };
    ERROR_PRINTF("Wrong SYNC\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the way to treat memory dependencies.
const char *get_enum_disambiguation_char(disambiguation_t type) {
    switch (type) {
        case DISAMBIGUATION_HASHED:     return "HASHED"; break;
        case DISAMBIGUATION_DISABLE:     return "DISABLE"; break;
    };
    ERROR_PRINTF("Wrong SELECTION\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the selection policy to pick a sender or next to be treated.
const char *get_enum_selection_char(selection_t type) {
    switch (type) {
        case SELECTION_RANDOM:          return "RANDOM"; break;
        case SELECTION_ROUND_ROBIN:     return "ROUND_ROBIN"; break;
        case SELECTION_BUFFER_LEVEL:     return "BUFFER_LEVEL"; break;
    };
    ERROR_PRINTF("Wrong SELECTION\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the routing algorithm
const char *get_enum_routing_algorithm_char(routing_algorithm_t type) {
    switch (type) {
        case ROUTING_ALGORITHM_XY:              return "XY"; break;
        case ROUTING_ALGORITHM_ODD_EVEN:        return "ODD_EVEN"; break;
        case ROUTING_ALGORITHM_FLOYD_WARSHALL:  return "FLOYD_WARSHALL"; break;
    };
    ERROR_PRINTF("Wrong ROUTING_ALGORITHM\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the cache replacement policy
const char *get_enum_replacement_char(replacement_t type) {
    switch (type) {
        case REPLACEMENT_LRU:               return "LRU"; break;
        case REPLACEMENT_DEAD_OR_LRU:       return "DEAR_OR_LRU"; break;
        case REPLACEMENT_INVALID_OR_LRU:    return "INVALID_OR_LRU"; break;
        case REPLACEMENT_RANDOM:            return "RANDOM"; break;
        case REPLACEMENT_FIFO:              return "FIFO"; break;
        case REPLACEMENT_LRF:               return "LRF"; break;
    };
    ERROR_PRINTF("Wrong REPLACEMENT\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the directory coherence protocol
const char *get_enum_coherence_protocol_char(coherence_protocol_t type) {
    switch (type) {
        case COHERENCE_PROTOCOL_MOESI:  return "MOESI"; break;
    };
    ERROR_PRINTF("Wrong COHERENCE_PROTOCOL\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the directory inclusiveness protocol
const char *get_enum_inclusiveness_char(inclusiveness_t type) {
    switch (type) {
        case INCLUSIVENESS_NON_INCLUSIVE:   return "NON_INCLUSIVE"; break;
        case INCLUSIVENESS_INCLUSIVE_LLC:   return "INCLUSIVE_LLC"; break;
        case INCLUSIVENESS_INCLUSIVE_ALL:   return "INCLUSIVE_ALL"; break;
    };
    ERROR_PRINTF("Wrong INCLUSIVENESS\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the prefetcher type
const char *get_enum_prefetch_policy_char(prefetch_policy_t type) {
    switch (type) {
        case PREFETCHER_STRIDE:     return "STRIDE"; break;
        case PREFETCHER_STREAM:     return "STREAM"; break;
        case PREFETCHER_DISABLE:    return "DISABLE"; break;
    };
    ERROR_PRINTF("Wrong PREFETCH_POLICY\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the prefetcher stride state
const char *get_enum_prefetch_stride_state_char(prefetch_stride_state_t type) {
    switch (type) {
        case PREFETCHER_STRIDE_STATE_INIT:       return "INIT"; break;
        case PREFETCHER_STRIDE_STATE_TRANSIENT:  return "TRANSIENT"; break;
        case PREFETCHER_STRIDE_STATE_STEADY:     return "STEADY"; break;
        case PREFETCHER_STRIDE_STATE_NO_PRED:    return "NO_PRED"; break;
    };
    ERROR_PRINTF("Wrong PREFETCHER_STRIDE_STATE\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the prefetcher stream state
const char *get_enum_prefetch_stream_state_char(prefetch_stream_state_t type) {
    switch (type) {
        case PREFETCHER_STREAM_STATE_INVALID:              return "INVALID"; break;
        case PREFETCHER_STREAM_STATE_ALLOCATED:            return "ALLOCATED"; break;
        case PREFETCHER_STREAM_STATE_TRAINING:             return "TRAINING"; break;
        case PREFETCHER_STREAM_STATE_MONITOR_AND_REQUEST:  return "MONITOR_AND_REQUEST"; break;
    };
    ERROR_PRINTF("Wrong PREFETCHER_STREAM_STATE\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the prefetcher full buffer policy
const char *get_enum_full_buffer_char(full_buffer_t type) {
    switch (type) {
        case FULL_BUFFER_OVERRIDE:  return "FULL_BUFFER_OVERRIDE"; break;
        case FULL_BUFFER_STOP:      return "FULL_BUFFER_STOP"; break;
    };
    ERROR_PRINTF("Wrong FULL_BUFFER\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the directory line lock status
const char *get_enum_lock_char(lock_t type) {
    switch (type) {
        case LOCK_FREE:     return "UNLOCK"; break;
        case LOCK_READ:     return "LOCK_R"; break;
        case LOCK_WRITE:    return "LOCK_W"; break;
    };
    ERROR_PRINTF("Wrong LOCK\n");
    return "FAIL";
};
// ============================================================================
/// Enumerates the memory cache address mask
const char *get_enum_cache_mask_char(cache_mask_t type) {
    switch (type) {
        case CACHE_MASK_TAG_INDEX_OFFSET:       return "TAG_INDEX_OFFSET"; break;
        case CACHE_MASK_TAG_INDEX_BANK_OFFSET:  return "TAG_INDEX_BANK_OFFSET"; break;
        case CACHE_MASK_TAG_BANK_INDEX_OFFSET:  return "TAG_BANK_INDEX_OFFSET"; break;
    };
    ERROR_PRINTF("Wrong CACHE_MASK\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the main memory address mask
const char *get_enum_memory_controller_mask_char(memory_controller_mask_t type) {
    switch (type) {
        case MEMORY_CONTROLLER_MASK_ROW_BANK_COLROW_COLBYTE:              return "ROW_BANK_COLROW_COLBYTE"; break;
        case MEMORY_CONTROLLER_MASK_ROW_BANK_CHANNEL_COLROW_COLBYTE:      return "ROW_BANK_CHANNEL_COLROW_COLBYTE"; break;
        case MEMORY_CONTROLLER_MASK_ROW_BANK_CHANNEL_CTRL_COLROW_COLBYTE: return "ROW_BANK_CHANNEL_CTRL_COLROW_COLBYTE"; break;
        case MEMORY_CONTROLLER_MASK_ROW_BANK_COLROW_CHANNEL_COLBYTE:      return "ROW_BANK_COLROW_CHANNEL_COLBYTE"; break;
        case MEMORY_CONTROLLER_MASK_ROW_BANK_COLROW_CTRL_CHANNEL_COLBYTE: return "ROW_BANK_COLROW_CTRL_CHANNEL_COLBYTE"; break;
        case MEMORY_CONTROLLER_MASK_ROW_CTRL_BANK_COLROW_COLBYTE:        return "ROW_CTRL_BANK_COLROW_COLBYTE,"; break;
        case MEMORY_CONTROLLER_MASK_ROW_COLROW_BANK_CHANNEL_COLBYTE:      return "ROW_COLROW_BANK_CHANNEL_COLBYTE"; break;
    };
    ERROR_PRINTF("Wrong MEMORY_CONTROLLER_MASK\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the memory controller commands to the DRAM
const char *get_enum_memory_controller_command_char(memory_controller_command_t type) {
    switch (type) {
        case MEMORY_CONTROLLER_COMMAND_PRECHARGE:       return "PRECHARGE"; break;
        case MEMORY_CONTROLLER_COMMAND_ROW_ACCESS:      return "ROW_ACCESS"; break;
        case MEMORY_CONTROLLER_COMMAND_COLUMN_READ:     return "COLUMN_READ"; break;
        case MEMORY_CONTROLLER_COMMAND_COLUMN_WRITE:    return "COLUMN_WRITE"; break;

        case MEMORY_CONTROLLER_COMMAND_NUMBER:          return "NUMBER"; break;
    };
    ERROR_PRINTF("Wrong MEMORY_CONTROLLER_COMMAND\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the policies to set the priority during the Row Buffer access
const char *get_enum_request_priority_char(request_priority_t type) {
    switch (type) {
        case REQUEST_PRIORITY_ROW_BUFFER_HITS_FIRST:    return "ROW_BUFFER_HITS_FIRST"; break;
        case REQUEST_PRIORITY_FIRST_COME_FIRST_SERVE:   return "FIRST_COME_FIRST_SERVE"; break;
    };
    ERROR_PRINTF("Wrong MEMORY_CONTROLLER REQUEST_PRIORITY\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the policies to control the page (row buffer) inside the memory controller
const char *get_enum_page_policy_char(page_policy_t type) {
    switch (type) {
        case PAGE_POLICY_OPEN_ROW:    return "PAGE_POLICY_OPEN_ROW"; break;
        case PAGE_POLICY_CLOSE_ROW:   return "PAGE_POLICY_CLOSE_ROW"; break;
    };
    ERROR_PRINTF("Wrong MEMORY_CONTROLLER PAGE_POLICY\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the policies to give privilege of some operations over others
const char *get_enum_write_priority_char(write_priority_t type) {
    switch (type) {
        case WRITE_PRIORITY_SERVICE_AT_NO_READ:     return "SERVICE_AT_NO_READ"; break;
        case WRITE_PRIORITY_DRAIN_WHEN_FULL:        return "DRAIN_WHEN_FULL"; break;
    };
    ERROR_PRINTF("Wrong WRITE_PRIORITY\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the line usage predictor type
const char *get_enum_line_usage_predictor_policy_char(line_usage_predictor_policy_t type) {
    switch (type) {
        case LINE_USAGE_PREDICTOR_POLICY_DISABLE:       return "DISABLE";       break;
        case LINE_USAGE_PREDICTOR_POLICY_DEWP:          return "DEWP";          break;
        case LINE_USAGE_PREDICTOR_POLICY_DEWP_ORACLE:   return "DEWP_ORACLE";   break;
        case LINE_USAGE_PREDICTOR_POLICY_SKEWED:        return "SKEWED";   break;
    };
    ERROR_PRINTF("Wrong LINE_USAGE_PREDICTOR_POLICY\n");
    return "FAIL";
};

// ============================================================================
/// Enumerates the valid sub-block type
const char *get_enum_line_prediction_t_char(line_prediction_t type) {
    switch (type) {
        case LINE_PREDICTION_TURNOFF:        return "DISABLE";     break;
        case LINE_PREDICTION_NORMAL:         return "NORMAL";      break;
        case LINE_PREDICTION_LEARN:          return "LEARN";       break;
        case LINE_PREDICTION_WRONG_FIRST:    return "WRONG";       break;
        case LINE_PREDICTION_WRITEBACK:      return "WRITEBACK";    break;
    };
    ERROR_PRINTF("Wrong LINE_PREDICTION\n");
    return "FAIL";
};
