var abs = func(n) { n < 0 ? -n : n }

var pow = func(x, y) { exp(y * ln(x)) }

var mod = func(n, m) {
    var x = n - m * int(n/m);      # int() truncates to zero, not -Inf
    return x < 0 ? x + abs(m) : x; # ...so must handle negative n's
}

var asin = func(y) { atan2(y, sqrt(1-y*y)) }

var acos = func(x) { atan2(sqrt(1-x*x), x) }

var _iln10 = 1/ln(10);
var log10 = func(x) { ln(x) * _iln10; }
