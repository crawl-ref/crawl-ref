#!/usr/bin/env perl
use warnings;
use strict;

my $ART_ENUM = 'art-enum.h';
my $ART_DATA = 'art-data.h';

my $line_num      = 0;
my @warnings      = ();
my @errors        = ();
my @all_artefacts = ();
my %used_names    = ();
my %used_appears  = ();
my %used_enums    = ();
my %found_funcs   = ();

my %field_type = (
    AC       => "num",
    ACROBAT  => "bool",
    ANGRY    => "num",
    APPEAR   => "str",
    ARCHMAGI => "bool",
    BASE_ACC => "num",
    BASE_DAM => "num",
    BASE_DELAY => "num",
    BLINK    => "bool",
    BRAND    => "enum",
    CHAOTIC  => "bool",
    CLARITY  => "bool",
    COLD     => "num",
    COLOUR   => "enum",
    CORRODE  => "bool",
    DBRAND   => "str",
    DELETED  => "bool",
    DEX      => "num",
    DESCRIP  => "str",
    DRAIN    => "bool",
    ELEC     => "bool",
    ENH_CONJ => "bool",
    ENH_HEXES => "bool",
    ENH_SUMM => "bool",
    ENH_NECRO => "bool",
    ENH_TLOC => "bool",
    ENH_FIRE => "bool",
    ENH_ICE  => "bool",
    ENH_AIR  => "bool",
    ENH_EARTH => "bool",
    ENH_ALCH => "bool",
    ENH_FORGE => "bool",
    EV       => "num",
    EVIL     => "bool",
    FOG      => "bool",
    FIRE     => "num",
    FRAGILE  => "bool",
    HARM     => "bool",
    HOLY     => "bool",
    INSCRIP  => "str",
    INT      => "num",
    INV      => "bool",
    FLY      => "bool",
    LIFE     => "num",
    WILL     => "num",
    HP       => "num",
    MP       => "num",
    MUTATE   => "bool",
    NAME     => "str",
    NOGEN    => "bool",
    NOISES   => "bool",
    NOSPELL  => "bool",
    NOTELEP  => "bool",
    NO_UPGRADE => "bool",
    POISON   => "bool",
    RAMPAGE  => "bool",
    RANDAPP  => "bool",
    RCORR    => "bool",
    REGEN    => "num",
    REGEN_MP => "num",
    RMSL     => "bool",
    RMUT     => "bool",
    SEEINV   => "bool",
    SKIP_EGO => "bool",
    SH       => "num",
    SLAY     => "num",
    SPECIAL  => "bool",
    SLOW     => "bool",
    STEALTH  => "num",
    STR      => "num",
    TYPE     => "str",
    UNIDED   => "bool",
    VALUE    => "num",

    TILE     => "str",
    TILE_EQ  => "str",
    TILERIM  => "bool",

    flags     => "flags",

    equip_func         => "func",
    unequip_func       => "func",
    world_reacts_func  => "func",
    melee_effects_func => "func",
    launch_func        => "func",
    death_effects_func => "func",

    plus      => "num",
    plus2     => "num",
    base_type => "enum",
    sub_type  => "enum",
    fallback_base_type => "enum",
    fallback_sub_type => "enum",
    FB_BRAND => "enum",

    unused    => "unused",
);

my @field_list = keys(%field_type);

sub error
{
    my ($artefact, $str) = @_;

    my $msg = "";

    my $name = $artefact->{NAME} || $artefact->{APPEAR} || "NAMELESS";

    if ($artefact->{_FINISHING})
    {
        $msg .= "Artefact $name starting at line "
              . $artefact->{_START_LINE} . ": ";
    }
    else
    {
        $msg = "Artefact $name at line $line_num: "
    }

    $msg .= $str;

    $artefact->{_ERROR} = 1;

    push(@errors, $msg);
}

sub is_number
{
    my ($num) = @_;
    return ($num =~ /^[+-]?\d+$/);
}

sub finish_art
{
    my ($artefact) = @_;

    $artefact->{_FINISHING} = 1;

    my $must;
    foreach $must ("NAME", "OBJ", "COLOUR")
    {
        if (!defined($artefact->{$must}))
        {
            error($artefact, "Required field '$must' missing");
            $artefact->{$must} = "_MISSING_";
        }
    }

    # Prevent further errors caused by absence of OBJ
    $artefact->{base_type} ||= "";
    $artefact->{sub_type}  ||= "";

    # Handled later
    $artefact->{flags} = "";

    # Default ego.
    if (!exists($artefact->{BRAND}))
    {
        my $type = $artefact->{base_type} || "";

        if ($type eq "OBJ_WEAPONS")
        {
            $artefact->{BRAND} = "SPWPN_NORMAL";
        }
        elsif ($type eq "OBJ_ARMOUR")
        {
            $artefact->{BRAND} = "SPARM_NORMAL";
        }
        else
        {
            $artefact->{BRAND} = "0";
        }
    }

    if (!exists($artefact->{FALLBACK}))
    {
        $artefact->{fallback_base_type} = "OBJ_UNASSIGNED";
        $artefact->{fallback_sub_type} = 250;
    }

    if (!exists($artefact->{FB_BRAND}))
    {
        $artefact->{FB_BRAND} = "-1";
    }

    # Appearance is no longer mandatory.
    $artefact->{APPEAR} = $artefact->{NAME} unless defined($artefact->{APPEAR});

    # Fill in function pointers.
    my $enum = $artefact->{_ENUM};
    my $funcs;
    if ($found_funcs{$enum})
    {
        $funcs = $found_funcs{$enum};
        delete($found_funcs{$enum});
    }
    else
    {
        $funcs = {};
    }

    foreach my $func_name (qw(equip unequip world_reacts melee_effects launch death_effects))
    {
        my $val;
        if ($funcs->{$func_name})
        {
            $val = "_${enum}_" . $funcs->{$func_name};
        }
        else
        {
            $val = "nullptr";
        }
        $artefact->{"${func_name}_func"} = $val;
    }

    # Default values.
    my $field;
    foreach $field (@field_list)
    {
        if (!exists($artefact->{$field}))
        {
            # Give default values for fields not specified.
            my $type = $field_type{$field} || "";
            if ($type eq "str")
            {
                $artefact->{$field} = "";
            }
            elsif ($type eq "num" || $type eq "bool")
            {
                $artefact->{$field} = "0";
            }
            elsif ($type eq "enum")
            {
                error($artefact, "No enumeration for field '$field'");
            }
            elsif ($type eq "func")
            {
                $artefact->{$field} = "nullptr";
            }
            elsif ($type eq "unused")
            {
                $artefact->{$field} = "0";
            }
            else
            {
                error($artefact, "Unknown type '$type' for field '$field'");
            }
        }
        elsif (!defined($artefact->{$field}))
        {
            error($artefact, "Field '$field' not defined");
        }
    }

    my $flags = "";
    my $flag;
    foreach $flag ("SPECIAL", "HOLY", "EVIL", "CHAOTIC", "DELETED", "NOGEN",
                   "RANDAPP", "UNIDED", "SKIP_EGO")
    {
        if ($artefact->{$flag})
        {
            $flags .= "UNRAND_FLAG_$flag | ";
        }
    }

    if ($flags eq "")
    {
        $flags = "UNRAND_FLAG_NONE";
    }
    else
    {
        chop($flags);
        chop($flags);
        chop($flags);
    }
    $artefact->{flags} = $flags;

    delete($artefact->{_FINISHING});
    $artefact->{_FINISHED} = 1;
}

sub process_line
{
    my ($artefact, $line) = @_;

    # A line can start with whitespace or a + if it's a continuation of a field
    # with a string value. + at the start of the line becomes a newline before
    # it is displayed, and any amount of whitespace becomes a single space.
    if ($line =~ /^[\s+]/)
    {
        my $prev_field = $artefact->{_PREV_FIELD} || "";
        if ($field_type{$prev_field} eq "str")
        {
            my $sep = ($line =~ /^\+/) ? '\n' : ' ';
            $line =~ s/^(\+|\s*)//;
            $artefact->{$prev_field} .= $sep . $line;
        }
        else
        {
            error($artefact, "line starts with an invalid character");
        }
        return;
    }

    $artefact->{_START_LINE} ||= $line_num;

    my ($field, $value) = ($line =~ /([^:]+):\s*(.*)/);

    $field ||= "";
    $value ||= "";

    # Strip leading and trailing white space.
    $field =~ s/^\s*|\s*$//g;
    $value =~ s/^\s*|\s*$//g;

    if ($field eq "")
    {
        error($artefact, "No field");
        return;
    }

    if (defined($artefact->{$field}))
    {
        error($artefact, "Field '$field' already set");
        return;
    }

    $artefact->{_PREV_FIELD} = $field;
    $artefact->{$field}      = $value;

    if ($value eq "")
    {
        error($artefact, "Field '$field' has no value");
        return;
    }

    if ($field eq "BOOL")
    {
        my @parts = split(/\s*,\s*/, $value);
        my $part;
        foreach $part (@parts)
        {
            my $up = uc($part);
            if (!exists($field_type{$up}))
            {
                error($artefact, "Unknown bool '$part'");
            }
            elsif ($field_type{$up} ne "bool")
            {
                error($artefact, "'$part' is not a boolean");
            }
            else
            {
                $artefact->{$up} = 1;
            }
        }
    }
    elsif ($field eq "OBJ")
    {
        my @parts = split(m!/!, $value);

        if (@parts > 2)
        {
            error($artefact, "Too many parts to OBJ");
            return;
        }
        elsif (@parts == 1)
        {
            error($artefact, "Too few parts to OBJ");
            return;
        }

        if ($parts[0] !~ /^OBJ_/)
        {
            error($artefact, "OBJ base type must start with 'OBJ_'");
            return;
        }
        $artefact->{base_type} = $parts[0];
        $artefact->{sub_type}  = $parts[1];
    }
    elsif ($field eq "FALLBACK")
    {
        my @parts = split(m!/!, $value);

        if (@parts > 2)
        {
            error($artefact, "Too many parts to FALLBACK");
            return;
        }
        elsif (@parts == 1)
        {
            error($artefact, "Too few parts to FALLBACK");
            return;
        }

        if ($parts[0] !~ /^OBJ_/)
        {
            error($artefact, "FALLBACK base type must start with 'OBJ_'");
            return;
        }
        $artefact->{fallback_base_type} = $parts[0];
        $artefact->{fallback_sub_type}  = $parts[1];
    }
    elsif ($field eq "PLUS")
    {
        my @parts = split(m!/!, $value);

        if (@parts > 2)
        {
            error($artefact, "Too many parts to PLUS");
            return;
        }

        if (!is_number($parts[0]))
        {
            error($artefact, "'$parts[0]' in PLUS is not a number");
            return;
        }
        if (@parts == 2 && !is_number($parts[1]))
        {
            error($artefact, "'$parts[1]' in PLUS is not a number");
            return;
        }
        $artefact->{plus}  = $parts[0];
        $artefact->{plus2} = $parts[1] if (@parts == 2);
    }
    elsif ($field eq "ENUM")
    {
    }
    else
    {
        if (!exists($field_type{$field}) || $field =~ /^[a-z]/)
        {
            error($artefact, "No such field as '$field'");
            return;
        }

        if ($field_type{$field} eq "bool")
        {
            error($artefact, "'$field' should be expressed using BOOL");
            return;
        }

        my $num =  is_number($value);
        my $type = $field_type{$field} || "";

        if (($type eq "num" && !$num) || ($type eq "str" && $num))
        {
            error($artefact, "'$value' invalid value type for field '$field'.");
            return;
        }
    }

    my $enum = "";

    if ($field eq "NAME")
    {
        if (exists($used_names{$value}))
        {
            error($artefact, "Name \"$value\" already used at line " .
                             $used_names{$value});
            return;
        }
        $used_names{$value} = $line_num;

        if (!exists($artefact->{_ENUM}))
        {
            $enum = $value;
            $enum =~ s/'//g;
            # If possible, make the enum literal the part of the name between
            # double quotes, or the part of the name after " of " or
            # " of the ".
            if ($enum =~ /"(.*)"/)
            {
                $enum = $1;
            }
            elsif ($enum =~ / of (?:the )?(.*)/)
            {
                $enum = $1;
            }
            $enum = uc($enum);
            $enum =~ s/[ -]/_/g;
        }
    }
    elsif ($field eq "APPEAR")
    {
        if (exists($used_appears{$value}))
        {
            error($artefact, "Name '$value' already used at line " .
                             $used_appears{$value});
            return;
        }
        $used_appears{$value} = $line_num;
    }
    elsif ($field eq "ENUM")
    {
        if (exists($artefact->{NAME}))
        {
            error($artefact, "ENUM must be before NAME");
            return;
        }
        $enum = "$value";
    }

    if ($enum ne "")
    {
        if (exists($used_enums{$enum}))
        { error($artefact, "Enum \"$enum\" already used by artefact " .
                             "\"$used_enums{$enum}\"");
            return;
        }

        $used_enums{$enum} = $value;
        $artefact->{_ENUM} = $enum;
    }
}

my @art_order = (
    "NAME", "APPEAR", "TYPE", "\n",
    "INSCRIP", "DBRAND", "DESCRIP", "\n",
    "base_type", "sub_type", "\n",
    "fallback_base_type", "fallback_sub_type", "FB_BRAND", "\n",
    "plus", "plus2", "COLOUR", "VALUE", "\n",
    "flags",

# start TAG_MAJOR_VERSION == 34
    # Remove six copies of "unused", when
    # it is no longer the case that TAG_MAJOR_VERSION == 34
    "{", "BRAND", "AC", "EV", "STR", "INT", "DEX", "\n",
    "FIRE", "COLD", "ELEC", "POISON", "LIFE", "WILL", "\n",
    "SEEINV", "INV", "FLY", "BLINK", "unused",  "NOISES", "\n",
    "NOSPELL", "unused", "NOTELEP", "ANGRY", "unused", "\n",
    "MUTATE", "unused", "SLAY", "unused", "STEALTH", "MP", "\n",
    "BASE_DELAY", "HP", "CLARITY", "BASE_ACC", "BASE_DAM", "\n",
    "RMSL", "unused", "REGEN", "unused", "NO_UPGRADE", "RCORR", "\n",
    "RMUT", "unused", "CORRODE", "DRAIN", "SLOW", "FRAGILE", "\n",
    "SH", "HARM", "RAMPAGE", "ARCHMAGI", "ENH_CONJ", "ENH_HEXES", "\n",
    "ENH_SUMM", "ENH_NECRO", "ENH_TLOC", "unused", "ENH_FIRE", "\n",
    "ENH_ICE", "ENH_AIR", "ENH_EARTH", "ENH_ALCH", "\n",
    "ACROBAT", "REGEN_MP", "ENH_FORGE",
    "}",
# end TAG_MAJOR_VERSION
# start TAG_MAJOR_VERSION == 35
#     "{", "BRAND", "AC", "EV", "STR", "INT", "DEX", "\n",
#     "FIRE", "COLD", "ELEC", "POISON", "LIFE", "WILL", "\n",
#     "SEEINV", "INV", "FLY", "BLINK", "NOISES", "\n",
#     "NOSPELL", "NOTELEP", "ANGRY", "\n",
#     "MUTATE", "SLAY", "STEALTH", "MP", "\n",
#     "BASE_DELAY", "HP", "CLARITY", "BASE_ACC", "BASE_DAM", "\n",
#     "RMSL", "REGEN", "NO_UPGRADE", "RCORR", "\n",
#     "RMUT", "CORRODE", "DRAIN", "SLOW", "FRAGILE", "\n",
#     "SH", "HARM", "RAMPAGE", "ARCHMAGI", "ENH_CONJ", "ENH_HEXES", "\n",
#     "ENH_SUMM", "ENH_NECRO", "ENH_TLOC", "ENH_FIRE", "\n",
#     "ENH_ICE", "ENH_AIR", "ENH_EARTH", "ENH_ALCH", "\n",
#     "ACROBAT", "REGEN_MP", "ENH_FORGE",
#     "}",
# end TAG_MAJOR_VERSION

    "equip_func", "unequip_func", "world_reacts_func", "melee_effects_func",
    "launch_func", "death_effects_func"
);

sub art_to_str
{
    my ($artefact) = @_;

    my $indent = 1;

    my $str = "{\n    ";

    for (my $i = 0; $i < @art_order; $i++)
    {
        my $part = $art_order[$i];

        if (length($part) == 1)
        {
            if ($part eq "{")
            {
                $str .= "\n" . (" " x ($indent * 4)) . "{";
                $indent++;
                $str .= "\n" . (" " x ($indent * 4));
            }
            elsif ($part eq "}")
            {
                $indent--;
                $str .= "\n" . (" " x ($indent * 4)) . "},";
                $str .= "\n" . (" " x ($indent * 4));
            }
            else
            {
                $str .= "\n" . (" " x ($indent * 4));
            }
            next;
        }

        my $bracketed = 0;
        if ($part =~ /^{(.*)/)
        {
            $part = $1;
            $str .= "{ ";
            $bracketed = 1;
        }

        if (!defined($field_type{$part}))
        {
            print STDERR "No field type for part '$part'\n";
            next;
        }

        if ($field_type{$part} eq "str")
        {
            my $temp = $artefact->{$part};
            $temp =~ s/"/\\"/g;
            $str .= ($temp eq "" && $part =~ /^(TYPE|INSCRIP|DESCRIP|DBRAND)$/)
                ? "nullptr" : "\"$temp\"";
        }
        else
        {
            $str .= $artefact->{$part};
        }

        $str .= " }" if ($bracketed);

        $str .= ", ";
    }

    $str .= "\n},\n\n";

    return ($str);
}

# Write $text to $filename unless it's already there.
# Checking this first may reduce the number of files make attempts to rebuild.
sub write_to_file($$)
{
    my ($filename, $text) = @_;
    my $old_text = "!$text";

    if (open(_, '<', $filename))
    {
        local $/;
        $old_text = <_>;
        if ($text eq $old_text)
        {
            unless (close _)
            {
                die "Couldn't close '$filename' after reading";
            }
            return
        }
    }

    unless (open(_, '>', $filename))
    {
        die "Couldn't open '$filename' for writing";
    }
    print _ $text;

    unless (close _)
    {
        die "Couldn't close '$filename' after writing";
    }
}


sub write_data
{
    print "    Generating $ART_DATA\n";

    my $text = <<"ENDofTEXT";
/* Definitions for unrandom artefacts. */

/**********************************************************************
 * WARNING!
 *
 * This file is automatically generated. Do not edit this file directly;
 * instead edit art-data.txt to modify unrandart data, or util/art-data.pl
 * if this file is not being generated to your taste.
 *
 * If the unrandart_entry struct is changed or a new artefact property is
 * added to artefact_prop_type, then change art-data.pl so that the
 * $ART_DATA file it produces matches up with the new structure.
 *
 * Ignoring these warnings and editing this file directly will cause you
 * to be hunted down and eaten by dire tarantulas.
 **********************************************************************/

#pragma once
#ifndef ART_FUNC_H
#error "art-func.h must be included before art-data.h"
#endif

ENDofTEXT

    my $artefact;
    foreach $artefact (@all_artefacts)
    {
        $text .= "/* UNRAND_$artefact->{_ENUM} */\n";
        $text .= art_to_str($artefact);
    }

    $text .= <<FOOTER;
FOOTER

    write_to_file $ART_DATA, $text;
}

sub unrand_enum_constants()
{
    my $result = '';
    my $i;
    my $longest_enum = 0;
    for ($i = 0; $i < @all_artefacts; $i++)
    {
        my $enum = $all_artefacts[$i]->{_ENUM};
        my $len  = length($enum);
        $longest_enum = $len if ($len > $longest_enum);
    }

    my $enum;
    for ($i = 0; $i < @all_artefacts; $i++)
    {
        $enum = $all_artefacts[$i]->{_ENUM};

        $result .= "    UNRAND_$enum";

        if ($i == 0)
        {
            $result .= " = UNRAND_START,\n";
        }
        else
        {
            $result .= ", ";
            $result .= " " x ($longest_enum - length($enum));
            $result .= "// $all_artefacts[$i]->{NAME}\n";
        }
    }
    $result .= "    UNRAND_LAST = UNRAND_$enum\n";

    $result
}

sub write_enums
{
    print "    Generating $ART_ENUM\n";

    my $unrand_enum = unrand_enum_constants();

    my $text = <<ARTENUM;
#pragma once

/**********************************************************************
 * WARNING!
 *
 * This file is automatically generated. Do not edit this file directly;
 * instead edit art-data.txt to modify unrandart data, or util/art-data.pl
 * if this file is not being generated to your taste.
 *
 * Ignoring these warnings and editing this file directly will cause you
 * to be hunted down and eaten by dire rattlesnakes.
 **********************************************************************/

#define NUM_UNRANDARTS @{ [ scalar(@all_artefacts) ] }

enum unrand_type
{
    UNRAND_START = 180,
$unrand_enum
};
ARTENUM
    write_to_file $ART_ENUM, $text;
}

sub write_tiles
{
    my $tilefile = "dc-unrand.txt";
    print "    Generating $tilefile\n";

    die "Can't write to $tilefile\n"  if (-e $tilefile && !-w $tilefile);

    my %art_by_type = ();
    foreach my $artefact (@all_artefacts)
    {
        next if ($artefact->{NAME} =~ /DUMMY/);

        if ($artefact->{TILE} eq "")
        {
            print STDERR "No TILE defined for '$artefact->{NAME}'\n";
            next;
        }

        # The path always has the form /item/$dir/artefact.
        my $type   = $artefact->{base_type} || "";
        my $dir = "";
        if ($type eq "OBJ_WEAPONS")
        {
            $dir = "weapon";
        }
        elsif ($type eq "OBJ_ARMOUR")
        {
            $dir = "armour";
        }
        elsif ($type eq "OBJ_JEWELLERY")
        {
            if ($artefact->{sub_type} =~ /RING_/)
            {
                $dir = "ring";
            }
            else
            {
                $dir = "amulet";
            }
        }
        else
        {
            next;
        }

        my $definition = "$artefact->{TILE} UNRAND_$artefact->{_ENUM}";
        my $needrim    = ($artefact->{TILERIM} ? "1" : "0");
        if (defined $art_by_type{$dir})
        {
            if (defined $art_by_type{$dir}{$needrim})
            {
                push @{$art_by_type{$dir}{$needrim}}, $definition;
            }
            else
            {
                $art_by_type{$dir}{$needrim} = [$definition];
            }
        }
        else
        {
            $art_by_type{$dir} = {$needrim => [$definition]};
        }
    }

        my $text = << "HEADER_END";

# WARNING!
#
# This file is automatically generated from source/art-data.txt via
# util/art-data.pl. Do not directly edit this file, but rather change
# art-data.txt.
#
# Ignoring these warnings and editing this file directly will cause you
# to be hunted down and eaten by dire groundhogs.
HEADER_END

    # Output the tile definitions sorted by type (and thus path).
    foreach my $type (sort keys %art_by_type)
    {
        $text .= "%sdir item/$type/artefact\n";

        foreach my $needrim (sort keys %{$art_by_type{$type}})
        {
            $text .= "%rim 1\n" if ($needrim);
            foreach my $def (@{$art_by_type{$type}{$needrim}})
            {
                $text .= "$def\n";
            }
            $text .= "%rim 0\n" if ($needrim);
        }
        $text .= "\n";
    }
    write_to_file $tilefile, $text;

    my %parts;
    foreach my $artefact (@all_artefacts)
    {
        next if ($artefact->{NAME} =~ /DUMMY/);
        if (not(defined $artefact->{TILE_EQ}) || $artefact->{TILE_EQ} eq "")
        {
            if ($artefact->{base_type} ne "OBJ_JEWELLERY")
            {
                print STDERR "TILE_EQ not defined for '$artefact->{NAME}'\n";
            }
            next;
        }

        my $part = "BODY";
        if ($artefact->{base_type} eq "OBJ_WEAPONS")
        {
            $part = "HAND1";
        }
        elsif ($artefact->{base_type} ne "OBJ_ARMOUR")
        {
            next;
        }
        elsif ($artefact->{sub_type} =~ /_SHIELD/
               || $artefact->{sub_type} =~ /_BUCKLER/
               || $artefact->{sub_type} =~ /_ORB/)
        {
            $part = "HAND2";
        }
        elsif ($artefact->{sub_type} =~ /_CLOAK/
               || $artefact->{sub_type} =~ /_SCARF/)
        {
            $part = "CLOAK";
        }
        elsif ($artefact->{sub_type} =~ /_HAT|_HELMET/)
        {
            $part = "HELM";
        }
        elsif ($artefact->{sub_type} =~ /_BOOTS|_BARDING/)
        {
            $part = "BOOTS";
        }
        elsif ($artefact->{sub_type} =~ /_GLOVES/)
        {
            $part = "ARM";
        }

        if (defined $parts{$part})
        {
            push @{$parts{$part}}, $artefact;
        }
        else
        {
            $parts{$part} = [$artefact];
        }
    }

    $tilefile = "dc-player.txt";
    unless (open(TILES, "<$tilefile"))
    {
        die "Couldn't open '$tilefile' for reading: $!\n";
    }

    my $curr_part = "";
    my $content = do { local $/; <TILES> };
    my @lines   = split "\n", $content;
    foreach my $line (@lines)
    {
        if ($line =~ /parts_ctg\s+(\S+)/)
        {
            $curr_part = $1;
            next;
        }
        next if (not defined $parts{$curr_part});

        if ($line =~ /^(\S+)\s+(\S+)/)
        {
            my $name = $1;
            my $enum = $2;

            foreach my $art (@{$parts{$curr_part}})
            {
                if ($art->{TILE_EQ} eq $name)
                {
                    $art->{TILE_EQ_ENUM} = "TILEP_".$curr_part."_".$enum;
                    # Don't break from the loop in case several artefacts
                    # share the same tile.
                }
            }
        }
    }
    close(TILES);

    # Create tiledef-unrand.cc for the function unrandart_to_tile().
    # Should we also create tiledef-unrand.h this way?
    $tilefile = "tiledef-unrand.cc";
    print "    Generating $tilefile\n";

    die "Can't write to $tilefile\n"  if (-e $tilefile && !-w $tilefile);

    $text = << "HEADER_END";
/**********************************************************************
 * WARNING!
 *
 * This file is automatically generated. Do not edit this file directly;
 * instead edit art-data.txt to modify unrandart data, or util/art-data.pl
 * if this file is not being generated to your taste.
 *
 * Ignoring these warnings and editing this file directly will cause you
 * to be hunted down and eaten by dire ravens.
 **********************************************************************/

#include "AppHdr.h"
#include "rltiles/tiledef-unrand.h"

#include "art-enum.h"
#include "rltiles/tiledef-main.h"
#include "rltiles/tiledef-player.h"

int unrandart_to_tile(int unrand)
{
    switch (unrand)
    {
HEADER_END

    my $longest_enum = 0;
    foreach my $artefact (@all_artefacts)
    {
        my $enum = $artefact->{_ENUM};
        my $len  = length($enum);
        $longest_enum = $len if ($len > $longest_enum);
    }
    $longest_enum += length("UNRAND_");

    foreach my $artefact (@all_artefacts)
    {
        next if ($artefact->{NAME} =~ /DUMMY/);
        next if ($artefact->{TILE} eq "");

        my $enum = "UNRAND_$artefact->{_ENUM}";
        $text .= (" " x 4) . "case $enum:"
            . " " x ($longest_enum - length($enum) + 2) . "return TILE_$enum;\n";
    }
    $text .= (" " x 4) . "default: return 0;\n";
    $text .= (" " x 4) . "}\n";
    $text .= "}\n\n";

    $text .= "int unrandart_to_doll_tile(int unrand)\n{\n";
    $text .= (" " x 4) . "switch (unrand)\n";
    $text .= (" " x 4) . "{\n";
    foreach my $part (sort keys %parts)
    {
        $text .= (" " x 4) . "// $part\n";
        foreach my $artefact (@{$parts{$part}})
        {
            if (not defined $artefact->{TILE_EQ_ENUM})
            {
                print STDERR "Tile '$artefact->{TILE_EQ}' for part '$part' not "
                           . "found in 'dc-player.txt'.\n";
                next;
            }
            my $enum   = "UNRAND_$artefact->{_ENUM}";
            my $t_enum = $artefact->{TILE_EQ_ENUM};
            $text .= (" " x 4) . "case $enum:"
                . " " x ($longest_enum - length($enum) + 2) . "return $t_enum;\n";
        }
    }
    $text .= (" " x 4) . "default: return 0;\n";
    $text .= (" " x 4) . "}\n";
    $text .= "}\n\n";
    write_to_file $tilefile, $text;
}

my %valid_func = (
    equip         => 1,
    unequip       => 1,
    world_reacts  => 1,
    melee_effects => 1,
    launch        => 1,
    death_effects => 1,
);

sub read_funcs
{
    unless (open(INPUT, "<art-func.h"))
    {
        die "Couldn't open art-func.h for reading: $!\n";
    }

    while (<INPUT>)
    {
        if (/^static .* _([A-Z_]+)_(\S+)\s*\(/)
        {
            my $enum = $1;
            my $func = $2;

            if (!$valid_func{$func})
            {
                push(@warnings, "Unrecognized func '$func' for artefact " .
                                "'$enum'");
                next;
            }

            $found_funcs{$enum} ||= {};
            my $func_list = $found_funcs{$enum};

            $func_list->{$func} = $func;
        }
    }
    close(INPUT);
}

sub read_data
{
    unless (open(INPUT, "<art-data.txt"))
    {
        die "Couldn't open art-data.txt for reading: $!\n";
    }

    my $prev_line = "";
    my $curr_art  = {};

    while (<INPUT>)
    {
        chomp;
        $line_num++;

        # Skip comment-only lines
        next if (/^#/);

        # Strip comments.
        s/#.*//;

        # Strip trailing whitespace; leading whitespace indicates the
        # continuation of a string field.
        s/\s*$//;

        if ($_ =~ /^\s*$/)
        {
            if ($prev_line !~ /^\s*$/)
            {
                finish_art($curr_art);
                push(@all_artefacts, $curr_art);
                $curr_art = {};
            }
        }
        else
        {
            process_line($curr_art, $_);
        }
        $prev_line = $_;
    }
    close(INPUT);

    if (keys(%$curr_art) > 0)
    {
        finish_art($curr_art);
        push(@all_artefacts, $curr_art);
    }
}

###############################################################3
###############################################################3
###############################################################3

chdir("..")     if (-e "../art-data.txt");
chdir("source") if (-e "source/art-data.txt");

for my $wanted (qw/art-data.txt art-func.h artefact.h/)
{
    die "Can't read $wanted" unless -r $wanted;
}

read_funcs();
read_data();

if (keys(%found_funcs) > 0)
{
    my $key;
    foreach $key (keys(%found_funcs))
    {
        push(@warnings, "Funcs for unknown artefact enum $key: " .
                        join(", ", keys(%{$found_funcs{$key}})));
    }
}

if (@warnings > 0)
{
    print STDERR "Warning(s):\n";

    my $warn;
    foreach $warn (@warnings)
    {
        print STDERR "$warn\n";
    }
    print STDERR "\n";
}

if (@errors > 0)
{
    print STDERR "Error(s) processing art-data.txt:\n\n";

    my $err;
    foreach $err (@errors)
    {
        print STDERR "$err\n";
    }
    exit (1);
}

write_data();
write_enums();

chdir("rltiles");
write_tiles();

exit (0);
