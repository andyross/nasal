import("gtk");
import("io");

##
# Pops up a synchronous message dialog.  The LABEL argument is the
# text of the dialog, which can include Pango markup.  IMAGE specifies
# either an image file to load or the name or a stock icon.  BUTTONS
# is an array of strings representing labels to place on each button;
# the function will return the button string selected, or nil if the
# dialog was closed by other means.
#
msg_dialog = func(LABEL, TITLE="Notice", IMAGE="gtk-dialog-info", BUTTONS=nil)
{
    if(BUTTONS == nil) BUTTONS = ["OK"];

    var dlg = gtk.Dialog().set("title", TITLE);
    
    dlg.vbox().pack_start(var hbox = gtk.HBox().set("spacing", 5));
    hbox.pack_start(_gen_image(IMAGE));
    hbox.pack_start(gtk.Label().set("label", LABEL, "use-markup", 1));
    
    forindex(bi; BUTTONS)
	dlg.action_area().pack_start(_gen_button(dlg, bi, BUTTONS[bi]));
    
    dlg.show_all();

    var id = gtk.dialog_run(dlg);
    return id < 0 ? nil : BUTTONS[id];
}

# Generates a button that emits the "response" signal to the dialog
# when clicked, passing the specified ID.
_gen_button = func(dlg, id, label) {
    var b = gtk.Button().set("label", label);
    b.connect("clicked", func { dlg.emit("response", id); });
    return b;
}

# Generates a GtkImage widget from either a file (if it exists) or a
# stock icon name.  Note that "6" is a hard coded GTK_ICON_SIZE_DIALOG
# value.
_gen_image = func(img) {
    var stock = (io.stat(img) == nil);
    return gtk.Image().set(stock ? "stock" : "file", img, "icon-size", 6);
}

########################################################################
# Sample usage:
#

var TITLE = "Important Notice";
var IMAGE = "gtk-dialog-warning";
var LABEL = ("\n<b>The printer is on fire.</b>\n\n"
	     "A fire has been detected in the printing subsystem of our\n"
	     "network hardware.  This may cause your job to be delayed or\n"
	     "canceled.  Note that a common, related failure mode is a\n"
	     "spread of the fire to other nearby areas, including the\n"
	     "employee cubicles.  If you feel that this is the case, please\n"
	     "do not hesitate to inform your system administrator.\n\n"
	     "Have a nice day!\n");
var BUTTONS = ["Ignore", "Retry", "OK"];

print(msg_dialog(LABEL, TITLE, IMAGE, BUTTONS), "\n");
