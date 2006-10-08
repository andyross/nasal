import("_gtk");
var gtk = _gtk;

# UIManager XML definition
var uixml = '
<ui>
 <menubar name="MenuBar">
  <menu action="FileMenu">
   <menuitem action="New"/>
   <menuitem action="Open"/>
   <menuitem action="Save"/>
   <separator/>
   <menuitem action="Quit"/>
  </menu>
  <menu action="PrefMenu">
   <menuitem action="Ginger"/>
   <menuitem action="Onion"/>
   <menuitem action="Garlic"/>
   <separator/>
   <menuitem action="Dir"/>
   <menuitem action="North"/>
   <menuitem action="South"/>
   <menuitem action="East"/>
   <menuitem action="West"/>
  </menu>
 </menubar>
</ui>';

# Global accelerator keys
var accelmap = '
(gtk_accel_path "<Actions>/File/New"  "<Control>n")
(gtk_accel_path "<Actions>/File/Open" "<Control>o")
(gtk_accel_path "<Actions>/File/Save" "<Control>s")
(gtk_accel_path "<Actions>/File/Quit" "<Control>q")
';
gtk.accel_map_parse(accelmap);

# Create an ActionGroup and add Action objects for each action in the XML.
# Remember the menus!  They need actions too, for the label.
var actiongroup = gtk.new("GtkActionGroup");

var a = gtk.new("GtkAction", "name", "FileMenu", "label", "_File");
gtk.action_group_add_action(actiongroup, a);

a = gtk.new("GtkAction", "name", "New", "label", "_New", "stock-id", "gtk-new",
	    "tooltip", "Create new document");
gtk.action_set_accel_path(a, "<Actions>/File/New");
gtk.connect(a, "activate", func { print("New!\n") });
gtk.action_group_add_action(actiongroup, a);

a = gtk.new("GtkAction", "name", "Open", "label", "_Open",
	    "stock-id", "gtk-open", "tooltip", "Open document");
gtk.action_set_accel_path(a, "<Actions>/File/Open");
gtk.connect(a, "activate", func { print("Open!\n") });
gtk.action_group_add_action(actiongroup, a);

a = gtk.new("GtkAction", "name", "Save", "label", "_Save",
	    "stock-id", "gtk-open", "tooltip", "Save document");
gtk.action_set_accel_path(a, "<Actions>/File/Save");
gtk.connect(a, "activate", func { print("Save!\n") });
gtk.action_group_add_action(actiongroup, a);

a = gtk.new("GtkAction", "name", "Quit", "label", "_Quit",
	    "stock-id", "gtk-open", "tooltip", "Quit document");
gtk.action_set_accel_path(a, "<Actions>/File/Quit");
gtk.connect(a, "activate", func { print("Quit!\n") });
gtk.action_group_add_action(actiongroup, a);
 
a = gtk.new("GtkAction", "name", "PrefMenu", "label", "_Preferences");
gtk.action_group_add_action(actiongroup, a);

a = gtk.new("GtkToggleAction", "name", "Ginger", "label", "Ginger");
gtk.connect(a, "activate", func { print("Ginger!\n") });
gtk.action_group_add_action(actiongroup, a);

a = gtk.new("GtkToggleAction", "name", "Onion", "label", "Onion");
gtk.connect(a, "activate", func { print("Onion!\n") });
gtk.action_group_add_action(actiongroup, a);

a = gtk.new("GtkToggleAction", "name", "Garlic", "label", "Garlic");
gtk.connect(a, "activate", func { print("Garlic!\n") });
gtk.action_group_add_action(actiongroup, a);

# You can add actions to a separate ActionGroup to en/disable them at
# runtime.
var dirgroup = gtk.new("GtkActionGroup");

a = gtk.new("GtkToggleAction", "name", "Dir", "label", "Use Directions");
gtk.connect(a, "activate", func(action) {
    gtk.set(dirgroup, "sensitive", gtk.toggle_action_get_active(action));
 });
gtk.action_group_add_action(actiongroup, a);
gtk.set(dirgroup, "sensitive", gtk.toggle_action_get_active(a));

# Radio actions need to be added all at once to get the grouping
# right.
var r0 = gtk.new("GtkRadioAction", "name", "North", "label", "North");
var r1 = gtk.new("GtkRadioAction", "name", "South", "label", "South");
var r2 = gtk.new("GtkRadioAction", "name", "East", "label", "East");
var r3 = gtk.new("GtkRadioAction", "name", "West", "label", "West");
gtk.connect(r0, "changed", func(action, active) {
    print("New direction: ", gtk.get(active, "name"), "\n");
});
gtk.action_group_add_radios(dirgroup, r0, r1, r2, r3);

# Create a UI manager, tell it about the actions, add the XML to
# create the widgets, then extract our menu widget.
var uim = gtk.new("GtkUIManager");
gtk.ui_manager_insert_action_group(uim, actiongroup, 0);
gtk.ui_manager_insert_action_group(uim, dirgroup, 0);
gtk.ui_manager_add_ui(uim, uixml);

var menu = gtk.ui_manager_get_widget(uim, "/MenuBar");

# Create the window, and be sure to add the accel group that the
# UIManager creates.
var win = gtk.new("GtkWindow");
gtk.window_add_accel_group(win, gtk.ui_manager_get_accel_group(uim));

var vbox = gtk.new("GtkVBox");
gtk.emit(win, "add", vbox);
gtk.emit(vbox, "add", menu);

var label = gtk.new("GtkLabel", "wrap", 1,
		    "label", "This is a demo of the Gtk+ UIManager class which manages menus and toolbars via an XML specification");
gtk.emit(vbox, "add", label);

gtk.widget_show_all(win);
gtk.main();
