class Builder():
    def __init__(self, data, signatures, variables_block, handlers_block, timer_block):
        self.bindings = list(elem for elem in data.get('bindings').get('general') if elem.get('enabled', True) == True)

        self.signatures = signatures
        self.variables_block = variables_block
        self.handlers_block = handlers_block
        self.timer_block = timer_block

    def get_value(self, properties, key):
        return "" if properties.get(key) is None else properties.get(key)

    def build(self):
        for binding in self.bindings:
            binding.update({'binding': 'GENERAL_BINDING'})
            properties = binding.get('properties')
            key = properties.get('name')

            binding.update({"sig_template": 'sig_timer'})
            self.signatures.update({key: binding})
            properties = binding.get('properties')

            binding.update({"publish": 'handler_timer_periodic'})
            self.handlers_block.update({key: binding})

            if binding.get('var_template') is not None:
                self.variables_block.update({key: binding})

            if self.get_value(properties, 'period') != "":
                binding.update({"timer_template": 'declare_timer_periodic'})
                properties.update({"period": "{" + self.get_value(properties, 'period') + "}"})
                self.timer_block.update({key: binding})