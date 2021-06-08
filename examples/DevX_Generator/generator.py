import json

with open("templates/device_twin_handler.c.template", "r") as t:
    device_twin_handler_tempate = t.read()

with open("templates/direct_method_handler.c.template", "r") as t:
    direct_method_handler_tempate = t.read()

with open("templates/timer_periodic_handler.c.template", "r") as t:
    timer_periodic_handler_tempate = t.read()

with open("templates/timer_oneshot_handler.c.template", "r") as t:
    timer_oneshot_handler_tempate = t.read()

open_bracket = '{'
close_bracket = '}'

gpio_binding_template = 'static DX_GPIO_BINDING gpio_{name} = {open_bracket} .pin = {pin}, .name = "{name}", .direction = {direction}, .initialState = {initialState}{invert}{close_bracket};'
timer_binding_template = 'static DX_TIMER_BINDING  tmr_{name} = {open_bracket}{period} .name = "{name}" .handler = {name}_handler {close_bracket};'
direct_method_binding_template = 'static DX_DIRECT_METHOD_BINDING dm_{name} = {open_bracket} .methodName = "{name}", .handler = {name}_handler {close_bracket};'
device_twin_binding_template = 'static DX_DEVICE_TWIN_BINDING dt_{name} = {open_bracket} .twinProperty = "{name}", .twinType = {twin_type} {handler}{close_bracket};'

device_twin_variable_template = "static DX_DEVICE_TWIN_BINDING* device_twin_bindings[] = {open_bracket}{variables} {close_bracket};"
direct_method_variable_template = "static DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {open_bracket}{variables} {close_bracket};"
timer_variable_template = "static DX_TIMER_BINDING *timer_bindings[] = {open_bracket}{variables} {close_bracket};"
gpio_variable_template = "static DX_GPIO_BINDING *gpio_bindings[] = {open_bracket}{variables} {close_bracket};"


device_twin_variable_list = []
device_twin_handler_list = []
device_twin_list = []

direct_method_variable_list = []
direct_method_handler_list = []
direct_method_list = []

timer_variable_list = []
timer_handler_list = []
timer_list = []

gpio_variable_list = []
gpio_list = []
gpio_variables_list = []

types = {"integer": "DX_TYPE_INT", "float": "DX_TYPE_FLOAT",
         "double": "DX_TYPE_DOUBLE", "boolean": "DX_TYPE_BOOL",  "string": "DX_TYPE_STRING"}

gpio_init = {"low": "GPIO_Value_Low", "high": "GPIO_Value_High"}

f = open('app.json',)

data = json.load(f)

devicetwins = (
    elem for elem in data if elem['binding'] == 'DEVICE_TWIN_BINDING')
directmethods = (
    elem for elem in data if elem['binding'] == 'DIRECT_METHOD_BINDING')
timers = (elem for elem in data if elem['binding'] == 'TIMER_BINDING')
gpios = (elem for elem in data if elem['binding'] == 'GPIO_BINDING')


def gen_gpios():
    global gpio_variables_list
    for item in gpios:
        properties = item.get('properties')

        if properties.get('invertPin') is not None:
            if properties.get('invertPin'):
                invert = ", .invertPin = true "
            else:
               invert = ", .invertPin = false " 
        else:
            invert = ""

        gpio_variable_list.append(
            "gpio_{name}".format(name=properties['name']))
        gpio_list.append(gpio_binding_template.format(pin=properties['pin'], name=properties['name'], direction=properties['direction'],
                                                      initialState=gpio_init.get(properties['initialState']), invert=invert, open_bracket=open_bracket, close_bracket=close_bracket))


def gen_timers():
    global timers_variables_list
    for item in timers:
        properties = item.get('properties')

        if properties.get('period') is None:
            period = ""
        else:
            period = " .period = {open_bracket}{period}{close_bracket},".format(period = properties.get('period'), open_bracket=open_bracket, close_bracket=close_bracket)

        timer_variable_list.append("dm_{name}".format(name=properties['name']))
        timer_list.append(timer_binding_template.format(
            name=properties['name'], period=period, open_bracket=open_bracket, close_bracket=close_bracket))
        timer_handler_list.append(
            "static void {name}_handler(EventLoopTimer *eventLoopTimer)".format(name=properties['name']))


def gen_direct_method():
    for item in directmethods:
        properties = item.get('properties')
        direct_method_variable_list.append(
            "dm_{name}".format(name=properties['name']))
        direct_method_list.append(direct_method_binding_template.format(
            name=properties['name'], open_bracket=open_bracket, close_bracket=close_bracket))
        direct_method_handler_list.append(
            "static DX_DIRECT_METHOD_RESPONSE_CODE {name}_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)".format(name=properties['name']))


def gen_twin_handler(item):
    global device_twin_handler_list
    properties = item.get('properties')
    handler = '.handler = {name}_handler '.format(name=properties['name'])
    device_twin_handler_list.append(
        "static void {name}_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding)".format(name=properties['name']))


def generate_twins():
    for item in devicetwins:
        properties = item.get('properties')

        if (properties.get('cloud2device')) is not None and properties.get('cloud2device'):
            handler = '.handler = {name}_handler '.format(
                name=properties['name'])
            gen_twin_handler(item)
        else:
            handler = ""

        device_twin_variable_list.append(
            "dt_{name}".format(name=properties['name']))
        device_twin_list.append(device_twin_binding_template.format(name=properties['name'], twin_type=types.get(
            properties['type']),  open_bracket=open_bracket, close_bracket=close_bracket, handler=handler))


def gen_variable_list(variable_list, template):
    variables_list = ""
    for name in variable_list:
        variables_list += " &" + name + ","

    if variables_list != "":
        return template.format(open_bracket=open_bracket, variables=variables_list[:-1], close_bracket=close_bracket)
    else:
        return None


def write_comment_block(f, msg):
    f.write("/****************************************************************************************\n")
    f.write("* {msg}\n".format(msg=msg))
    f.write("****************************************************************************************/\n")


def write_signatures(f):
    # Write Device Twin Signatures
    if len(device_twin_handler_list) > 0:
        for item in device_twin_handler_list:
            f.write(item)
            f.write(";")
            f.write("\n")

    # Write Direct Method Signatures
    if len(direct_method_handler_list) > 0:
        for item in direct_method_handler_list:
            f.write(item)
            f.write(";")
            f.write("\n")

    if len(timer_handler_list) > 0:
        for item in timer_handler_list:
            f.write(item)
            f.write(";")
            f.write("\n")

    f.write("\n")


def write_variables_template(f, list, comment, variable_list, variable_template, variable_comment):
    write_comment_block(f, comment)
    if len(list) > 0:
        for item in list:
            f.write(item)
            f.write("\n")

    variables = gen_variable_list(variable_list, variable_template)
    if variables != "":
        f.write(variable_comment)
        f.write(variables)
        f.write("\n\n")


def write_variables(f):
    write_variables_template(f, device_twin_list, "Azure IoT Device Twin Bindings", device_twin_variable_list, device_twin_variable_template,
        "\n// All device twins listed in device_twin_bindings will be subscribed to in the InitPeripheralsAndHandlers function\n")
    
    write_variables_template(f, direct_method_list, "Azure IoT Direct Method Bindings", direct_method_variable_list, direct_method_variable_template,
        "\n// All direct methods referenced in direct_method_bindings will be subscribed to in the InitPeripheralsAndHandlers function\n")
    
    write_variables_template(f, timer_list, "Timer Bindings", timer_variable_list, timer_variable_template,
        "\n// All timers referenced in timers with be opened in the InitPeripheralsAndHandlers function\n")

    write_variables_template(f, gpio_list, "GPIO Bindings", gpio_variable_list, gpio_variable_template,
        "\n// All GPIOs referenced in gpio_set with be opened in the InitPeripheralsAndHandlers function\n")    


def write_handlers(f):
    write_comment_block(f, "Implementation code")
    f.write("\n")
    # Write Device Twin Handlers
    if len(device_twin_handler_list) > 0:
        for item in device_twin_handler_list:
            f.write(device_twin_handler_tempate.format(
                name=item, open_bracket=open_bracket, close_bracket=close_bracket))
            f.write("\n")

    # Write Direct Method Handlers
    if len(direct_method_handler_list) > 0:
        for item in direct_method_handler_list:
            f.write(direct_method_handler_tempate.format(
                name=item, open_bracket=open_bracket, close_bracket=close_bracket))
            f.write("\n")

    # Write timer Handlers
    if len(timer_handler_list) > 0:
        for item in timer_handler_list:
            if not item.startswith("static void PublishTelemetry_handler") and not item.startswith("static void Watchdog_handler"):
                f.write(timer_periodic_handler_tempate.format(
                    name=item, open_bracket=open_bracket, close_bracket=close_bracket))
                f.write("\n")


def write_main():
    with open("templates/header.c.template", "r") as f:
        data = f.read()

    with open("main.c", "w") as f:
        f.write(data)

        write_signatures(f)
        write_variables(f)
        write_handlers(f)

        with open("templates/footer.c.template", "r") as footer:
            f.write(footer.read())


generate_twins()
gen_direct_method()

measureTimer = {
    "binding": "TIMER_BINDING",
    "properties": {"period": "5, 0", "name": "PublishTelemetry"}
}
data.append(measureTimer)

watchdogTimer = {
    "binding": "TIMER_BINDING",
    "properties": {"period": "5, 0", "name": "Watchdog"}
}
data.append(watchdogTimer)

gen_timers()
gen_gpios()

write_main()
