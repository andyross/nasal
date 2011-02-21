var ARRAY_SIZE = 1000;
var NVALUES = 10; # how many distinct sortables

var in = [];
for(var i=0; i<ARRAY_SIZE; i+=1)
    append(in, { I : i, val : int(rand() * NVALUES) });

out = sort(in, func(a,b){a.val - b.val});

foreach(e; out) {
    print(sprintf("I %d val %d\n", e.I, e.val));
}
