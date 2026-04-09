c_int8_limits = [-128, 127]
c_uint8_limits = [0, 255]
c_int16_limits = [-32768, 32767]
c_uint16_limits = [0, 65535]
c_int32_limits = [-2147483648, 2147483647]
c_uint32_limits = [0, 4294967295]
c_float_limits = [-3.402823466e38, 3.402823466e38]

json_settings_fields = {"id", "default", "min", "max", "length", "conversion"}
json_command_fields = {"id", "length", "conversion", "value"}
json_message_fields = {"port", "id", "length", "conversion"}


def write_float(var):
    """Format a Python float as a float literal."""
    split = str(var).split(".", 1)
    if len(split) > 1:
        while len(split[1]) > 4:
            split[1] = split[1][:-1]
        while len(split[1]) < 4:
            split[1] = split[1] + "0"
        if split[0][0] == "-":
            split[1] = "-" + split[1]
        return "{" + split[0] + "," + split[1] + "}"
    else:
        return "{" + split[0] + ", 0 }"


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


def field_check(var, json_fields):
    """Return whether all required JSON fields are present."""
    for field in json_fields:
        if field not in var:
            return False
    return True


def check_type(var, val_type):
    """Verify that default, min, and max share the expected Python type."""
    if type(var["default"]) is not val_type:
        return False
    if type(var["min"]) is not val_type:
        return False
    if type(var["max"]) is not val_type:
        return False
    return True


def check_range(var, limits):
    """Validate that values fit within the supplied numeric bounds."""
    if (var["default"] < limits[0]) or (var["default"] > limits[1]):
        print("Def value error")
        return False
    if (var["min"] < limits[0]) or (var["min"] > limits[1]):
        print("Min value error")
        return False
    if (var["max"] < limits[0]) or (var["max"] > limits[1]):
        print("Max value error")
        return False
    if (var["default"] < var["min"]) or (var["default"] > var["max"]):
        print("Def bound value error")
        return False
    return True


def check_length(var):
    """Validate that the declared length matches the selected conversion type."""
    if var["conversion"] == "uint8" and var["length"] != 1:
        return False
    elif var["conversion"] == "uint16" and var["length"] != 2:
        return False
    elif var["conversion"] == "uint32" and var["length"] != 4:
        return False
    elif var["conversion"] == "int8" and var["length"] != 1:
        return False
    elif var["conversion"] == "int16" and var["length"] != 2:
        return False
    elif var["conversion"] == "int32" and var["length"] != 4:
        return False
    elif var["conversion"] == "float" and var["length"] != 4:
        return False
    elif var["conversion"] == "byte_array" and (
        var["length"] < 0 or var["length"] > 255
    ):
        return False
    elif var["conversion"] == "bool" and var["length"] != 1:
        return False
    return True


def check_values(var, val_type, limits=None):
    """Validate a settings entry before it is converted for code generation."""
    # Check ID format
    if type(var["id"]) is not str:
        print("ID type error")
        return False
    if not var["id"].startswith("0x"):
        print("ID value error")
        return False
    # check range of values
    if limits is not None:
        # For int and float check range
        if not check_range(var, limits):
            print("Range error")
            return False
    elif val_type is str:
        # For string check string length
        """
        if len(var["default"]) > var["length"]:
            print("String length error")
            return False
        if len(var["min"]) > var["length"]:
            print("String length error")
            return False
        if len(var["max"]) > var["length"]:
            print("String length error")
            return False
        """
    # Check length
    if type(var["length"]) is not int:
        print("length type error")
        return False
    if not check_length(var):
        print("length error")
        return False

    return True


# Construct dict containing setting data and verify values
def construct_dict_setting(setting):
    """Build a normalized setting dictionary from validated JSON input."""
    setting_dict = {}
    if setting["conversion"] == "uint8":
        if not check_values(setting, int, c_uint8_limits):
            return False
        setting_dict["struct_name"] = "value_uint8"
        setting_dict["conversion"] = "UINT8_T"
        setting_dict["default"] = str(setting["default"])
        setting_dict["min"] = str(setting["min"])
        setting_dict["max"] = str(setting["max"])

    elif setting["conversion"] == "uint16":
        if not check_values(setting, int, c_uint16_limits):
            return False
        setting_dict["struct_name"] = "value_uint16"
        setting_dict["conversion"] = "UINT16_T"
        setting_dict["default"] = str(setting["default"])
        setting_dict["min"] = str(setting["min"])
        setting_dict["max"] = str(setting["max"])

    elif setting["conversion"] == "uint32":
        if not check_values(setting, int, c_uint32_limits):
            return False
        setting_dict["struct_name"] = "value_uint32"
        setting_dict["conversion"] = "UINT32_T"
        setting_dict["default"] = str(setting["default"])
        setting_dict["min"] = str(setting["min"])
        setting_dict["max"] = str(setting["max"])

    elif setting["conversion"] == "int8":
        if not check_values(setting, int, c_int8_limits):
            return False
        setting_dict["struct_name"] = "value_int8"
        setting_dict["conversion"] = "INT8_T"
        setting_dict["default"] = str(setting["default"])
        setting_dict["min"] = str(setting["min"])
        setting_dict["max"] = str(setting["max"])

    elif setting["conversion"] == "int16":
        if not check_values(setting, int, c_int16_limits):
            return False
        setting_dict["struct_name"] = "value_int16"
        setting_dict["conversion"] = "INT16_T"
        setting_dict["default"] = str(setting["default"])
        setting_dict["min"] = str(setting["min"])
        setting_dict["max"] = str(setting["max"])

    elif setting["conversion"] == "int32":
        if not check_values(setting, int, c_int32_limits):
            return False
        setting_dict["struct_name"] = "value_int32"
        setting_dict["conversion"] = "INT32_T"
        setting_dict["default"] = str(setting["default"])
        setting_dict["min"] = str(setting["min"])
        setting_dict["max"] = str(setting["max"])

    elif setting["conversion"] == "float":
        if not check_values(setting, float, c_float_limits):
            return False
        setting_dict["struct_name"] = "value_float"
        setting_dict["conversion"] = "FLOAT"
        setting_dict["default"] = write_float(setting["default"])
        setting_dict["min"] = write_float(setting["min"])
        setting_dict["max"] = write_float(setting["max"])

    elif setting["conversion"] == "string":
        if not check_values(setting, str):
            return False
        setting_dict["struct_name"] = "value_byte_array"
        setting_dict["conversion"] = "BYTE_ARRAY"
        setting_dict["default"] = write_string(setting["default"])
        setting_dict["min"] = write_string(setting["min"])
        setting_dict["max"] = write_string(setting["max"])

    elif setting["conversion"] == "byte_array":
        if not check_values(setting, str):
            return False
        setting_dict["struct_name"] = "value_byte_array"
        setting_dict["conversion"] = "BYTE_ARRAY"
        setting_dict["default"] = write_byte_array(setting["default"])
        setting_dict["min"] = write_byte_array(setting["min"])
        setting_dict["max"] = write_byte_array(setting["max"])

    elif setting["conversion"] == "bool":
        if not check_values(setting, bool):
            return False
        setting_dict["struct_name"] = "value_bool"
        setting_dict["conversion"] = "BOOL"
        setting_dict["default"] = write_bool(setting["default"])
        setting_dict["min"] = write_bool(setting["min"])
        setting_dict["max"] = write_bool(setting["max"])

    else:
        return False

    setting_dict["id"] = setting["id"]
    setting_dict["length"] = str(setting["length"])

    return setting_dict


# Construct dict containing command data and verify values
def construct_dict_command(command):
    """Build a normalized command dictionary from validated JSON input."""
    command_dict = {}

    if command["conversion"] == "uint8":
        command_dict["conversion"] = "UINT8_T"

    elif command["conversion"] == "uint16":
        command_dict["conversion"] = "UINT16_T"

    elif command["conversion"] == "uint32":
        command_dict["conversion"] = "UINT32_T"

    elif command["conversion"] == "int8":
        command_dict["conversion"] = "INT8_T"

    elif command["conversion"] == "int16":
        command_dict["conversion"] = "INT16_T"

    elif command["conversion"] == "int32":
        command_dict["conversion"] = "INT32_T"

    elif command["conversion"] == "float":
        command_dict["conversion"] = "FLOAT"

    elif command["conversion"] == "byte_array" or command["conversion"] == "string":
        command_dict["conversion"] = "BYTE_ARRAY"

    elif command["conversion"] == "bool":
        command_dict["conversion"] = "BOOL"

    else:
        return False

    command_dict["id"] = command["id"]
    command_dict["length"] = str(command["length"])
    command_dict["value"] = write_string(str(command["value"]))

    return command_dict


# Construct dict containing message data and verify values
def construct_dict_message(message):
    """Build a normalized message dictionary from validated JSON input."""
    message_dict = {}

    if message["conversion"] == "uint8":
        message_dict["conversion"] = "UINT8_T"

    elif message["conversion"] == "uint16":
        message_dict["conversion"] = "UINT16_T"

    elif message["conversion"] == "uint32":
        message_dict["conversion"] = "UINT32_T"

    elif message["conversion"] == "int8":
        message_dict["conversion"] = "INT8_T"

    elif message["conversion"] == "int16":
        message_dict["conversion"] = "INT16_T"

    elif message["conversion"] == "int32":
        message_dict["conversion"] = "INT32_T"

    elif message["conversion"] == "float":
        message_dict["conversion"] = "FLOAT"

    elif message["conversion"] == "byte_array" or message["conversion"] == "string":
        message_dict["conversion"] = "BYTE_ARRAY"

    elif message["conversion"] == "bool":
        message_dict["conversion"] = "BOOL"

    else:
        print("Error, wrong conversion type!")
        return False

    message_dict["id"] = message["id"]
    message_dict["length"] = str(message["length"])
    message_dict["port"] = message["port"]

    return message_dict


# Validate setting json entry and return dict structure
def setting_validate(json_data):
    """Validate and normalize a settings JSON entry."""
    # Check if all fields are present
    if not field_check(json_data, json_settings_fields):
        print("Missing value in field!")
        return False
    return construct_dict_setting(json_data)


# Validate command json entry and return dict structure
def command_validate(json_data):
    """Validate and normalize a command JSON entry."""
    # Check if all fields are present
    if not field_check(json_data, json_command_fields):
        print("Missing value in field!")
        return False
    return construct_dict_command(json_data)


# Validate message json entry and return dict structure
def message_validate(json_data):
    """Validate and normalize a message JSON entry."""
    # Check if all fields are present
    if not field_check(json_data, json_message_fields):
        print("Missing value in field!")
        return False
    return construct_dict_message(json_data)
