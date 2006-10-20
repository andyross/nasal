# Simple CGI handling for the mod_nasal apache module

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
