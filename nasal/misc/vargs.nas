tst1 = func() { return 0; }
tst2 = func(a) { return a; }
tst3 = func(a, b) { return a + b; }
tst4 = func(a, b, c) { return a + b + c; }

print("tst1() = ", tst1(1, 2, 3), "\n");
print("tst2() = ", tst2(1, 2, 3), "\n");
print("tst3() = ", tst3(1, 2, 3), "\n");
print("tst4() = ", tst4(1, 2, 3), "\n");

vargs = func(a, b, rest...) {
    print("vargs a = ", a, ", b = ", b);
    print(" rest = [ ");
    foreach(e; rest) { print(e, " "); }
    print("]\n");
}

vargs(1, 2, 3, 4, 5, 6, 7);

tst5 = func(a, b=123, c...)
{
    print("a = ", a, " b = ", b, " size(c) = ", size(c), "\n");
}

tst5(1, 2);
