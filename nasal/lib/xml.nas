import("regex");
import("utf8");

# A featureful, namespace-aware XML parser written in a surprisingly
# small amount of Nasal.  This implements the full XML 1.0
# specification (http://www.w3.org/TR/REC-xml/) with the following
# exceptions:
#
# + The parser is non-validating, so DOCTYPE is ignored.
# + Internal DTD definitions are not supported (the grammar is
#   infuriatingly recursive and would add significant complexity for
#   such a minor and rarely used feature) and will cause a parse
#   error.
# + The only encoding supported is UTF8 (and ASCII, which is a proper
#   subset).  Actually, any encoding that exposes the markup
#   characters (<>'"=) unambiguously will parse successfully, but
#   non-ascii numeric entity references (&#...;) will be emitted only
#   in UTF8.
#
# Public functions:
#  parse(string)      Parses the XML string and returns a Tag object
#  decode(string)     Interprets XML entities and returns the decoded string
#  encode(string)     Substutes XML entities as needed and returns the
#                     modified string.
#  newparser(handler) Creates a new Parser object (see below) with the
#                     specified handler.
#
# The "Tag" class implements a DOM-like tree returned from
# xml.parse():
#
# name():            returns the name (w/o namespace prefix) of the tag
# namespace():       the fully qualified namespace URI of the tag
# attr(name, ns?):   the specified attribute, or nil
# attrs():           all attributes in a list of [name, namespace] pairs.
#                    Note that the namespace strings are the fully
#                    qualified URI strings, and NOT the abbreviations
#                    used in the code.  The "raw" name of the tag
#                    (e.g. "myns:tagname") appears in the list with a
#                    namespace of "" (the empty string, which itself
#                    is never a legal Namspace URI).
# parent():          the Tag's parent node, or nil
# child(index):      the specified child, or nil.  Even children (0, 2,
#                    etc...) are always text strings.  Odd indices refer
#                    to Tag objects.
# tag(name, ns?):    the first sub-tag with the specified name
# tags(name, ns?):   list of all tags with the specified name
# text():            the first text node, without leading/trailing whitespace

# The Parser interface is a lower-level callback-based parser
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

########################################################################
# Exported functions

var encode = func(str) { _entsubRE.clone().sub(str, "${_ents2[_1]}", 1); }

var decode = func(s) {
    var entchr = func(e) {
	if(find("#x", e) == 0) {
	    return utf8.chstr(call(compile("0x"~substr(e, 2))));
	} elsif(find("#", e) == 0) {
	    return utf8.chstr(call(compile(substr(e, 1))));
	} elsif(contains(_ents, e))
	    return _ents[e];
	return "";
    }
    _entityRE.clone().sub(s, "${entchr(_1)}", 1);
}

var parse = func(str) {
    var tp = TagParser.new();
    tp.feed(str);
    return tp.top();
}

########################################################################

Parser = {};

newparser = func(handler=nil) {
    var p = { parents : [Parser] };
    p.reset(handler);
    p.textRE = _textRE.clone();
    p.commentRE = _commentRE.clone();
    p.procRE = _procRE.clone();
    p.cdataRE = _cdataRE.clone();
    p.idtdRE = _idtdRE.clone();
    p.doctypeRE = _doctypeRE.clone();
    p.openRE = _openRE.clone();
    p.closeRE = _closeRE.clone();
    p.sqRE = _sqRE.clone();
    p.dqRE = _dqRE.clone();
    p.endstartRE = _endstartRE.clone();
    p.endcloseRE = _endcloseRE.clone();
    p.newlineRE = _newlineRE.clone();
    return p;
};

Parser.reset = func(handler) {
    me.handler = handler;
    me.unparsed = "";
    me.txt = "";
    me.attrs = nil;
    me.stack = [];
}

Parser.done = func { !size(me.unparsed) and !size(me.stack) and !size(me.txt); }

Parser._closeTag = func(tag) {
    if(size(me.stack) == 0)
	die(sprintf("unmatched close tag: </%s>", tag));
    elsif(tag != me.stack[-1])
	die(sprintf("mismatched tags: <%s> vs. </%s>", me.stack[-1], tag));
    me.handler.close(tag);
    pop(me.stack);
}

Parser.feed = func(str) {
    var src = me.unparsed ~ me.newlineRE.sub(str, "\n$1", 1);
    var src = me.unparsed ~ str;
    var usz = size(src);
    var next = 0;
    var m = func(re) {
	if(re.match(src, next)) { next = re.next(); return 1; }
	return 0;
    }
    while(next < usz) {
	if(me.attrs != nil) {
	    if(m(me.sqRE)) {
		me.attrs[me.sqRE.group(1)] = decode(me.sqRE.group(2));
	    } elsif(m(me.dqRE)) {
		me.attrs[me.dqRE.group(1)] = decode(me.dqRE.group(2));
	    } elsif(m(me.endstartRE)) {
		me.handler.open(me.stack[-1], me.attrs);
		me.attrs = nil;
	    } elsif(m(me.endcloseRE)) {
		me.handler.open(me.stack[-1], me.attrs);
		me.attrs = nil;
		me._closeTag(me.stack[-1]);
	    } else break;
	} elsif(m(me.textRE)) {
	    me.txt ~= decode(me.textRE.group(1));
	} elsif(m(me.openRE)) {
	    # Push out any text we have accumulated and reset the attrs
	    me.handler.text(me.txt);
	    me.txt = "";
	    me.attrs = {};
	    append(me.stack, me.openRE.group(1));
	} elsif(m(me.closeRE)) {
	    me.handler.text(me.txt);
	    me.txt = "";
	    me._closeTag(me.closeRE.group(1));
	} elsif(m(me.commentRE)) {
	    # Noop, just eat it
	} elsif(m(me.procRE)) {
	    me.handler.proc(me.procRE.group(1), me.procRE.group(2));
	} elsif(m(me.cdataRE)) {
	    me.txt ~= me.cdataRE.group(1);
	} elsif(m(me.idtdRE)) {
	    die("Internal DTDs are not supported");
	} elsif(m(me.doctypeRE)) {
	    # Another noop, as we don't validate
	} else
	    break; # nothing matched, give up and wait for more data
    }
    me.unparsed = substr(src, next);
}

########################################################################

Tag = {};

Tag.name = func { me._name };

Tag.namespace = func { me._ns };

Tag.attr = func(name, ns="") {
    (me._att[ns] != nil) ? me._att[ns][name] : nil;
};

Tag.attrs = func {
    var result = [];
    foreach(ns; keys(me._att))
	foreach(name; keys(me._att[ns]))
	    append(result, [name, ns]);
    return result;
};

Tag.parent = func { me._parent };

Tag.child = func(idx) {
    return idx >= 0 and idx < size(me._children) ? me._children[idx] : nil;
};

Tag.tag = func(name, ns=nil) {
    for(var i=1; (var t = me.child(i)) != nil; i += 2)
	if(t.name() == name)
	    return t;
};

Tag.tags = func(name, ns=nil) {
    var result = [];
    for(var i=1; (var t = me.child(i)) != nil; i += 2)
	if(t.name() == name)
    	    append(result, t);
    return result;
}

Tag.text = func {
    if(!size(me._children)) return "";
    (var wsRE = _wsRE.clone()).match(me._children[0]);
    return wsRE.group(1);
};

########################################################################

var TagParser = { parents : [Parser] };

var _newtag = func {
    { parents : [Tag], _name : "", _att : {}, _parent : nil, _children : [] };
}

TagParser.new = func {
    var tp = newparser();
    tp.parents = [TagParser];
    tp.reset(tp);
    tp._curr = tp._top = _newtag();
    tp._result = nil;
    return tp;
}

TagParser.top = func {
    if(!me.done()) return nil;
    if(me._top.child(1) == nil or me._top.child(3) != nil)
	die("XML parse error");
    return me._top.child(1);
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
TagParser.text = func(text) { append(me._curr._children, text); }
TagParser.proc = func{}; # noop

# All the regexes in one place.  Feel the line noise!  These get
# cloned as needed; they aren't used directly for threadsafety
# reasons.
var _textRE = regex.new('^([^<]+)');
var _commentRE = regex.new('^<!--(?:[^-]|(?:-[^-])|(?:--[^>]))*-->');
var _procRE = regex.new('^<\?([^\s]+)\s+((?:[^?]|\?[^>])*)\?>');
var _cdataRE = regex.new('^<!\[CDATA\[((?:[^\]]|][^\]]|]][^>])*)]]>');
var _idtdRE = regex.new('^<!DOCTYPE[^>]*\[');
var _doctypeRE = regex.new('^<!DOCTYPE\s[^[>]*>');
var _openRE = regex.new('^<([^!?/][^>/\s]*)\s*');
var _closeRE = regex.new('^</([^>/\s]+)>');
var _sqRE = regex.new("^\s*([^\s=]+)\s*=\s*'([^']*)'\s*");
var _dqRE = regex.new('^\s*([^\s=]+)\s*=\s*"([^"]*)"\s*');
var _endstartRE = regex.new('^\s*>');
var _endcloseRE = regex.new('^\s*/>');
var _newlineRE = regex.new('\r\n|\r([^\n])');
var _wsRE = regex.new('\s*(.*)\s*');
var _entityRE = regex.new('&((?:#x?[a-fA-F0-9]+)|(?:[^&;]+));');
var _entsubRE = regex.new('([&<>"\'])');

# Note numeric "#39" instead of standard "apos" -- MSIE bug workaround
var _ents = { "amp":"&", "gt":">", "lt":"<", "#39":"'", "quot":'"' };
var _ents2 = { "&":"&amp;",">":"&gt;","<":"&lt;","'":"&#39;",'"':"&quot;" };

########################################################################
# Test code:

if(0) {
import("debug");
import("bits");

var fmttag = func(tag) {
    var s = "<" ~ tag.name();
    foreach(var att; tag.attrs()) {
	if(att[1] != "") continue; # Only "raw" no-namespace names
	s ~= sprintf(" %s=\"%s\"", att[0], tag.attr(att[0]));
    }
    return s ~ ">";
}

var dumptag = func(tag, lvl=0) {
    var pref = bits.buf(lvl);
    for(var i=0; i<lvl; i+=1) pref[i] = ` `;
    print(pref, fmttag(tag));
    if(tag.child(1) == nil) {
	print(tag.child(0), "</", tag.name(), ">\n");
    } else {
	print("\n");
	if(tag.child(0) != "")
	    print(pref, " ", tag.child(0), "\n");
	for(i=1; tag.child(i) != nil; i += 2) {
	    dumptag(tag.child(i), lvl+1);
	    if(tag.child(i+1) == nil) die("Need trailing string!");
	    if(tag.child(i+1) != "")
		print(pref, tag.child(i+1), "\n");
	}
	print(pref, "</", tag.name(), ">\n");
    }
}

t = parse('<a b=\'1\' c="2"><d></d><e>Eeee!</e></a>');

dumptag(t);
print(t.tag("e").text(), "\n");

}
