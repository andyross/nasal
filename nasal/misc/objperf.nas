# Generate a big array of objects and run through it doing (mostly)
# field access.

REPS = 10;
COUNT = 65536;

print("Initializing...\n");
v = [];
for(i=0; i<COUNT; i=i+1) { append(v, {}); }


print("Starting test\n");
for(rep=0; rep<REPS; rep=rep+1) {
    print(rep, "...\n");
    for(i=0; i<COUNT; i=i+1) {
        obj = v[i];
        obj.a = i;
        obj.b = i;
        obj.c = i;
        obj.d = i;
    }
    for(i=0; i<COUNT; i=i+1) {
        obj = v[i];
        if(obj.a != i) { print("Ack!\n"); return; }
        if(obj.b != i) { print("Ack!\n"); return; }
        if(obj.c != i) { print("Ack!\n"); return; }
        if(obj.d != i) { print("Ack!\n"); return; }
    }
}

## An identical perl script.  The Nasl code runs about 1.5x slower on
## my machine.

# my $REPS = 10;
# my $COUNT = 65536;
# 
# print "Initializing...\n";
# my @v = ();
# for(my $i=0; $i<$COUNT; $i++) { push @v, {}; }
# 
# print "Starting test\n";
# for(my $rep=0; $rep<$REPS; $rep++) {
#     print "$rep...\n";
#     for(my $i=0; $i<$COUNT; $i++) {
# 	my $obj = $v[$i];
# 	$obj->{a} = i;
# 	$obj->{b} = i;
# 	$obj->{c} = i;
# 	$obj->{d} = i;
#     }
#     for(my $i=0; $i<$COUNT; $i++) {
# 	my $obj = $v[$i];
# 	if($obj->{a} != i) { die "Ack!"; }
# 	if($obj->{b} != i) { die "Ack!"; }
# 	if($obj->{c} != i) { die "Ack!"; }
# 	if($obj->{d} != i) { die "Ack!"; }
#     }
# }
