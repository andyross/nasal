a = 1;

#print("a = ", a, "\n");
if(a != 1) { die(); }

(func{a = 2})();

#print("a = ", a, "\n");
if(a != 2) { die(); }

(func{
    if(a != 2) { die(); }
    var a = 3;
    if(a != 3) { die(); }
    #print("a = ", a, "\n");
 })();

#print("a = ", a, "\n");
if(a != 2) { die(); }

