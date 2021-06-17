import hashlib

# This class will remove unmodified generated functions and associated signatures plus reference variables

class Clean():
    def __init__(self, ):
        self.removed_blocks = []
        self.clean_main(filename='../DevX_Generated/main.c')
        self.clean_declarations(filename='../DevX_Generated/declarations.h')    

    def clean_main(self, filename):

        lines = []

        with open(filename, 'r') as j:
            lines = j.readlines()

        block_found = False
        block_lines = []
        
        current_block = ''
        block_chars = ''
        output_lines = []
        md5_hash = ''

        for line in lines:
            if not block_found and line.startswith('/// DX_GENERATED_BEGIN'):
                md5_hash = line.split('MD5:')[1].split('\n')[0]
                block_found = True
                current_block = line[:-1]
                block_lines = []
                block_chars = ''
            elif block_found and line.startswith('/// DX_GENERATED_END'):
                block_found = False;            
                hash_object = hashlib.md5(block_chars[:-1].encode())
                if md5_hash != hash_object.hexdigest():
                    for block_line in block_lines:
                        output_lines.append(block_line)
                    block_lines = []
                    block_chars = ''
                else:
                    self.removed_blocks.append(current_block)
            elif block_found:
                block_lines.append(line)
                block_chars += line
            else:
                output_lines.append(line)


        with open(filename, 'w') as j:
            j.writelines(output_lines)


    # Function cleans out signatures and variables
    def clean_declarations(self, filename):

        output_lines = []
        variables_found = []

        with open(filename, 'r') as j:
            lines = j.readlines()

            # Remove signature and binding

            for line in lines:
                found_handler_reference = False
                for item in self.removed_blocks:
                    name = item.split('ID:')[1].split(' ')[0] + "_handler"
                    if name in line:
                        found_handler_reference = True
                        if name + '(' in line:   # Is this a forward signature
                            pass
                        elif 'BINDING ' in line:   # Is this a binding declaration
                            if len(line.split('BINDING ')) > 0:
                                variable_name = line.split('BINDING ')[1].split(' ')[0]
                                variables_found.append(variable_name)
                        break
                if not found_handler_reference:
                    output_lines.append(line)


        variable_name = ''
        final_lines = []

        # remove variables from the bindings

        for line in output_lines:
            for variable in variables_found:
                if '&' + variable in line:
                    variable_name = variable
                    line = line.replace('&' + variable_name + ', ', '')
                    line = line.replace('&' + variable_name, '')

            final_lines.append(line)


        # Finally write out the updated declarations file
        with open(filename, 'w') as j: 
            j.writelines(final_lines)  


clean = Clean()