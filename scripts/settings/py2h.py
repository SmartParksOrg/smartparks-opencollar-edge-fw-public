import json

import validate_data as V

# Ports dictionary
ports = {}

# Settings arrays
setting_struct_array = []
setting_name_array = []
setting_id_array = []
setting_family_array = []
setting_type_array = []
setting_length_array = []
setting_default_array = []

# Values array
value_struct_array = []
value_name_array = []
value_id_array = []
value_family_array = []
value_type_array = []
value_length_array = []

# Messages array
message_name_array = []
message_port_array = []
message_id_array = []
message_length_array = []

# Path to settings.h file
path_hardware = "app/src/settings/generated_settings/hardware_def.h"
path_settings_h = "app/src/settings/generated_settings/settings_def.h"
path_settings_c = "app/src/settings/generated_settings/settings_def.c"
path_commands_h = "app/src/settings/generated_settings/commands_def.h"
path_values_c = "app/src/settings/generated_settings/values_def.c"
path_values_h = "app/src/settings/generated_settings/values_def.h"
path_messages_c = "app/src/settings/generated_settings/messages_def.c"
path_messages_h = "app/src/settings/generated_settings/messages_def.h"


# Store values to arrays - settings structs
def add_setting_to_array(setting_dict):
    """Append generated setting metadata to the global lookup arrays."""
    setting_struct_array.append(setting_dict["struct_name"])
    setting_name_array.append(setting_name)
    setting_id_array.append(setting_dict["id"])
    setting_family_array.append(setting_dict["family"])
    setting_type_array.append(setting_dict["conversion"].lower())
    setting_length_array.append(setting_dict["length"])
    setting_default_array.append(setting_dict["default"])


# Store values to arrays - values structs
def add_value_to_array(setting_dict):
    """Append generated value metadata to the global lookup arrays."""
    value_struct_array.append(setting_dict["struct_name"])
    value_name_array.append(value_name)
    value_id_array.append(setting_dict["id"])
    value_family_array.append(setting_dict["family"])
    value_type_array.append(setting_dict["conversion"].lower())
    value_length_array.append(setting_dict["length"])


# Store messages to arrays - message structs
def add_message_to_array(setting_dict):
    """Append generated message metadata to the global lookup arrays."""
    message_name_array.append(message_name)
    message_port_array.append(str(ports[setting_dict["port"]]))
    message_id_array.append(setting_dict["id"])
    message_length_array.append(setting_dict["length"])


def write_bool(var):
    """Convert a Python boolean to a lowercase string literal."""
    if var is True:
        return "true"
    else:
        return "false"


def write_string(var):
    """Wrap a Python string in C string literal quotes."""
    return '"' + var + '"'


def write_byte_array(var):
    """Return a byte-array literal unchanged for code generation."""
    return var


# write_head()
# Write .h head and includes
def write_head(name):
    """Write the common header guard and includes for a generated header."""
    h.write("#ifndef " + name + "_H_\n")
    h.write("#define " + name + "_H_\n\n")
    h.write("#include <stdio.h>\n")
    h.write("#include <zephyr/kernel.h>\n")
    h.write('#include "settings_types.h"\n\n')


# end write_head()


def write_char_arrays(setting_dict, setting_name):
    """Emit backing arrays for byte-array settings and update references."""
    # Define arrays
    h.write(
        "uint8_t "
        + setting_name
        + "_def["
        + setting_dict["length"]
        + "] = "
        + setting_dict["default"]
        + ";\n"
    )
    h.write(
        "uint8_t "
        + setting_name
        + "_min["
        + setting_dict["length"]
        + "] = "
        + setting_dict["min"]
        + ";\n"
    )
    h.write(
        "uint8_t "
        + setting_name
        + "_max["
        + setting_dict["length"]
        + "] = "
        + setting_dict["max"]
        + ";\n\n"
    )
    setting_dict["default"] = setting_name + "_def"
    setting_dict["min"] = setting_name + "_min"
    setting_dict["max"] = setting_name + "_max"


def write_setting(setting_name, setting_dict):
    """Write a generated C definition for one setting."""
    # Write setting
    if setting_dict["conversion"] == "BYTE_ARRAY":
        write_char_arrays(setting_dict, setting_name)

    h.write(setting_dict["struct_name"] + " " + setting_name + " = \n")
    h.write("{\n    ")
    h.write(setting_dict["id"] + ",\n    ")
    h.write(setting_dict["default"] + ",\n    ")
    h.write(setting_dict["min"] + ",\n    ")
    h.write(setting_dict["max"] + ",\n    ")
    h.write(setting_dict["length"] + ",\n    ")
    h.write(setting_dict["conversion"])
    if "family" in setting_dict:
        h.write(",\n    " + setting_dict["family"])
    h.write("\n};\n\n")


def write_command(command_name, command_dict):
    """Write a generated C macro definition for one command."""
    h.write("#define " + command_name.upper() + " " + command_dict["id"] + "\n")


def write_port(port_name, port_nr):
    """Write a generated C macro definition for one port."""
    h.write("#define " + port_name.upper() + " " + str(port_nr) + "\n")


def write_message(message_name, message_dict):
    """Write a generated C definition for one message descriptor."""
    # Write message
    if message_dict["port"] not in ports:
        print("Error, port name does not exist!")
        return False

    h.write("cmd_message " + message_name + " = \n")
    h.write("{\n    ")
    h.write(str(ports[message_dict["port"]]) + ",\n    ")
    h.write(message_dict["id"] + ",\n    ")
    h.write(message_dict["length"] + ",\n    ")
    h.write(message_dict["conversion"])
    h.write("\n};\n\n")


def write_setting_struct():
    """Write the aggregate settings structure and its lookup tables."""
    # ID array
    h.write("uint8_t id_array[] = {")
    for i, myid in enumerate(setting_id_array):
        h.write(myid)
        if i < len(setting_id_array) - 1:
            h.write(", ")
    h.write("};\n")

    # Family array
    h.write("uint8_t family_array[] = {")
    for i, myfam in enumerate(setting_family_array):
        h.write(myfam)
        if i < len(setting_family_array) - 1:
            h.write(", ")
    h.write("};\n\n")

    # Length array
    h.write("uint8_t len_array[] = {")
    for i, mylen in enumerate(setting_length_array):
        h.write(mylen)
        if i < len(setting_length_array) - 1:
            h.write(", ")
    h.write("};\n\n")

    # Careful! Order matters and must be preserved.
    h.write("main_settings Main_settings = \n")
    h.write("{\n")
    for name in setting_name_array:
        h.write("    &" + name + ",\n")
    h.write("    " + str(len(setting_id_array)) + ",\n")
    h.write("    id_array,\n")
    h.write("    family_array,\n")
    h.write("    len_array\n")
    h.write("};\n\n")


# Write main values structure
def write_values_struct():
    """Write the aggregate values structure and its lookup tables."""
    # ID array
    h.write("uint8_t val_id_array[] = {")
    for myid in value_id_array:
        h.write(myid)
        if myid != value_id_array[-1]:
            h.write(", ")
    h.write("};\n")

    # Family array
    h.write("uint8_t val_family_array[] = {")
    for myfam in range(len(value_family_array)):
        h.write(value_family_array[myfam])
        if myfam < len(value_family_array) - 1:
            h.write(", ")
    h.write("};\n\n")

    # Length array
    h.write("uint8_t val_len_array[] = {")
    for i in range(len(value_length_array)):
        h.write(value_length_array[i])
        if i != len(value_length_array) - 1:
            h.write(", ")
    h.write("};\n\n")

    h.write("main_values Main_values = \n")
    h.write("{\n")
    for name in value_name_array:
        h.write("    &" + name + ",\n")
    h.write("    " + str(len(value_id_array)) + ",\n")
    h.write("    val_id_array,\n")
    h.write("    val_family_array,\n")
    h.write("    val_len_array\n")
    h.write("};\n\n")


def write_message_struct():
    """Write the aggregate messages structure and its lookup tables."""
    # ID array
    h.write("uint8_t mes_id_array[] = {")
    for i, myid in enumerate(message_id_array):
        h.write(myid)
        if i < len(message_id_array) - 1:
            h.write(", ")
    h.write("};\n")

    # Length array
    h.write("uint8_t mes_len_array[] = {")
    for i, mylen in enumerate(message_length_array):
        h.write(mylen)
        if i < len(message_length_array) - 1:
            h.write(", ")
    h.write("};\n")

    # Port array
    h.write("uint8_t mes_port_array[] = {")
    for i, myport in enumerate(message_port_array):
        h.write(myport)
        if i < len(message_port_array) - 1:
            h.write(", ")
    h.write("};\n\n")

    h.write("main_messages Main_messages = \n")
    h.write("{\n")
    for name in message_name_array:
        h.write("    &" + name + ",\n")
    h.write("    " + str(len(message_id_array)) + ",\n")
    h.write("    mes_id_array,\n")
    h.write("    mes_len_array,\n")
    h.write("    mes_port_array\n")
    h.write("};\n\n")


def write_function_get_setting_struct_byID():
    """Write the generated helper that returns a setting struct by ID."""
    # Function get setting by id
    h.write("void *get_setting_struct_by_id(uint8_t family, uint8_t id){\n")
    h.write("switch(MAKE_KEY(family, id)) {\n")
    for name, myid, myfam in zip(
        setting_name_array, setting_id_array, setting_family_array
    ):
        h.write("case MAKE_KEY(" + myfam + ", " + myid + "):\n")
        h.write("    return Main_settings." + name + ";\n")
    h.write("default:\n")
    h.write("    return NULL;\n}\n}\n\n")


def write_function_get_setting_byID():
    """Write the generated helper that serializes a setting by ID."""
    # Function get setting by id
    h.write("int get_setting_by_id(uint8_t family, uint8_t id, uint8_t *bytes){\n")
    h.write("switch(MAKE_KEY(family, id)) {\n")
    for name, myid, myfam, val_type, mylen in zip(
        setting_name_array,
        setting_id_array,
        setting_family_array,
        setting_type_array,
        setting_length_array,
    ):
        h.write("case MAKE_KEY(" + myfam + ", " + myid + "):\n")
        if val_type == "byte_array":
            h.write(
                "    "
                + val_type
                + "_to_bytes(bytes, Main_settings."
                + name
                + "->def_val, "
                + mylen
                + ");\n"
            )
        else:
            h.write(
                "    "
                + val_type
                + "_to_bytes(bytes, Main_settings."
                + name
                + "->def_val);\n"
            )
        h.write("    return " + mylen + ";\n")
    h.write("default:\n")
    h.write("    return 0;\n}\n}\n\n")


def write_function_get_setting_default_byID():
    """Serialize immutable JSON defaults independently of live setting values."""
    scalar_widths = {
        "uint8_t": 8,
        "int8_t": 8,
        "bool": 8,
        "uint16_t": 16,
        "int16_t": 16,
        "uint32_t": 32,
        "int32_t": 32,
    }
    for width in sorted(
        {scalar_widths[t] for t in setting_type_array if t in scalar_widths}
    ):
        length = str(width // 8)
        h.write(
            "static int setting_default_uint"
            + str(width)
            + "_to_bytes(uint"
            + str(width)
            + "_t value, uint8_t *bytes, uint8_t len){\n"
        )
        h.write("if (len != " + length + ") {\n    return 0;\n}\n")
        if width == 8:
            h.write("bytes[0] = value;\n")
        else:
            h.write("sys_put_le" + str(width) + "(value, bytes);\n")
        h.write("return " + length + ";\n}\n\n")

    h.write(
        "int get_setting_default_by_id(uint8_t family, uint8_t id, uint8_t *bytes, "
        "uint8_t len){\n"
    )
    h.write("if (bytes == NULL) {\n    return 0;\n}\n")
    h.write("switch(MAKE_KEY(family, id)) {\n")
    for myid, myfam, val_type, mylen, default in zip(
        setting_id_array,
        setting_family_array,
        setting_type_array,
        setting_length_array,
        setting_default_array,
    ):
        h.write("case MAKE_KEY(" + myfam + ", " + myid + "):")
        if val_type in scalar_widths:
            width = str(scalar_widths[val_type])
            h.write(
                "\n    return setting_default_uint"
                + width
                + "_to_bytes((uint"
                + width
                + "_t)("
                + default
                + "), bytes, len);\n"
            )
            continue
        h.write(" {\n")
        h.write("    if (len != " + mylen + ") {\n        return 0;\n    }\n")
        if val_type == "byte_array":
            h.write(
                "    static const uint8_t default_value["
                + mylen
                + "] = "
                + default
                + ";\n"
            )
            h.write("    memcpy(bytes, default_value, sizeof(default_value));\n")
        elif val_type == "float":
            h.write("    static const int16_t default_value[2] = " + default + ";\n")
            h.write("    sys_put_le16((uint16_t)default_value[0], bytes);\n")
            h.write("    sys_put_le16((uint16_t)default_value[1], bytes + 2);\n")
        h.write("    return " + mylen + ";\n}\n")
    h.write("default:\n")
    h.write("    return 0;\n}\n}\n\n")


def write_function_setting_value_in_range():
    """Validate serialized scalar ranges without changing the live settings."""
    decoders = {
        "uint8_t": "data[0]",
        "uint16_t": "sys_get_le16(data)",
        "uint32_t": "sys_get_le32(data)",
        "int8_t": "(int8_t)data[0]",
        "int16_t": "(int16_t)sys_get_le16(data)",
        "int32_t": "(int32_t)sys_get_le32(data)",
        # Keep the raw byte so that an invalid bool (e.g. 2) is not coerced to true.
        "bool": "data[0]",
        # FLOAT is a pair of signed int16 whole/fraction parts, with 4 decimals.
        "float": "(int16_t)sys_get_le16(data) * 10000 + "
        "(int16_t)sys_get_le16(data + 2)",
    }
    for val_type in dict.fromkeys(setting_type_array):
        if val_type == "byte_array":
            continue
        index = setting_type_array.index(val_type)
        h.write(
            "static bool setting_"
            + val_type
            + "_in_range(const "
            + setting_struct_array[index]
            + " *setting, uint8_t *data, uint8_t len){\n"
        )
        h.write(
            "if (len != " + setting_length_array[index] + ") {\n    return false;\n}\n"
        )
        c_type = {"bool": "uint8_t", "float": "int32_t"}.get(val_type, val_type)
        h.write(c_type + " value = " + decoders[val_type] + ";\n")
        minimum = "setting->min"
        maximum = "setting->max"
        if val_type == "float":
            minimum = "(" + minimum + "[0] * 10000 + " + minimum + "[1])"
            maximum = "(" + maximum + "[0] * 10000 + " + maximum + "[1])"
        h.write("return value >= " + minimum + " && value <= " + maximum + ";\n}\n\n")

    h.write(
        "bool setting_value_in_range(uint8_t family, uint8_t id, uint8_t *data, "
        "uint8_t len){\n"
    )
    h.write("if (data == NULL) {\n    return false;\n}\n")
    h.write("switch(MAKE_KEY(family, id)) {\n")
    for name, myid, myfam, val_type, mylen in zip(
        setting_name_array,
        setting_id_array,
        setting_family_array,
        setting_type_array,
        setting_length_array,
    ):
        h.write("case MAKE_KEY(" + myfam + ", " + myid + "):\n")
        if val_type == "byte_array":
            # Array/string min and max fields are placeholders, not numeric bounds.
            h.write("    return len == " + mylen + ";\n")
        else:
            h.write(
                "    return setting_"
                + val_type
                + "_in_range(Main_settings."
                + name
                + ", data, len);\n"
            )
    h.write("default:\n")
    h.write("    return false;\n}\n}\n\n")


def write_function_get_value_struct_byID():
    """Write the generated helper that returns a value struct by ID."""
    # Function get setting by id
    h.write("void *get_value_struct_by_id(uint8_t family, uint8_t id){\n")
    h.write("switch(MAKE_KEY(family, id)) {\n")
    for name, myid, myfam in zip(value_name_array, value_id_array, value_family_array):
        h.write("case MAKE_KEY(" + myfam + ", " + myid + "):\n")
        h.write("    return Main_values." + name + ";\n")
    h.write("default:\n")
    h.write("    return NULL;\n}\n}\n\n")


def write_function_get_value_byID():
    """Write the generated helper that serializes a value by ID."""
    # Function get setting by id
    h.write("int get_value_by_id(uint8_t family, uint8_t id, uint8_t *bytes){\n")
    h.write("switch(MAKE_KEY(family, id)) {\n")
    for name, myid, myfam, val_type, mylen in zip(
        value_name_array,
        value_id_array,
        value_family_array,
        value_type_array,
        value_length_array,
    ):
        h.write("case MAKE_KEY(" + myfam + ", " + myid + "):\n")
        if val_type == "byte_array":
            h.write(
                "    "
                + val_type
                + "_to_bytes(bytes, Main_values."
                + name
                + "->def_val, "
                + mylen
                + ");\n"
            )
        else:
            h.write(
                "    "
                + val_type
                + "_to_bytes(bytes, Main_values."
                + name
                + "->def_val);\n"
            )
        h.write("    return " + mylen + ";\n")
    h.write("default:\n")
    h.write("    return 0;\n}\n}\n\n")


def write_function_set_setting_value_byID():
    """Write the generated helper that updates a setting value by ID."""
    # Function set value by id
    h.write(
        "int set_setting_value_by_id(uint8_t family, uint8_t id, uint8_t *data, uint8_t len){\n"
    )
    h.write("switch(MAKE_KEY(family, id)) {\n")
    for name, myid, myfam, val_type in zip(
        setting_name_array, setting_id_array, setting_family_array, setting_type_array
    ):
        h.write("case MAKE_KEY(" + myfam + ", " + myid + "):\n")
        if val_type == "byte_array":
            h.write("    if(len <= Main_settings." + name + "->len) {\n")
            h.write("        for(int i=0; i<len; i++) {\n")
            h.write("           Main_settings." + name + "->def_val[i] = data[i];\n")
            h.write("        }\n")
            h.write(
                "        for(int i=len; i<Main_settings." + name + "->len; i++) {\n"
            )
            h.write("           Main_settings." + name + "->def_val[i] = '\\0';\n")
            h.write("        }\n")
            h.write("    }\n")
        elif val_type == "float":
            h.write("    if(len <= Main_settings." + name + "->len) {\n")
            h.write(
                "        bytes_to_"
                + val_type
                + "(data, Main_settings."
                + name
                + "->def_val);\n"
            )
            h.write("    }\n")
        elif val_type == "bool":
            h.write("    if(len <= Main_settings." + name + "->len) {\n")
            h.write(
                "        Main_settings."
                + name
                + "->def_val = bytes_to_uint8_t(data);\n"
            )
            h.write("    }\n")
        else:
            h.write("    if(len == Main_settings." + name + "->len) {\n")
            h.write(
                "        Main_settings."
                + name
                + "->def_val = bytes_to_"
                + val_type
                + "(data);\n"
            )
            h.write("    }\n")
        h.write("    else return -1;\n")
        h.write("    break;\n")
    h.write("default:\n")
    h.write("    return -1;\n}\nreturn 0;\n}\n\n")


# Define main settings struct
def write_define_setting_struct():
    """Write the typedef for the generated main settings structure."""
    h.write("typedef struct main_settings {\n")
    for struct, name in zip(setting_struct_array, setting_name_array):
        h.write("    " + struct + " *" + name + ";\n")
    h.write("\n    uint16_t n_settings;\n")
    h.write("    uint8_t *settings_id;\n")
    h.write("    uint8_t *settings_family;\n")
    h.write("    uint8_t *settings_length;\n")
    h.write("} main_settings;\n\n")


# Define main values struct
def write_define_values_struct():
    """Write the typedef for the generated main values structure."""
    h.write("typedef struct main_values {\n")
    for struct, name in zip(value_struct_array, value_name_array):
        h.write("    " + struct + " *" + name + ";\n")
    h.write("\n    uint8_t n_values;\n")
    h.write("    uint8_t *values_id;\n")
    h.write("    uint8_t *values_family;\n")
    h.write("    uint8_t *values_length;\n")
    h.write("} main_values;\n\n")


# Define main messages struct
def write_define_messages_struct():
    """Write the typedef for the generated main messages structure."""
    for myname, myid in zip(message_name_array, message_id_array):
        h.write("#define " + myname.upper() + "_ID " + myid + "\n")

    h.write("\n\n")
    h.write("typedef struct main_messages {\n")
    for name in message_name_array:
        h.write("    cmd_message *" + name + ";\n")
    h.write("\n    uint8_t n_values;\n")
    h.write("    uint8_t *messages_id;\n")
    h.write("    uint8_t *messages_length;\n")
    h.write("    uint8_t *messages_port;\n")
    h.write("} main_messages;\n\n")


# Construct dict containing port data
def construct_dict_ports(ports_setting):
    """Populate the port-name to port-number lookup table."""
    for port_name in ports_setting:
        port_nr = ports_setting[port_name]
        ports[port_name] = port_nr
    print(ports)


# Open JSON file
f = open("scripts/settings/settings.json")
json_data = json.load(f)  # Create JSON object

if json_data:
    # Get port numbers and save them to dictionary
    if "ports" in json_data:
        construct_dict_ports(json_data["ports"])

    # SETTINGS ===============================================================================
    with open(path_settings_c, "w") as h:
        # Head
        h.write("/* AUTOGENERATED FILE - DO NOT MODIFY! */\n")
        h.write('#include "settings_def.h"\n')
        h.write("#include <stdio.h>\n")
        h.write("#include <string.h>\n")
        h.write("#include <zephyr/sys/byteorder.h>\n\n")

        h.write("#define MAKE_KEY(family, id) (((family) << 8) | (id))\n")

        # Settings
        if "settings" in json_data:
            # Loop over settings
            for setting_name in json_data["settings"]:
                setting = json_data["settings"][setting_name]
                # Validate data and construct dict to write
                setting_dict = V.setting_validate(setting)
                # Check if setting entry is valid
                if not setting_dict:
                    print("Error in field " + setting_name + "!")
                    continue

                add_setting_to_array(setting_dict)
                write_setting(setting_name, setting_dict)

        # Generate main setting structure
        write_setting_struct()
        # Generate functions
        write_function_get_setting_struct_byID()
        write_function_get_setting_byID()
        write_function_get_setting_default_byID()
        write_function_setting_value_in_range()
        write_function_set_setting_value_byID()

    # Header file
    with open(path_settings_h, "w") as h:
        # Head
        h.write("/* AUTOGENERATED FILE - DO NOT MODIFY! */\n")
        write_head("SETTINGS_DEF")
        h.write('#include "type_conversion.h"\n\n')
        write_define_setting_struct()
        h.write("extern main_settings Main_settings;\n")
        h.write("int get_setting_by_id(uint8_t family, uint8_t id, uint8_t *data);\n")
        h.write("void *get_setting_struct_by_id(uint8_t family, uint8_t id);\n\n")
        h.write(
            "/**\n"
            " * @brief Serialize the compiled default without reading mutable live values.\n"
            " *\n"
            " * @param family Setting family.\n"
            " * @param id Setting ID within the family.\n"
            " * @param bytes Output buffer with capacity for len bytes.\n"
            " * @param len Must equal the setting's declared length.\n"
            " * @return Number of bytes written, or 0 for an unknown setting, NULL buffer,\n"
            " *         or length mismatch. On failure, the buffer is unchanged.\n"
            " */\n"
            "int get_setting_default_by_id(uint8_t family, uint8_t id, uint8_t *bytes, "
            "uint8_t len);\n\n"
            "/**\n"
            " * @brief Check serialized values against inclusive setting min/max limits.\n"
            " *\n"
            " * Numeric values use their signedness and little-endian wire encoding.\n"
            " * Boolean bytes are checked before conversion to bool. Byte arrays and\n"
            " * strings require only their exact declared length; their placeholder\n"
            " * min/max arrays are not numeric bounds. Does not change live settings.\n"
            " *\n"
            " * @param family Setting family.\n"
            " * @param id Setting ID within the family.\n"
            " * @param data Serialized setting value.\n"
            " * @param len Number of bytes in data.\n"
            " * @return true if the value passes; false for an unknown setting, NULL\n"
            " *         buffer, length mismatch, or out-of-range scalar value.\n"
            " */\n"
            "bool setting_value_in_range(uint8_t family, uint8_t id, uint8_t *data, "
            "uint8_t len);\n\n"
        )
        h.write(
            "int set_setting_value_by_id(uint8_t family, uint8_t id, uint8_t *data, uint8_t len);\n\n"
        )
        h.write("#endif\n")
    # END SETTINGS ===========================================================================

    # COMMANDS ==================================================================
    with open(path_commands_h, "w") as h:
        # Head
        h.write("/* AUTOGENERATED FILE - DO NOT MODIFY! */\n")
        write_head("COMMANDS_DEF")
        # Commands
        if "commands" in json_data:
            # Loop over commands
            for command_name in json_data["commands"]:
                command = json_data["commands"][command_name]
                # Write command
                command_dict = V.command_validate(command)
                # Check if setting entry is valid
                if not setting_dict:
                    print("Error in field " + command_name + "!")
                    continue
                write_command(command_name, command_dict)

        h.write("\n#endif\n")

    # END COMMANDS ===========================================================================

    # VALUES     =============================================================================
    with open(path_values_c, "w") as h:
        # Head
        h.write("/* AUTOGENERATED FILE - DO NOT MODIFY! */\n")
        h.write('#include "values_def.h"\n')
        h.write("#include <stdio.h>\n\n")

        h.write("#define MAKE_KEY(family, id) (((family) << 8) | (id))\n")

        if "values" in json_data:
            # Loop over commands
            for value_name in json_data["values"]:
                value = json_data["values"][value_name]
                # Validate data and construct dict to write
                value_dict = V.setting_validate(value)
                # Check if setting entry is valid
                if not value_dict:
                    print("Error in field " + value_name + "!")
                    continue

                write_setting(value_name, value_dict)
                add_value_to_array(value_dict)

        # Generate main setting structure
        write_values_struct()
        # Generate functions
        write_function_get_value_struct_byID()
        write_function_get_value_byID()

    # Header file
    with open(path_values_h, "w") as h:
        # Head
        h.write("/* AUTOGENERATED FILE - DO NOT MODIFY! */\n")
        write_head("VALUES_DEF")
        h.write('#include "type_conversion.h"\n\n')
        write_define_values_struct()
        h.write("extern main_values Main_values;\n")
        h.write("void *get_value_struct_by_id(uint8_t family, uint8_t id);\n")
        h.write("int get_value_by_id(uint8_t family, uint8_t id, uint8_t *data);\n")
        h.write(
            "int set_value_by_id(uint8_t family, uint8_t id, uint8_t *data, uint8_t len);\n\n"
        )
        h.write("#endif\n")

    # MESSAGES     =============================================================================
    with open(path_messages_c, "w") as h:
        # Head
        h.write("/* AUTOGENERATED FILE - DO NOT MODIFY! */\n")
        h.write('#include "messages_def.h"\n')
        h.write("#include <stdio.h>\n\n")
        if "messages" in json_data:
            # Loop over messages
            for message_name in json_data["messages"]:
                message = json_data["messages"][message_name]
                # Validate data and construct dict to write
                message_dict = V.message_validate(message)
                # Check if setting entry is valid
                if not message_dict:
                    print("Error in field " + message_name + "!")
                    continue

                write_message(message_name, message_dict)
                add_message_to_array(message_dict)

        # Generate main message structure
        write_message_struct()
        # Generate functions
        # write_function_get_value_struct_byID()
        # write_function_get_value_byID()

    # Header file
    with open(path_messages_h, "w") as h:
        # Head
        h.write("/* AUTOGENERATED FILE - DO NOT MODIFY! */\n")
        write_head("MESSAGES_DEF")
        h.write('#include "type_conversion.h"\n\n')
        if "ports" in json_data:
            for port_name in json_data["ports"]:
                port_nr = json_data["ports"][port_name]
                write_port(port_name, port_nr)
        h.write("\n\n")
        write_define_messages_struct()
        h.write("extern main_messages Main_messages;\n")
        h.write("#endif\n")

    # END SETTINGS ===========================================================================
