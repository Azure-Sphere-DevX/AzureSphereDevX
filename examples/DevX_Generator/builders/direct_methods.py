class Builder():
    def __init__(self, data, signatures, variables_block, handlers_block):
        self.bindings = list(elem for elem in data.get('bindings').get('direct_methods') if elem.get('enabled', True) == True)

        self.signatures = signatures
        self.variables_block = variables_block
        self.handlers_block = handlers_block

    def build(self):
        for binding in self.bindings:
            binding.update({'binding': 'DIRECT_METHOD_BINDING'})
            properties = binding.get('properties')
            key = properties.get('name')

            binding.update({"sig_template": 'sig_direct_method'})
            self.signatures.update({key: binding})

            binding.update({"var_template": 'declare_direct_method'})
            self.variables_block.update({key: binding})

            binding.update({"handler_template": 'handler_direct_method'})
            self.handlers_block.update({key: binding})