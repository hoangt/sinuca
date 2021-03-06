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

class line_usage_predictor_t : public interconnection_interface_t {
    protected:
        // ====================================================================
        /// Set by sinuca_configurator
        // ====================================================================
        line_usage_predictor_policy_t line_usage_predictor_type;

        // ====================================================================
        /// Set by this->allocate()
        // ====================================================================


        // ====================================================================
        /// Statistics related
        // ====================================================================

    public:
        // ====================================================================
        /// Methods
        // ====================================================================
        line_usage_predictor_t();
        ~line_usage_predictor_t();
        inline const char* get_type_component_label() {
            return "LINE_USAGE_PREDICTOR";
        };

        // ====================================================================
        /// Inheritance from interconnection_interface_t
        // ====================================================================
        /// Basic Methods
        void allocate();
        void clock(uint32_t sub_cycle);
        int32_t send_package(memory_package_t *package);
        bool receive_package(memory_package_t *package, uint32_t input_port, uint32_t transmission_latency);
        /// Token Controller Methods
        bool pop_token_credit(uint32_t src_id, memory_operation_t memory_operation);
        /// Debug Methods
        void periodic_check();
        void print_structures();
        void panic();
        /// Statistics Methods
        void reset_statistics();
        void print_statistics();
        void print_configuration();
        // ====================================================================

        /// Inspections
        virtual bool check_line_is_disabled(cache_memory_t *cache, cache_line_t *cache_line, uint32_t index, uint32_t way)=0;
        virtual bool check_line_is_last_access(cache_memory_t *cache, cache_line_t *cache_line, uint32_t index, uint32_t way)=0;
        virtual bool check_line_is_last_write(cache_memory_t *cache, cache_line_t *cache_line, uint32_t index, uint32_t way)=0;
        /// Cache Operations
        virtual void line_hit(cache_memory_t *cache, cache_line_t *cache_line, memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void line_miss(cache_memory_t *cache, cache_line_t *cache_line, memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void sub_block_miss(cache_memory_t *cache, cache_line_t *cache_line, memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void line_send_writeback(cache_memory_t *cache, cache_line_t *cache_line, memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void line_recv_writeback(cache_memory_t *cache, cache_line_t *cache_line, memory_package_t *package, uint32_t index, uint32_t way)=0;
        virtual void line_eviction(cache_memory_t *cache, cache_line_t *cache_line, uint32_t index, uint32_t way)=0;
        virtual void line_invalidation(cache_memory_t *cache, cache_line_t *cache_line, uint32_t index, uint32_t way)=0;


        INSTANTIATE_GET_SET(line_usage_predictor_policy_t, line_usage_predictor_type);

        // ====================================================================
        /// Statistics related
        // ====================================================================
};
