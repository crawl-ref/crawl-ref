#! /usr/bin/env perl

# Generates aptitude table from skills2.cc and the aptitude template file.
# All species names are discovered from skills2.cc and all skill abbreviations
# are discovered from the apt template file, so this script should be
# reasonably insulated from skill and species changes.
#

use strict;
use warnings;

my ($target, $template, $modfile, $skillfile) = @ARGV;
die "Usage: $0 <target> <template> player.cc skills2.cc\n"
  unless ($modfile && $skillfile && $template && $target && -r $template
          && -r $skillfile && -r $modfile);

my %ABBR_SKILL;
my %SKILL_ABBR;
my %SPECIES_SKILLS;
my @SPECIES;
my %SEEN_SPECIES;

main();

sub main {
  load_skill_abbreviations($template);
  load_aptitudes($skillfile);
  load_mods($modfile);
  create_aptitude_file($template, $target);
}

sub create_aptitude_file {
  my ($template, $target) = @_;
  open my $outf, '>', $target or die "Can't write $target: $!\n";
  open my $inf, '<', $template or die "Can't read $template: $!\n";
  my $foundmarkers;
  my $lastline;
  while (<$inf>) {
    print $outf $_;
    if (/^-{60,}/) {
      $foundmarkers = 1;
      print $outf (aptitude_table($lastline));

      # Repeat separator and headerline under the table.
      print $outf $_;
      print $outf $lastline;
    }
    $lastline = $_;
  }
  die "Could not find skill table sections in $template\n" unless $foundmarkers;
}

sub abbr_to_skill {
  my $abbr = shift;
  my $skill = $ABBR_SKILL{$abbr};
  die "Could not find skill corresponding to abbreviation: $abbr\n"
    unless $skill;
  $skill
}

sub fix_draco_species {
  my ($sp, $rseen) = @_;
  if ($sp =~ /^(\w+) Draconian/) {
    my $flavour = $1;
    if (!$$rseen) {
      $$rseen = length($sp);
      $sp = "Draconian $flavour";
    }
    else {
      $sp = sprintf("%*s", $$rseen, $flavour);
    }
    $sp =~ s/ Base//;
  }
  $sp
}

sub find_skill {
  my ($species, $skill) = @_;
  my $sk = $SPECIES_SKILLS{$species}{$skill};
  $sk
}

sub split_species {
  my $sp = shift;
  if ($sp =~ /^(.*) (.*)$/) {
    return ($2, $1);
  }
  return ($sp, $sp);
}

sub compare_species {
  my ($a, $b) = @_;
  return -1 if $a eq 'Human';
  return 1 if $b eq 'Human';

  my ($abase, $asub) = split_species($a);
  my ($bbase, $bsub) = split_species($b);
  my $basecmp = $abase cmp $bbase;
  return $basecmp if $basecmp;
  return $asub cmp $bsub;
}

sub sort_species {
  sort { compare_species($a, $b) } @_
}

sub aptitude_table {
  chomp(my $headers = shift);
  my @skill_abbrevs = $headers =~ /(\S+)/g;

  my $text = '';
  my $seen_draconian_length;
  for my $sp (sort_species(@SPECIES)) {
    my $line = '';
    $line .= fix_draco_species($sp, \$seen_draconian_length);

    for my $abbr (@skill_abbrevs) {
      my $skill = find_skill($sp, abbr_to_skill($abbr)) || 0;

      my $pos = index($headers, " $abbr");
      die "Could not find $abbr in $headers?\n" if $pos == -1;
      $pos++;
      if ($pos > length($line)) {
        $line .= " " x ($pos - length($line));
      }

      my $cwidth = length($abbr);
      $cwidth = 3 if $abbr eq "UC"; #$cwidth < 3;
      my $fmt = "%+*d";
      $fmt = "%*d" if $skill == 0;
      $fmt = " NA" if $skill == -99;
      $fmt = "%*d0", $cwidth = 2 if $abbr eq "Exp";
      $line .= sprintf($fmt, $cwidth, $skill);
    }
    $text .= "$line\n";
  }
  $text
}

sub skill_name {
  my $text = shift;
  $text =~ tr/a-zA-Z / /c;
  $text =~ s/ +/ /g;
  propercase_string($text)
}

sub load_skill_abbreviations {
  my $template = shift;
  open my $inf, '<', $template or die "Can't read $template: $!\n";
  my $in_abbr;
  while (<$inf>) {
    $in_abbr = 1 if /-{15,}/;
    next unless $in_abbr;
    while (/(\S+(?: \S+)*) {1,3}- {1,3}(\S+(?: \S+)*)/g) {
      my $skill = $2;
      my $abbr = $1;
      $skill = skill_name($skill);
      $SKILL_ABBR{$skill} = $abbr;
      $ABBR_SKILL{$abbr} = $skill;
    }
  }
  close $inf;

  die "No skill names found in $template\n" unless %SKILL_ABBR;
}

sub propercase_string {
  my $s = lc(shift);
  $s =~ s/\b(\w)/\u$1/g;
  $s
}

sub fix_underscores {
  my $s = shift;
  $s =~ tr/_/ /;
  $s
}

sub load_aptitudes {
  my $skillfile = shift;
  open my $inf, '<', $skillfile or die "Can't read $skillfile: $!\n";
  my $seen_skill_start;
  my $species;

  while (<$inf>) {
    last if /\*{40,}/;
    if (!$seen_skill_start) {
      $seen_skill_start = 1 if /species_skill_aptitudes\[/;
    }
    else {
      if (/APT\(\s*SP_(\w+)\s*,\s*SK_(\w+)\s*,\s*(-?\d+)\s*\)/) {
        $species = propercase_string(fix_underscores($1));
        if (!$SEEN_SPECIES{$species}) {
          $SEEN_SPECIES{$species} = 1;
          push @SPECIES, $species;
        }

        my $apt = $3;
        my $skill = skill_name($2);
        die "$skillfile:$.: Unknown skill: $skill\n"
          unless $SKILL_ABBR{$skill};
        die "$skillfile:$.: Repeated skill def $1 for $species.\n"
          if $SPECIES_SKILLS{$species}{$skill};
        ($SPECIES_SKILLS{$species}{$skill}) = $apt;
      }
    }
  }
  die "Could not find aptitudes for species in $skillfile\n"
    unless %SPECIES_SKILLS;
}

sub load_mods {
  my $modfile = shift;
  open my $inf, '<', $modfile or die "Can't read $modfile: $!\n";

  my $table;

  my @species;
  while (<$inf>) {
    $table = $1 if m{// table: ([A-Za-z ]+)\s*$};
    next unless $table;

    if (/SP_(\w+)/) {
      push @species, propercase_string(fix_underscores($1));
    }

    if (/return/ && /(-?\d+)/) {
      my $mod = $1;
      #die "$modfile:$.: No species associated with $table mod $1\n"
      #  unless @species;
      for my $sp (@species) {
        $SPECIES_SKILLS{$sp}{$table} = $1;
      }
      @species = ();
    }

    undef $table if /}/;
  }
  close $inf;
  for ("Experience", "Hit Points", "Magic Points")
  {
      die "Can't find table: $_ in $modfile\n" unless $SPECIES_SKILLS{Spriggan}{$_};
  }
}
