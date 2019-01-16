#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace bedrock
{

/// Base class for SPI flash chip related operations. Could be a real flash chip, could be a simulated flash chip, who
/// knows!?
class flash
{
public:
    /// The operations a user can perform on the chip.
    enum class user_operation
    {
        /// Toggling the chip-enable pin is used to begin or end an SPI command.
        toggle_chip_enable,

        /// Toggling the serial-input pin is used to set whether a write command reads in a 1 or a 0.
        toggle_serial_input,

        /// Toggling the clock pin will read in the next bit of the command, address, or data. For ongoing read commands
        /// toggling the clock will read in the next bit of data and output it over the serial-output pin.
        toggle_clock,

        /// On the real chip the page write operation and the chip erase operation take some time to complete. This user
        /// operation means that the user should wait for the write to complete before issuing the next write. For human
        /// users of the real chip this probably just means waiting a fixed amount of time (given in the datasheet for
        /// the flash chip). For automation this probably means reading the status register of the chip and making sure
        /// the WIP (write in progress) bit is zero before continuing.
        wait_for_write_complete
    };

    /// Represents the state of a pin on the chip.
    ///
    /// Using high and low rather than true and false makes things a bit more readable.
    enum class pin_state
    {
        /// A high voltage is being output to the pin.
        high,

        /// A low voltage is being output to the pin.
        low
    };

    virtual ~flash() = default;

    /// Reads the current state of the chip-enable pin.
    virtual pin_state get_chip_enable() const noexcept = 0;

    /// Reads the current state of the serial-input pin.
    virtual pin_state get_serial_input() const noexcept = 0;

    /// Reads the current state of the serial-output pin.
    virtual pin_state get_serial_output() const noexcept = 0;

    /// Toggles the chip-enable pin, i.e. if it is pin_state::high it will transition to pin_state::low and vice versa.
    virtual void toggle_chip_enable() = 0;

    /// Toggles the serial-input pin, i.e. if it is pin_state::high it will transition to pin_state::low and vice versa.
    virtual void toggle_serial_input() = 0;

    /// Toggles the clock pin by setting it to pin_state::high then back to pin_state::low.
    ///
    /// It's not useful to make users call toggle_clock twice to initiate a clock cycle, so this method sets it to high
    /// and then to low instead of just having one transition.
    virtual void toggle_clock() = 0;

    /// Waits for an in-progress write to complete.
    virtual void wait_for_write_complete() = 0;

    /// Performs one of the possible operations that can be performed on the chip.
    void perform_user_operation(user_operation op);

    /// Gets a character representing a user operation, so a series of user operations can be serialized.
    char user_operation_to_char(user_operation op);
    
    /// Deserializes a user_operation from a character.
    user_operation char_to_user_operation(char c);

    /// Uses the serial-input and clock pins to input a number of bits to the flash chip.
    template <std::size_t num_bits, typename data_type>
    void clock_in_data(data_type data);

    /// Uses the serial-output and clock pins to get a number of bits from the flash chip.
    template <std::size_t num_bits, typename data_type>
    data_type clock_out_data();
};

} // End namespace bedrock.

#include "flash.ipp"
