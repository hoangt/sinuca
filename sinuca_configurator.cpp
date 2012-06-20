//==============================================================================
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
//==============================================================================
#include "./sinuca.hpp"

#if defined(CONFIGURATOR_DEBUG)
    #define CONFIGURATOR_DEBUG_PRINTF(...) SINUCA_PRINTF(__VA_ARGS__);
#else
    #define CONFIGURATOR_DEBUG_PRINTF(...)
#endif
//==============================================================================

//==============================================================================
void sinuca_engine_t::initialize() {
    // =========================================================================
    // Read the file. If there is an error, report it and exit.
    // =========================================================================
    libconfig::Config cfg;
    try {
        cfg.readFile(this->arg_configuration_file_name);
    }
    catch(libconfig::FileIOException &fioex) {
        ERROR_PRINTF("I/O error while reading file.\n")
    }
    catch(libconfig::ParseException &pex) {
        ERROR_PRINTF("Parse error at %s on line %d: %s\n", pex.getFile(), pex.getLine(), pex.getError());
    }

    libconfig::Setting &cfg_root = cfg.getRoot();

    /// Check if all required components exist
    for (uint32_t i = 0 ; i < COMPONENT_NUMBER; i++) {
        if(! cfg_root.exists(get_enum_component_char((component_t)i))) {
            ERROR_PRINTF("Required COMPONENT missing:\"%s\"\n", get_enum_component_char((component_t)i));
        }
    }

    SINUCA_PRINTF("\n");
    this->initialize_processor();
    this->initialize_cache_memory();
    this->initialize_main_memory();
    this->initialize_interconnection_router();

    this->initialize_directory_controller();
    this->initialize_interconnection_controller();
    SINUCA_PRINTF("\n");
    CONFIGURATOR_DEBUG_PRINTF("Finished components instantiation\n");

    /// ========================================================================
    /// Connect the pointers to the interconnection interface
    this->set_interconnection_interface_array_size( this->get_processor_array_size() +
                                                    this->get_cache_memory_array_size() +
                                                    this->get_main_memory_array_size() +
                                                    this->get_interconnection_router_array_size()
                                                    );
    this->interconnection_interface_array = utils_t::template_allocate_initialize_array<interconnection_interface_t*>(this->interconnection_interface_array_size, NULL);


    uint32_t total_components = 0;
    for (uint32_t i = 0; i < this->get_processor_array_size(); i++) {
        this->interconnection_interface_array[total_components] = this->processor_array[i];
        this->interconnection_interface_array[total_components]->set_id(total_components);
        total_components++;
    }
    for (uint32_t i = 0; i < this->get_cache_memory_array_size(); i++) {
        this->interconnection_interface_array[total_components] = this->cache_memory_array[i];
        this->interconnection_interface_array[total_components]->set_id(total_components);
        total_components++;
    }
    for (uint32_t i = 0; i < this->get_main_memory_array_size(); i++) {
        this->interconnection_interface_array[total_components] = this->main_memory_array[i];
        this->interconnection_interface_array[total_components]->set_id(total_components);
        total_components++;
    }
    for (uint32_t i = 0; i < this->get_interconnection_router_array_size(); i++) {
        this->interconnection_interface_array[total_components] = this->interconnection_router_array[i];
        this->interconnection_interface_array[total_components]->set_id(total_components);
        total_components++;
    }
    ERROR_ASSERT_PRINTF(total_components == this->get_interconnection_interface_array_size(), "Wrong number of components (%u).\n", total_components)

    /// ========================================================================
    /// Call the allocate() from all components
    for (uint32_t i = 0; i < this->get_interconnection_interface_array_size(); i++) {
        CONFIGURATOR_DEBUG_PRINTF("Allocating %s\n", this->interconnection_interface_array[i]->get_label());
        this->interconnection_interface_array[i]->allocate();
        this->interconnection_interface_array[i]->allocate_base();
    }
    this->set_is_simulation_allocated(true);
    CONFIGURATOR_DEBUG_PRINTF("Finished allocation\n");

    /// ========================================================================
    /// Add processor and directory inside the cache_memory
    this->make_connections();
    CONFIGURATOR_DEBUG_PRINTF("Finished make connections\n");

    /// Check if all the ports has connections and are connected.
    for (uint32_t i = 0; i < get_interconnection_interface_array_size(); i++) {
        interconnection_interface_t *obj = interconnection_interface_array[i];
        for (uint32_t j = 0; j < obj->get_max_ports(); j++) {
            ERROR_ASSERT_PRINTF(obj->get_interface_output_component(j) !=  NULL, "Port %u of component %s is not connected\n", j, obj->get_label());
        }
    }

    /// Print the Cache connections.
    std::string tree = "";
    for (uint32_t i = 0; i < get_cache_memory_array_size(); i++) {
        cache_memory_t *cache_memory = cache_memory_array[i];
        container_ptr_cache_memory_t *lower_level_cache = cache_memory->get_lower_level_cache();
        /// Found the Lowest Level Cache (Closest to the Main Memory)
        if (lower_level_cache->size() == 0) {
            tree += utils_t::connections_pretty(cache_memory, 0);
        }
    }
    SINUCA_PRINTF("CONNECTIONS:\n%s\n", tree.c_str())

    this->directory_controller->allocate();
    this->interconnection_controller->allocate();
};

//==============================================================================
void sinuca_engine_t::initialize_processor() {
    libconfig::Config cfg;
    cfg.readFile(this->arg_configuration_file_name);

    libconfig::Setting &cfg_root = cfg.getRoot();
    libconfig::Setting &cfg_processor_list = cfg_root["PROCESSOR"];
    SINUCA_PRINTF("PROCESSORS:%d\n", cfg_processor_list.getLength());

    this->set_processor_array_size(cfg_processor_list.getLength());
    this->processor_array = utils_t::template_allocate_initialize_array<processor_t*>(this->processor_array_size, NULL);

    /// ========================================================================
    /// Required PROCESSOR Parameters
    /// ========================================================================
    for (int32_t i = 0; i < cfg_processor_list.getLength(); i++) {
        this->processor_array[i] = new processor_t;

        std::deque<const char*> processor_parameters;
        std::deque<const char*> branch_predictor_parameters;

        libconfig::Setting &cfg_processor = cfg_processor_list[i];
        /// ====================================================================
        /// PROCESSOR PARAMETERS
        /// ====================================================================
        try {
            this->processor_array[i]->set_core_id(i);

            processor_parameters.push_back("LABEL");
            this->processor_array[i]->set_label( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("INTERCONNECTION_LATENCY");
            this->processor_array[i]->set_interconnection_latency( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("INTERCONNECTION_WIDTH");
            this->processor_array[i]->set_interconnection_width( cfg_processor[ processor_parameters.back() ] );

            /// ================================================================
            /// Pipeline Latency
            /// ================================================================
            processor_parameters.push_back("STAGE_FETCH_CYCLES");
            this->processor_array[i]->set_stage_fetch_cycles( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_DECODE_CYCLES");
            this->processor_array[i]->set_stage_decode_cycles( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_RENAME_CYCLES");
            this->processor_array[i]->set_stage_rename_cycles( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_DISPATCH_CYCLES");
            this->processor_array[i]->set_stage_dispatch_cycles( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_EXECUTION_CYCLES");
            this->processor_array[i]->set_stage_execution_cycles( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_COMMIT_CYCLES");
            this->processor_array[i]->set_stage_commit_cycles( cfg_processor[ processor_parameters.back() ] );


            /// ================================================================
            /// Pipeline Width
            /// ================================================================
            processor_parameters.push_back("STAGE_FETCH_WIDTH");
            this->processor_array[i]->set_stage_fetch_width( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_DECODE_WIDTH");
            this->processor_array[i]->set_stage_decode_width( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_RENAME_WIDTH");
            this->processor_array[i]->set_stage_rename_width( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_DISPATCH_WIDTH");
            this->processor_array[i]->set_stage_dispatch_width( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_EXECUTION_WIDTH");
            this->processor_array[i]->set_stage_execution_width( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("STAGE_COMMIT_WIDTH");
            this->processor_array[i]->set_stage_commit_width( cfg_processor[ processor_parameters.back() ] );

            /// ================================================================
            /// Buffers
            /// ================================================================
            processor_parameters.push_back("FETCH_BUFFER_SIZE");
            this->processor_array[i]->set_fetch_buffer_size( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("DECODE_BUFFER_SIZE");
            this->processor_array[i]->set_decode_buffer_size( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("REORDER_BUFFER_SIZE");
            this->processor_array[i]->set_reorder_buffer_size( cfg_processor[ processor_parameters.back() ] );

            /// ================================================================
            /// Integer Funcional Units
            /// ================================================================
            processor_parameters.push_back("NUMBER_FU_INT_ALU");
            this->processor_array[i]->set_number_fu_int_alu( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("LATENCY_FU_INT_ALU");
            this->processor_array[i]->set_latency_fu_int_alu( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("NUMBER_FU_INT_MUL");
            this->processor_array[i]->set_number_fu_int_mul( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("LATENCY_FU_INT_MUL");
            this->processor_array[i]->set_latency_fu_int_mul( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("NUMBER_FU_INT_DIV");
            this->processor_array[i]->set_number_fu_int_div( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("LATENCY_FU_INT_DIV");
            this->processor_array[i]->set_latency_fu_int_div( cfg_processor[ processor_parameters.back() ] );

            /// ================================================================
            /// Floating Point Funcional Units
            /// ================================================================
            processor_parameters.push_back("NUMBER_FU_FP_ALU");
            this->processor_array[i]->set_number_fu_fp_alu( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("LATENCY_FU_FP_ALU");
            this->processor_array[i]->set_latency_fu_fp_alu( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("NUMBER_FU_FP_MUL");
            this->processor_array[i]->set_number_fu_fp_mul( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("LATENCY_FU_FP_MUL");
            this->processor_array[i]->set_latency_fu_fp_mul( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("NUMBER_FU_FP_DIV");
            this->processor_array[i]->set_number_fu_fp_div( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("LATENCY_FU_FP_DIV");
            this->processor_array[i]->set_latency_fu_fp_div( cfg_processor[ processor_parameters.back() ] );

            /// ================================================================
            /// Memory Funcional Units
            /// ================================================================
            processor_parameters.push_back("NUMBER_FU_MEM_LOAD");
            this->processor_array[i]->set_number_fu_mem_load( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("LATENCY_FU_MEM_LOAD");
            this->processor_array[i]->set_latency_fu_mem_load( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("NUMBER_FU_MEM_STORE");
            this->processor_array[i]->set_number_fu_mem_store( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("LATENCY_FU_MEM_STORE");
            this->processor_array[i]->set_latency_fu_mem_store( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("READ_BUFFER_SIZE");
            this->processor_array[i]->set_read_buffer_size( cfg_processor[ processor_parameters.back() ] );
            processor_parameters.push_back("WRITE_BUFFER_SIZE");
            this->processor_array[i]->set_write_buffer_size( cfg_processor[ processor_parameters.back() ] );

            processor_parameters.push_back("CONNECTED_DATA_CACHE");
            processor_parameters.push_back("CONNECTED_INST_CACHE");

            this->processor_array[i]->set_max_ports(PROCESSOR_PORT_NUMBER);

            /// ================================================================
            /// Required BRANCH PREDICTOR Parameters
            /// ================================================================
            processor_parameters.push_back("BRANCH_PREDICTOR");
            libconfig::Setting &cfg_branch_predictor = cfg_processor_list[i][processor_parameters.back()];

            branch_predictor_parameters.push_back("TYPE");
            if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "TWO_LEVEL") ==  0) {
                this->processor_array[i]->branch_predictor.set_branch_predictor_type(BRANCH_PREDICTOR_TWO_LEVEL);
            }
            else if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "STATIC_TAKEN") ==  0) {
                this->processor_array[i]->branch_predictor.set_branch_predictor_type(BRANCH_PREDICTOR_STATIC_TAKEN);
            }
            else if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "DISABLE") ==  0) {
                this->processor_array[i]->branch_predictor.set_branch_predictor_type(BRANCH_PREDICTOR_DISABLE);
            }
            else {
                ERROR_PRINTF("PROCESSOR %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_branch_predictor[ branch_predictor_parameters.back() ].c_str(), branch_predictor_parameters.back());
            }

            branch_predictor_parameters.push_back("BTB_LINE_NUMBER");
            this->processor_array[i]->branch_predictor.set_btb_line_number(cfg_branch_predictor[ branch_predictor_parameters.back() ]);

            branch_predictor_parameters.push_back("BTB_ASSOCIATIVITY");
            this->processor_array[i]->branch_predictor.set_btb_associativity(cfg_branch_predictor[ branch_predictor_parameters.back() ]);

            branch_predictor_parameters.push_back("BTB_REPLACEMENT_POLICY");
            if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "FIFO") ==  0) {
                this->processor_array[i]->branch_predictor.set_btb_replacement_policy(REPLACEMENT_FIFO);
            }
            else if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "LRF") ==  0) {
                this->processor_array[i]->branch_predictor.set_btb_replacement_policy(REPLACEMENT_LRF);
            }
            else if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "LRU") ==  0) {
                this->processor_array[i]->branch_predictor.set_btb_replacement_policy(REPLACEMENT_LRU);
            }
            else if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "RANDOM") ==  0) {
                this->processor_array[i]->branch_predictor.set_btb_replacement_policy(REPLACEMENT_RANDOM);
            }
            else {
                ERROR_PRINTF("PROCESSOR %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_branch_predictor[ branch_predictor_parameters.back() ].c_str(), branch_predictor_parameters.back());
            }

            branch_predictor_parameters.push_back("BHT_SIGNATURE_BITS");
            this->processor_array[i]->branch_predictor.set_bht_signature_bits(cfg_branch_predictor[ branch_predictor_parameters.back() ]);

            branch_predictor_parameters.push_back("BHT_SIGNATURE_HASH");
            if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "INPUT1_ONLY") ==  0) {
                this->processor_array[i]->branch_predictor.set_bht_signature_hash(HASH_FUNCTION_INPUT1_ONLY);
            }
            else if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "INPUT2_ONLY") ==  0) {
                this->processor_array[i]->branch_predictor.set_bht_signature_hash(HASH_FUNCTION_INPUT2_ONLY);
            }
            else if (strcasecmp(cfg_branch_predictor[ branch_predictor_parameters.back() ], "XOR_SIMPLE") ==  0) {
                this->processor_array[i]->branch_predictor.set_bht_signature_hash(HASH_FUNCTION_XOR_SIMPLE);
            }
            else {
                ERROR_PRINTF("PROCESSOR %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_branch_predictor[ branch_predictor_parameters.back() ].c_str(), branch_predictor_parameters.back());
            }

            branch_predictor_parameters.push_back("BHT_FSM_BITS");
            this->processor_array[i]->branch_predictor.set_bht_fsm_bits(cfg_branch_predictor[ branch_predictor_parameters.back() ]);

            /// ================================================================
            /// Check if any PROCESSOR non-required parameters exist
            for (int32_t j = 0 ; j < cfg_processor.getLength(); j++) {
                bool is_required = false;
                for (uint32_t k = 0 ; k < processor_parameters.size(); k++) {
                    if (strcmp(cfg_processor[j].getName(), processor_parameters[k]) == 0) {
                        is_required = true;
                        break;
                    }
                }
                ERROR_ASSERT_PRINTF(is_required, "PROCESSOR %d %s has PARAMETER not required: \"%s\"\n", i, this->processor_array[i]->get_label(), cfg_processor[j].getName());
            }

            /// Check if any BRANCH PREDICTOR non-required parameters exist
            for (int32_t j = 0 ; j < cfg_branch_predictor.getLength(); j++) {
                bool is_required = false;
                for (uint32_t k = 0 ; k < branch_predictor_parameters.size(); k++) {
                    if (strcmp(cfg_branch_predictor[j].getName(), branch_predictor_parameters[k]) == 0) {
                        is_required = true;
                        break;
                    }
                }
                ERROR_ASSERT_PRINTF(is_required, "PROCESSOR %d %s BRANCH PREDICTOR has PARAMETER not required: \"%s\"\n", i, this->processor_array[i]->get_label(), cfg_processor[j].getName());
            }
        }
        catch(libconfig::SettingNotFoundException &nfex) {
            ERROR_PRINTF(" PROCESSOR %d %s has required PARAMETER missing: \"%s\" \"%s\"\n", i, this->processor_array[i]->get_label(), processor_parameters.back(), branch_predictor_parameters.size() != 0 ? branch_predictor_parameters.back() : "");
        }
        catch(libconfig::SettingTypeException &tex) {
            ERROR_PRINTF(" PROCESSOR %d %s has PARAMETER wrong type: \"%s\" \"%s\"\n", i, this->processor_array[i]->get_label(), processor_parameters.back(), branch_predictor_parameters.size() != 0 ? branch_predictor_parameters.back() : "");
        }
    }
};


//==============================================================================
void sinuca_engine_t::initialize_cache_memory() {
    libconfig::Config cfg;
    cfg.readFile(this->arg_configuration_file_name);

    libconfig::Setting &cfg_root = cfg.getRoot();
    libconfig::Setting &cfg_cache_memory_list = cfg_root["CACHE_MEMORY"];
    SINUCA_PRINTF("CACHE_MEMORIES:%d\n", cfg_cache_memory_list.getLength());

    this->set_cache_memory_array_size(cfg_cache_memory_list.getLength());
    this->cache_memory_array = utils_t::template_allocate_initialize_array<cache_memory_t*>(this->cache_memory_array_size, NULL);

    /// ========================================================================
    /// Required CACHE MEMORY Parameters
    /// ========================================================================
    for (int32_t i = 0; i < cfg_cache_memory_list.getLength(); i++) {
        this->cache_memory_array[i] = new cache_memory_t;

        std::deque<const char*> cache_memory_parameters;
        std::deque<const char*> prefetcher_parameters;
        std::deque<const char*> line_usage_predictor_parameters;

        libconfig::Setting &cfg_cache_memory = cfg_cache_memory_list[i];

        /// ====================================================================
        /// CACHE_MEMORY PARAMETERS
        /// ====================================================================
        try {
            this->cache_memory_array[i]->set_cache_id(i);

            cache_memory_parameters.push_back("LABEL");
            this->cache_memory_array[i]->set_label( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("INTERCONNECTION_LATENCY");
            this->cache_memory_array[i]->set_interconnection_latency( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("INTERCONNECTION_WIDTH");
            this->cache_memory_array[i]->set_interconnection_width( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("HIERARCHY_LEVEL");
            this->cache_memory_array[i]->set_hierarchy_level( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("PENALTY_READ");
            this->cache_memory_array[i]->set_penalty_read( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("PENALTY_WRITE");
            this->cache_memory_array[i]->set_penalty_write( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("LINE_NUMBER");
            this->cache_memory_array[i]->set_line_number( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("LINE_SIZE");
            this->cache_memory_array[i]->set_line_size( cfg_cache_memory[ cache_memory_parameters.back() ] );
            this->set_global_line_size(this->cache_memory_array[i]->get_line_size());

            cache_memory_parameters.push_back("ASSOCIATIVITY");
            this->cache_memory_array[i]->set_associativity( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("MSHR_BUFFER_REQUEST_RESERVED_SIZE");
            this->cache_memory_array[i]->set_mshr_buffer_request_reserved_size( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("MSHR_BUFFER_COPYBACK_RESERVED_SIZE");
            this->cache_memory_array[i]->set_mshr_buffer_copyback_reserved_size( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("MSHR_BUFFER_PREFETCH_RESERVED_SIZE");
            this->cache_memory_array[i]->set_mshr_buffer_prefetch_reserved_size( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("REPLACEMENT_POLICY");
            if (strcasecmp(cfg_cache_memory[ cache_memory_parameters.back() ], "FIFO") ==  0) {
                this->cache_memory_array[i]->set_replacement_policy(REPLACEMENT_FIFO);
            }
            else if (strcasecmp(cfg_cache_memory[ cache_memory_parameters.back() ], "LRF") ==  0) {
                this->cache_memory_array[i]->set_replacement_policy(REPLACEMENT_LRF);
            }
            else if (strcasecmp(cfg_cache_memory[ cache_memory_parameters.back() ], "LRU") ==  0) {
                this->cache_memory_array[i]->set_replacement_policy(REPLACEMENT_LRU);
            }
            else if (strcasecmp(cfg_cache_memory[ cache_memory_parameters.back() ], "LRU_DSBP") ==  0) {
                this->cache_memory_array[i]->set_replacement_policy(REPLACEMENT_LRU_DSBP);
            }
            else if (strcasecmp(cfg_cache_memory[ cache_memory_parameters.back() ], "RANDOM") ==  0) {
                this->cache_memory_array[i]->set_replacement_policy(REPLACEMENT_RANDOM);
            }
            else {
                ERROR_PRINTF("CACHE MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_cache_memory[ cache_memory_parameters.back() ].c_str(), cache_memory_parameters.back());
            }

            cache_memory_parameters.push_back("BANK_NUMBER");
            this->cache_memory_array[i]->set_bank_number( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("TOTAL_BANKS");
            this->cache_memory_array[i]->set_total_banks( cfg_cache_memory[ cache_memory_parameters.back() ] );

            cache_memory_parameters.push_back("ADDRESS_MASK");
            if (strcasecmp(cfg_cache_memory[ cache_memory_parameters.back() ], "TAG_INDEX_OFFSET") ==  0) {
                this->cache_memory_array[i]->set_address_mask_type(CACHE_MASK_TAG_INDEX_OFFSET);
            }
            else if (strcasecmp(cfg_cache_memory[ cache_memory_parameters.back() ], "TAG_INDEX_BANK_OFFSET") ==  0) {
                this->cache_memory_array[i]->set_address_mask_type(CACHE_MASK_TAG_INDEX_BANK_OFFSET);
            }
            else {
                ERROR_PRINTF("CACHE MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_cache_memory[ cache_memory_parameters.back() ].c_str(), cache_memory_parameters.back());
            }

            cache_memory_parameters.push_back("LOWER_LEVEL_CACHE");

            cache_memory_parameters.push_back("MAX_PORTS");
            this->cache_memory_array[i]->set_max_ports( cfg_cache_memory[ cache_memory_parameters.back() ] );

            /// ================================================================
            /// Required PREFETCHER Parameters
            /// ================================================================
            cache_memory_parameters.push_back("PREFETCHER");
            libconfig::Setting &cfg_prefetcher = cfg_cache_memory_list[i][cache_memory_parameters.back()];


            prefetcher_parameters.push_back("TYPE");
            if (strcasecmp(cfg_prefetcher[ prefetcher_parameters.back() ], "STREAM") ==  0) {
                this->cache_memory_array[i]->prefetcher.set_prefetcher_type(PREFETCHER_STREAM);
            }
            else if (strcasecmp(cfg_prefetcher[ prefetcher_parameters.back() ], "DISABLE") ==  0) {
                this->cache_memory_array[i]->prefetcher.set_prefetcher_type(PREFETCHER_DISABLE);
            }
            else {
                ERROR_PRINTF("CACHE MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_prefetcher[ prefetcher_parameters.back() ].c_str(), prefetcher_parameters.back());
            }

            prefetcher_parameters.push_back("FULL_BUFFER");
            if (strcasecmp(cfg_prefetcher[ prefetcher_parameters.back() ], "OVERRIDE") ==  0) {
                this->cache_memory_array[i]->prefetcher.set_full_buffer_type(FULL_BUFFER_OVERRIDE);
            }
            else if (strcasecmp(cfg_prefetcher[ prefetcher_parameters.back() ], "STOP") ==  0) {
                this->cache_memory_array[i]->prefetcher.set_full_buffer_type(FULL_BUFFER_STOP);
            }
            else {
                ERROR_PRINTF("CACHE MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_prefetcher[ prefetcher_parameters.back() ].c_str(), prefetcher_parameters.back());
            }

            prefetcher_parameters.push_back("REQUEST_BUFFER_SIZE");
            this->cache_memory_array[i]->prefetcher.set_request_buffer_size( cfg_prefetcher[ prefetcher_parameters.back() ] );

            prefetcher_parameters.push_back("STREAM_TABLE_SIZE");
            this->cache_memory_array[i]->prefetcher.set_stream_table_size( cfg_prefetcher[ prefetcher_parameters.back() ] );

            prefetcher_parameters.push_back("STREAM_ADDRESS_DISTANCE");
            this->cache_memory_array[i]->prefetcher.set_stream_address_distance( cfg_prefetcher[ prefetcher_parameters.back() ] );

            prefetcher_parameters.push_back("STREAM_WINDOW");
            this->cache_memory_array[i]->prefetcher.set_stream_window( cfg_prefetcher[ prefetcher_parameters.back() ] );

            prefetcher_parameters.push_back("STREAM_THRESHOLD_ACTIVATE");
            this->cache_memory_array[i]->prefetcher.set_stream_threshold_activate( cfg_prefetcher[ prefetcher_parameters.back() ] );

            prefetcher_parameters.push_back("STREAM_PREFETCH_DEGREE");
            this->cache_memory_array[i]->prefetcher.set_stream_prefetch_degree( cfg_prefetcher[ prefetcher_parameters.back() ] );

            prefetcher_parameters.push_back("STREAM_WAIT_BETWEEN_REQUESTS");
            this->cache_memory_array[i]->prefetcher.set_stream_wait_between_requests( cfg_prefetcher[ prefetcher_parameters.back() ] );


            /// ================================================================
            /// Required LINE_USAGE_PREDICTOR Parameters
            /// ================================================================
            cache_memory_parameters.push_back("LINE_USAGE_PREDICTOR");
            libconfig::Setting &cfg_line_usage_predictor = cfg_cache_memory_list[i][cache_memory_parameters.back()];

            line_usage_predictor_parameters.push_back("TYPE");
            if (strcasecmp(cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ], "DSBP") ==  0) {
                this->cache_memory_array[i]->line_usage_predictor.set_line_usage_predictor_type(LINE_USAGE_PREDICTOR_POLICY_DSBP);
            }
            else if (strcasecmp(cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ], "SPP") ==  0) {
                this->cache_memory_array[i]->line_usage_predictor.set_line_usage_predictor_type(LINE_USAGE_PREDICTOR_POLICY_SPP);
            }
            else if (strcasecmp(cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ], "DISABLE") ==  0) {
                this->cache_memory_array[i]->line_usage_predictor.set_line_usage_predictor_type(LINE_USAGE_PREDICTOR_POLICY_DISABLE);
            }
            else {
                ERROR_PRINTF("CACHE MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ].c_str(), line_usage_predictor_parameters.back());
            }


            /// DSBP Metadata
            line_usage_predictor_parameters.push_back("DSBP_LINE_NUMBER");
            this->cache_memory_array[i]->line_usage_predictor.set_DSBP_line_number( cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ] );
            ERROR_ASSERT_PRINTF(this->cache_memory_array[i]->get_line_number() == this->cache_memory_array[i]->line_usage_predictor.get_DSBP_line_number(),
                                "CACHE MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_prefetcher[ line_usage_predictor_parameters.back() ].c_str(), line_usage_predictor_parameters.back());

            line_usage_predictor_parameters.push_back("DSBP_ASSOCIATIVITY");
            this->cache_memory_array[i]->line_usage_predictor.set_DSBP_associativity( cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ] );
            ERROR_ASSERT_PRINTF(this->cache_memory_array[i]->get_associativity() == this->cache_memory_array[i]->line_usage_predictor.get_DSBP_associativity(),
                                "CACHE MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_prefetcher[ line_usage_predictor_parameters.back() ].c_str(), line_usage_predictor_parameters.back());

            line_usage_predictor_parameters.push_back("DSBP_SUB_BLOCK_SIZE");
            this->cache_memory_array[i]->line_usage_predictor.set_DSBP_sub_block_size( cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ] );

            line_usage_predictor_parameters.push_back("DSBP_USAGE_COUNTER_BITS");
            this->cache_memory_array[i]->line_usage_predictor.set_DSBP_usage_counter_bits( cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ] );

            /// PHT
            line_usage_predictor_parameters.push_back("DSBP_PHT_LINE_NUMBER");
            this->cache_memory_array[i]->line_usage_predictor.set_DSBP_PHT_line_number( cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ] );

            line_usage_predictor_parameters.push_back("DSBP_PHT_ASSOCIATIVITY");
            this->cache_memory_array[i]->line_usage_predictor.set_DSBP_PHT_associativity( cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ] );

            line_usage_predictor_parameters.push_back("DSBP_PHT_REPLACEMENT_POLICY");
            if (strcasecmp(cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ], "FIFO") ==  0) {
                this->cache_memory_array[i]->line_usage_predictor.set_DSBP_PHT_replacement_policy(REPLACEMENT_FIFO);
            }
            else if (strcasecmp(cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ], "LRF") ==  0) {
                this->cache_memory_array[i]->line_usage_predictor.set_DSBP_PHT_replacement_policy(REPLACEMENT_LRF);
            }
            else if (strcasecmp(cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ], "LRU") ==  0) {
                this->cache_memory_array[i]->line_usage_predictor.set_DSBP_PHT_replacement_policy(REPLACEMENT_LRU);
            }
            else if (strcasecmp(cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ], "RANDOM") ==  0) {
                this->cache_memory_array[i]->line_usage_predictor.set_DSBP_PHT_replacement_policy(REPLACEMENT_RANDOM);
            }
            else {
                ERROR_PRINTF("CACHE MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_line_usage_predictor[ line_usage_predictor_parameters.back() ].c_str(), line_usage_predictor_parameters.back());
            }


            /// ================================================================
            /// Check if any CACHE_MEMORY non-required parameters exist
            for (int32_t j = 0 ; j < cfg_cache_memory.getLength(); j++) {
                bool is_required = false;
                for (uint32_t k = 0 ; k < cache_memory_parameters.size(); k++) {
                    if (strcmp(cfg_cache_memory[j].getName(), cache_memory_parameters[k]) == 0) {
                        is_required = true;
                        break;
                    }
                }
                ERROR_ASSERT_PRINTF(is_required, "CACHE_MEMORY %d %s has PARAMETER not required: \"%s\"\n", i, this->cache_memory_array[i]->get_label(), cfg_cache_memory[j].getName());
            }

            /// Check if any PREFETCHER non-required parameters exist
            for (int32_t j = 0 ; j < cfg_prefetcher.getLength(); j++) {
                bool is_required = false;
                for (uint32_t k = 0 ; k < prefetcher_parameters.size(); k++) {
                    if (strcmp(cfg_prefetcher[j].getName(), prefetcher_parameters[k]) == 0) {
                        is_required = true;
                        break;
                    }
                }
                ERROR_ASSERT_PRINTF(is_required, "CACHE_MEMORY %d %s PREFETCHER has PARAMETER not required: \"%s\"\n", i, this->cache_memory_array[i]->get_label(), cfg_cache_memory[j].getName());
            }

            /// Check if any LINE_USAGE_PREDICTOR non-required parameters exist
            for (int32_t j = 0 ; j < cfg_line_usage_predictor.getLength(); j++) {
                bool is_required = false;
                for (uint32_t k = 0 ; k < line_usage_predictor_parameters.size(); k++) {
                    if (strcmp(cfg_line_usage_predictor[j].getName(), line_usage_predictor_parameters[k]) == 0) {
                        is_required = true;
                        break;
                    }
                }
                ERROR_ASSERT_PRINTF(is_required, "CACHE_MEMORY %d %s LINE_USAGE_PREDICTOR has PARAMETER not required: \"%s\"\n", i, this->cache_memory_array[i]->get_label(), cfg_cache_memory[j].getName());
            }

        }
        catch(libconfig::SettingNotFoundException &nfex) {
            /// Cache Memory Parameter
            if (prefetcher_parameters.size() == 0 && line_usage_predictor_parameters.size() == 0) {
                ERROR_PRINTF(" CACHE_MEMORY %d %s has required PARAMETER missing: \"%s\"\n", i, this->cache_memory_array[i]->get_label(), cache_memory_parameters.back());
            }
            /// Prefetcher Parameter
            else if (prefetcher_parameters.size() != 0 && line_usage_predictor_parameters.size() == 0) {
                ERROR_PRINTF(" CACHE_MEMORY %d %s has required PARAMETER missing: \"%s\" \"%s\" \n", i, this->cache_memory_array[i]->get_label(), cache_memory_parameters.back(), prefetcher_parameters.back());
            }
            /// Line_Usage_Predictor Parameter
            else {
                ERROR_PRINTF(" CACHE_MEMORY %d %s has required PARAMETER missing: \"%s\" \"%s\" \n", i, this->cache_memory_array[i]->get_label(), cache_memory_parameters.back(), line_usage_predictor_parameters.back());
            }
        }
        catch(libconfig::SettingTypeException &tex) {
            /// Cache Memory Parameter
            if (prefetcher_parameters.size() == 0 && line_usage_predictor_parameters.size() == 0) {
                ERROR_PRINTF(" CACHE_MEMORY %d %s has required PARAMETER wrong type: \"%s\"\n", i, this->cache_memory_array[i]->get_label(), cache_memory_parameters.back());
            }
            /// Prefetcher Parameter
            else if (prefetcher_parameters.size() != 0 && line_usage_predictor_parameters.size() == 0) {
                ERROR_PRINTF(" CACHE_MEMORY %d %s has required PARAMETER wrong type: \"%s\" \"%s\" \n", i, this->cache_memory_array[i]->get_label(), cache_memory_parameters.back(), prefetcher_parameters.back());
            }
            /// Line_Usage_Predictor Parameter
            else {
                ERROR_PRINTF(" CACHE_MEMORY %d %s has required PARAMETER wrong type: \"%s\" \"%s\" \n", i, this->cache_memory_array[i]->get_label(), cache_memory_parameters.back(), line_usage_predictor_parameters.back());
            }
        }
    }
};


//==============================================================================
void sinuca_engine_t::initialize_main_memory() {
    libconfig::Config cfg;
    cfg.readFile(this->arg_configuration_file_name);

    libconfig::Setting &cfg_root = cfg.getRoot();
    libconfig::Setting &cfg_main_memory_list = cfg_root["MAIN_MEMORY"];
    SINUCA_PRINTF("MAIN_MEMORIES:%d\n", cfg_main_memory_list.getLength());

    this->set_main_memory_array_size(cfg_main_memory_list.getLength());
    this->main_memory_array = utils_t::template_allocate_initialize_array<main_memory_t*>(this->main_memory_array_size, NULL);

    /// ========================================================================
    /// Required MAIN_MEMORY Parameters
    /// ========================================================================
    for (int32_t i = 0; i < cfg_main_memory_list.getLength(); i++) {
        this->main_memory_array[i] = new main_memory_t;

        std::deque<const char*> main_memory_parameters;
        libconfig::Setting &cfg_main_memory = cfg_main_memory_list[i];

        /// ====================================================================
        /// MAIN_MEMORY PARAMETERS
        /// ====================================================================
        try {
            main_memory_parameters.push_back("LABEL");
            this->main_memory_array[i]->set_label( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("INTERCONNECTION_LATENCY");
            this->main_memory_array[i]->set_interconnection_latency( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("INTERCONNECTION_WIDTH");
            this->main_memory_array[i]->set_interconnection_width( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("LINE_SIZE");
            this->main_memory_array[i]->set_line_size( cfg_main_memory[ main_memory_parameters.back() ] );
            this->set_global_line_size(this->main_memory_array[i]->get_line_size());

            main_memory_parameters.push_back("DATA_BUS_LATENCY");
            this->main_memory_array[i]->set_data_bus_latency( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("READ_BUFFER_SIZE");
            this->main_memory_array[i]->set_read_buffer_size( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("WRITE_BUFFER_SIZE");
            this->main_memory_array[i]->set_write_buffer_size( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("CHANNEL_NUMBER");
            this->main_memory_array[i]->set_channel_number( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("TOTAL_CHANNELS");
            this->main_memory_array[i]->set_total_channels( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("BANKS_PER_CHANNEL");
            this->main_memory_array[i]->set_banks_per_channel( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("ADDRESS_MASK");
            if (strcasecmp(cfg_main_memory[ main_memory_parameters.back() ], "ROW_BANK_CHANNEL_COLUMN") ==  0) {
                this->main_memory_array[i]->set_address_mask_type(MAIN_MEMORY_MASK_ROW_BANK_CHANNEL_COLUMN);
            }
            else if (strcasecmp(cfg_main_memory[ main_memory_parameters.back() ], "ROW_BANK_COLUMN") ==  0) {
                this->main_memory_array[i]->set_address_mask_type(MAIN_MEMORY_MASK_ROW_BANK_COLUMN);
            }
            else {
                ERROR_PRINTF("MAIN MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_main_memory[ main_memory_parameters.back() ].c_str(), main_memory_parameters.back());
            }

            main_memory_parameters.push_back("BANK_PENALTY_CAS");
            this->main_memory_array[i]->set_bank_penalty_cas( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("BANK_PENALTY_RAS");
            this->main_memory_array[i]->set_bank_penalty_ras( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("ROW_BUFFER_SIZE");
            this->main_memory_array[i]->set_row_buffer_size( cfg_main_memory[ main_memory_parameters.back() ] );

            main_memory_parameters.push_back("ROW_BUFFER_POLICY");
            if (strcasecmp(cfg_main_memory[ main_memory_parameters.back() ], "HITS_FIRST") ==  0) {
                this->main_memory_array[i]->set_row_buffer_policy(ROW_BUFFER_HITS_FIRST);
            }
            else if (strcasecmp(cfg_main_memory[ main_memory_parameters.back() ], "FIFO") ==  0) {
                this->main_memory_array[i]->set_row_buffer_policy(ROW_BUFFER_FIFO);
            }
            else {
                ERROR_PRINTF("MAIN MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_main_memory[ main_memory_parameters.back() ].c_str(), main_memory_parameters.back());
            }

            main_memory_parameters.push_back("WRITE_PRIORITY_POLICY");
            if (strcasecmp(cfg_main_memory[ main_memory_parameters.back() ], "SERVICE_AT_NO_READ") ==  0) {
                this->main_memory_array[i]->set_write_priority_policy(WRITE_PRIORITY_SERVICE_AT_NO_READ);
            }
            else if (strcasecmp(cfg_main_memory[ main_memory_parameters.back() ], "DRAIN_WHEN_FULL") ==  0) {
                this->main_memory_array[i]->set_write_priority_policy(WRITE_PRIORITY_DRAIN_WHEN_FULL);
            }
            else {
                ERROR_PRINTF("MAIN MEMORY %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_main_memory[ main_memory_parameters.back() ].c_str(), main_memory_parameters.back());
            }

            this->main_memory_array[i]->set_max_ports(1);

            /// ================================================================
            /// Check if any MAIN_MEMORY non-required parameters exist
            for (int32_t j = 0 ; j < cfg_main_memory.getLength(); j++) {
                bool is_required = false;
                for (uint32_t k = 0 ; k < main_memory_parameters.size(); k++) {
                    if (strcmp(cfg_main_memory[j].getName(), main_memory_parameters[k]) == 0) {
                        is_required = true;
                        break;
                    }
                }
                ERROR_ASSERT_PRINTF(is_required, "MAIN_MEMORY %d has PARAMETER not required: \"%s\"\n", i, cfg_main_memory[j].getName());
            }

        }
        catch(libconfig::SettingNotFoundException &nfex) {
            ERROR_PRINTF(" MAIN_MEMORY %d has required PARAMETER missing: \"%s\"\n", i, main_memory_parameters.back());
        }
        catch(libconfig::SettingTypeException &tex) {
            ERROR_PRINTF(" MAIN_MEMORY %d has PARAMETER wrong type: \"%s\"\n", i, main_memory_parameters.back());
        }
    }
};

//==============================================================================
void sinuca_engine_t::initialize_interconnection_router() {
    libconfig::Config cfg;
    cfg.readFile(this->arg_configuration_file_name);

    libconfig::Setting &cfg_root = cfg.getRoot();
    libconfig::Setting &cfg_interconnection_router_list = cfg_root["INTERCONNECTION_ROUTER"];
    SINUCA_PRINTF("INTERCONNECTION_ROUTERS:%d\n", cfg_interconnection_router_list.getLength());

    this->set_interconnection_router_array_size(cfg_interconnection_router_list.getLength());
    this->interconnection_router_array = utils_t::template_allocate_initialize_array<interconnection_router_t*>(this->interconnection_router_array_size, NULL);

    /// ========================================================================
    /// Required CACHE MEMORY Parameters
    /// ========================================================================
    for (int32_t i = 0; i < cfg_interconnection_router_list.getLength(); i++) {
        this->interconnection_router_array[i] = new interconnection_router_t;

        std::deque<const char*> interconnection_router_parameters;
        libconfig::Setting &cfg_interconnection_router = cfg_interconnection_router_list[i];

        /// ====================================================================
        /// INTERCONNECTION_ROUTER PARAMETERS
        /// ====================================================================
        try {
            interconnection_router_parameters.push_back("LABEL");
            this->interconnection_router_array[i]->set_label( cfg_interconnection_router[ interconnection_router_parameters.back() ] );

            interconnection_router_parameters.push_back("INTERCONNECTION_LATENCY");
            this->interconnection_router_array[i]->set_interconnection_latency( cfg_interconnection_router[ interconnection_router_parameters.back() ] );

            interconnection_router_parameters.push_back("INTERCONNECTION_WIDTH");
            this->interconnection_router_array[i]->set_interconnection_width( cfg_interconnection_router[ interconnection_router_parameters.back() ] );

            interconnection_router_parameters.push_back("SELECTION_POLICY");
            if (strcasecmp(cfg_interconnection_router[ interconnection_router_parameters.back() ], "RANDOM") ==  0) {
                this->interconnection_router_array[i]->set_selection_policy(SELECTION_RANDOM);
            }
            else if (strcasecmp(cfg_interconnection_router[ interconnection_router_parameters.back() ], "ROUND_ROBIN") ==  0) {
                this->interconnection_router_array[i]->set_selection_policy(SELECTION_ROUND_ROBIN);
            }
            else {
                ERROR_PRINTF("INTERCONNECTION_ROUTER %d found a strange VALUE %s for PARAMETER %s\n", i, cfg_interconnection_router[ interconnection_router_parameters.back() ].c_str(), interconnection_router_parameters.back());
            }

            interconnection_router_parameters.push_back("INPUT_BUFFER_SIZE");
            this->interconnection_router_array[i]->set_input_buffer_size( cfg_interconnection_router[ interconnection_router_parameters.back() ] );

            interconnection_router_parameters.push_back("CONNECTED_COMPONENT");
            this->interconnection_router_array[i]->set_max_ports( cfg_interconnection_router[ interconnection_router_parameters.back() ].getLength() );

            /// ================================================================
            /// Check if any INTERCONNECTION_ROUTER non-required parameters exist
            for (int32_t j = 0 ; j < cfg_interconnection_router.getLength(); j++) {
                bool is_required = false;
                for (uint32_t k = 0 ; k < interconnection_router_parameters.size(); k++) {
                    if (strcmp(cfg_interconnection_router[j].getName(), interconnection_router_parameters[k]) == 0) {
                        is_required = true;
                        break;
                    }
                }
                ERROR_ASSERT_PRINTF(is_required, "INTERCONNECTION_ROUTER %d has PARAMETER not required: \"%s\"\n", i, cfg_interconnection_router[j].getName());
            }

        }
        catch(libconfig::SettingNotFoundException &nfex) {
            ERROR_PRINTF(" INTERCONNECTION_ROUTER %d has required PARAMETER missing: \"%s\"\n", i, interconnection_router_parameters.back());
        }
        catch(libconfig::SettingTypeException &tex) {
            ERROR_PRINTF(" INTERCONNECTION_ROUTER %d has PARAMETER wrong type: \"%s\"\n", i, interconnection_router_parameters.back());
        }
    }
};

//==============================================================================
void sinuca_engine_t::initialize_directory_controller() {
    libconfig::Config cfg;
    cfg.readFile(this->arg_configuration_file_name);

    libconfig::Setting &cfg_root = cfg.getRoot();
    libconfig::Setting &cfg_directory = cfg_root["DIRECTORY_CONTROLLER"];
    SINUCA_PRINTF("DIRECTORY_CONTROLLER\n");

    /// ========================================================================
    /// Required DIRECTORY Parameters
    /// ========================================================================
    this->directory_controller = new directory_controller_t;
    std::deque<const char*> directory_parameters;
    /// ====================================================================
    /// DIRECTORY PARAMETERS
    /// ====================================================================
    try {
        directory_parameters.push_back("COHERENCE_PROTOCOL");
        if (strcasecmp(cfg_directory[ directory_parameters.back() ], "MOESI") ==  0) {
            this->directory_controller->set_coherence_protocol_type(COHERENCE_PROTOCOL_MOESI);
        }
        else {
            ERROR_PRINTF("DIRECTORY found a strange VALUE %s for PARAMETER %s\n", cfg_directory[ directory_parameters.back() ].c_str(), directory_parameters.back());
        }

        directory_parameters.push_back("INCLUSIVENESS");
        if (strcasecmp(cfg_directory[ directory_parameters.back() ], "NON_INCLUSIVE") ==  0) {
            this->directory_controller->set_inclusiveness_type(INCLUSIVENESS_NON_INCLUSIVE);
        }
        else {
            ERROR_PRINTF("DIRECTORY found a strange VALUE %s for PARAMETER %s\n", cfg_directory[ directory_parameters.back() ].c_str(), directory_parameters.back());
        }

        /// ================================================================
        /// Check if any DIRECTORY non-required parameters exist
        for (int32_t j = 0 ; j < cfg_directory.getLength(); j++) {
            bool is_required = false;
            for (uint32_t k = 0 ; k < directory_parameters.size(); k++) {
                if (strcmp(cfg_directory[j].getName(), directory_parameters[k]) == 0) {
                    is_required = true;
                    break;
                }
            }
            ERROR_ASSERT_PRINTF(is_required, "DIRECTORY has PARAMETER not required: \"%s\"\n", cfg_directory[j].getName());
        }

    }
    catch(libconfig::SettingNotFoundException &nfex) {
        ERROR_PRINTF(" DIRECTORY has required PARAMETER missing: \"%s\"\n", directory_parameters.back());
    }
    catch(libconfig::SettingTypeException &tex) {
        ERROR_PRINTF(" DIRECTORY has PARAMETER wrong type: \"%s\"\n", directory_parameters.back());
    }
};

//==============================================================================
void sinuca_engine_t::initialize_interconnection_controller() {
    libconfig::Config cfg;
    cfg.readFile(this->arg_configuration_file_name);

    libconfig::Setting &cfg_root = cfg.getRoot();
    libconfig::Setting &cfg_interconnection_controller = cfg_root["INTERCONNECTION_CONTROLLER"];
    SINUCA_PRINTF("INTERCONNECTION_CONTROLLER\n");

    /// ========================================================================
    /// Required INTERCONNECTION_CONTROLLER Parameters
    /// ========================================================================
    this->interconnection_controller = new interconnection_controller_t;
    std::deque<const char*> interconnection_controller_parameters;

    /// ====================================================================
    /// INTERCONNECTION_CONTROLLER PARAMETERS
    /// ====================================================================
    try {
        interconnection_controller_parameters.push_back("ROUTING_ALGORITHM");
        if (strcasecmp(cfg_interconnection_controller[ interconnection_controller_parameters.back() ], "XY") ==  0) {
            this->interconnection_controller->set_routing_algorithm(ROUTING_ALGORITHM_XY);
        }
        else if (strcasecmp(cfg_interconnection_controller[ interconnection_controller_parameters.back() ], "ODD_EVEN") ==  0) {
            this->interconnection_controller->set_routing_algorithm(ROUTING_ALGORITHM_ODD_EVEN);
        }
        else if (strcasecmp(cfg_interconnection_controller[ interconnection_controller_parameters.back() ], "FLOYD_WARSHALL") ==  0) {
            this->interconnection_controller->set_routing_algorithm(ROUTING_ALGORITHM_FLOYD_WARSHALL);
        }
        else {
            ERROR_PRINTF("INTERCONNECTION_CONTROLLER found a strange VALUE %s for PARAMETER %s\n", cfg_interconnection_controller[ interconnection_controller_parameters.back() ].c_str(), interconnection_controller_parameters.back());
        }

        /// ================================================================
        /// Check if any INTERCONNECTION_CONTROLLER non-required parameters exist
        for (int32_t j = 0 ; j < cfg_interconnection_controller.getLength(); j++) {
            bool is_required = false;
            for (uint32_t k = 0 ; k < interconnection_controller_parameters.size(); k++) {
                if (strcmp(cfg_interconnection_controller[j].getName(), interconnection_controller_parameters[k]) == 0) {
                    is_required = true;
                    break;
                }
            }
            ERROR_ASSERT_PRINTF(is_required, "INTERCONNECTION_CONTROLLER has PARAMETER not required: \"%s\"\n", cfg_interconnection_controller[j].getName());
        }

    }
    catch(libconfig::SettingNotFoundException &nfex) {
        ERROR_PRINTF(" INTERCONNECTION_CONTROLLER has required PARAMETER missing: \"%s\"\n", interconnection_controller_parameters.back());
    }
    catch(libconfig::SettingTypeException &tex) {
        ERROR_PRINTF(" INTERCONNECTION_CONTROLLER has PARAMETER wrong type: \"%s\"\n", interconnection_controller_parameters.back());
    }

};


//==============================================================================
void sinuca_engine_t::make_connections() {
    libconfig::Config cfg;
    cfg.readFile(this->arg_configuration_file_name);
    libconfig::Setting &cfg_root = cfg.getRoot();

    /// ========================================================================
    /// PROCESSOR => CACHE_MEMORY
    /// ========================================================================
    libconfig::Setting &cfg_processor_list = cfg_root["PROCESSOR"];
    for (int32_t i = 0; i < cfg_processor_list.getLength(); i++) {
        bool found_component;
        libconfig::Setting &cfg_processor = cfg_processor_list[i];

        /// ====================================================================
        /// Connected DATA CACHE_MEMORY
        /// ====================================================================
        found_component = false;
        const char *data_cache_memory_label = cfg_processor[ "CONNECTED_DATA_CACHE" ];
        for (uint32_t component = 0; component < this->get_cache_memory_array_size(); component++) {
            if (strcmp(this->cache_memory_array[component]->get_label(), data_cache_memory_label) == 0) {
                /// Connect the Data Cache to the Core
                this->processor_array[i]->set_data_cache(this->cache_memory_array[component]);
                interconnection_interface_t *obj_A = this->processor_array[i];
                uint32_t port_A = PROCESSOR_PORT_DATA_CACHE;

                interconnection_interface_t *obj_B = this->cache_memory_array[component];
                uint32_t port_B = this->cache_memory_array[component]->get_used_ports();

                CONFIGURATOR_DEBUG_PRINTF("Linking %s(%u) to %s(%u)\n\n", obj_A->get_label(), port_A, obj_B->get_label(), port_B);
                this->processor_array[i]->set_higher_lower_level_component( obj_A, port_A, obj_B, port_B);
                this->processor_array[i]->add_used_ports();
                this->cache_memory_array[component]->add_used_ports();
                found_component = true;
                break;
            }
        }
        ERROR_ASSERT_PRINTF(found_component == true, "PROCESSOR has a not found CONNECTED_DATA_CACHE:\"%s\"\n", data_cache_memory_label);

        /// ====================================================================
        /// Connected INST CACHE_MEMORY
        /// ====================================================================
        found_component = false;
        const char *inst_cache_memory_label = cfg_processor[ "CONNECTED_INST_CACHE" ];
        for (uint32_t component = 0; component < this->get_cache_memory_array_size(); component++) {
            if (strcmp(this->cache_memory_array[component]->get_label(), inst_cache_memory_label) == 0) {
                /// Connect the Inst Cache to the Core
                this->processor_array[i]->set_inst_cache(this->cache_memory_array[component]);
                interconnection_interface_t *obj_A = this->processor_array[i];
                uint32_t port_A = PROCESSOR_PORT_INST_CACHE;

                interconnection_interface_t *obj_B = this->cache_memory_array[component];
                uint32_t port_B = this->cache_memory_array[component]->get_used_ports();

                CONFIGURATOR_DEBUG_PRINTF("Linking %s(%u) to %s(%u)\n\n", obj_A->get_label(), port_A, obj_B->get_label(), port_B);
                this->processor_array[i]->set_higher_lower_level_component( obj_A, port_A, obj_B, port_B);
                this->processor_array[i]->add_used_ports();
                this->cache_memory_array[component]->add_used_ports();
                found_component = true;
                break;
            }
        }
        ERROR_ASSERT_PRINTF(found_component == true, "PROCESSOR has a not found CONNECTED_INST_CACHE:\"%s\"\n", inst_cache_memory_label);
    }

    /// ========================================================================
    /// CACHE_MEMORY => LOWER_LEVEL_CACHE
    /// ========================================================================
    libconfig::Setting &cfg_cache_memory_list = cfg_root["CACHE_MEMORY"];
    for (int32_t i = 0; i < cfg_cache_memory_list.getLength(); i++) {
        /// ====================================================================
        /// LOWER_LEVEL_CACHE items
        /// ====================================================================
        libconfig::Setting &cfg_lower_level_cache = cfg_cache_memory_list[i][ "LOWER_LEVEL_CACHE" ];
        for (int32_t j = 0; j < cfg_lower_level_cache.getLength(); j++) {
            bool found_component = false;
            const char *cache_label = cfg_lower_level_cache[j];
            for (uint32_t component = 0; component < this->get_cache_memory_array_size(); component++) {
                if (strcmp(this->cache_memory_array[component]->get_label(), cache_label) == 0) {
                    /// Connect the Lower Level
                    this->cache_memory_array[i]->add_lower_level_cache(this->cache_memory_array[component]);
                    /// Connect the Higher Level
                    this->cache_memory_array[component]->add_higher_level_cache(this->cache_memory_array[i]);
                    found_component = true;
                    break;
                }
            }
            ERROR_ASSERT_PRINTF(found_component == true, "CACHE_MEMORY has a not found LOWER_LEVEL_CACHE:\"%s\"\n", cache_label);
        }
    }

    /// ========================================================================
    /// INTERCONNECTION_ROUTER => CONNECTED_COMPONENTS
    /// ========================================================================
    libconfig::Setting &cfg_interconnection_router_list = cfg_root["INTERCONNECTION_ROUTER"];
    for (int32_t i = 0; i < cfg_interconnection_router_list.getLength(); i++) {
        /// ====================================================================
        /// Connected INTERCONNECTION_INTERFACE
        /// ====================================================================
        libconfig::Setting &cfg_connected_component = cfg_interconnection_router_list[i][ "CONNECTED_COMPONENT" ];
        CONFIGURATOR_DEBUG_PRINTF("Component %s connections\n", this->interconnection_router_array[i]->get_label());
        for (int32_t j = 0; j < cfg_connected_component.getLength(); j++) {
            bool found_component = false;
            const char *connected_component_label = cfg_connected_component[j];
            for (uint32_t component = 0; component < this->get_interconnection_interface_array_size(); component++) {
                if (strcmp(this->interconnection_interface_array[component]->get_label(), connected_component_label) == 0) {
                    interconnection_interface_t *obj_A = this->interconnection_router_array[i];
                    interconnection_interface_t *obj_B = this->interconnection_interface_array[component];
                    uint32_t port_A = 0;
                    uint32_t port_B = 0;

                    int32_t port_A_exist = obj_A->find_port_to_obj(obj_B);
                    /// If is a connect between routers
                    if (port_A_exist != POSITION_FAIL) {
                        port_A = port_A_exist;
                    }
                    else {
                        port_A = obj_A->get_used_ports();
                        obj_A->add_used_ports();
                    }

                    int32_t port_B_exist = obj_B->find_port_to_obj(obj_A);
                    /// Check if already connected
                    if (port_B_exist != POSITION_FAIL) {
                        port_B = port_B_exist;
                    }
                    else {
                        port_B = obj_B->get_used_ports();
                        obj_B->add_used_ports();
                    }

                    CONFIGURATOR_DEBUG_PRINTF("Linking %s(%u) to %s(%u)\n", obj_A->get_label(), port_A, obj_B->get_label(), port_B);
                    obj_A->set_higher_lower_level_component(obj_A, port_A, obj_B, port_B);
                    found_component = true;
                    break;
                }
            }
            ERROR_ASSERT_PRINTF(found_component == true, "ROUTER has a not found CONNECTED_COMPONENT:\"%s\"\n", connected_component_label);
        }
        CONFIGURATOR_DEBUG_PRINTF("========================\n");
    }
};
