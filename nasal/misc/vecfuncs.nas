#
# Regression test for the vector setsize() and subvec() functions
#

v1 = [];
setsize(v1, 100);
if(size(v1) != 100) { print("wrong v1 size: ", size(v1)); }
for(i=0; i<100; i=i+1) {
    if(v1[i] != nil) { print("error ", i, "\n"); }
    v1[i] = i + 100;
}

# Clone it
v2 = subvec(v1, 0);
for(i=0; i<100; i=i+1) {
    if(v1[i] != i + 100) { print("v1 error ", i, " ", v1[i],"\n"); }
    if(v2[i] != i + 100) { print("v2 error ", i, " ", v2[i],"\n"); }
}

# Truncate
setsize(v2, 50);
if(size(v2) != 50) { print("wrong size: ", size(v2), "\n"); }
for(i=0; i<50; i=i+1) {
    if(v1[i] != v2[i]) { print("truncate error ", i, " ", v1[i],"\n"); }
}

# Expand again
setsize(v2, 100);
for(i=50; i<100; i=i+1) {
    if(v2[i] != nil) { print("expand error ", i, " ", v2[i],"\n"); }
}

# To nothing...
setsize(v2, 0);
if(size(v2) != 0) { print("wrong size: ", size(v2), "\n"); }

# Equivalent of shift()
v2 = subvec(v1, 1);
if(size(v2) != 99) { print("wrong v2 size: ", size(v2)); }
for(i=0; i<99; i=i+1) {
    if(v2[i] != v1[i+1]) { print("v2 error ", i, " ", v2[i],"\n"); }
}

# Last element
v2 = subvec(v1, 99);
if(size(v2) != 1) { print("wrong v2 size: ", size(v2)); }
if(v2[0] != v1[99]) { print("v2 error ", i, " ", v2[i],"\n"); }

# Middle, single element
v2 = subvec(v1, 50, 1);
if(size(v2) != 1) { print("wrong v2 size: ", size(v2)); }
if(v2[0] != v1[50]) { print("v2 error ", i, " ", v2[i],"\n"); }

# Middle block
v2 = subvec(v1, 50, 10);
if(size(v2) != 10) { print("wrong v2 size: ", size(v2)); }
for(i=0; i<10; i=i+1) {
    if(v2[i] != v1[50+i]) { print("v2 error ", i, " ", v2[i],"\n"); }
}
