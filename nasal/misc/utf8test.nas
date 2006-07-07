var c = `©`;
var s = "Mötley Crüe";
print(c, "\n");
print(s, "\n");
print(sprintf("size(s) = %d, utf8.size(s) = %d\n", size(s), utf8.size(s)));

for(var i=0; i<size(s); i+=1)
    print(sprintf(" %d: %x, %c\n", i, s[i], s[i]));

for(var i=0; i<utf8.size(s); i+=1) {
    var c2 = utf8.strc(s, i);
    print(sprintf(" %d: %x, %s\n", i, c2, utf8.chstr(c2)));
 }

s2 = s ~ "";
s2 = utf8.validate(s2);
if(s != s2)
    die("validate changed a valid string?");

s2[3] = 129;
utf8.size(s2 = utf8.validate(s2, `¤`)); # size will die if it's invalid
print("stomped & validated: ", s2, "\n");

var sz = utf8.size(s);
print(sprintf("sz = %d\n", sz));
for(var start=0; start<sz; start+=1) {
    print(sprintf("sub(%d) = '%s'\n", start,
                  utf8.substr(s, start)));
    for(var len=0; len <= (sz-start); len += 1) {
        print(sprintf("  sub(%d, %d) = '%s'\n", start, len,
                      utf8.substr(s, start, len)));
    }
}
