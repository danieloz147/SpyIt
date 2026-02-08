var metadata = {
    name: "Enum-Screens-BOF",
    description: "Enumerate display monitors (BOF)"
};

var cmd_enum_screens = ax.create_command(
    "enum-screens",
    "Enumerate display monitors on target (BOF)",
    "enum-screens [help]"
);

cmd_enum_screens.setPreHook(function (id, cmdline, parsed_json) {
    var arch = ax.arch(id);
    var bof_path = ax.script_dir() + "enumerate_screens." + arch + ".o";
    
    // Check if BOF exists
    if (!ax.file_exists(bof_path)) {
        throw new Error("BOF not found at " + bof_path);
    }
    ax.execute_alias(id, cmdline, "execute bof \"" + bof_path + "\"", "BOF: enumerate screens");
});

var group_enum_screens = ax.create_commands_group("Enum-Screens", [cmd_enum_screens]);
ax.register_commands_group(group_enum_screens, ["beacon", "gopher", "kharon"], ["windows"], []);