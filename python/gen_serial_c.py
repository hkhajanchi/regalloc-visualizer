import random
import string
import argparse 

''' 
Creates a C program with the following structure

    - initialize a variable set with 8 members
    - update each variable using itself and the variable prior to it in the set 
        - i.e x1 += x0 + (some random offset) + (function parameter) 
        - this would be wrapped in a conditional 
    
    - use the next variable set and define the first member in terms of the last member of the previous set
        
'''
variables_per_set = 4
binary_ops = ["+","-"]

def create_serial_prog(fname, func_params, num_sets): 

    prog_lines = []
    prog_lines.append('char myFunc (char {},\n'.format(func_params[0]))

    # Create function header 
    for i in range(1, len(func_params)):

        if i == len(func_params)-1: 
            prog_lines.append('char {}) {{\n'.format(func_params[i]))
        else:
            prog_lines.append('char {},\n'.format(func_params[i]))


    prog_lines.append('char retVal;\n')
    variable_sets = []

    for i in range(num_sets):

        curr_variable_set = []
        set_letter = "{}{}{}{}".format(random.choice(string.ascii_letters),random.choice(string.ascii_letters),random.choice(string.ascii_letters),random.choice(string.ascii_letters))

        for i in range(variables_per_set):
            curr_variable_set.append("{}{}".format(set_letter,i))
            prog_lines.append("char {};\n".format(curr_variable_set[i]))

        variable_sets.append(curr_variable_set)

        initVal = "{} {} {}".format( random.choice(func_params), random.choice(binary_ops), random.randint(0,40))
        prog_lines.append("{} = {} + {};\n".format(curr_variable_set[0], random.choice(func_params), random.randint(0,40)))
        prog_lines.append("{}={}={}={};\n".format(curr_variable_set[1],curr_variable_set[2],curr_variable_set[3],initVal))

           
    prog_lines.append("\n");
    prog_lines.append("\n");
    prog_lines.append("\n");

    # Generate serial code
    for j,setw in enumerate(variable_sets):

        for i in range(len(setw)):
            prog_lines.append("if ({} > {}) {{\n".format(setw[i], random.randint(0,50)))

            if i == len(setw)-1:
                prog_lines.append("{} += {} - {} + {};\n".format( setw[i],setw[i-1], random.randint(0,20), random.choice(func_params)))

                if (j < len(variable_sets)-1):
                    prog_lines.append("{} += {} - {} + {};\n".format( variable_sets[j+1][0],setw[i], random.randint(0,20), random.choice(func_params)))
                else: 
                    prog_lines.append("{} += {} - {} + {};\n".format( variable_sets[j][0],setw[i], random.randint(0,30), random.choice(func_params)))

            elif (i == 0):
                prog_lines.append("{} += {} - {} + {};\n".format( setw[i+1],setw[i], random.randint(0,30), random.choice(func_params)))

            else: 
                prog_lines.append("{} += {} - {} + {};\n".format( setw[i],setw[i-1], random.randint(0,30), random.choice(func_params)))
                prog_lines.append("{} += {} - {} + {};\n".format( setw[i+1],setw[i], random.randint(0,30), random.choice(func_params)))

            prog_lines.append("}\n")

                        

    prog_lines.append("\n");
    prog_lines.append("\n");
    prog_lines.append("\n");


    prog_lines.append('retVal = {};\n'.format(variable_sets[len(variable_sets)-1][3]))
    prog_lines.append('return retVal;\n')
    prog_lines.append( '}\n'); 

    f = open(fname,'w')
    for line in prog_lines: 
        f.write(line)
    f.close()


if __name__ == '__main__': 

    parser = argparse.ArgumentParser(description="Generates a serialized C program for testing LLVM register allocation")
    parser.add_argument('--num_sets', metavar='N',type=int, help="the number of variable sets in the generated program.")
    parser.add_argument('--save_path', metavar='O',type=str,help="save path for the output file")
    args = parser.parse_args()

    func_params = ['x','n','a','b','c'] 
    fname = args.save_path
    num_sets = args.num_sets 

    create_serial_prog(fname,func_params,num_sets)

