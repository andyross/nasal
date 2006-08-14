# Returns the specified object in (mostly) valid Nasal syntax (string
# escaping is not done properly).  The ttl parameter specifies a
# maximum reference depth.
var dump = func(o, ttl=16) {
    var ot = typeof(o);
    if(ot == "scalar") { return num(o)==nil ? sprintf("'%s'", o) : o; }
    elsif(ot == "nil") { return "nil"; }
    elsif(ot == "vector" and ttl >= 0) {
        var result = "[ ";
	forindex(i; o)
	    result ~= (i==0 ? "" : ", ") ~ dump(o[i], ttl-1);
        return result ~ " ]";
    } elsif(ot == "hash" and ttl >= 0) {
        var ks = keys(o);
        var result = "{ ";
        forindex(i; ks)
            result ~= (i==0?"":", ") ~ ks[i] ~ " : " ~ dump(o[ks[i]], ttl-1);
        return result ~ " }";
    } else return sprintf("<%s>", ot)
}

