#!/usr/bin/perl
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

#
# generate.pl
#
# Parse .keymap files, producing corresponding .c files that can be included
# into the RDP plugin source.
#

# We need at least Perl 5.8 for Unicode's sake
use 5.008;

sub keymap_symbol {
    my $name = shift;
    $name =~ s/-/_/g;
    return 'guac_rdp_keymap_' . $name;
}

#
# _generated_keymaps.c
#

my @keymaps = ();

open OUTPUT, ">", "_generated_keymaps.c";
print OUTPUT 
       '#include <guacamole/config.h>'                      . "\n"
     . '#include <rdp/rdp_keymap.h>'                        . "\n"
     . '#include <freerdp/input.h>'                         . "\n"
     .                                                        "\n"
     . '#ifdef HAVE_FREERDP_LOCALE_KEYBOARD_H'              . "\n"
     . '#include <freerdp/locale/keyboard.h>'               . "\n"
     . '#else'                                              . "\n"
     . '#include <freerdp/kbd/layouts.h>'                   . "\n"
     . '#endif'                                             . "\n"
     .                                                        "\n"
     . '#include <stddef.h>'                                . "\n"
     .                                                        "\n";

for my $filename (@ARGV) {

    my $content = "";
    my $parent = "";
    my $layout_name = "";
    my $freerdp = "";

    # Parse file
    open INPUT, '<', "$filename";
    binmode INPUT, ":encoding(utf8)";
    while (<INPUT>) {

        chomp;

        # Comments
        if (m/^\s*#/) {}

        # Name
        elsif ((my $name) = m/^\s*name\s+"(.*)"\s*(?:#.*)?$/) {
            $layout_name = $name;
        }

        # Parent map
        elsif ((my $name) = m/^\s*parent\s+"(.*)"\s*(?:#.*)?$/) {
            $parent = keymap_symbol($name);
        }

        # FreeRDP equiv 
        elsif ((my $name) = m/^\s*freerdp\s+"(.*)"\s*(?:#.*)?$/) {
            $freerdp = $name;
        }

        # Map
        elsif ((my $range, my $onto) =
               m/^\s*map\s+([^~]*)\s+~\s+(".*"|0x[0-9A-Fa-f]+)\s*(?:#.*)?$/) {

            my @keysyms   = ();
            my @scancodes = ();

            my $ext_flags = 0;
            my $set_shift = 0;
            my $set_altgr = 0;
            my $set_num   = 0;

            my $clear_shift = 0;
            my $clear_altgr = 0;
            my $clear_num   = 0;

            # Parse ranges and options
            foreach $_ (split(/\s+/, $range)) {

                # Set option/modifier
                if ((my $opt) = m/^\+([a-z]+)$/) {
                    if    ($opt eq "shift") { $set_shift = 1; }
                    elsif ($opt eq "altgr") { $set_altgr = 1; }
                    elsif ($opt eq "num")   { $set_num   = 1; }
                    elsif ($opt eq "ext")   { $ext_flags = 1; }
                    else {
                        die "$filename: $.: ERROR: "
                          . "Invalid set option\n";
                    }
                }

                # Clear option/modifier
                elsif ((my $opt) = m/^-([a-z]+)$/) {
                    if    ($opt eq "shift") { $clear_shift = 1; }
                    elsif ($opt eq "altgr") { $clear_altgr = 1; }
                    elsif ($opt eq "num")   { $clear_num   = 1; }
                    else {
                        die "$filename: $.: ERROR: "
                          . "Invalid clear option\n";
                    }
                }

                # Single scancode
                elsif ((my $scancode) = m/^(0x[0-9A-Fa-f]+)$/) {
                    $scancodes[++$#scancodes] = hex($scancode);
                }

                # Range of scancodes
                elsif ((my $start, my $end) =
                       m/^(0x[0-9A-Fa-f]+)\.\.(0x[0-9A-Fa-f]+)$/) {
                    for (my $i=hex($start); $i<=hex($end); $i++) {
                        $scancodes[++$#scancodes] = $i;
                    }
                }

                # Invalid token
                else {
                    die "$filename: $.: ERROR: "
                      . "Invalid token\n";
                }

            }

            # Parse onto
            if ($onto =~ m/^0x/) {
                $keysyms[0] = hex($onto);
            }
            else {
                foreach my $char (split('',
                                  substr($onto, 1, length($onto)-2))) {
                    my $codepoint = ord($char);
                    if ($codepoint >= 0x0100) {
                        $keysyms[++$#keysyms] = 0x01000000 | $codepoint;
                    }
                    else {
                        $keysyms[++$#keysyms] = $codepoint;
                    }
                }
            }

            # Check mapping
            if ($#keysyms != $#scancodes) {
                die "$filename: $.: ERROR: "
                  . "Keysym and scancode range lengths differ\n";
            }

            # Write keysym/scancode pairs
            for (my $i=0; $i<=$#keysyms; $i++) {

                $content .= "    {"
                         .  " "   . $keysyms[$i] . ","
                         .  " "   . $scancodes[$i];

                # Flags
                if ($ext_flags) {
                    $content .= ", KBD_FLAGS_EXTENDED";
                }
                else {
                    $content .= ", 0";
                }

                # Set requirements
                if ($set_shift && !$set_altgr) {
                    $content .= ", GUAC_KEYSYMS_SHIFT";
                }
                elsif (!$set_shift && $set_altgr) {
                    $content .= ", GUAC_KEYSYMS_ALTGR";
                }
                elsif ($set_shift && $set_altgr) {
                    $content .= ", GUAC_KEYSYMS_SHIFT_ALTGR";
                }
                else {
                    $content .= ", NULL";
                }

                # Clear requirements
                if ($clear_shift && !$clear_altgr) {
                    $content .= ", GUAC_KEYSYMS_ALL_SHIFT";
                }
                elsif (!$clear_shift && $clear_altgr) {
                    $content .= ", GUAC_KEYSYMS_ALTGR";
                }
                elsif ($clear_shift && $clear_altgr) {
                    $content .= ", GUAC_KEYSYMS_ALL_SHIFT_ALTGR";
                }
                else {
                    $content .= ", NULL";
                }

                # Set locks
                if ($set_num) {
                    $content .= ", KBD_SYNC_NUM_LOCK";
                }
                else {
                    $content .= ", 0";
                }

                # Clear locks
                if ($clear_num) {
                    $content .= ", KBD_SYNC_NUM_LOCK";
                }
                else {
                    $content .= ", 0";
                }

                $content .= " },\n";

            }

        }

        # Invalid token
        elsif (m/\S/) {
            die "$filename: $.: ERROR: "
              . "Invalid token\n";
        }

    }
    close INPUT;

    # Header
    my $sym = keymap_symbol($layout_name);
    print OUTPUT
                                                                  "\n"
         . '/* Autogenerated from ' . $filename . ' */'         . "\n"
         . 'static guac_rdp_keysym_desc __' . $sym . '[] = {'   . "\n"
         .      $content
         . '    {0}'                                            . "\n"
         . '};'                                                 . "\n";

    # Desc header
    print OUTPUT                                                   "\n"
          . 'static const guac_rdp_keymap ' . $sym . ' = { '     . "\n";

    # Parent layout (if any)
    if ($parent) {
        print OUTPUT "    &$parent,\n";
    }
    else{
        print OUTPUT "    NULL,\n";
    }

    # Layout name
    print OUTPUT "    \"$layout_name\",\n";

    # Desc footer
    print OUTPUT
            '    __' . $sym.",\n";      

    # FreeRDP layout (if any)
    if ($freerdp) {
        print OUTPUT "    $freerdp	            ";
    }
    else{
        print OUTPUT "    0";
    }

    print OUTPUT "\n };";

    $keymaps[++$#keymaps] = $sym;
    print STDERR "Added: $layout_name\n";

}

print OUTPUT                                                   "\n"
      . 'const guac_rdp_keymap* GUAC_KEYMAPS[] = {'          . "\n";

foreach my $keymap (@keymaps) {
    print OUTPUT "    &$keymap,\n";
}
print OUTPUT
        '    NULL'                                           . "\n"
      . '};'                                                 . "\n";

close OUTPUT;

