import("regex");
import("utf8");

# A featureful, namespace-aware XML parser written in a surprisingly
# small amount of Nasal.  This implements the full XML 1.0
# specification (http://www.w3.org/TR/REC-xml/) with the following
# exceptions:
#
# + The parser is non-validating, so DOCTYPE is ignored.
# + Internal DTD declarations are not supported (the grammar is rather
#   stupidly recursive and would add significant complexity for such a
#   minor and siused feature) and will cause a parse error.
# + The only encoding supported is UTF8 (and ASCII, which is a proper
#   subset).  Actually, any encoding that exposes the markup
#   characters (<>'"=) unambiguously will parse successfully, but
#   non-ascii numeric entity references (&#...;) will be emitted only
#   in UTF8.

# The Tag class implements a DOM-like parse tree.  Simply call
# xml.parse() with your string and inspect the returned Tag.

parse = func(str) {
    var tp = TagParser.new();
    tp.feed(str);
    return tp.tag();
}

Tag = {};

# The tag name (no namespace prefix)
Tag.name = func { me._name };

# The fully qualified (!) namespace for a Tag
Tag.namespace = func { me._ns };

# Retrieves the specified attribute, or nil.
Tag.attr = func(name, ns="") {
    if(contains(me._att, ns) and contains(me._att[ns], name))
	return me._att[ns][name];
    return nil;
};

# Returns all attributes of a tag as a list of [name, ns] pairs
Tag.attrs = func {
    var result = [];
    foreach(ns; keys(me._atts))
	foreach(name; keys(me._atts[ns]))
	    append(result, [name, ns]);
    return result;
};

# Returns a Tag object parent, or nil
Tag.parent = func { me._parent };

# Returns the child with the specified index.  Even indices (0, 2,
# ...) always return string objects representing (possibly
# zero-length) text.  Odd indices (1, 3...) are always Tag objects.
Tag.child = func(idx) {
    return idx >= 0 and idx < size(me._children) ? me._children[idx] : nil;
};

# Finds and returns the first (!) child of the specified name, or nil.
Tag.tag = func(name, ns=nil){};

# Trims whitespace from the beggining and end of the first (!) text
# child (i.e. tag.child(0)) and returns it, or the empty string.
Tag.text = func {
    if(!size(me._children)) return "";
    if(me._wsRE == nil) me._wsRE = regex.new('\s*(.*)\s*');
    me._wsRE.match(me._children[0]);
    return _wsRE.group(1);
};

# The xml.Parser interface is a lower-level callback-based parser
# (similar to expat, for example).  It does not do namespace
# processing, and merely exposes the tag and attribute names
# directly. Set it up with a handler object implementing the following
# methods:
#
#    open(tagname, attr_hash)
#    text(string)
#    close(tagname)
#    proc(name, string)  (for processing instructions: <?name string?>)
#
# Then call feed() to send it strings as you read them from the XML
# input.  Call done() at the end to check that parsing has completed
# successfully and that all the input data has been handled.
Parser = {};

Parser.new = func(handler) {
    var p = { parents : [Parser] };
    p.reset(handler);

    p.textRE = regex.new('^([^<]+)');
    p.commentRE = regex.new('^<!--(?:[^-]|(?:-[^-])|(?:--[^>]))*-->');
    p.procRE = regex.new('^<\?([^\s]+)\s+((?:[^?]|\?[^>])*)\?>');
    p.cdataRE = regex.new('^<!\[CDATA\[((?:[^\]]|][^\]]|]][^>])*)]]>');
    p.idtdRE = regex.new('^<!DOCTYPE[^>]*\[');
    p.doctypeRE = regex.new('^<!DOCTYPE\s[^[>]*>');
    p.openRE = regex.new('^<([^!?][^>/\s]*)\s*');
    p.closeRE = regex.new('^</([^>/\s]+)>');
    p.sqRE = regex.new("^\s*([^\s=]+)\s*=\s*'([^']*)'\s*");
    p.dqRE = regex.new('^\s*([^\s=]+)\s*=\s*"([^"]*)"\s*');
    p.endstartRE = regex.new('^\s*>');
    p.endcloseRE = regex.new('^\s*/>');
    p.entityRE = regex.new('&((?:#x?[a-fA-F0-9]+)|(?:[^&;]+));');
    p.newlineRE = regex.new('\r\n|\r([^\n])');
    return p;
};

Parser.reset = func(handler=nil) {
    if(handler != nil)
	me.handler = handler;
    me.unparsed = "";
    me.text = "";
    me.attrs = nil;
    me.stack = [];
}

Parser.done = func {
    size(me.unparsed) == 0 and size(me.stack) == 0 and size(me.text) == 0;
}

Parser._closeTag = func(tag) {
    if(size(me.stack) == 0)
	die(sprintf("unmatched close tag: </%s>", tag));
    elsif(tag != me.stack[-1])
	die(sprintf("mismatched tags: <%s> vs. </%s>", me.stack[-1], tag));
    me.handler.close(tag);
    pop(me.stack);
}

Parser.feed = func(str) {
    var src = me.unparsed ~ me.newlineRE.sub_all(str, "\n$1");
    var src = me.unparsed ~ str;
    var usz = size(src);
    var next = 0;
    var m = func(re) {
	if(re.match(src, next)) { next = re.next(); return 1; }
	return 0;
    }
    while(next < usz) {
	#print("loop: ", substr(src, next), "\n");

	if(me.attrs != nil) {
	    if(m(me.sqRE)) {
		me.attrs[me.sqRE.group(1)] = me.decode(me.sqRE.group(2));
	    } elsif(m(me.dqRE)) {
		me.attrs[me.dqRE.group(1)] = me.decode(me.dqRE.group(2));
	    } elsif(m(me.endstartRE)) {
		me.handler.open(me.stack[-1], me.attrs);
		me.attrs = nil;
	    } elsif(m(me.endcloseRE)) {
		me.handler.open(me.stack[-1], me.attrs);
		me.attrs = nil;
		me._closeTag(me.stack[-1]);
	    } else break;
	} elsif(m(me.textRE)) {
	    me.text ~= me.decode(me.textRE.group(1));
	} elsif(m(me.openRE)) {
	    # Push out any text we have accumulated and reset the attrs
	    me.handler.text(me.text);
	    me.text = "";
	    me.attrs = {};
	    append(me.stack, me.openRE.group(1));
	} elsif(m(me.closeRE)) {
	    me.handler.text(me.text);
	    me.text = "";
	    me._closeTag(me.closeRE.group(1));
	} elsif(m(me.commentRE)) {
	    # Noop, just eat it
	} elsif(m(me.procRE)) {
	    me.handler.proc(me.procRE.group(1), me.procRE.group(2));
	} elsif(m(me.cdataRE)) {
	    me.text ~= me.cdataRE.group(1);
	} elsif(m(me.idtdRE)) {
	    die("Internal DTDs not supported");
	} elsif(m(me.doctypeRE)) {
	    # Another noop, as we don't validate
	} else
	    break; # nothing matched, give up and wait for more data
    }
    me.unparsed = substr(src, next);
}

var _ents = { "amp":"&", "gt":">", "lt":"<", "apos":"'", "quot":'"' };
entity = func(e) {
    if(find("#x", e) == 0) {
	return utf8.chstr(call(compile("0x"~substr(e, 2))));
    } elsif(find("#", e) == 0) {
	return utf8.chstr(call(compile(substr(e, 1))));
    } elsif(contains(_ents, e))
	return _ents[e];
    return "";
}

# TagParser does the work of using a Parser to build a Tag tree.
var TagParser = { parents : [Parser] };

var _newtag = func {
    { parents : [Tag], _name : nil, _att : {}, _parent : nil, _children : [] };
}

TagParser.new = func {
    var tp = Parser.new();
    tp.parents = [TagParser];
    tp.reset(tp);
    tp._curr = tp._top = _newtag;
    tp._result = nil;
    return tp;
}

TagParser.tag = func {
    if(!me.done()) return nil;
    if(me._result == nil) me._result = me.fixup(me._top.child(1));
    return me._result;
};

TagParser.open = func(tag, attrs) {
    var t = _newtag();
    t._name = tag;
    t._att[""] = attrs;
    t._parent = me._curr;
    append(me._curr._children, t);
    me._curr = t;
}
TagParser.close = func(tag) { me._curr = me._curr._parent; }
TagParser.text = func(text) { append(me._curr.children, text); }
TagParser.proc = func{}; # noop

#
# FIXME: decode/encode should be cleaned up and exported outside the
# Parser class.
#
Parser.decode = func(str) { me.entityRE.sub_all(str, "${entity(_1)}") }

var _ents2 =
{ "&":"&amp;", ">":"&gt;", "<":"&lt;", "'":"&apos;", '"':"&quot;" };
var encode = func(str) { regex.sub_all('([&<>"\'])', "${_ents2[_1]}", str); }

########################################################################

import("io");
import("bits");

var readfile = func(file) {
    var sz = io.stat(file)[7];
    var buf = bits.buf(sz);
    io.read(io.open(file), buf, sz);
    return buf;
}

h = {};
h.open = func(tag, attrs) {
    print("\n<", tag);
    foreach(a; keys(attrs))
	print(sprintf(" %s='%s'", a, attrs[a]));
    print(">\n");
}
h.close = func(tag) {
    print("</", tag, ">\n");
}
h.proc = func(type, arg) {
    print(sprintf("PROC: '%s' '%s'\n", type, arg));
}
h.text = func(text) { print("TEXT: ", text); }

var p = Parser.new(h);
foreach(f; arg) {
    var SZ = 256;
    var buf = readfile(f);
    print(encode(buf), "\n\n\n");
    p.feed(buf);
#    for(var i=0; size(buf)-i >= SZ; i+=SZ)
#	p.feed(substr(buf, i, SZ));
#    p.feed(substr(buf, i));
}
