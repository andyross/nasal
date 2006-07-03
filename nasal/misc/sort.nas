# Usage: sort(vector, [less-than-function])
# Stable sort.  Does not modify the input array.  Returns a new array,
# and uses an equal amount of space in overhead.  Creates a lot of
# garbage objects -- about 2 for every element in the input list.
sort = func(vec, lessthan=func(a,b){a<b}) {
    vappend = func(a, b) { foreach(elem; b) { append(a, elem); } }
    _sort = func(vec) {
        n = size(vec);
        if(n <= 1) { return vec; }
        pn = int(n / 2);
        pivot = vec[pn];
        a = []; b = [];
        for(i=0; i<n; i=i+1) {
            if(i == pn) { continue; }
            elem = vec[i];
            if(lessthan(vec[i], pivot)) { append(a, elem); }
            else { append(b, elem); }
        }
        vec = _sort(a);
        append(vec, pivot);
        vappend(vec, _sort(b));
        return vec;
    }
    _sort(vec);
}
    
# Quicksort implementation.  Much faster: no excess garbage to
# collect.  Unstable sort.  Sorts the input array in place.  No space
# overhead.
qsort = func(vec, lessthan=func(a,b){a<b}) {
    var swap = func(a, b) { var tmp = vec[a]; vec[a] = vec[b]; vec[b] = tmp; }
    _qsort = func(lo, hi) {
        if(lo >= hi) { return }
        var lo0 = lo; var hi0 = hi;
        var pn = int((lo + hi) / 2);
        var pivot = vec[pn];
        while(lo < hi) {
            if(lessthan(vec[lo], pivot)) { lo = lo + 1; }
            elsif(lessthan(pivot, vec[hi])) { hi = hi - 1; }
            else {
                if(lo == pn) { pn = hi; }
                elsif(hi == pn) { pn = lo; }
                swap(lo, hi);
            }
        }
        var mid = lessthan(vec[lo], pivot) ? lo + 1 : lo;
        swap(mid, pn);
        _qsort(lo0, mid-1);
        _qsort(mid+1, hi0);
    }
    _qsort(0, size(vec) - 1);
    return vec;
}

##
## Test Code
##

array = [99, 234, 78, 18, 34, 28, 201, 3, 120, 81, 169, 219, 222, 115, 217,
         83, 86, 185, 69, 21, 7, 237, 15, 66, 172, 240, 154, 35, 235, 216];
array2 = sort(array);
qsort(array);

if(size(array) != size(array2)) {
    die("Size mismatch!\n");
}

for(i=0; i<size(array); i=i+1) {
    #print(array[i], " ", array2[i], "\n");
    if(array[i] != array2[i]) {
	die("[Mismatch!]\n");
    }
    if(i > 0) {
	if(array2[i] < array2[i-1]) {
            die("[sort error!]\n");
	}
    }
}
