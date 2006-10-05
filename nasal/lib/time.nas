import("math", "mod");
import("io");
import("bits");

# Portable, general and cross-platform calendar and time zone handling
# written in pure Nasal.  Uses POSIX time zone files directly, no
# dependence on the C library or platform.  You can have a handle for
# a named time zone without hacking at a global TZ variable!
# 
# to_epoch(year, mon, day, hour, min, sec, tz=<local>)
#   Converts the specified date to a numbic time value in the Unix epoch
#   (zero == midnight 1 Jan 1970 GMT).  Year is the calendar year
#   (i.e. 2006), month is one indexed (1 == Jan), as is day (pass in
#   numbers from 1-31, zero is meaningless).  The sec parameter need not
#   be an integer number, fractional seconds are supported.
# 
# from_epoch(epoch, tz=<local>)
#   Returns an object with the following fields:
#     year, mon, day, hour, min, sec: Same meanings as to_epoch above.
#     wday: day of the week, zero-indexed (0 == Sun ... 6 == Sat)
#     tzoff: time zone offset from GMT
# 
# timezone(name=nil)
#   Looks up, loads if necessary, and returns a time zone object
#   representing the specified zone name, or the local time zone if
#   unspecified.

# Returns the integer numbers of days between midnight January 1st,
# 1AD and before the end of the specified AD year.  FIXME: make this
# work for BC years too.  Is year zero (i.e. 1BC) a leap year?
var _ydays = func(y) {
    (y * 365) + int(y/4) - int(y/100) + int(y/400);
}

# Cache this: conversion between AD year values and the unix epoch
var _yd1969 = _ydays(1969);

# Is this a leap year?
var _leap = func(y) {
    if(mod(y, 4) != 0) return 0;
    if(mod(y, 100) != 0) return 1;
    return (mod(y, 400) == 0) ? 1 : 0;
}

var MDAYS = [31, 28, 31, 30,  31, 30, 31, 31, 30, 31, 30, 31];

# Returns the epoch value in days of the specified date.  Note that
# month and day here are ZERO INDEXED here for simplicity.
var _edays = func(year, mon, day) {
    var result = _ydays(year-1) - _yd1969;
    for(var i=0; i<mon; i+=1)
	result += MDAYS[i] + (i==1 ? _leap(year) : 0);
    return result + day;
}

var to_epoch = func(year, mon, day, hh, mm, ss, tz=nil) {
    var e = 60 * (60 * (24 * _edays(year, mon-1, day-1) + hh) + mm) + ss;
    return _tz2gmt(e, tz);
}

var from_epoch = func(epoch, tz=nil) {
    var lepoch = _tz2local(epoch, tz);
    var day = _yd1969 + lepoch / 86400;
    var wday = mod(int(day)+1, 7);
    var year = int(day / 365.2425) - 2;
    while(_ydays(year) <= day) { year += 1; }
    day -= _ydays(year-1);
    forindex(mon; MDAYS) {
	var md = MDAYS[mon] + (mon==1 ? _leap(year) : 0);
	#print(sprintf("day %s mon %s md %s\n", day, mon, md));
	if(day < md) break;
	day -= md;
    }
    var sec = mod(lepoch, 86400);
    var hour = int(sec / 3600);  sec -= 3600 * hour;
    var min = int(sec / 60);     sec -= 60 * min;
    return { year:year, mon:mon+1, day:int(day)+1, wday:wday,
             hour:hour, min:min, sec:sec, tzoff: lepoch-epoch };
}

var _parsetz = func(buf) {
    # FIXME: make this work with bits.sfld()
    var word = func {
	var val = 0; var neg = buf[off] > 127 ? 0x100000000 : 0;
	for(var i=0; i<4; i+=1) { val = val * 256 + buf[off]; off += 1; }
	return val - neg;
    }

    if(substr(buf, 0, 4) != "TZif") die();
    var tz = {};
    var off = 20; # Skip magic and 16 byte "future use" area
    var ttisgmtcnt = word(); var ttisstdcnt = word(); var leapcnt = word();
    var timecnt = word();    var typecnt = word();    var charcnt = word();
    
    tz.times = setsize([], timecnt);
    for(var i=0; i<timecnt; i+=1)
	tz.times[i] = word();
    tz.ttypes = substr(buf, off, timecnt);
    off += timecnt;
    tz.type0 = nil; # Type to use for times before the 1st transition
    tz.types = setsize([], typecnt);
    for(var i=0; i<typecnt; i+=1) {
	tz.types[i] = { isstd:0, isgmt:0 };
	tz.types[i].gmtoff  = word();
	tz.types[i].isdst   = buf[off]; off += 1;
	tz.types[i].abbrind = buf[off]; off += 1;
	if(tz.type0 == nil and tz.types[i].isdst == 0)
	    tz.type0 = tz.types[i]; # Pick the 1st non-DST type
    }
    for(var i=0; i<typecnt; i+=1) {
	var start = off + tz.types[i].abbrind;
	for(var end = start; buf[end]; end += 1) {}
	tz.types[i].abbr = substr(buf, start, end-start);
    }
    off += charcnt;
    tz.leaps = setsize([], leapcnt);
    for(var i=0; i<leapcnt; i+=1)
	tz.leaps[i] = { time:word(), off:word() };
    for(var i=0; i<ttisstdcnt; i+=1)
	tz.types[i].isstd = buf[(off+=1)-1];
    for(var i=0; i<ttisgmtcnt; i+=1)
	tz.types[i].isgmt = buf[(off+=1)-1];
    if(size(tz.times) == 0) {
	tz.times = [0];
	tz.ttypes = "\x00"
    }
    return tz;
}

var zones = {};
var zone_path = "/usr/share/zoneinfo";

var timezone = func(tz=nil) {
    if(contains(zones, tz)) return zones[tz];
    if(tz == nil) tz = "/etc/localtime"; # FIXME: check $TZ
    else tz = zone_path ~ "/" ~ tz;
    return (zones[tz] = _parsetz(io.readfile(tz)));
}

# Binary search a time zone definition
var _tzsearch = func(e, tz, togmt) {
    if(tz == nil) tz = timezone();
    var off = 0;
    if(e >= tz.times[-1]) {
	off = tz.types[tz.ttypes[-1]].gmtoff;
	return togmt ? e-off : e+off;
    } elsif(e < tz.times[0]) {
	off = tz.type0.gmtoff;
	return togmt ? e-off : e+off;
    }
    var lo = 0; var hi = size(tz.times) - 1;
    while(hi - lo > 1) {
	var mid = int((hi+lo)/2);
	off = tz.types[tz.ttypes[mid]].gmtoff;
	var e2 = e - (togmt ? off : 0);
	if(tz.times[mid] > e2) hi = mid;
	else                   lo = mid;
    }
    off = tz.types[tz.ttypes[lo]].gmtoff;
    return togmt ? e-off : e+off;
}

var _tz2local = func(e, tz) { _tzsearch(e, tz, 0); }
var _tz2gmt = func(le, tz) { _tzsearch(le, tz, 1); }
