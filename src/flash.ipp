#pragma once

#include "flash.hpp"

namespace bedrock
{

template <std::size_t num_bits, typename data_type>
void flash::clock_in_data(data_type data)
{
    static_assert(sizeof(data_type) * 8 >= num_bits);
    for (int bit = num_bits; bit != 0; --bit)
    {
        bool             bit_set       = data & (1 << (bit - 1));
        flash::pin_state current_state = get_serial_input();
        if ((bit_set && current_state == flash::pin_state::low) || !bit_set && current_state == flash::pin_state::high)
            toggle_serial_input();
        toggle_clock();
    }
}

template <std::size_t num_bits, typename data_type>
data_type flash::clock_out_data()
{
    static_assert(sizeof(data_type) * 8 >= num_bits);
    data_type value{0};
    for (int bit = num_bits; bit != 0; --bit)
    {
        if (get_serial_output() == pin_state::high)
            value |= 1 << (bit - 1);
        toggle_clock();
    }
    return value;
}

} // End namespace bedrock.
