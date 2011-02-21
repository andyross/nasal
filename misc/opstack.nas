# Exercise the operand stack by doing recursion inside of nested expressions.

opstack = func {
    if(arg[0] == 0) {
        return 0;
    } else {
        return 0+(0+(0+(0+(0+(0+(0+(0+(0+(0+(0+(0+opstack(arg[0] - 1))))))))))));
    }
}
    
for(i=0; i<1024; i=i+1) {
    print(i, "...\n");
    opstack(i);
}
