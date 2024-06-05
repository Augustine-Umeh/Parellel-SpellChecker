import sys

def replaceApostrophe(input_filename):
    with open(input_filename, 'r', encoding='utf-8') as inputfile:
        lines = inputfile.readlines()

    output_filename = 'processed_' + input_filename
    
    with open(output_filename, 'w', encoding='utf-8') as outfile:
        for line in lines:
            
            # Replace curly apostrophe with straight apostrophe
            modified_line = line.replace('’', "'")
            outfile.write(modified_line)
            
    return output_filename

if __name__ == "__main__":
    if len(sys.argv) > 1:
        input_filename = sys.argv[1]
        processed_filename = replaceApostrophe(input_filename)
        print(processed_filename)
    else:
        print("No input file provided.")
