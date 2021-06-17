import json
import pathlib
import time
import watcher
import hashlib

from builders import device_twin
from builders import direct_methods
from builders import timer_bindings
from builders import gpio_in_bindings
from builders import gpio_out_bindings
from builders import general_bindings

# declare dictionaries
signatures = {}
timer_block = {}
variables_block = {}
handlers_block = {}
templates = {}

bindings_init = None
device_twins_updates = None
device_twin_variables = None

dt = None

builders = None


def load_bindings():
    global signatures, timer_block, variables_block, handlers_block, templates, bindings_init, builders, dt

    signatures = {}
    timer_block = {}
    variables_block = {}
    handlers_block = {}
    templates = {}

    bindings_init = {"tmr": False, "dm": False, "dt": False, "gpio": False}

    time.sleep(0.5)
    with open('app_model.json', 'r') as j:
        data = json.load(j)

    dt = device_twin.Builder(data, signatures=signatures, variables_block=variables_block, handlers_block=handlers_block)
    dm = direct_methods.Builder(data, signatures=signatures, variables_block=variables_block, handlers_block=handlers_block)
    timers = timer_bindings.Builder(data, signatures=signatures, variables_block=variables_block, handlers_block=handlers_block, timer_block=timer_block)
    gpio_input = gpio_in_bindings.Builder(data, signatures=signatures, variables_block=variables_block,
                                    handlers_block=handlers_block, timer_block=timer_block)
    gpio_output = gpio_out_bindings.Builder(data, signatures=signatures, variables_block=variables_block,
                                    handlers_block=handlers_block, timer_block=timer_block)
    general = general_bindings.Builder(data, signatures=signatures, variables_block=variables_block,
                            handlers_block=handlers_block, timer_block=timer_block)

    builders = [dt, dm, timers, gpio_input, gpio_output, general]


def get_value(properties, key):
    if properties is None or key is None:
        return ""
    return "" if properties.get(key) is None else properties.get(key)


def write_comment_block(f, msg):
    f.write("\n/****************************************************************************************\n")
    f.write("* {msg}\n".format(msg=msg))
    f.write("****************************************************************************************/\n")



def load_templates():
    for path in pathlib.Path("templates").iterdir():
        if path.is_file():
            template_key = path.name.split(".")[0]
            with open(path, "r") as tf:
                templates.update({template_key: tf.read()})


def build_buckets():
    for builder in builders:
        builder.build()


def render_signatures(f):
    for item in sorted(signatures):
        sig = signatures.get(item)
        name = sig.get('name')
        template_key = sig.get('sig_template')
        f.write(templates[template_key].format(name=name))


def render_timer_block(f):
    if len(timer_block) <= 0:
        return
    bindings_init.update({"tmr": True})
    variables_list = ""
    write_comment_block(f, templates['comment_block_timer'])
    for item in sorted(timer_block):
        var = timer_block.get(item)

        name = var.get('name')
        properties = var.get('properties')

        if properties is not None:
            variables_list += " &" + "tmr" + "_" + name + ","
            period = "{0, 0}" if get_value(properties, 'period') == "" else get_value(properties, 'period')
            template_key = var.get('timer_template')
            f.write(templates[template_key].format(name=name, period=period,
                                                   device_twins_updates=device_twins_updates,
                                                   device_twin_variables=device_twin_variables))

    if variables_list != "":
        f.write(templates['declare_bindings_timer'].format(variables=variables_list[:-1]))


def render_variable_block(f, key_binding, key_block, prefix):
    first = True
    variables_list = ""

    for item in sorted(variables_block):
        var = variables_block.get(item)
        binding = var['binding']

        if binding is not None and binding == key_binding:
            if first:
                first = False
                write_comment_block(f, templates['comment_block_' + key_block])
                bindings_init.update({prefix: True})

            name = var.get('name')
            properties = var.get('properties')

            variables_list += " &" + prefix + "_" + name + ","
            template_key = var.get('var_template')

            # if properties is not None:
            pin = get_value(properties, 'pin')
            initialState = get_value(properties, 'initialState')
            invert = "true" if get_value(properties, 'invertPin') else "false"
            twin_type = get_value(properties, 'twin_type')

            f.write(templates[template_key].format(
                name=name, pin=pin,
                initialState=initialState,
                invert=invert,
                twin_type=twin_type
            ))

    if variables_list != "":
        f.write(templates['declare_bindings_' + key_block].format(variables=variables_list[:-1]))


def render_handler_block(f, key_binding, block_comment):
    first = True

    for item in sorted(handlers_block):
        var = handlers_block.get(item)
        binding = var['binding']

        # if var.get('handler_template') is not None:

        if binding is not None and binding == key_binding:
            if first:
                first = False
                # f.write("\n")
                # write_comment_block(f, block_comment)
                # f.write("\n")

            name = var.get('name')
            properties = var.get('properties')

            # if properties is not None:

            template_key = var.get('handler_template')
            block_chars = templates[template_key].format(name=name, device_twins_updates=device_twins_updates,
                                                    device_twin_variables=device_twin_variables)

            hash_object = hashlib.md5(block_chars.encode())

            f.write("\n")
            f.write('/// DX_GENERATED_BEGIN_DO_NOT_REMOVE ID:{name} MD5:{hash}\n'.format(name=name, hash=hash_object.hexdigest()))
            
            f.write(templates[template_key].format(name=name, device_twins_updates=device_twins_updates,
                                                    device_twin_variables=device_twin_variables))
            f.write('\n/// DX_GENERATED_END_DO_NOT_REMOVE ID:{name}'.format(name=name))
            f.write("\n\n")


def write_main():
    global device_twins_updates, device_twin_variables
    with open('../DevX_Generated/declarations.h', 'w') as df:
        with open("../DevX_Generated/main.c", "w") as main_c:

            main_c.write(templates["header"])

            df.write(templates["declarations"])

            render_signatures(df)
            device_twins_updates, device_twin_variables = dt.build_publish_device_twins()
            # device_twins_updates = result[0]
            # device_twin_variables = result[1]

            render_variable_block(df, "GENERAL_BINDING", "general", "gen")
            render_timer_block(df)

            render_variable_block(df, "DEVICE_TWIN_BINDING", "device_twin", "dt")
            render_variable_block(df, "DIRECT_METHOD_BINDING", "direct_method", "dm")
            render_variable_block(df, "GPIO_BINDING", "gpio", "gpio")
            df.write(templates["declarations_footer"])

            render_handler_block(main_c, "GPIO_BINDING", "Implement your gpio code")
            render_handler_block(main_c, "DEVICE_TWIN_BINDING", "Implement your device twins code")
            render_handler_block(main_c, "DIRECT_METHOD_BINDING", "Implement your direct method code")
            render_handler_block(main_c, "TIMER_BINDING", "Implement your timer code")
            render_handler_block(main_c, "GENERAL_BINDING", "Implement general code")



def validate_schema():
    pass
    # TODO: app.json schema validation


def process_update():

    load_bindings()
    load_templates()
    build_buckets()
    write_main()

watch_file = 'app_model.json'

# watcher = Watcher(watch_file)  # simple
watcher = watcher.Watcher(watch_file, process_update)  # also call custom action function
watcher.watch()  # start the watch going