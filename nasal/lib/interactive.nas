### Interactive nasal interpreter by Jonatan Liljedahl

var namespace = new_nasal_env();

import("readline");
import("bits");
import("debug");

# returns the first occurence of character c in string s or -1 if not found
strchr = func(s, c) {
    var buf=bits.buf(1);
    buf[0]=c;
    return find(buf,s);
}

# Encapsulate 'static' variables to the comp func, by putting them inside an outer
# func that returns the completion function.
make_completer = func {
    var ns = namespace;
    var pos = 0;
    var len = 0;
    var index = 0;
    var candidate = [];

    # The actual completion function. Based on C code by Manabu Nishiyama.
    return func(text,state) {
        if(state==0) {
            ns = namespace;
            pos = 0;

            var membs = split(".",text);
            if(size(membs)>1) {
                foreach(var member;membs) {
                    if(!contains(ns,member)) break;
                    ns = ns[member];
                    pos += size(member)+1;
                    if(typeof(ns)!="hash") break;
                }
            }
            
            index = 0;
            len = size(text)-pos;
            candidate = keys(ns);
        }
        while(candidate != nil and index<size(candidate)) {
            var name = candidate[index];
            index += 1;
            if(substr(text,pos,len)==substr(name,0,len)) {
                text = substr(text,0,pos) ~ name;
                if(typeof(ns[name])=="hash") { text = text ~ "."; }
                elsif(typeof(ns[name])=="func") { text = text ~ "()"; }
                return text;
            }
        }
        return nil;
    }
}

# This is the main routine
var run = func {
    var level=0;
    var in_string=0;
    var escape=0;
    var text=nil;
    var oldline="";

    readline.set_completion_entry_function(make_completer());
    
    print("Nasal interactive interpreter. Press Ctrl-D to quit.\n");

    while(1) {
        var s = readline.readline(level?"... ":">>> ");

        if(s==nil) break;
        if(!size(s)) continue;

        for(var i=0;i<size(s);i+=1) {
            if(!escape and s[i] == `"`) {
                if(in_string==2) {
                    in_string=0;
                    level -= 1;
                } elsif(in_string==0) {
                    in_string=2;
                    level += 1;
                }
            }
            elsif(!escape and s[i] == `'`) {
                if(!escape and in_string==1) {
                    in_string=0;
                    level -= 1;
                } elsif(in_string==0) {
                    in_string=1;
                    level += 1;
                }
            }
            elsif(!(in_string or escape) and strchr("{[(",s[i])>=0)
                level+=1;
            elsif(!(in_string or escape) and strchr("}])",s[i])>=0)
                level-=1;

            escape=0;
            if(s[0] == `\\`)
                escape=1;
            elsif(!in_string and s == `\``)
                escape=1;
        }
        if(level<0) level=0;

        if(text!=nil) {
            text = text ~ "\n" ~ s;
        } else
            text = s;

        if(s!=oldline) readline.add_history(s); # don't add line twice
        oldline=s;
        
        if(level==0) { # no missing braces, run it!
            var err = [];
            var f = call(func compile(text,"<interactive>"),nil,nil,nil,err);
            if(size(err)) {
                print(err[0],"\n");
            } else {
                f = bind(f,namespace,nil);
                result = call(f,nil,nil,namespace,err);
                
                if(size(err)) {
                    print(sprintf("%s at %s line %d\n", err[0], err[1], err[2]));
                    for(var i=3; i<size(err); i+=2)
                        print(sprintf("  called from %s line %d\n", err[i], err[i+1]));
                } else
                    print(debug.dump(result),"\n");
            }
            text=nil;
        }
    }
}

