var cgi_vars = [ "PATH", "SERVER_SIGNATURE", "SERVER_SOFTWARE",
		 "SERVER_NAME", "SERVER_ADDR", "SERVER_PORT",
		 "REMOTE_ADDR", "DOCUMENT_ROOT", "SERVER_ADMIN",
		 "SCRIPT_FILENAME", "REMOTE_PORT",
		 "GATEWAY_INTERFACE", "SERVER_PROTOCOL",
		 "REQUEST_METHOD", "QUERY_STRING", "REQUEST_URI",
		 "SCRIPT_NAME", "CONTENT_LENGTH" ];

var read_headers = ["Content-Length", "Host", "Cookie"];

func {
    sethdr("XNasalHandler1", "Woo hoo!");
    setstatus(201);

    print("handler", 1, "\n");
    foreach(v; cgi_vars)
	print("  ", v, ": ", getcgi(v), "\n");

    foreach(h; read_headers)
	print("  ", h, ": ", gethdr(h), "\n");
    

    var total = 0;
    while(var s = read()) {
	print("read ", size(s), ":\n", s, "\n");
	total += size(s);
    }


    if(!getcgi("CONTENT_LENGTH") or total == gethdr("ConTeNt-length"))
	print("read sizes match!\n");
    else
	print("READ SIZE MISMATCH!\n");

}
