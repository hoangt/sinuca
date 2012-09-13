/// ============================================================================
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
/// ============================================================================
class line_usage_predictor_t : public interconnection_interface_t {
    protected:
        /// ====================================================================
        /// Set by sinuca_configurator
        /// ====================================================================
        line_usage_predictor_policy_t line_usage_predictor_type;

        /// ====================================================================
        /// Set by this->allocate()
        /// ====================================================================


        /// ====================================================================
        /// Statistics related
        /// ====================================================================

    public:
        /// ====================================================================
        /// Methods
        /// ====================================================================
        line_usage_predictor_t();
        ~line_usage_predictor_t();
        inline const char* get_type_component_label() {
            return "LINE_USAGE_PREDICTOR";
        };

        /// ====================================================================
        /// Inheritance from interconnection_interface_t
        /// ====================================================================
        /// Basic Methods
        void allocate();
        void clock(uint32_t sub_cycle);
        int32_t send_package(memory_package_t *package);
        bool receive_package(memory_package_t *package, uint32_t input_port, uint32_t transmission_latency);
        /// Token Controller Methods
        void allocate_token_list();
        bool check_token_list(memory_package_t *package);
        uint32_t check_token_space(memory_package_t *package);
        void remove_token_list(memory_package_t *package);
        /// Debug Methods
        void periodic_check();
        void print_structures();
        void panic();
        /// Statistics Methods
        void reset_statistics();
        void print_statistics();
        void print_configuration();
        /// ====================================================================

        /// Inspections
        virtual void fill_package_sub_blocks(memory_package_t *package)=0;
        virtual bool check_sub_block_is_hit(memory_package_t *package, uint64_t index, uint32_t way)=0;
        virtual bool check_line_is_dead(uint32_t index, uint32_t way)=0;
        /// Cache Operations
        virtual void line_hit(memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void line_miss(memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void sub_block_miss(memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void line_get_copyback(memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void line_insert_copyback(memory_package_t *package, cache_memory_t *cache_memory, cache_line_t *cache_line, uint32_t index, uint32_t way)=0;
        virtual void line_eviction(uint32_t index, uint32_t way)=0;
        virtual void line_invalidation(uint32_t index, uint32_t way)=0;


        INSTANTIATE_GET_SET(line_usage_predictor_policy_t, line_usage_predictor_type);

        /// ====================================================================
        /// Statistics related
        /// ====================================================================
};
