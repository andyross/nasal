eval = func {
    fn = arg[0];
    if(typeof(fn) == "scalar") { fn = compile(fn); }
    return call(fn, []);
}

outer = "blah";
print("outer = ", outer, "\n");

f1 = func {
    print("[call] arg[0] = ", arg[0], "\n");
    print("[call] outer = ", outer, "\n");
}
call(f1, [1]);
outer = "blee";
call(f1, [2]);

eval('print("[eval] outer = ", outer, "\n");');
outer = "bloo";
eval('print("[eval] outer = ", outer, "\n");');

eval('call(f1, ["eval -> call"]);');

f2 = func { eval('print("Inside f2!\n");'); }
call(f2, []);

f3 = func { 3 };
print("f3() = ", f3(), "\n");
print("[call] f3() = ", call(f3, []), "\n");


# A Lisp-like map function.
map = func {
    fun = arg[0]; list = arg[1];
    result =  [];
    for(i=0; i<size(list); i=i+1) {
        append(result, call(fun, [list[i]]));
    }
    return result;
}

list = [1, 2, 3, 4, 5];
print("Before map():");
foreach(elem; list) { print(" ", elem); }
list = map(func { arg[0] + 100 }, list);
print("\nAfter map():");
foreach(elem; list) { print(" ", elem); }
print("\n");

fn = func { print("a = ", a, "\n"); var b = a; }
hash = { a : 2 };
call(fn, nil, nil, hash);
hash.a += 1;
call(fn, nil, nil, hash);
hash.a += 1;
call(fn, nil, nil, hash);
hash.a += 1;
call(fn, nil, nil, hash);
print("hash.b = ", hash.b, "\n");
