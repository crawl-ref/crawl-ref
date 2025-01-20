#include "catch_amalgamated.hpp"
#include "AppHdr.h"
#ifdef USE_TILE
#include "tilecell.h"


TEST_CASE( "add_overlay deduplication", "[single-file]")
{
    packed_cell cell;
    cell.add_overlay(5);
    REQUIRE(cell.dngn_overlay[0] == 5);
    REQUIRE(cell.dngn_overlay[1] == 0);
    REQUIRE(cell.num_dngn_overlay == 1);

    // Adding the same overlay twice in a row gets deduplicated
    cell.add_overlay(5);
    REQUIRE(cell.dngn_overlay[0] == 5);
    REQUIRE(cell.dngn_overlay[1] == 0);
    REQUIRE(cell.num_dngn_overlay == 1);

    // Adding the same overlay separately gets deduplicated
    cell.add_overlay(6);
    cell.add_overlay(7);
    cell.add_overlay(6);
    REQUIRE(cell.dngn_overlay[0] == 5);
    REQUIRE(cell.dngn_overlay[1] == 6);
    REQUIRE(cell.dngn_overlay[2] == 7);
    REQUIRE(cell.num_dngn_overlay == 3);

    cell.clear();
    REQUIRE(cell.dngn_overlay[0] == 0);
    REQUIRE(cell.dngn_overlay[1] == 0);
    REQUIRE(cell.dngn_overlay[2] == 0);
    REQUIRE(cell.num_dngn_overlay == 0);
}
#endif
