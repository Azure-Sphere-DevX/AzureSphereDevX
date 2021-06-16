import json
import pathlib
import win32file
import win32con
import os
import time

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

path_to_watch = "."  # look at the current directory
file_to_watch = "app.json"  # look for changes to a file called test.txt

FILE_LIST_DIRECTORY = 0x0001
hDir = win32file.CreateFile(
    path_to_watch,
    FILE_LIST_DIRECTORY,
    win32con.FILE_SHARE_READ | win32con.FILE_SHARE_WRITE,
    None,
    win32con.OPEN_EXISTING,
    win32con.FILE_FLAG_BACKUP_SEMANTICS,
    None
)

ACTIONS = {1: "Created", 2: "Deleted", 3: "Updated", 4: "Renamed from something", 5: "Renamed to something"}

bindings_init = None
device_twins_updates = None
device_twin_variables = None
bindings = None

dt = None

builders = None


def load_bindings():
    global bindings, signatures, timer_block, variables_block, handlers_block, templates, bindings_init, builders, dt

    signatures = {}
    timer_block = {}
    variables_block = {}
    handlers_block = {}
    templates = {}

    bindings_init = {"tmr": False, "dm": False, "dt": False, "gpio": False}

    time.sleep(0.5)
    with open('app_v2.json', 'r') as j:
        data = json.load(j)
    j.close()

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
        name = sig['properties'].get('name')
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
        properties = var.get('properties')
        if properties is not None:
            name = properties.get('name')
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

            properties = var.get('properties')
            if properties is not None:
                name = properties.get('name')
                variables_list += " &" + prefix + "_" + name + ","

                pin = get_value(properties, 'pin')
                initialState = get_value(properties, 'initialState')
                invert = "true" if get_value(properties, 'invertPin') else "false"
                twin_type = get_value(properties, 'twin_type')

                template_key = var.get('var_template')

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
                f.write("\n")
                write_comment_block(f, block_comment)
                f.write("\n")

            properties = var.get('properties')
            if properties is not None:
                name = properties.get('name')
                template_key = var.get('handler_template')
                f.write(templates[template_key].format(name=name, device_twins_updates=device_twins_updates,
                                                       device_twin_variables=device_twin_variables))
                f.write("\n")
                f.write("\n")


def write_main():
    with open('../DevX_Generated/declarations.h', 'w') as df:
        with open("../DevX_Generated/main.c", "w") as main_c:

            main_c.write(templates["header"])

            df.write(templates["declarations"])

            render_signatures(df)
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

            main_c.write(templates["footer"])


def validate_schema():
    pass
    # TODO: app.json schema validation


def process_update():

    load_bindings()
    load_templates()
    build_buckets()
    result = dt.build_publish_device_twins()
    # bind_templated_handlers()

    write_main()

process_update()



# # Wait for new data and call ProcessNewData for each new chunk that's written
# while 1:
#     # Wait for a change to occur
#     results = win32file.ReadDirectoryChangesW(
#         hDir,
#         1024,
#         False,
#         win32con.FILE_NOTIFY_CHANGE_LAST_WRITE,
#         None,
#         None
#     )

#     # For each change, check to see if it's updating the file we're interested in
#     for action, file in results:
#         full_filename = os.path.join(path_to_watch, file)
#         print(ACTIONS.get(action, "Unknown"))
#         if file == file_to_watch and action == 3:
#             process_update()
