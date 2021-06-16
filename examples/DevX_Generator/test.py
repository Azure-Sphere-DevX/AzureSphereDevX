import json
from builders import device_twin
from builders import direct_methods
from builders import timers
from builders import gpio_input
from builders import gpio_output
from builders import general

signatures = {}
timer_block = {}
variables_block = {}
handlers_block = {}
templates = {}

with open('app_v2.json', 'r') as j:
    data = json.load(j)

dt = device_twin.Builder(data, signatures=signatures, variables_block=variables_block, handlers_block=handlers_block)
dm = direct_methods.Builder(data, signatures=signatures, variables_block=variables_block, handlers_block=handlers_block)
timers = timers.Builder(data, signatures=signatures, variables_block=variables_block,
                        handlers_block=handlers_block, timer_block=timer_block)
gpio_input = gpio_input.Builder(data, signatures=signatures, variables_block=variables_block,
                                handlers_block=handlers_block, timer_block=timer_block)
gpio_output = gpio_output.Builder(data, signatures=signatures, variables_block=variables_block,
                                  handlers_block=handlers_block, timer_block=timer_block)
general = general.Builder(data, signatures=signatures, variables_block=variables_block,
                          handlers_block=handlers_block, timer_block=timer_block)

builders = [dt, dm, timers, gpio_input, gpio_output, general]

for builder in builders:
    builder.build()

print(signatures)
