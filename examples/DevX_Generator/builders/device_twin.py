

class Builder():
    def __init__(self, data, signatures, variables_block, handlers_block):
        self.bindings = list(elem for elem in data.get('bindings').get('device_twins') if elem.get('enabled', True) == True)

        self.signatures = signatures
        self.variables_block = variables_block
        self.handlers_block = handlers_block

        self.device_twin_types = {"integer": "DX_TYPE_INT", "float": "DX_TYPE_FLOAT", "double": "DX_TYPE_DOUBLE",
                                  "boolean": "DX_TYPE_BOOL",  "string": "DX_TYPE_STRING"}

        self.device_twin_template = {"integer": "handler_device_twin_int", "float": "handler_device_twin_float",
                                     "double": "handler_device_twin_double", "boolean": "handler_device_twin_bool",
                                     "string": "handler_device_twin_string"}


    def build(self):
        for binding in self.bindings:
            binding.update({'binding': 'DEVICE_TWIN_BINDING'})
            properties = binding.get('properties')
            key = properties.get('name')

            properties.update({'twin_type': self.device_twin_types[properties.get('type')]})

            if properties.get("cloud2device") == True:

                binding.update({"sig_template": 'sig_device_twin'})
                self.signatures.update({key: binding})

                binding.update({"var_template": 'declare_device_twin_with_handler'})
                self.variables_block.update({key: binding})

                binding.update({"handler_template": self.device_twin_template[properties.get('type')]})
                self.handlers_block.update({key: binding})

            else:
                binding.update({"var_template": 'declare_device_twin'})
                self.variables_block.update({key: binding})

    def build_publish_device_twins(self):
        # global device_twins_updates, device_twin_variables
        device_twins_updates = ''
        device_twin_variables = ''

        for item in self.variables_block:
            var = self.variables_block.get(item)
            if var.get("binding") == "DEVICE_TWIN_BINDING":
                properties = var.get('properties')
                property_type = properties.get("type", None)
                property_name = properties.get("name", None)

                if property_type == 'integer':
                    device_twin_variables += '    int {property_name}_value = 10;\n'.format(
                        property_name=property_name)
                    device_twins_updates += '        dx_deviceTwinReportState(&dt_{property_name}, &{property_name}_value);     // DX_TYPE_INT\n'.format(
                        property_name=property_name)
                elif property_type == 'float':
                    device_twin_variables += '    float {property_name}_value = 30.0f;\n'.format(
                        property_name=property_name)
                    device_twins_updates += '        dx_deviceTwinReportState(&dt_{property_name}, &{property_name}_value);     // DX_TYPE_FLOAT\n'.format(
                        property_name=property_name)
                elif property_type == 'double':
                    device_twin_variables += '    double {property_name}_value = 10.0;\n'.format(
                        property_name=property_name)
                    device_twins_updates += '        dx_deviceTwinReportState(&dt_{property_name}, &{property_name}_value);     // DX_TYPE_DOUBLE\n'.format(
                        property_name=property_name)
                elif property_type == 'boolean':
                    device_twin_variables += '    bool {property_name}_value = true;\n'.format(
                        property_name=property_name)
                    device_twins_updates += '        dx_deviceTwinReportState(&dt_{property_name}, &{property_name}_value);     // DX_TYPE_BOOL\n'.format(
                        property_name=property_name)
                elif property_type == 'string':
                    device_twin_variables += '    char {property_name}_value[] = "hello, world";\n'.format(
                        property_name=property_name)
                    device_twins_updates += '        dx_deviceTwinReportState(&dt_{property_name}, {property_name}_value);     // DX_TYPE_STRING\n'.format(
                        property_name=property_name)

        return (device_twins_updates, device_twin_variables)
