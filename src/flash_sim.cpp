#include "flash_sim.hpp"

namespace bedrock
{

/**********************************************************************************************************************\
* flash                                                                                                                *
\**********************************************************************************************************************/

std::map<std::byte, std::function<std::unique_ptr<flash_sim::operation>(flash_sim&)>> flash_sim::_operation_map = {
    {std::byte{0x02}, [](flash_sim& f) { return std::make_unique<write_operation>(f); }},
    {std::byte{0x03}, [](flash_sim& f) { return std::make_unique<read_operation>(f); }},
    {std::byte{0x06}, [](flash_sim& f) { return std::make_unique<write_enable_operation>(f); }},
    {std::byte{0x60}, [](flash_sim& f) { return std::make_unique<chip_erase_operation>(f); }},
};

flash_sim::flash_sim(std::size_t num_bytes)
        : _chip_enable(pin_state::high)
        , _serial_input(pin_state::low)
        , _serial_output(pin_state::low)
        , _chip_state(chip_state::deselected)
        , _write_enabled(false)
        , _bit_index(0)
        , _instruction_register{0}
        , _operation()
        , _data(num_bytes)
{
}

flash_sim::chip_state flash_sim::get_chip_state() const noexcept
{
    return _chip_state;
}

flash_sim::pin_state flash_sim::get_chip_enable() const noexcept
{
    return _chip_enable;
}

flash_sim::pin_state flash_sim::get_serial_input() const noexcept
{
    return _serial_input;
}

flash_sim::pin_state flash_sim::get_serial_output() const noexcept
{
    return _serial_output;
}

const std::vector<std::byte>& flash_sim::get_data() const noexcept
{
    return _data;
}

const std::vector<flash::user_operation>& flash_sim::get_user_operations() const noexcept
{
    return _user_operations;
}

void flash_sim::toggle_chip_enable()
{
    switch (_chip_state)
    {
    case chip_state::deselected:
        _user_operations.push_back(user_operation::toggle_chip_enable);
        _chip_state = chip_state::command;
        _bit_index  = 8;
        break;

    case chip_state::command:
        throw std::runtime_error("Cannot toggle chip enable while chip is in command state.");

    case chip_state::operation:
        if (!_operation)
            throw std::logic_error("In operation state without an operation.");
        _user_operations.push_back(user_operation::toggle_chip_enable);
        _operation->toggle_chip_enable();
        _operation.reset();
        _instruction_register = std::byte{0};
        _chip_state           = chip_state::deselected;
        break;
    }
}

void flash_sim::toggle_serial_input()
{
    switch (_chip_state)
    {
    case chip_state::deselected:
        throw std::runtime_error("Cannot toggle serial input while chip is deselected.");

    case chip_state::command:
        [[fallthrough]];

    case chip_state::operation:
        if (_user_operations.back() == user_operation::toggle_serial_input)
            throw std::runtime_error("Cannot toggle serial input twice in a row.");
        _user_operations.push_back(user_operation::toggle_serial_input);
        _serial_input = _serial_input == pin_state::high ? pin_state::low : pin_state::high;
        break;
    }
}

void flash_sim::toggle_clock()
{
    switch (_chip_state)
    {
    case chip_state::deselected:
        throw std::runtime_error("Cannot toggle serial input while chip is deselected.");

    case chip_state::command:
        _user_operations.push_back(user_operation::toggle_clock);
        _instruction_register |= (_serial_input == pin_state::high ? std::byte{1} : std::byte{0}) << --_bit_index;
        if (_bit_index == 0)
        {
            // Throws std::out_of_range for an unknown opcode.
            _operation  = _operation_map.at(_instruction_register)(*this);
            _chip_state = chip_state::operation;
        }
        break;

    case chip_state::operation:
        if (!_operation)
            throw std::logic_error("In operation state without an operation.");
        _user_operations.push_back(user_operation::toggle_clock);
        _operation->toggle_clock();
        break;
    }
}

void flash_sim::wait_for_write_complete()
{
}

/**********************************************************************************************************************\
* flash_sim::operation                                                                                                 *
\**********************************************************************************************************************/

flash_sim::operation::operation(flash_sim& f)
        : _flash(f)
{
}

/**********************************************************************************************************************\
* flash_sim::operation_with_address                                                                                    *
\**********************************************************************************************************************/

flash_sim::operation_with_address::operation_with_address(flash_sim& f)
        : operation(f)
        , _bit_index(12)
{
}

void flash_sim::operation_with_address::toggle_chip_enable()
{
    if (_bit_index != 0)
        throw std::runtime_error("Cannot toggle chip enable while command is reading address.");
    toggle_chip_enable_impl();
}

void flash_sim::operation_with_address::toggle_clock()
{
    if (_bit_index != 0)
        _address |= (_flash._serial_input == pin_state::high ? std::uint32_t{1} : std::uint32_t{0}) << --_bit_index;
    else
        toggle_clock_impl();
}

std::uint32_t flash_sim::operation_with_address::address() const
{
    if (_bit_index != 0)
        throw std::runtime_error("Requested address, but address is not ready yet.");
    return _address;
}

/**********************************************************************************************************************\
* flash_sim::read_operation                                                                                            *
\**********************************************************************************************************************/

flash_sim::read_operation::read_operation(flash_sim& f)
        : operation_with_address(f)
{
}

void flash_sim::read_operation::toggle_chip_enable_impl() {}

void flash_sim::read_operation::toggle_clock_impl() {}

/**********************************************************************************************************************\
* flash_sim::write_operation                                                                                           *
\**********************************************************************************************************************/

flash_sim::write_operation::write_operation(flash_sim& f)
        : operation_with_address(f)
        , _write_buffer(256)
        , _current_byte(std::begin(_write_buffer))
        , _bit_index(8)
{
    if (!_flash._write_enabled)
        throw std::runtime_error("Cannot chip erase without write enabled.");
}

void flash_sim::write_operation::toggle_chip_enable_impl()
{
    std::copy(std::begin(_write_buffer), std::end(_write_buffer), std::begin(_flash._data) + _write_buffer.size());
    _flash._write_enabled = false;
}

void flash_sim::write_operation::toggle_clock_impl()
{
    if (_current_byte == _write_buffer.end())
        throw std::runtime_error("Write buffer is full.");

    // This will also throw std::out_of_range if our current address is beyond the capacity of the flash device.
    auto byte_index = std::distance(std::begin(_write_buffer), _current_byte);
    if (_bit_index == 8 && _flash._data.at(address() + byte_index) != std::byte{0xff})
        throw std::runtime_error("Writing a non-erased byte.");

    *_current_byte |= (_flash._serial_input == pin_state::high ? std::byte{1} : std::byte{0}) << --_bit_index;
    if (_bit_index == 0)
    {
        ++_current_byte;
        _bit_index = 8;
    }
}

/**********************************************************************************************************************\
* flash_sim::write_enable_operation                                                                                    *
\**********************************************************************************************************************/

flash_sim::write_enable_operation::write_enable_operation(flash_sim& f)
        : operation(f)
{
}

void flash_sim::write_enable_operation::toggle_chip_enable()
{
    if (_flash._write_enabled)
        throw std::runtime_error("Cannot write enable when already write enabled.");
    _flash._write_enabled = true;
}

void flash_sim::write_enable_operation::toggle_clock()
{
    throw std::runtime_error("Write enable does not require clock toggling.");
}

/**********************************************************************************************************************\
* flash_sim::chip_erase_operation                                                                                      *
\**********************************************************************************************************************/

flash_sim::chip_erase_operation::chip_erase_operation(flash_sim& f)
        : operation(f)
{
    if (!_flash._write_enabled)
        throw std::runtime_error("Cannot chip erase without write enabled.");
}

void flash_sim::chip_erase_operation::toggle_chip_enable()
{
    for (std::byte& b : _flash._data)
        b = std::byte{0xff};
    _flash._write_enabled = false;
}

void flash_sim::chip_erase_operation::toggle_clock()
{
    throw std::runtime_error("Chip erase does not require clock toggling.");
}

} // End namespace bedrock.
