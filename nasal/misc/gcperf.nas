
for(j=0; j<10; j=j+1) {
    #print(j, "/10\n");

    # Make some tables to store stuff.  This will clobber the contents
    # from the previous loop, making them available for garbage
    # collection.
    v = [];   h1 = {};   h2 = {};

    # Fill them
    for(i=0; i<65536; i=i+1) {
        str = "i" ~ i;
        append(v, str);
        h1[str] = i;
        h2[i] = [str];
    }

    # Check that we get back what we put in
    for(i=0; i<65536; i=i+1) {
        if(v[i] != h2[h1[v[i]]][0]) {
            print("Ack!\n");
            return;
        }
    }
}

## As an interesting comparison, here is an essentially identical
## script written in perl.  On my machine, it is almost twice as slow
## as the Nasal variant.  Presumably, the difference is in perl's
## reference counter overhead.  In tests of pure (non-allocating)
## interpretation speed, perl typically beats Nasal's bytecode
## interpreter by 50% or so.

# for(my $j=0; $j<10; $j++) {
#     print "$j/100\n";
#     my @v = ();
#     my %h1 = ();
#     my %h2 = ();
#     for(my $i=0; $i<65536; $i++) {
#         my $str = "i$i";
#         push @v, $str;
#         $h1{$str} = $i;
#         $h2{$i} = [$str];
#     }
#     for(my $i=0; $i<65536; $i++) {
#         die if $v[$i] ne $h2{$h1{$v[$i]}}->[0];
#     }
# }


