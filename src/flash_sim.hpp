#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "flash.hpp"

namespace bedrock
{

/// Simulates the functionality of an SPI flash memory chip, specifically the IS25LP128 found on the SiFive HiFive-1
/// development board.
class flash_sim : public flash
{
public:
    /// Simple state machine that the chip follows for every command.
    enum class chip_state
    {
        /// The chip-enable pin is pin_state::high and no command is in progress.
        deselected,

        /// The chip-enable pin is pin_state::low and the opcode is currently being read.
        command,

        /// The chip-enable pin is pin_state::low, and the opcode has already been read so an operation is in progress.
        operation
    };

    /// Makes a new flash chip simulation.
    ///
    /// Every byte starts with the value 0x00. The chip-enable pin starts as high, meaning the chip is deselected. The
    /// serial-input pin starts as low.
    ///
    /// \param num_bytes The number of bytes the flash chip contains.
    flash_sim(std::size_t num_bytes = 0xffffffUL);

    virtual ~flash_sim() = default;

    /// Gets the current state of the chip as a whole.
    chip_state get_chip_state() const noexcept;

    /// Reads the current state of the chip-enable pin.
    virtual pin_state get_chip_enable() const noexcept override;

    /// Reads the current state of the serial-input pin.
    virtual pin_state get_serial_input() const noexcept override;

    /// Reads the current state of the serial-output pin.
    virtual pin_state get_serial_output() const noexcept override;

    /// Accesses the raw data of this flash chip.
    const std::vector<std::byte>& get_data() const noexcept;

    /// Accesses the raw data of this flash chip.
    const std::vector<user_operation>& get_user_operations() const noexcept;

    /// Toggles the chip-enable pin, i.e. if it is pin_state::high it will transition to pin_state::low and vice versa.
    virtual void toggle_chip_enable() override;

    /// Toggles the serial-input pin, i.e. if it is pin_state::high it will transition to pin_state::low and vice versa.
    virtual void toggle_serial_input() override;

    /// Toggles the clock pin by setting it to pin_state::high then back to pin_state::low.
    ///
    /// It's not useful to make users call toggle_clock twice to initiate a clock cycle, so this method sets it to high
    /// and then to low instead of just having one transition.
    virtual void toggle_clock() override;

    /// Waits for an in-progress write to complete. Does nothing for this implementation, since simulated writes always
    /// complete immediately.
    virtual void wait_for_write_complete() override;

private:
    /// Abstract base class for any operation that the chip can perform.
    class operation
    {
    public:
        /// Starts execution of the operation.
        ///
        /// \param f The underlying flash object upon which this operation is running.
        operation(flash_sim& f);

        /// Called when toggle_chip_enable is called on the flash object and this operation is currently running.
        virtual void toggle_chip_enable() = 0;

        /// Called when toggle_clock is called on the flash object and this operation is currently running.
        virtual void toggle_clock() = 0;

    protected:
        /// The flash object upon which this operation is running.
        flash_sim& _flash;
    };

    /// Any operation that requires a data address. The address is always read in immediately following the operation's
    /// opcode.
    class operation_with_address : public operation
    {
    public:
        /// Starts execution of the operation.
        ///
        /// \param f The underlying flash object upon which this operation is running.
        operation_with_address(flash_sim& f);

        /// Called when toggle_chip_enable is called on the flash object and this operation is currently running.
        ///
        /// \throws std::runtime_error if the address has not been fully clocked in yet.
        virtual void toggle_chip_enable() override final;

        /// Called when toggle_chip_enable is called on the flash object and this operation is currently running.
        ///
        /// \throws std::runtime_error if the address has not been fully clocked in yet.
        virtual void toggle_clock() override final;

        /// Derived classes must override this. Called when toggle_chip_enable is called on the flash object and this
        /// operation is currently running and the address has been fully clocked in.
        virtual void toggle_chip_enable_impl() = 0;

        /// Derived classes must override this. Called when toggle_clock is called on the flash object and this
        /// operation is currently running and the address has been fully clocked in.
        virtual void toggle_clock_impl() = 0;

        /// Gets the completed address that has been clocked in.
        ///
        /// \throws std::runtime_error if the address has not been fully clocked in yet.
        std::uint32_t address() const;

    private:
        /// The address being read.
        std::uint32_t _address;

        /// Which bit of _address is currently being read.
        std::uint8_t _bit_index;
    };

    /// An operation that starts reading data at a given address.
    class read_operation : public operation_with_address
    {
    public:
        /// Starts execution of the operation.
        ///
        /// \param f The underlying flash object upon which this operation is running.
        read_operation(flash_sim& f);

        /// Ends the read operation.
        virtual void toggle_chip_enable_impl() override;

        /// Clocks in the next bit of read data, and outputs that bit on the serial-output pin.
        virtual void toggle_clock_impl() override;
    };

    /// An operation that starts writing data at a given address.
    class write_operation : public operation_with_address
    {
    public:
        /// Starts execution of the operation.
        ///
        /// \param f The underlying flash object upon which this operation is running.
        write_operation(flash_sim& f);

        /// Ends the write operation and actually commits the write buffer to the flash data.
        virtual void toggle_chip_enable_impl() override;

        /// Clocks the next bit from the serial-input pin into the write buffer.
        virtual void toggle_clock_impl() override;

    private:
        /// A buffer into which the data to be written is read.
        std::vector<std::byte> _write_buffer;

        /// Which byte of the write buffer is currently being read.
        std::vector<std::byte>::iterator _current_byte;

        /// Which bit of the current byte in the write buffer is currently being read.
        std::uint8_t _bit_index;
    };

    /// An operation that sets the write enable bit so that the next operation can perform a write.
    class write_enable_operation : public operation
    {
    public:
        /// Starts execution of the operation.
        ///
        /// \param f The underlying flash object upon which this operation is running.
        write_enable_operation(flash_sim& f);

        /// Completes the write enable operation by actually setting the write enable bit.
        virtual void toggle_chip_enable() override;

        /// Toggling the clock is not a valid thing to do for write enable operation, so this method always throws.
        virtual void toggle_clock() override;
    };

    /// An operation that sets all data on the chip to 0xff.
    class chip_erase_operation : public operation
    {
    public:
        /// Starts execution of the operation.
        ///
        /// \param f The underlying flash object upon which this operation is running.
        chip_erase_operation(flash_sim& f);

        /// Completes the chip erase operation by actually setting all of the data bytes to 0xff.
        virtual void toggle_chip_enable() override;

        /// Toggling the clock is not a valid thing to do for a chip erase operation, so this method always throws.
        virtual void toggle_clock() override;
    };

    /// Mapping from opcode value to a function that can create an operation object. When the instruction reigster is
    /// completed we look up the operation given by the instruction register in this map and then start executing that
    /// operation.
    static std::map<std::byte, std::function<std::unique_ptr<operation>(flash_sim&)>> _operation_map;

    /// The state of the chip enable (CE) pin on the flash chip. This pin has inverted logic, so the chip is deselected
    /// when the pin is set to high, and it is selected when the pin is set to low.
    pin_state _chip_enable;

    /// The state of the serial input (SI) pin on the flash chip. Used when reading in commands, addresses, and data.
    pin_state _serial_input;

    /// The state of the serial output (SO) pin on the flash chip. Used when reading out data from the chip.
    pin_state _serial_output;

    /// The current state of the chip in the chip state machine.
    chip_state _chip_state;

    /// Whether or not the write enabled register bit is set.
    bool _write_enabled;

    /// Which bit of the instruction register is currently being read.
    std::uint8_t _bit_index;

    /// The contents of the instruction register, used during the command phase.
    std::byte _instruction_register;

    /// The currently ongoing operation object. This is nullptr when there is no active operation.
    std::unique_ptr<operation> _operation;

    /// The actual data stored by the flash chip.
    std::vector<std::byte> _data;

    /// The series of operations that a user would need to perform to get the data of the chip into the current state.
    std::vector<user_operation> _user_operations;
};

} // End namespace bedrock.
