# Generate a big array of objects and run through it doing (mostly)
# field access.

REPS = 10;
COUNT = 16384;

print("Initializing...\n");
v = [];
for(i=0; i<COUNT; i=i+1) { append(v, {}); }


print("Starting test\n");
for(rep=0; rep<REPS; rep=rep+1) {
    print(rep, "...\n");
    for(i=0; i<COUNT; i=i+1) {
        obj = v[i];
        obj.fielda = i;
        obj.fieldb = i;
        obj.fieldc = i;
        obj.fieldd = i;
    }
    for(i=0; i<COUNT; i=i+1) {
        obj = v[i];
        if(obj.fielda != i) { print("Ack!\n"); return; }
        if(obj.fieldb != i) { print("Ack!\n"); return; }
        if(obj.fieldc != i) { print("Ack!\n"); return; }
        if(obj.fieldd != i) { print("Ack!\n"); return; }
        
        if(obj.fielda != i) { print("Ack!\n"); return; }
        if(obj.fieldb != i) { print("Ack!\n"); return; }
        if(obj.fieldc != i) { print("Ack!\n"); return; }
        if(obj.fieldd != i) { print("Ack!\n"); return; }
        
        if(obj.fielda != i) { print("Ack!\n"); return; }
        if(obj.fieldb != i) { print("Ack!\n"); return; }
        if(obj.fieldc != i) { print("Ack!\n"); return; }
        if(obj.fieldd != i) { print("Ack!\n"); return; }
        
        if(obj.fielda != i) { print("Ack!\n"); return; }
        if(obj.fieldb != i) { print("Ack!\n"); return; }
        if(obj.fieldc != i) { print("Ack!\n"); return; }
        if(obj.fieldd != i) { print("Ack!\n"); return; }
        
        if(obj.fielda != i) { print("Ack!\n"); return; }
        if(obj.fieldb != i) { print("Ack!\n"); return; }
        if(obj.fieldc != i) { print("Ack!\n"); return; }
        if(obj.fieldd != i) { print("Ack!\n"); return; }
        
        if(obj.fielda != i) { print("Ack!\n"); return; }
        if(obj.fieldb != i) { print("Ack!\n"); return; }
        if(obj.fieldc != i) { print("Ack!\n"); return; }
        if(obj.fieldd != i) { print("Ack!\n"); return; }
        
        if(obj.fielda != i) { print("Ack!\n"); return; }
        if(obj.fieldb != i) { print("Ack!\n"); return; }
        if(obj.fieldc != i) { print("Ack!\n"); return; }
        if(obj.fieldd != i) { print("Ack!\n"); return; }
    }
}

## An identical perl script.  The Nasal code runs about 2x slower on
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
# 	$obj->{fielda} = $i;
# 	$obj->{fieldb} = $i;
# 	$obj->{fieldc} = $i;
# 	$obj->{fieldd} = $i;
#     }
#     for(my $i=0; $i<$COUNT; $i++) {
# 	my $obj = $v[$i];
# 	if($obj->{fielda} != $i) { die "Ack!"; }
# 	if($obj->{fieldb} != $i) { die "Ack!"; }
# 	if($obj->{fieldc} != $i) { die "Ack!"; }
# 	if($obj->{fieldd} != $i) { die "Ack!"; }
#     }
# }
