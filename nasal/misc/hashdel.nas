key = 0;
val = "dummy string";
hash = {};
hash[key] = val;
while(key < 10000) {
    size0 = size(hash);
    key = key + 1;
    hash[key] = val;
    delete(hash, key-1);
    size1 = size(hash);
    if(size0 != size1) {
        print("ERROR on key ", key, ", ", size0, " != ", size1, "\n");
        foreach(k; keys(hash)) {
            print("  ", k, " => ", hash[k], "\n");
        }
    }
}
