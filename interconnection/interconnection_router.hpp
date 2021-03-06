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

// ============================================================================
/// Network-on-Chip Router.
// ============================================================================
 /*! Class used to interconnect interconnection_interface_t components
  */
class interconnection_router_t : public interconnection_interface_t {
    private:
        // ====================================================================
        /// Set by sinuca_configurator
        // ====================================================================
        uint32_t input_buffer_size;  /// Input buffer depth.
        selection_t selection_policy;

        // ====================================================================
        /// Set by this->allocate()
        // ====================================================================
        circular_buffer_t<memory_package_t> *input_buffer; /// Circular Input buffer [ports][input_buffer_size].
        uint32_t packages_inside_router;

        uint64_t send_ready_cycle;  /// Time left for the router's next send operation.
        uint64_t *recv_ready_cycle;  /// Time left for the router's next receive operation (per port).

        uint32_t last_selected;  /// The last port that has something been sent. Used by RoundRobin and BufferLevel selection.

        // ====================================================================
        /// Statistics related
        // ====================================================================
        uint64_t stat_transmissions;

        uint64_t stat_total_send_size;
        uint64_t stat_total_recv_size;

        uint64_t stat_total_send_flits;
        uint64_t stat_total_recv_flits;

        uint64_t *stat_transmitted_package_size;
    public:
        // ====================================================================
        /// Methods
        // ====================================================================
        interconnection_router_t();
        ~interconnection_router_t();

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

        /// Selection strategies
        uint32_t selection_random();
        uint32_t selection_round_robin();
        uint32_t selection_buffer_level();


        INSTANTIATE_GET_SET(selection_t, selection_policy)
        INSTANTIATE_GET_SET(uint32_t, send_ready_cycle)
        INSTANTIATE_GET_SET(uint32_t, input_buffer_size)

        // ====================================================================
        /// Statistics related
        // ====================================================================
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_transmissions)

        INSTANTIATE_GET_SET_ADD(uint64_t, stat_total_send_size)
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_total_recv_size)

        INSTANTIATE_GET_SET_ADD(uint64_t, stat_total_send_flits)
        INSTANTIATE_GET_SET_ADD(uint64_t, stat_total_recv_flits)
};
