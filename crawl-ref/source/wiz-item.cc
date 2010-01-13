/*
 *  File:       wiz-item.cc
 *  Summary:    Item related wizard functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#include "AppHdr.h"

#include "wiz-item.h"

#include <errno.h>

#include "artefact.h"
#include "coordit.h"
#include "message.h"
#include "cio.h"
#include "dbg-util.h"
#include "decks.h"
#include "effects.h"
#include "env.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "it_use2.h"
#include "invent.h"
#include "makeitem.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "output.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stash.h"
#include "stuff.h"
#include "terrain.h"

#ifdef WIZARD
static void _make_all_books()
{
    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        int thing = items(0, OBJ_BOOKS, i, true, 0, MAKE_ITEM_NO_RACE,
                          0, 0, AQ_WIZMODE);
        if (thing == NON_ITEM)
            continue;

        move_item_to_grid(&thing, you.pos());

        if (thing == NON_ITEM)
            continue;

        item_def book(mitm[thing]);

        mark_had_book(book);
        set_ident_flags(book, ISFLAG_KNOW_TYPE);
        set_ident_flags(book, ISFLAG_IDENT_MASK);

        mprf("%s", book.name(DESC_PLAIN).c_str());
    }
}

//---------------------------------------------------------------
//
// create_spec_object
//
//---------------------------------------------------------------
void wizard_create_spec_object()
{
    char           specs[80];
    char           keyin;
    monster_type   mon;

    object_class_type class_wanted   = OBJ_UNASSIGNED;

    int            thing_created;

    while (class_wanted == OBJ_UNASSIGNED)
    {
        mpr(") - weapons     ( - missiles  [ - armour  / - wands    ?  - scrolls",
            MSGCH_PROMPT);
        mpr("= - jewellery   ! - potions   : - books   | - staves   0  - The Orb",
            MSGCH_PROMPT);
        mpr("} - miscellany  X - corpses   % - food    $ - gold    ESC - exit",
            MSGCH_PROMPT);

        mpr("What class of item? ", MSGCH_PROMPT);

        keyin = toupper( get_ch() );

        if (keyin == ')')
            class_wanted = OBJ_WEAPONS;
        else if (keyin == '(')
            class_wanted = OBJ_MISSILES;
        else if (keyin == '[' || keyin == ']')
            class_wanted = OBJ_ARMOUR;
        else if (keyin == '/' || keyin == '\\')
            class_wanted = OBJ_WANDS;
        else if (keyin == '?')
            class_wanted = OBJ_SCROLLS;
        else if (keyin == '=' || keyin == '"')
            class_wanted = OBJ_JEWELLERY;
        else if (keyin == '!')
            class_wanted = OBJ_POTIONS;
        else if (keyin == ':' || keyin == '+')
            class_wanted = OBJ_BOOKS;
        else if (keyin == '|')
            class_wanted = OBJ_STAVES;
        else if (keyin == '0' || keyin == 'O')
            class_wanted = OBJ_ORBS;
        else if (keyin == '}' || keyin == '{')
            class_wanted = OBJ_MISCELLANY;
        else if (keyin == 'X' || keyin == '&')
            class_wanted = OBJ_CORPSES;
        else if (keyin == '%')
            class_wanted = OBJ_FOOD;
        else if (keyin == '$')
            class_wanted = OBJ_GOLD;
        else if (keyin == ESCAPE || keyin == ' '
                || keyin == '\r' || keyin == '\n')
        {
            canned_msg( MSG_OK );
            return;
        }
    }

    // Allocate an item to play with.
    thing_created = get_item_slot();
    if (thing_created == NON_ITEM)
    {
        mpr("Could not allocate item.");
        return;
    }

    // turn item into appropriate kind:
    if (class_wanted == OBJ_ORBS)
    {
        mitm[thing_created].base_type = OBJ_ORBS;
        mitm[thing_created].sub_type  = ORB_ZOT;
        mitm[thing_created].quantity  = 1;
    }
    else if (class_wanted == OBJ_GOLD)
    {
        int amount = debug_prompt_for_int( "How much gold? ", true );
        if (amount <= 0)
        {
            canned_msg( MSG_OK );
            return;
        }

        mitm[thing_created].base_type = OBJ_GOLD;
        mitm[thing_created].sub_type  = 0;
        mitm[thing_created].quantity  = amount;
    }
    else if (class_wanted == OBJ_CORPSES)
    {
        mon = debug_prompt_for_monster();

        if (mon == MONS_NO_MONSTER || mon == MONS_PROGRAM_BUG)
        {
            mpr("No such monster.");
            return;
        }

        if (mons_weight(mon) <= 0)
        {
            if (!yesno("That monster doesn't leave corpses; make one "
                       "anyway?", true, 'y'))
            {
                return;
            }
        }

        if (mon >= MONS_DRACONIAN_CALLER && mon <= MONS_DRACONIAN_SCORCHER)
        {
            mpr("You can't make a draconian corpse by its job.");
            mon = MONS_DRACONIAN;
        }

        monsters dummy;
        dummy.type = mon;

        if (mons_genus(mon) == MONS_HYDRA)
            dummy.number = debug_prompt_for_int("How many heads? ", false);

        if (fill_out_corpse(&dummy, mitm[thing_created], true) == -1)
        {
            mpr("Failed to create corpse.");
            mitm[thing_created].clear();
            return;
        }
    }
    else
    {
        if (class_wanted == OBJ_BOOKS)
            mpr("What type of item? (\"all\" for all) ", MSGCH_PROMPT);
        else
            mpr("What type of item? ", MSGCH_PROMPT);
        get_input_line( specs, sizeof( specs ) );

        std::string temp = specs;
        trim_string(temp);
        lowercase(temp);
        strcpy(specs, temp.c_str());

        if (class_wanted == OBJ_BOOKS && temp == "all")
        {
            _make_all_books();
            return;
        }

        if (specs[0] == '\0')
        {
            canned_msg( MSG_OK );
            return;
        }

        if (!get_item_by_name(&mitm[thing_created], specs, class_wanted, true))
        {
            mpr("No such item.");

            // Clean up item
            destroy_item(thing_created);
            return;
        }
        if (Options.autoinscribe_artefacts && is_artefact(mitm[thing_created]))
        {
            mitm[thing_created].inscription
                = artefact_auto_inscription(mitm[thing_created]);
        }
    }

    // Deck colour (which control rarity) already set.
    if (!is_deck(mitm[thing_created]))
        item_colour( mitm[thing_created] );

    move_item_to_grid( &thing_created, you.pos() );

    if (thing_created != NON_ITEM)
    {
        // orig_monnum is used in corpses for things like the Animate
        // Dead spell, so leave it alone.
        if (class_wanted != OBJ_CORPSES)
            origin_acquired(mitm[thing_created], AQ_WIZMODE);
        canned_msg(MSG_SOMETHING_APPEARS);

        // Tell the stash tracker.
        maybe_update_stashes();
    }
}

const char* _prop_name[ARTP_NUM_PROPERTIES] = {
    "Brand",
    "AC",
    "EV",
    "Str",
    "Int",
    "Dex",
    "Fire",
    "Cold",
    "Elec",
    "Pois",
    "Neg",
    "Mag",
    "SInv",
    "Inv",
    "Lev",
    "Blnk",
    "Bers",
    "Nois",
    "NoSpl",
    "RndTl",
    "NoTel",
    "Anger",
    "Metab",
    "Mut",
    "Acc",
    "Dam",
    "Curse",
    "Stlth",
    "MP"
};

#define ARTP_VAL_BOOL 0
#define ARTP_VAL_POS  1
#define ARTP_VAL_ANY  2

char _prop_type[ARTP_NUM_PROPERTIES] = {
    ARTP_VAL_POS,  //BRAND
    ARTP_VAL_ANY,  //AC
    ARTP_VAL_ANY,  //EVASION
    ARTP_VAL_ANY,  //STRENGTH
    ARTP_VAL_ANY,  //INTELLIGENCE
    ARTP_VAL_ANY,  //DEXTERITY
    ARTP_VAL_ANY,  //FIRE
    ARTP_VAL_ANY,  //COLD
    ARTP_VAL_BOOL, //ELECTRICITY
    ARTP_VAL_BOOL, //POISON
    ARTP_VAL_BOOL, //NEGATIVE_ENERGY
    ARTP_VAL_POS,  //MAGIC
    ARTP_VAL_BOOL, //EYESIGHT
    ARTP_VAL_BOOL, //INVISIBLE
    ARTP_VAL_BOOL, //LEVITATE
    ARTP_VAL_BOOL, //BLINK
    ARTP_VAL_BOOL, //BERSERK
    ARTP_VAL_POS,  //NOISES
    ARTP_VAL_BOOL, //PREVENT_SPELLCASTING
    ARTP_VAL_BOOL, //CAUSE_TELEPORTATION
    ARTP_VAL_BOOL, //PREVENT_TELEPORTATION
    ARTP_VAL_POS,  //ANGRY
    ARTP_VAL_POS,  //METABOLISM
    ARTP_VAL_POS,  //MUTAGENIC
    ARTP_VAL_ANY,  //ACCURACY
    ARTP_VAL_ANY,  //DAMAGE
    ARTP_VAL_POS,  //CURSED
    ARTP_VAL_ANY,  //STEALTH
    ARTP_VAL_ANY   //MAGICAL_POWER
};

static void _tweak_randart(item_def &item)
{
    if (item_is_equipped(item))
    {
        mpr("You can't tweak the randart properties of an equipped item.",
            MSGCH_PROMPT);
        return;
    }
    else
        mesclr();

    artefact_properties_t props;
    artefact_wpn_properties(item, props);

    std::string prompt = "";

    std::vector<unsigned int> choice_to_prop;
    for (unsigned int i = 0, choice_num = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        if (_prop_name[i] == std::string("UNUSED"))
            continue;
        choice_to_prop.push_back(i);
        if (choice_num % 8 == 0 && choice_num != 0)
            prompt += "\n";

        char choice;
        char buf[80];

        if (choice_num < 26)
            choice = 'A' + choice_num;
        else
            choice = '1' + choice_num - 26;

        if (props[i])
            sprintf(buf, "%c) <w>%-5s</w> ", choice, _prop_name[i]);
        else
            sprintf(buf, "%c) %-5s ", choice, _prop_name[i]);

        prompt += buf;

        choice_num++;
    }
    formatted_message_history(prompt, MSGCH_PROMPT, 0, 80);

    mpr("Change which field? ", MSGCH_PROMPT);

    char     keyin = tolower( get_ch() );
    unsigned int  choice;

    if (isalpha(keyin))
        choice = keyin - 'a';
    else if (isdigit(keyin) && keyin != '0')
        choice = keyin - '1' + 26;
    else
        return;

    if (choice >= choice_to_prop.size())
        return;

    unsigned int prop = choice_to_prop[choice];
    ASSERT(prop >= 0);
    ASSERT(prop < ARRAYSZ(_prop_type));

    int val;
    switch (_prop_type[prop])
    {
    case ARTP_VAL_BOOL:
        mprf(MSGCH_PROMPT, "Toggling %s to %s.", _prop_name[prop],
             props[prop] ? "off" : "on");
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             !props[prop]);
        break;

    case ARTP_VAL_POS:
        mprf(MSGCH_PROMPT, "%s was %d.", _prop_name[prop], props[prop]);
        val = debug_prompt_for_int("New value? ", true);

        if (val < 0)
        {
            mprf(MSGCH_PROMPT, "Value for %s must be non-negative",
                 _prop_name[prop]);
            return;
        }
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             val);
        break;
    case ARTP_VAL_ANY:
        mprf(MSGCH_PROMPT, "%s was %d.", _prop_name[prop], props[prop]);
        val = debug_prompt_for_int("New value? ", false);
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             val);
        break;
    }

    if (Options.autoinscribe_artefacts)
        item.inscription = artefact_auto_inscription(item);
}

void wizard_tweak_object(void)
{
    char specs[50];
    char keyin;

    int item = prompt_invent_item("Tweak which item? ", MT_INVLIST, -1);
    if (item == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (item == you.equip[EQ_WEAPON])
        you.wield_change = true;

    const bool is_art = is_artefact(you.inv[item]);

    while (true)
    {
        void *field_ptr = NULL;

        while (true)
        {
            mpr(you.inv[item].name(DESC_INVENTORY_EQUIP).c_str());

            if (is_art)
            {
                mpr("a - plus  b - plus2  c - art props  d - quantity  "
                    "e - flags  ESC - exit", MSGCH_PROMPT);
            }
            else
            {
                mpr("a - plus  b - plus2  c - special  d - quantity  "
                    "e - flags  ESC - exit", MSGCH_PROMPT);
            }

            mpr("Which field? ", MSGCH_PROMPT);

            keyin = tolower( get_ch() );

            if (keyin == 'a')
                field_ptr = &(you.inv[item].plus);
            else if (keyin == 'b')
                field_ptr = &(you.inv[item].plus2);
            else if (keyin == 'c')
                field_ptr = &(you.inv[item].special);
            else if (keyin == 'd')
                field_ptr = &(you.inv[item].quantity);
            else if (keyin == 'e')
                field_ptr = &(you.inv[item].flags);
            else if (keyin == ESCAPE || keyin == ' '
                    || keyin == '\r' || keyin == '\n')
            {
                canned_msg( MSG_OK );
                return;
            }

            if (keyin >= 'a' && keyin <= 'e')
                break;
        }

        if (is_art && keyin == 'c')
        {
            _tweak_randart(you.inv[item]);
            continue;
        }

        if (keyin != 'c' && keyin != 'e')
        {
            const short *const ptr = static_cast< short * >( field_ptr );
            mprf("Old value: %d (0x%04x)", *ptr, *ptr );
        }
        else
        {
            const long *const ptr = static_cast< long * >( field_ptr );
            mprf("Old value: %ld (0x%08lx)", *ptr, *ptr );
        }

        mpr("New value? ", MSGCH_PROMPT);
        get_input_line( specs, sizeof( specs ) );

        if (specs[0] == '\0')
            return;

        char *end;
        const bool hex = (keyin == 'e');
        int   new_value = strtol(specs, &end, hex ? 16 : 0);

        if (new_value == 0 && end == specs)
            return;

        if (keyin != 'c' && keyin != 'e')
        {
            short *ptr = static_cast< short * >( field_ptr );
            *ptr = new_value;
        }
        else
        {
            long *ptr = static_cast< long * >( field_ptr );
            *ptr = new_value;
        }
    }
}

// Returns whether an item of this type can be an artefact.
static bool _item_type_can_be_artefact( int type)
{
    return (type == OBJ_WEAPONS || type == OBJ_ARMOUR || type == OBJ_JEWELLERY
            || type == OBJ_BOOKS);
}

static bool _make_book_randart(item_def &book)
{
    char type;

    do
    {
        mpr("Make book fixed [t]heme or fixed [l]evel? ", MSGCH_PROMPT);
        type = tolower(getch());
    }
    while (type != 't' && type != 'l');

    if (type == 'l')
        return make_book_level_randart(book);
    else
        return make_book_theme_randart(book);
}

void wizard_value_artefact()
{
    int i = prompt_invent_item( "Value of which artefact?", MT_INVLIST, -1 );

    if (!prompt_failed(i))
    {
        const item_def& item(you.inv[i]);
        if (!is_artefact(item))
            mpr("That item is not an artefact!");
        else
            mprf("%s", debug_art_val_str(item).c_str());
    }
}

void wizard_create_all_artefacts()
{
    // Create all unrandarts.
    for (int i = 0; i < NO_UNRANDARTS; ++i)
    {
        const int              index = i + UNRAND_START;
        const unrandart_entry* entry = get_unrand_entry(index);

        // Skip dummy entries.
        if (entry->base_type == OBJ_UNASSIGNED)
            continue;

        int islot = get_item_slot();
        if (islot == NON_ITEM)
            break;

        item_def& item = mitm[islot];
        make_item_unrandart(item, index);
        item.quantity = 1;
        set_ident_flags(item, ISFLAG_IDENT_MASK);

        msg::streams(MSGCH_DIAGNOSTICS) << "Made " << item.name(DESC_NOCAP_A)
                                        << " (" << debug_art_val_str(item)
                                        << ")" << std::endl;
        move_item_to_grid(&islot, you.pos());
    }

    // Create Horn of Geryon
    int islot = get_item_slot();
    if (islot != NON_ITEM)
    {
        item_def& item = mitm[islot];
        item.clear();
        item.base_type = OBJ_MISCELLANY;
        item.sub_type  = MISC_HORN_OF_GERYON;
        item.quantity  = 1;
        item_colour(item);

        set_ident_flags(item, ISFLAG_IDENT_MASK);
        move_item_to_grid(&islot, you.pos());

        msg::streams(MSGCH_DIAGNOSTICS) << "Made " << item.name(DESC_NOCAP_A)
                                        << std::endl;
    }
}

void wizard_make_object_randart()
{
    int i = prompt_invent_item( "Make an artefact out of which item?",
                                MT_INVLIST, -1 );

    if (prompt_failed(i))
        return;

    item_def &item(you.inv[i]);

    if (is_unrandom_artefact(item))
    {
        mpr("That item is already an unrandom artefact.");
        return;
    }

    if (!_item_type_can_be_artefact(item.base_type))
    {
        mpr("That item cannot be turned into an artefact.");
        return;
    }

    if (you.weapon() == &item)
        you.wield_change = true;

    if (is_random_artefact(item))
    {
        if (!yesno("Is already a randart; wipe and re-use?", true, 'n'))
        {
            canned_msg(MSG_OK);
            return;
        }

        if (item_is_equipped(item))
            unuse_artefact(item);

        item.special = 0;
        item.flags  &= ~ISFLAG_RANDART;
        item.props.clear();
    }

    mpr("Fake item as gift from which god (ENTER to leave alone): ",
        MSGCH_PROMPT);
    char name[80];
    if (!cancelable_get_line(name, sizeof( name )) && name[0])
    {
        god_type god = string_to_god(name, false);
        if (god == GOD_NO_GOD)
           mpr("No such god, leaving item origin alone.");
        else
        {
           mprf("God gift of %s.", god_name(god, false).c_str());
           item.orig_monnum = -god;
        }
    }

    if (item.base_type == OBJ_BOOKS)
    {
        if (!_make_book_randart(item))
        {
            mpr("Failed to turn book into randart.");
            return;
        }
    }
    else if (!make_item_randart(item))
    {
        mpr("Failed to turn item into randart.");
        return;
    }

    if (Options.autoinscribe_artefacts)
        add_autoinscription(item, artefact_auto_inscription(you.inv[i]));

    // If equipped, apply new randart benefits.
    if (item_is_equipped(item))
        use_artefact(item);

    mpr(item.name(DESC_INVENTORY_EQUIP).c_str());
}

// Returns whether an item of this type can be cursed.
static bool _item_type_can_be_cursed(int type)
{
    return (type == OBJ_WEAPONS || type == OBJ_ARMOUR || type == OBJ_JEWELLERY);
}

void wizard_uncurse_item()
{
    const int i = prompt_invent_item("(Un)curse which item?", MT_INVLIST, -1);

    if (!prompt_failed(i))
    {
        item_def& item(you.inv[i]);

        if (item.cursed())
            do_uncurse_item(item);
        else if (_item_type_can_be_cursed(item.base_type))
            do_curse_item(item);
        else
            mpr("That type of item cannot be cursed.");
    }
}

void wizard_identify_pack()
{
    mpr("You feel a rush of knowledge.");
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (item.is_valid())
        {
            set_ident_type(item, ID_KNOWN_TYPE);
            set_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
    you.wield_change  = true;
    you.redraw_quiver = true;
}

void wizard_unidentify_pack()
{
    mpr("You feel a rush of antiknowledge.");
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (item.is_valid())
        {
            set_ident_type(item, ID_UNKNOWN_TYPE);
            unset_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
    you.wield_change  = true;
    you.redraw_quiver = true;

    // Forget things that nearby monsters are carrying, as well.
    // (For use with the "give monster an item" wizard targetting
    // command.)
    for (monster_iterator mon(&you.get_los()); mon; ++mon)
    {
        for (int j = 0; j < NUM_MONSTER_SLOTS; ++j)
        {
            if (mon->inv[j] == NON_ITEM)
                continue;

            item_def &item = mitm[mon->inv[j]];

            if (!item.is_valid())
                continue;

            set_ident_type(item, ID_UNKNOWN_TYPE);
            unset_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
}

void wizard_list_items()
{
    bool has_shops = false;

    for (int i = 0; i < MAX_SHOPS; ++i)
        if (env.shop[i].type != SHOP_UNASSIGNED)
        {
            has_shops = true;
            break;
        }

    if (has_shops)
    {
        mpr("Shop items:");

        for (int i = 0; i < MAX_SHOPS; ++i)
            if (env.shop[i].type != SHOP_UNASSIGNED)
            {
                for (stack_iterator si(coord_def(0, i+5)); si; ++si)
                    mpr(si->name(DESC_PLAIN, false, false, false).c_str());
            }

        mpr(EOL);
    }

    mpr("Item stacks (by location and top item):");
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        item_def &item(mitm[i]);
        if (!item.is_valid() || item.held_by_monster())
            continue;

        if (item.link != NON_ITEM)
        {
            mprf("(%2d,%2d): %s", item.pos.x, item.pos.y,
                 item.name(DESC_PLAIN, false, false, false).c_str() );
        }
    }

    mpr(EOL);
    mpr("Floor items (stacks only show top item):");

    const coord_def start(1,1), end(GXM-1, GYM-1);
    for (rectangle_iterator ri(start, end); ri; ++ri)
    {
        int item = igrd(*ri);
        if (item != NON_ITEM)
        {
            mprf("%3d at (%2d,%2d): %s", item, ri->x, ri->y,
                 mitm[item].name(DESC_PLAIN, false, false, false).c_str());
        }
    }
}

//---------------------------------------------------------------
//
// debug_item_statistics
//
//---------------------------------------------------------------
static void _debug_acquirement_stats(FILE *ostat)
{
    int p = get_item_slot(11);
    if (p == NON_ITEM)
    {
        mpr("Too many items on level.");
        return;
    }
    mitm[p].base_type = OBJ_UNASSIGNED;

    mesclr();
    mpr("[a] Weapons [b] Armours [c] Jewellery      [d] Books");
    mpr("[e] Staves  [f] Wands   [g] Miscellaneous  [h] Food");
    mpr("What kind of item would you like to get acquirement stats on? ",
        MSGCH_PROMPT);

    object_class_type type;
    const int keyin = tolower( get_ch() );
    switch ( keyin )
    {
    case 'a': type = OBJ_WEAPONS;    break;
    case 'b': type = OBJ_ARMOUR;     break;
    case 'c': type = OBJ_JEWELLERY;  break;
    case 'd': type = OBJ_BOOKS;      break;
    case 'e': type = OBJ_STAVES;     break;
    case 'f': type = OBJ_WANDS;      break;
    case 'g': type = OBJ_MISCELLANY; break;
    case 'h': type = OBJ_FOOD;       break;
    default:
        canned_msg( MSG_OK );
        return;
    }

    const int num_itrs = debug_prompt_for_int("How many iterations? ", true);

    if (num_itrs == 0)
    {
        canned_msg( MSG_OK );
        return;
    }

    int last_percent = 0;
    int acq_calls    = 0;
    int total_quant  = 0;
    int max_plus     = -127;
    int total_plus   = 0;
    int num_arts     = 0;

    int subtype_quants[256];
    int ego_quants[SPWPN_DEBUG_RANDART];

    memset(subtype_quants, 0, sizeof(subtype_quants));
    memset(ego_quants, 0, sizeof(ego_quants));

    for (int i = 0; i < num_itrs; ++i)
    {
        if (kbhit())
        {
            getch();
            mpr("Stopping early due to keyboard input.");
            break;
        }

        int item_index = NON_ITEM;

        if (!acquirement(type, AQ_WIZMODE, true, &item_index, true)
            || item_index == NON_ITEM
            || !mitm[item_index].is_valid())
        {
            mpr("Acquirement failed, stopping early.");
            break;
        }

        item_def &item(mitm[item_index]);

        acq_calls++;
        total_quant += item.quantity;
        subtype_quants[item.sub_type] += item.quantity;

        max_plus    = std::max(max_plus, item.plus + item.plus2);
        total_plus += item.plus + item.plus2;

        if (is_artefact(item))
        {
            num_arts++;
            if (type == OBJ_BOOKS)
            {
                if (item.sub_type == BOOK_RANDART_THEME)
                {
                    const int disc1 = item.plus & 0xFF;
                    ego_quants[disc1]++;
                }
                else if (item.sub_type == BOOK_RANDART_LEVEL)
                {
                    const int level = item.plus;
                    ego_quants[SPTYP_LAST_EXPONENT + level]++;
                }
            }
        }
        else if (type == OBJ_ARMOUR) // Exclude artefacts when counting egos.
            ego_quants[get_armour_ego_type(item)]++;
        else if (type == OBJ_BOOKS && item.sub_type == BOOK_MANUAL)
        {
            // Store skills in subtype array, so as not to overlap
            // with artefact spell disciplines/levels.
            const int skill = item.plus;
            subtype_quants[200 + skill]++;
        }

        // Include artefacts for weapon brands.
        if (type == OBJ_WEAPONS)
            ego_quants[get_weapon_brand(item)]++;

        destroy_item(item_index, true);

        int curr_percent = acq_calls * 100 / num_itrs;
        if (curr_percent > last_percent)
        {
            mesclr();
            mprf("%2d%% done.", curr_percent);
            last_percent = curr_percent;
        }
    }

    if (total_quant == 0 || acq_calls == 0)
    {
        mpr("No items generated.");
        return;
    }

    // Print acquirement base type.
    fprintf(ostat, "Acquiring %s for:\n\n",
            type == OBJ_WEAPONS    ? "weapons" :
            type == OBJ_ARMOUR     ? "armour"  :
            type == OBJ_JEWELLERY  ? "jewellery" :
            type == OBJ_BOOKS      ? "books" :
            type == OBJ_STAVES     ? "staves" :
            type == OBJ_WANDS      ? "wands" :
            type == OBJ_MISCELLANY ? "misc. items" :
            type == OBJ_FOOD       ? "food"
                                   : "buggy items");

    // Print player species/profession.
    std::string godname = "";
    if (you.religion != GOD_NO_GOD)
        godname += " of " + god_name(you.religion);

    fprintf(ostat, "%s the %s, Level %d %s %s%s\n\n",
            you.your_name.c_str(), player_title().c_str(),
            you.experience_level,
            species_name(you.species, you.experience_level).c_str(),
            you.class_name, godname.c_str());

    // Print player equipment.
    const int e_order[] =
    {
        EQ_WEAPON, EQ_BODY_ARMOUR, EQ_SHIELD, EQ_HELMET, EQ_CLOAK,
        EQ_GLOVES, EQ_BOOTS, EQ_AMULET, EQ_RIGHT_RING, EQ_LEFT_RING
    };

    bool naked = true;
    for (int i = 0; i < NUM_EQUIP; i++)
    {
        int eqslot = e_order[i];

        // Only output filled slots.
        if (you.equip[ e_order[i] ] != -1)
        {
            // The player has something equipped.
            const int item_idx   = you.equip[e_order[i]];
            const item_def& item = you.inv[item_idx];
            const bool melded    = !player_wearing_slot(e_order[i]);

            fprintf(ostat, "%-7s: %s %s\n", equip_slot_to_name(eqslot),
                    item.name(DESC_PLAIN, true).c_str(),
                    melded ? "(melded)" : "");
            naked = false;
        }
    }
    if (naked)
        fprintf(ostat, "Not wearing or wielding anything.\n");

    // Also print the skills, in case they matter.
    std::string skills = "\nSkills:\n";
    dump_skills(skills);
    fprintf(ostat, "%s\n\n", skills.c_str());

    if (type == OBJ_BOOKS && you.skills[SK_SPELLCASTING])
    {
        // For spellbooks, for each spell discipline, list the number of
        // unseen and total spells available.
        std::vector<int> total_spells(SPTYP_LAST_EXPONENT);
        std::vector<int> unseen_spells(SPTYP_LAST_EXPONENT);

        for (int i = 0; i < NUM_SPELLS; ++i)
        {
            const spell_type spell = (spell_type) i;

            if (!is_valid_spell(spell))
                continue;

            if (you_cannot_memorise(spell))
                continue;

            // Only use spells available in books you might find lying about
            // the dungeon.
            if (spell_rarity(spell) == -1)
                continue;

            const bool seen = you.seen_spell[spell];

            const unsigned int disciplines = get_spell_disciplines(spell);
            for (int d = 0; d < SPTYP_LAST_EXPONENT; ++d)
            {
                const int disc = 1 << d;
                if (disc & SPTYP_DIVINATION)
                    continue;

                if (disciplines & disc)
                {
                    total_spells[d]++;
                    if (!seen)
                        unseen_spells[d]++;
                }
            }
        }
        for (int d = 0; d < SPTYP_LAST_EXPONENT; ++d)
        {
            const int disc = 1 << d;
            if (disc & SPTYP_DIVINATION)
                continue;

            fprintf(ostat, "%-13s:  %2d/%2d spells unseen\n",
                    spelltype_long_name(disc),
                    unseen_spells[d], total_spells[d]);
        }
    }

    fprintf(ostat, "\nAcquirement called %d times, total quantity = %d\n\n",
            acq_calls, total_quant);

    fprintf(ostat, "%5.2f%% artefacts.\n",
            100.0 * (float) num_arts / (float) acq_calls);

    if (type == OBJ_WEAPONS)
    {
        fprintf(ostat, "Maximum combined pluses: %d\n", max_plus);
        fprintf(ostat, "Average combined pluses: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);

        fprintf(ostat, "Egos (including artefacts):\n");

        const char* names[] = {
            "normal",
            "flaming",
            "freezing",
            "holy wrath",
            "electrocution",
            "orc slaying",
            "dragon slaying",
            "venom",
            "protection",
            "draining",
            "speed",
            "vorpal",
            "flame",
            "frost",
            "vampiricism",
            "pain",
            "distortion",
            "reaching",
            "returning",
            "chaos",
            "confusion",
        };

        for (int i = 0; i <= SPWPN_CONFUSE; ++i)
            if (ego_quants[i] > 0)
            {
                fprintf(ostat, "%14s: %5.2f\n", names[i],
                        100.0 * (float) ego_quants[i] / (float) acq_calls);
            }

        fprintf(ostat, "\n\n");
    }
    else if (type == OBJ_ARMOUR)
    {
        fprintf(ostat, "Maximum plus: %d\n", max_plus);
        fprintf(ostat, "Average plus: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);

        fprintf(ostat, "Egos (excluding artefacts):\n");

        const char* names[] = {
            "normal",
            "running",
            "fire resistance",
            "cold resistance",
            "poison resistance",
            "see invis",
            "darkness",
            "strength",
            "dexterity",
            "intelligence",
            "ponderous",
            "levitation",
            "magic reistance",
            "protection",
            "stealth",
            "resistance",
            "positive energy",
            "archmagi",
            "preservation",
            "reflection"
         };

        const int non_art = acq_calls - num_arts;
        for (int i = 0; i <= SPARM_REFLECTION; ++i)
        {
           if (ego_quants[i] > 0)
               fprintf(ostat, "%17s: %5.2f\n", names[i],
                       100.0 * (float) ego_quants[i] / (float) non_art);
        }
        fprintf(ostat, "\n\n");
    }
    else if (type == OBJ_BOOKS)
    {
        // Print disciplines of artefact spellbooks.
        if (subtype_quants[BOOK_RANDART_THEME]
            + subtype_quants[BOOK_RANDART_LEVEL] > 0)
        {
            fprintf(ostat, "Primary disciplines/levels of randart books:\n");

            const char* names[] = {
                "conjuration",
                "enchantment",
                "fire magic",
                "ice magic",
                "transmutation",
                "necromancy",
                "summoning",
                "divination",
                "translocation",
                "poison magic",
                "earth magic",
                "air magic",
                "holy magic"
            };

            for (int i = 0; i < SPTYP_LAST_EXPONENT; ++i)
            {
                if (ego_quants[i] > 0)
                {
                    fprintf(ostat, "%17s: %5.2f\n", names[i],
                            100.0 * (float) ego_quants[i] / (float) num_arts);
                }
            }
            // List levels for fixed level randarts.
            for (int i = 1; i < 9; ++i)
            {
                const int k = SPTYP_LAST_EXPONENT + i;
                if (ego_quants[k] > 0)
                {
                    fprintf(ostat, "%15s %d: %5.2f\n", "level", i,
                            100.0 * (float) ego_quants[i] / (float) num_arts);
                }
            }
        }

        // Also list skills for manuals.
        if (subtype_quants[BOOK_MANUAL] > 0)
        {
            const int mannum = subtype_quants[BOOK_MANUAL];
            fprintf(ostat, "\nManuals:\n");
            for (int i = SK_FIGHTING; i <= SK_EVOCATIONS; ++i)
            {
                const int k = 200 + i;
                if (subtype_quants[k] > 0)
                {
                    fprintf(ostat, "%17s: %5.2f\n", skill_name(i),
                            100.0 * (float) subtype_quants[k] / (float) mannum);
                }
            }
        }
        fprintf(ostat, "\n\n");
    }

    item_def item;
    item.quantity  = 1;
    item.base_type = type;

    const description_level_type desc = (type == OBJ_BOOKS ? DESC_PLAIN
                                                           : DESC_DBNAME);
    const bool terse = (type == OBJ_BOOKS ? false : true);

    // First, get the maximum name length.
    int max_width = 0;
    for (int i = 0; i < 256; ++i)
    {
        if (type == OBJ_BOOKS && i >= 200)
            break;

        if (subtype_quants[i] == 0)
            continue;

        item.sub_type = i;
        std::string name = item.name(desc, terse, true);

        max_width = std::max(max_width, (int) name.length());
    }

    // Now output the sub types.
    char format_str[80];
    sprintf(format_str, "%%%ds: %%6.2f\n", max_width);

    for (int i = 0; i < 256; ++i)
    {
        if (type == OBJ_BOOKS && i >= 200)
            break;

        if (subtype_quants[i] == 0)
            continue;

        item.sub_type = i;
        std::string name = item.name(desc, terse, true);

        fprintf(ostat, format_str, name.c_str(),
                (float) subtype_quants[i] * 100.0 / (float) total_quant);
    }
    fprintf(ostat, "-----------------------------------------\n\n");

    mpr("Results written into 'items.stat'.");
}

static void _debug_rap_stats(FILE *ostat)
{
    int i = prompt_invent_item( "Generate randart stats on which item?",
                                MT_INVLIST, -1 );

    if (i == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    // A copy of the item, rather than a reference to the inventory item,
    // so we can fiddle with the item at will.
    item_def item(you.inv[i]);

    // Start off with a non-artefact item.
    item.flags  &= ~ISFLAG_ARTEFACT_MASK;
    item.special = 0;
    item.props.clear();

    if (!make_item_randart(item))
    {
        mpr("Can't make a randart out of that type of item.");
        return;
    }

    // -1 = always bad, 1 = always good, 0 = depends on value
    const int good_or_bad[] = {
         1, //ARTP_BRAND
         0, //ARTP_AC
         0, //ARTP_EVASION
         0, //ARTP_STRENGTH
         0, //ARTP_INTELLIGENCE
         0, //ARTP_DEXTERITY
         0, //ARTP_FIRE
         0, //ARTP_COLD
         1, //ARTP_ELECTRICITY
         1, //ARTP_POISON
         1, //ARTP_NEGATIVE_ENERGY
         1, //ARTP_MAGIC
         1, //ARTP_EYESIGHT
         1, //ARTP_INVISIBLE
         1, //ARTP_LEVITATE
         1, //ARTP_BLINK
         1, //ARTP_CAN_TELEPORT
         1, //ARTP_BERSERK
         1, //ARTP_UNUSED_1
        -1, //ARTP_NOISES
        -1, //ARTP_PREVENT_SPELLCASTING
        -1, //ARTP_CAUSE_TELEPORTATION
        -1, //ARTP_PREVENT_TELEPORTATION
        -1, //ARTP_ANGRY
        -1, //ARTP_METABOLISM
        -1, //ARTP_MUTAGENIC
         0, //ARTP_ACCURACY
         0, //ARTP_DAMAGE
        -1, //ARTP_CURSED
         0, //ARTP_STEALTH
         0  //ARTP_MAGICAL_POWER
    };

    // No bounds checking to speed things up a bit.
    int all_props[ARTP_NUM_PROPERTIES];
    int good_props[ARTP_NUM_PROPERTIES];
    int bad_props[ARTP_NUM_PROPERTIES];
    for (i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        all_props[i] = 0;
        good_props[i] = 0;
        bad_props[i] = 0;
    }

    int max_props         = 0, total_props         = 0;
    int max_good_props    = 0, total_good_props    = 0;
    int max_bad_props     = 0, total_bad_props     = 0;
    int max_balance_props = 0, total_balance_props = 0;

    int num_randarts = 0, bad_randarts = 0;

    artefact_properties_t proprt;

    for (i = 0; i < RANDART_SEED_MASK; ++i)
    {
        if (kbhit())
        {
            getch();
            mpr("Stopping early due to keyboard input.");
            break;
        }

        item.special = i;

        // Generate proprt once and hand it off to randart_is_bad(),
        // so that randart_is_bad() doesn't generate it a second time.
        artefact_wpn_properties( item, proprt );
        if (randart_is_bad(item, proprt))
        {
            bad_randarts++;
            continue;
        }

        num_randarts++;
        proprt[ARTP_CURSED] = 0;

        int num_props = 0, num_good_props = 0, num_bad_props = 0;
        for (int j = 0; j < ARTP_NUM_PROPERTIES; ++j)
        {
            const int val = proprt[j];
            if (val)
            {
                num_props++;
                all_props[j]++;
                switch (good_or_bad[j])
                {
                case -1:
                    num_bad_props++;
                    break;
                case 1:
                    num_good_props++;
                    break;
                case 0:
                    if (val > 0)
                    {
                        good_props[j]++;
                        num_good_props++;
                    }
                    else
                    {
                        bad_props[j]++;
                        num_bad_props++;
                    }
                }
            }
        }

        int balance = num_good_props - num_bad_props;

        max_props         = std::max(max_props, num_props);
        max_good_props    = std::max(max_good_props, num_good_props);
        max_bad_props     = std::max(max_bad_props, num_bad_props);
        max_balance_props = std::max(max_balance_props, balance);

        total_props         += num_props;
        total_good_props    += num_good_props;
        total_bad_props     += num_bad_props;
        total_balance_props += balance;

        if (i % 16777 == 0)
        {
            mesclr();
            float curr_percent = (float) i * 1000.0
                / (float) RANDART_SEED_MASK;
            mprf("%4.1f%% done.", curr_percent / 10.0);
        }

    }

    fprintf(ostat, "Randarts generated: %d valid, %d invalid\n\n",
            num_randarts, bad_randarts);

    fprintf(ostat, "max # of props = %d, avg # = %5.2f\n",
            max_props, (float) total_props / (float) num_randarts);
    fprintf(ostat, "max # of good props = %d, avg # = %5.2f\n",
            max_good_props, (float) total_good_props / (float) num_randarts);
    fprintf(ostat, "max # of bad props = %d, avg # = %5.2f\n",
            max_bad_props, (float) total_bad_props / (float) num_randarts);
    fprintf(ostat, "max (good - bad) props = %d, avg # = %5.2f\n\n",
            max_balance_props,
            (float) total_balance_props / (float) num_randarts);

    const char* rap_names[] = {
        "ARTP_BRAND",
        "ARTP_AC",
        "ARTP_EVASION",
        "ARTP_STRENGTH",
        "ARTP_INTELLIGENCE",
        "ARTP_DEXTERITY",
        "ARTP_FIRE",
        "ARTP_COLD",
        "ARTP_ELECTRICITY",
        "ARTP_POISON",
        "ARTP_NEGATIVE_ENERGY",
        "ARTP_MAGIC",
        "ARTP_EYESIGHT",
        "ARTP_INVISIBLE",
        "ARTP_LEVITATE",
        "ARTP_BLINK",
        "ARTP_BERSERK",
        "ARTP_NOISES",
        "ARTP_PREVENT_SPELLCASTING",
        "ARTP_CAUSE_TELEPORTATION",
        "ARTP_PREVENT_TELEPORTATION",
        "ARTP_ANGRY",
        "ARTP_METABOLISM",
        "ARTP_MUTAGENIC",
        "ARTP_ACCURACY",
        "ARTP_DAMAGE",
        "ARTP_CURSED",
        "ARTP_STEALTH",
        "ARTP_MAGICAL_POWER"
    };

    fprintf(ostat, "                            All    Good   Bad\n");
    fprintf(ostat, "                           --------------------\n");

    for (i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        if (all_props[i] == 0)
            continue;

        fprintf(ostat, "%-25s: %5.2f%% %5.2f%% %5.2f%%\n", rap_names[i],
                (float) all_props[i] * 100.0 / (float) num_randarts,
                (float) good_props[i] * 100.0 / (float) num_randarts,
                (float) bad_props[i] * 100.0 / (float) num_randarts);
    }

    fprintf(ostat, "\n-----------------------------------------\n\n");
    mpr("Results written into 'items.stat'.");
}

void debug_item_statistics( void )
{
    FILE *ostat = fopen("items.stat", "a");

    if (!ostat)
    {
        mprf(MSGCH_ERROR, "Can't write items.stat: %s", strerror(errno));
        return;
    }

    mpr("Generate stats for: [a] acquirement [b] randart properties");

    const int keyin = tolower( get_ch() );
    switch ( keyin )
    {
    case 'a': _debug_acquirement_stats(ostat); break;
    case 'b': _debug_rap_stats(ostat);
    default:
        canned_msg( MSG_OK );
        break;
    }

    fclose(ostat);
}

void wizard_draw_card()
{
    msg::streams(MSGCH_PROMPT) << "Which card? " << std::endl;
    char buf[80];
    if (cancelable_get_line_autohist(buf, sizeof buf))
    {
        mpr("Unknown card.");
        return;
    }

    std::string wanted = buf;
    lowercase(wanted);

    bool found_card = false;
    for ( int i = 0; i < NUM_CARDS; ++i )
    {
        const card_type c = static_cast<card_type>(i);
        std::string card = card_name(c);
        lowercase(card);
        if ( card.find(wanted) != std::string::npos )
        {
            card_effect(c, DECK_RARITY_LEGENDARY);
            found_card = true;
            break;
        }
    }
    if (!found_card)
        mpr("Unknown card.");
}
#endif


