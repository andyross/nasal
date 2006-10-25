# Simple CGI handling for the mod_nasal apache module.

# Parses a CGI-style "key=val&key2=val2..." string and returns a
# corresponding hash
var parse = func(s) {
    var result = {};
    foreach(arg; split("&", s)) {
        if((var eq = find("=", arg)) >= 0) {
            var key = urldec(substr(arg, 0, eq));
            var val = urldec(substr(arg, eq+1));
            result[key] = val;
        }
    }
    return result;
}

# Returns a hash containing only the query string CGI arguments
var query = func { parse(getcgi("QUERY_STRING")) }

# Returns a hash containing only the POST CGI arguments
var post = func {
    var data = "";
    while((var s = read()) != nil) data ~= s;
    return parse(data);
}

# Returns a hash containing all CGI arguments; the query string takes
# precedence over the POST data.
var args = func {
    var result = post();
    var q = query();
    foreach(k; keys(q)) result[k] = q[k];
    return result;
}

# Trims and returns whitespace from substr(s, a, b-a)
var _trim = func(s, a, b) {
    while(b > a and s[b-1] == ` `) b -= 1;
    while(a < b and s[a] == ` `) a += 1;
    return substr(s, a, b-a);
}

# Note: this parser doesn't implement the full syntax from RFC2109
var _parse_cookies = func(chdr) {
    result = {};
    if(chdr == nil) return result;
    foreach(cs; split(";", chdr)) {
	if((var eq = find("=", cs)) >= 0) {
	    var key = _trim(cs, 0, eq);
	    var val = _trim(cs, eq+1, size(cs));
	    result[key] = val;
	}
    }
    return result;
}

# Returns the specified cookie, or a hash of all cookies
var cookie = func(req=nil) {
    var result = _parse_cookies(gethdr("Cookie"));
    return req == nil ? result : (contains(result, req) ? result[req] : nil);
}
