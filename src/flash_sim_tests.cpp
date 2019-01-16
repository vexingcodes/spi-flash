#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "flash_sim.hpp"

namespace bedrock::test
{

TEST_CASE("flash", "[flash]")
{
    flash_sim f(4096);

    f.toggle_chip_enable();
    f.clock_in_data<8>(0x06);
    f.toggle_chip_enable();

    f.toggle_chip_enable();
    f.clock_in_data<8>(0x60);
    f.toggle_chip_enable();
}

} // End namespace bedrock::test.
