vappend = func {
    a = arg[0]; b = arg[1];
    n = size(b);
    for(i=0; i<n; i=i+1) { append(a, b[i]); }
}

# Usage: sort(vector, [less-than-function])
# Stable sort.  Does not modify the input array.  Returns a new array,
# and uses an equal amount of space in overhead.  Creates a lot of
# garbage objects -- about 2 for every element in the input list.
sort = func {
    lessthan = if(size(arg) > 1) { arg[1] } else { func { arg[0] < arg[1] } };
    _sort = func {
        vec = arg[0];
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
    _sort(arg[0]);
}
    
# Quicksort implementation.  Much faster: no excess garbage to
# collect.  Unstable sort.  Sorts the input array in place.  No space
# overhead.
qsort = func {
    vec = arg[0];
    lessthan = if(size(arg) > 1) { arg[1] } else { func { arg[0] < arg[1] } };
    _qsort = func {
        lo = arg[0]; hi = arg[1];
        if(lo < hi) {
            pn = int((lo + hi) / 2);
            pivot = vec[pn];
            lo0 = lo; hi0 = hi;
            while(lo < hi) {
                if(lessthan(vec[lo], pivot)) { lo = lo + 1; }
                elsif(lessthan(pivot, vec[hi])) { hi = hi - 1; }
                else {
		    if(lo == pn) { pn = hi; }
		    elsif(hi == pn) { pn = lo; }
                    tmp = vec[lo];
                    vec[lo] = vec[hi];
                    vec[hi] = tmp;
                }
            }
            mid = if(lessthan(vec[lo], pivot)) { lo + 1 } else { lo };
            tmp = vec[mid];
            vec[mid] = vec[pn];
            vec[pn] = tmp;
            _qsort(lo0, mid-1);
            _qsort(mid+1, hi0);
        }
    }
    _qsort(0, size(vec) - 1);
}

##
## Test Code
##

array = [99, 234, 78, 18, 34, 28, 201, 3, 120, 81, 169, 219, 222, 115, 217,
         83, 86, 185, 69, 21, 7, 237, 15, 66, 172, 240, 154, 35, 235, 216];
array2 = sort(array);
qsort(array);

if(size(array) != size(array2)) {
    print("Size mismatch!\n");
}

for(i=0; i<size(array); i=i+1) {
    #print(array[i], " ", array2[i], "\n");
    if(array[i] != array2[i]) {
	print("[Mismatch!]\n");
    }
    if(i > 0) {
	if(array2[i] < array2[i-1]) {
	    print("[sort error!]\n");
	}
    }
}
