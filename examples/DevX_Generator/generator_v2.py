import json
import pathlib

# declare dictionaries
signatures = {}
timer_block = {}
variables_block = {}
handlers_block = {}
templates = {}

device_twin_types = {"integer": "DX_TYPE_INT", "float": "DX_TYPE_FLOAT", "double": "DX_TYPE_DOUBLE",
                     "boolean": "DX_TYPE_BOOL",  "string": "DX_TYPE_STRING"}

gpio_init = {"low": "GPIO_Value_Low", "high": "GPIO_Value_High"}

gpio_direction = {"input": "DX_INPUT", "output": "DX_OUTPUT", "unknown": "DX_DIRECTION_UNKNOWN"}

device_twin_template = {"integer": "handler_device_twin_int", "float": "handler_device_twin_float",
                        "double": "handler_device_twin_double", "boolean": "handler_device_twin_bool",
                        "string": "handler_device_twin_string"}

device_twins_updates = None
device_twin_variables = None

with open('app.json', 'r') as j:
    data = json.load(j)

bindings = list(elem for elem in data if elem.get('active', True) == True)


def write_comment_block(f, msg):
    f.write("\n/****************************************************************************************\n")
    f.write("* {msg}\n".format(msg=msg))
    f.write("****************************************************************************************/\n")


def build_device_twin(binding, key):
    binding['properties'].update(
        {'twin_type': device_twin_types[binding['properties']['type']]})
    if binding['properties'].get("cloud2device") == True:

        binding.update({"sig_template": 'sig_device_twin'})
        signatures.update({key: binding})

        binding.update({"var_template": 'declare_device_twin_with_handler'})
        variables_block.update({key: binding})

        binding.update({"handler_template": device_twin_template[binding['properties']['type']]})
        handlers_block.update({key: binding})

    else:
        binding.update({"var_template": 'declare_device_twin'})
        variables_block.update({key: binding})


def build_direct_method(binding, key):
    binding.update({"sig_template": 'sig_direct_method'})
    signatures.update({key: binding})

    binding.update({"var_template": 'declare_direct_method'})
    variables_block.update({key: binding})

    binding.update({"handler_template": 'handler_direct_method'})
    handlers_block.update({key: binding})


def build_gpio(binding, key):
    binding.update({"sig_template": 'sig_timer'})
    signatures.update({key: binding})

    direction = binding['properties'].get('direction', "output")
    if direction == 'input':
        binding.update({"var_template": 'declare_gpio_input'})
        variables_block.update({key: binding})

        binding.update({"timer_template": 'declare_timer_periodic'})
        binding['properties'].update({"period": '4, 0'})
        timer_block.update({key: binding})

        binding.update({"handler_template": 'handler_gpio_input'})
        handlers_block.update({key: binding})
    else:
        binding.update({"var_template": 'declare_gpio_output'})
        variables_block.update({key: binding})

        binding.update({"timer_template": 'declare_timer_periodic'})
        binding['properties'].update({"period": '0, 200000000'})
        timer_block.update({key: binding})

        binding.update({"handler_template": 'handler_gpio_output'})
        handlers_block.update({key: binding})


def build_timer(binding, key):
    binding.update({"sig_template": 'sig_timer'})
    signatures.update({key: binding})

    period = binding['properties'].get('period')
    if period is None:
        binding.update({"timer_template": 'declare_timer_periodic'})
        timer_block.update({key: binding})

        binding.update({"handler_template": 'handler_timer_periodic'})
        handlers_block.update({key: binding})
    else:
        binding.update({"timer_template": 'declare_timer_oneshot'})
        timer_block.update({key: binding})

        binding.update({"handler_template": 'handler_timer_oneshot'})
        handlers_block.update({key: binding})


def build_invalid(binding, key):
    pass


build_binding = {"DEVICE_TWIN_BINDING": build_device_twin,
                 "DIRECT_METHOD_BINDING": build_direct_method,
                 "GPIO_BINDING": build_gpio,
                 "TIMER_BINDING": build_timer}


def load_templates():
    for path in pathlib.Path("templates").iterdir():
        if path.is_file():
            template_key = path.name.split(".")[0]
            with open(path, "r") as tf:
                templates.update({template_key: tf.read()})


def build_buckets():
    for item in bindings:
        key = item['properties'].get('name')
        if key is not None:
            build_binding.get(item["binding"], build_invalid)(item, key)


def build_signatures(f):
    for item in sorted(signatures):
        sig = signatures.get(item)
        name = sig['properties'].get('name')
        template_key = sig.get('sig_template')
        f.write(templates[template_key].format(name=name))


def get_value(properties, key):
    return "" if properties.get(key) is None else properties.get(key)


def build_timer_block(f):
    variables_list = ""
    write_comment_block(f, templates['comment_block_timer'])
    for item in sorted(timer_block):
        var = timer_block.get(item)
        properties = var.get('properties')
        if properties is not None:
            name = properties.get('name')
            variables_list += " &" + name + ","
            period = get_value(properties, 'period')
            template_key = var.get('timer_template')
            f.write(templates[template_key].format(name=name, period=period))

    if variables_list != "":
        f.write("\n// " + templates['comment_bindings_timer'] + "\n")
        f.write(templates['declare_bindings_timer'].format(variables=variables_list[:-1]))


def build_variable_block(f, key_binding, key_block):
    variables_list = ""

    write_comment_block(f, templates['comment_block_' + key_block])
    for item in sorted(variables_block):
        var = variables_block.get(item)
        binding = var['binding']

        if binding is not None and binding == key_binding:
            properties = var.get('properties')
            if properties is not None:
                name = properties.get('name')
                variables_list += " &" + name + ","

                pin = get_value(properties, 'pin')
                initialState = get_value(properties, 'initialState')
                invert = get_value(properties, 'invertPin')
                twin_type = get_value(properties, 'twin_type')

                template_key = var.get('var_template')

                f.write(templates[template_key].format(
                    name=name, pin=pin,
                    initialState=initialState,
                    invert=invert,
                    twin_type=twin_type
                ))

    if variables_list != "":
        f.write("\n// " + templates['comment_bindings_' + key_block] + "\n")
        f.write(templates['declare_bindings_' + key_block].format(variables=variables_list[:-1]))


def build_handler_block(f, key_binding, block_comment):
    first = True

    for item in handlers_block:
        var = handlers_block.get(item)
        binding = var['binding']

        if binding is not None and binding == key_binding:
            if first:
                first = False
                f.write("\n")
                write_comment_block(f, block_comment)
                f.write("\n")

            properties = var.get('properties')
            if properties is not None:
                name = properties.get('name')
                template_key = var.get('handler_template')
                f.write(templates[template_key].format(name=name))


def write_main():
    with open('declarations.h', 'w') as df:
        with open("main.c", "w") as main_c:

            main_c.write(templates["header"])

            build_signatures(main_c)
            build_timer_block(main_c)
            build_variable_block(main_c, "DEVICE_TWIN_BINDING", "device_twin")
            build_variable_block(main_c, "DIRECT_METHOD_BINDING", "direct_method")
            build_variable_block(main_c, "GPIO_BINDING", "gpio")
            build_handler_block(main_c, "DEVICE_TWIN_BINDING", "Implement your device twins code")
            build_handler_block(main_c, "DIRECT_METHOD_BINDING", "Implement your direct method code")
            build_handler_block(main_c, "TIMER_BINDING", "Implement your timer code")

            # main_c.write(templates["footer"].format(
            #     gpio_bindings_open=gpio_bindings_open,
            #     device_twins_subscribe=device_twins_subscribe,
            #     direct_method_subscribe=direct_method_subscribe,
            #     timer_start=timer_start,
            #     device_twins_unsubscribe=device_twins_unsubscribe,
            #     direct_method_unsubscribe=direct_method_unsubscribe,
            #     timer_stop=timer_stop,
            #     gpio_bindings_close=gpio_bindings_close,
            #     open_bracket=open_bracket, close_bracket=close_bracket))


# This is for two special case handlers - Watchdog and PublishTelemetry
# def bind_templated_handlers():
#     # Add in watchdog
#     sig = "static void Watchdog_handler(EventLoopTimer *eventLoopTimer)"
#     item = {'binding': 'TIMER_BINDING', 'properties': {
#         'period': '.period = {15, 0}, ', 'template': 'watchdog', 'name': 'Watchdog'}}

#     signatures.update({sig: item})

#     key = "tmr_Watchdog"
#     value = templates['timer_binding_template'].format(
#         name="Watchdog", period='.period = {15, 0},', open_bracket=open_bracket, close_bracket=close_bracket)

#     timer_block.update({key: value})

#     # add in publish
#     sig = "static void PublishTelemetry_handler(EventLoopTimer *eventLoopTimer)"
#     item = {'binding': 'TIMER_BINDING', 'properties': {
#         'period': '.period = {5, 0}, ', 'template': 'publish', 'name': 'PublishTelemetry'}}

#     signatures.update({sig: item})

#     key = "tmr_PublishTelemetry"
#     value = templates['timer_binding_template'].format(
#         name="PublishTelemetry", period='.period = {5, 0},', open_bracket=open_bracket, close_bracket=close_bracket)

#     timer_block.update({key: value})


def validate_schema():
    pass
    # TODO: app.json schema validation


load_templates()
build_buckets()
# bind_templated_handlers()

write_main()
