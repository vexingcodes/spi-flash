#include "flash.hpp"

namespace bedrock
{

void flash::perform_user_operation(user_operation op)
{
    switch (op)
    {
    case user_operation::toggle_chip_enable: toggle_chip_enable(); break;
    case user_operation::toggle_serial_input: toggle_serial_input(); break;
    case user_operation::toggle_clock: toggle_clock(); break;
    case user_operation::wait_for_write_complete: wait_for_write_complete(); break;
    }
}

char flash::user_operation_to_char(user_operation op)
{
    switch (op)
    {
    case user_operation::toggle_chip_enable: return 'e';
    case user_operation::toggle_serial_input: return 'i';
    case user_operation::toggle_clock: return 'c';
    case user_operation::wait_for_write_complete: return 'w';
    default: throw std::invalid_argument("Cannot convert user_operation to char.");
    }
}

flash::user_operation flash::char_to_user_operation(char c)
{
    switch (c)
    {
    case 'e': return user_operation::toggle_chip_enable;
    case 'i': return user_operation::toggle_serial_input;
    case 'c': return user_operation::toggle_clock;
    case 'w': return user_operation::wait_for_write_complete;
    default: throw std::invalid_argument("Cannot convert char to user_operation.");
    }
}

} // End namespace bedrock.
