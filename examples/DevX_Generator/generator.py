import json
import pathlib

# declare dictionaries
signatures = {}
device_twin_block = {}
direct_method_block = {}
timer_block = {}
gpio_block = {}
templates = {}

open_bracket = '{'
close_bracket = '}'

binding_set_templates = {
    "device_twins": {
        "template": "static DX_DEVICE_TWIN_BINDING* device_twin_bindings[] = {open_bracket}{variables} {close_bracket};",
        "block": "Azure IoT Device Twin Bindings",
        "set": "\n// All device twins listed in device_twin_bindings will be subscribed to in the InitPeripheralsAndHandlers function\n"},
    "direct_methods": {
        "template": "static DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {open_bracket}{variables} {close_bracket};",
        "block": "Azure IoT Direct Method Bindings",
        "set": "\n// All direct methods referenced in direct_method_bindings will be subscribed to in the InitPeripheralsAndHandlers function\n"},
    "timers": {
        "template": "static DX_TIMER_BINDING *timer_bindings[] = {open_bracket}{variables} {close_bracket};",
        "block": "Timer Bindings",
        "set": "\n// All timers referenced in timer_bindings with be opened in the InitPeripheralsAndHandlers function\n"
    },
    "gpios": {
        "template": "static DX_GPIO_BINDING *gpio_bindings[] = {open_bracket}{variables} {close_bracket};",
        "block": "GPIO Bindings",
        "set": "\n// All GPIOs referenced in gpio_bindings with be opened in the InitPeripheralsAndHandlers function\n"
    }
}

gpio_output_binding_template = 'static DX_GPIO_BINDING gpio_{name} = {open_bracket} .pin = {pin}, .name = "{name}", .direction = {direction}, .initialState = {initialState}{invert}{close_bracket};'
gpio_input_binding_template = 'static DX_GPIO_BINDING gpio_{name} = {open_bracket} .pin = {pin}, .name = "{name}", .direction = {direction} {close_bracket};'
timer_binding_template = 'static DX_TIMER_BINDING  tmr_{name} = {open_bracket}{period} .name = "{name}", .handler = {name}_handler {close_bracket};'
direct_method_binding_template = 'static DX_DIRECT_METHOD_BINDING dm_{name} = {open_bracket} .methodName = "{name}", .handler = {name}_handler {close_bracket};'
device_twin_binding_template = 'static DX_DEVICE_TWIN_BINDING dt_{name} = {open_bracket} .twinProperty = "{name}", .twinType = {twin_type}{handler}{close_bracket};'

device_twin_types = {"integer": "DX_TYPE_INT", "float": "DX_TYPE_FLOAT",
                     "double": "DX_TYPE_DOUBLE", "boolean": "DX_TYPE_BOOL",  "string": "DX_TYPE_STRING"}

device_twin_state = {
    "integer": '"%d\\n", *(int*)deviceTwinBinding->twinState',
    "float": '"%f\\n", *(float*)deviceTwinBinding->twinState',
    "double": '"%f\\n", *(float*)deviceTwinBinding->twinState',
    "boolean": '"%s\\n", *(bool*)deviceTwinBinding->twinState ? "true": "false"',
    "string": '"%s\\n", (char*)deviceTwinBinding->twinState'
}

gpio_init = {"low": "GPIO_Value_Low", "high": "GPIO_Value_High"}
gpio_direction = {"input": "DX_INPUT",
                  "output": "DX_OUTPUT", "unknown": "DX_DIRECTION_UNKNOWN"}

with open('app.json', 'r') as j:
    data = json.load(j)

devicetwins = (
    elem for elem in data if elem['binding'] == 'DEVICE_TWIN_BINDING')
directmethods = (
    elem for elem in data if elem['binding'] == 'DIRECT_METHOD_BINDING')
timers = (elem for elem in data if elem['binding'] == 'TIMER_BINDING')
gpios = (elem for elem in data if elem['binding'] == 'GPIO_BINDING')


def load_templates():
    for path in pathlib.Path("templates").iterdir():
        if path.is_file():
            template_key = path.name.split(".")[0]
            with open(path, "r") as tf:
                templates.update({template_key: tf.read()})


def gen_gpios():
    for item in gpios:
        properties = item.get('properties')
        name = properties['name']
        direction = gpio_direction[properties['direction']]

        if properties.get('invertPin') is not None:
            if properties.get('invertPin'):
                invert = ", .invertPin = true "
            else:
                invert = ", .invertPin = false "
        else:
            invert = ""

        key = "gpio_{name}".format(name=name)

        if direction == "DX_INPUT":
            value = gpio_input_binding_template.format(
                pin=properties['pin'], name=name, direction=direction, open_bracket=open_bracket, close_bracket=close_bracket)

            gpio_block.update({key: value})

            # start generate a timer and associate current input gpio

            key = "tmr_{name}".format(name=properties['name'])
            value = timer_binding_template.format(
                name=properties['name'], period='.period = {0, 200000000}, ', open_bracket=open_bracket, close_bracket=close_bracket)

            timer_block.update({key: value})

            sig = "static void {name}_handler(EventLoopTimer *eventLoopTimer)".format(
                name=properties['name'])

            item = {'binding': 'TIMER_BINDING', 'properties': {
                'period': '.period = {0, 200000000}, ', 'template': 'gpio_input', 'name': '{name}'.format(name=name)}}

            signatures.update({sig: item})

            # finish generating a timer and associate current input gpio

            return

        if direction == "DX_OUTPUT":

            value = gpio_output_binding_template.format(pin=properties['pin'], name=name, direction=direction,
                                                        initialState=gpio_init.get(properties['initialState']), invert=invert, open_bracket=open_bracket, close_bracket=close_bracket)

            gpio_block.update({key: value})

            # TODO Generate timer to associate current output gpio


def gen_timers():
    for item in timers:
        properties = item.get('properties')

        if properties.get('period') is None:
            period = ""
        else:
            period = " .period = {open_bracket}{period}{close_bracket},".format(
                period=properties.get('period'), open_bracket=open_bracket, close_bracket=close_bracket)

        key = "tmr_{name}".format(name=properties['name'])
        value = timer_binding_template.format(
            name=properties['name'], period=period, open_bracket=open_bracket, close_bracket=close_bracket)

        timer_block.update({key: value})

        sig = "static void {name}_handler(EventLoopTimer *eventLoopTimer)".format(
            name=properties['name'])

        if item["properties"].get("template") is None:
            if period == "":
                item["properties"].update({"template": "timer_oneshot"})
            else:
                item["properties"].update({"template": "timer_periodic"})

            signatures.update({sig: item})


def gen_direct_method():
    for item in directmethods:
        properties = item.get('properties')
        name = properties['name']

        sig = "static DX_DIRECT_METHOD_RESPONSE_CODE {name}_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)".format(
            name=properties['name'])

        item["properties"].update({"template": "direct_method"})

        signatures.update({sig: item})

        key = "dm_{name}".format(name=name)
        value = direct_method_binding_template.format(
            name=name, open_bracket=open_bracket, close_bracket=close_bracket)

        direct_method_block.update({key: value})


def gen_twin_handler(item):
    properties = item.get('properties')

    sig = "static void {name}_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding)".format(
        name=properties['name'])

    item["properties"].update({"template": "device_twin"})
    signatures.update({sig: item})


def generate_twins():
    for item in devicetwins:
        properties = item.get('properties')

        if (properties.get('cloud2device')) is not None and properties.get('cloud2device'):
            handler = ', .handler = {name}_handler'.format(
                name=properties['name'])
            gen_twin_handler(item)
        else:
            handler = ""

        key = "dt_{name}".format(name=properties['name'])
        value = device_twin_binding_template.format(name=properties['name'], twin_type=device_twin_types.get(
            properties['type']),  open_bracket=open_bracket, close_bracket=close_bracket, handler=handler)

        device_twin_block.update({key: value})


def write_comment_block(f, msg):
    f.write("/****************************************************************************************\n")
    f.write("* {msg}\n".format(msg=msg))
    f.write("****************************************************************************************/\n")


def write_signatures(f):
    if len(signatures) > 0:
        for item in sorted(signatures):
            f.write(item)
            f.write(";")
            f.write("\n")

    f.write("\n")


def build_variable_set(list, template):
    variables_list = ""
    for name in list:
        variables_list += " &" + name + ","

    if variables_list != "":
        return template.format(open_bracket=open_bracket, variables=variables_list[:-1], close_bracket=close_bracket)
    else:
        return None


def write_variables_template(f, list, set_template):
    if len(list) > 0:
        write_comment_block(f, set_template["block"])
        for item in sorted(list):
            f.write(list[item])
            f.write("\n")

    variables = build_variable_set(sorted(list), set_template["template"])
    if variables != "":
        f.write(set_template["set"])
        f.write(variables)
        f.write("\n\n")


def write_variables(f):

    write_variables_template(f, device_twin_block,
                             binding_set_templates["device_twins"])
    write_variables_template(
        f, direct_method_block, binding_set_templates["direct_methods"])
    write_variables_template(f, timer_block, binding_set_templates["timers"])
    write_variables_template(f, gpio_block, binding_set_templates["gpios"])


def write_handler_template(f, binding_key):
    for item in sorted(signatures):
        if signatures[item] is not None and signatures[item]["binding"] == binding_key:
            template_key = signatures[item]["properties"]["template"]
            template = templates[template_key]
            if template_key == "gpio_input":
                value = signatures[item]
                gpio_name = signatures[item]["properties"]["name"]
                f.write(template.format(name=item, gpio_name=gpio_name,
                                        open_bracket=open_bracket, close_bracket=close_bracket))
            elif template_key == "device_twin":
                twin_state = device_twin_state[signatures[item]
                                               ["properties"]["type"]]
                f.write(template.format(name=item, twin_state=twin_state,
                                        open_bracket=open_bracket, close_bracket=close_bracket))
            else:
                property_name = signatures[item]["properties"]["name"]
                f.write(template.format(
                    name=item, property_name=property_name, open_bracket=open_bracket, close_bracket=close_bracket))
            f.write("\n")


def write_handlers(f):
    f.write("\n")

    write_comment_block(f, "Implement your timer code")
    f.write("\n")
    write_handler_template(f, "TIMER_BINDING")

    f.write("\n")
    write_comment_block(f, "Implement your device twins code")
    f.write("\n")
    write_handler_template(f, "DEVICE_TWIN_BINDING")

    f.write("\n")
    write_comment_block(f, "Implement your direct method code")
    f.write("\n")
    write_handler_template(f, "DIRECT_METHOD_BINDING")


def write_main():
    with open("main.c", "w") as main_c:
        main_c.write(templates["header"])

        write_signatures(main_c)
        write_variables(main_c)
        write_handlers(main_c)

        main_c.write(templates["footer"])


# This is for two special case handlers - Watchdog and PublishTelemetry
def bind_templated_handlers():
    # Add in watchdog
    sig = "static void Watchdog_handler(EventLoopTimer *eventLoopTimer)"
    item = {'binding': 'TIMER_BINDING', 'properties': {
        'period': '.period = {15, 0}, ', 'template': 'watchdog', 'name': 'Watchdog'}}

    signatures.update({sig: item})

    key = "tmr_Watchdog"
    value = timer_binding_template.format(
        name="Watchdog", period='.period = {15, 0},', open_bracket=open_bracket, close_bracket=close_bracket)

    timer_block.update({key: value})

    # add in publish
    sig = "static void PublishTelemetry_handler(EventLoopTimer *eventLoopTimer)"
    item = {'binding': 'TIMER_BINDING', 'properties': {
        'period': '.period = {5, 0}, ', 'template': 'publish', 'name': 'PublishTelemetry'}}

    signatures.update({sig: item})

    key = "tmr_PublishTelemetry"
    value = timer_binding_template.format(
        name="PublishTelemetry", period='.period = {5, 0},', open_bracket=open_bracket, close_bracket=close_bracket)

    timer_block.update({key: value})


load_templates()
generate_twins()
gen_direct_method()
gen_timers()
gen_gpios()
bind_templated_handlers()

write_main()
