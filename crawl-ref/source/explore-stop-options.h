#pragma once

enum explore_stop_options
{
    ES_NONE                      = 0x00000,

    // Explored into view of an item that is NOT eligible for autopickup.
    ES_ITEM                      = 0x00001,

    // Picked up an item during greedy explore; will stop for anything
    // that's not explicitly ignored and that is not gold.
    ES_GREEDY_PICKUP             = 0x00002,

    // Stop when picking up gold with greedy explore.
    ES_GREEDY_PICKUP_GOLD        = 0x00004,

    // Picked up an item during greedy explore, ignoring items that were
    // thrown by the PC, and items that the player already has one of in
    // inventory, or a bunch of other conditions (see
    // _interesting_explore_pickup in items.cc)
    ES_GREEDY_PICKUP_SMART       = 0x00008,

    // Greedy-picked up an item previously thrown by the PC.
    ES_GREEDY_PICKUP_THROWN      = 0x00010,
    ES_GREEDY_PICKUP_MASK        = (ES_GREEDY_PICKUP
                                    | ES_GREEDY_PICKUP_GOLD
                                    | ES_GREEDY_PICKUP_SMART
                                    | ES_GREEDY_PICKUP_THROWN),

    // Explored into view of an item eligible for autopickup.
    ES_GREEDY_ITEM               = 0x00020,

    // Stepped onto a stack of items that was previously unknown to
    // the player (for instance, when stepping onto the heap of items
    // of a freshly killed monster).
    ES_GREEDY_VISITED_ITEM_STACK = 0x00040,

    // Explored into view of a stair, shop, altar, portal, glowing
    // item, artefact, or branch entrance.... etc.
    ES_STAIR                     = 0x00080,
    ES_SHOP                      = 0x00100,
    ES_ALTAR                     = 0x00200,
    ES_PORTAL                    = 0x00400,
    ES_GLOWING_ITEM              = 0x00800,
    ES_ARTEFACT                  = 0x01000,
    ES_RUNE                      = 0x02000,
    ES_BRANCH                    = 0x04000,
    ES_RUNED_DOOR                = 0x08000,
    ES_TRANSPORTER               = 0x10000,
    ES_RUNELIGHT                 = 0x20000,
};
