import time
import sys

class AdotsInterpreter:
    def __init__(self):
        self.variables = {}

    def run_line(self, line):
        line = line.strip()
        if not line or line.startswith("#"):
            return

        # NEW: input: varname: Prompt Text
        if line.startswith("input:"):
            parts = line.split(":", 2)
            var_name = parts[1].strip()
            prompt = parts[2].strip()
            self.variables[var_name] = input(prompt + " ")

        # NEW: wait: seconds
        elif line.startswith("wait:"):
            seconds = line.split(":", 1)[1].strip()
            time.sleep(float(seconds))

        elif line.startswith("set:"):
            parts = line.split(":")
            var_name = parts[1].strip()
            var_value = parts[2].strip()
            self.variables[var_name] = int(var_value) if var_value.isdigit() else var_value

        elif line.startswith("printvar:"):
            var_name = line.split(":", 1)[1].strip()
            print(self.variables.get(var_name, f"Error: Variable '{var_name}' not found."))

        elif line.startswith("print:"):
            text = line.split(":", 1)[1].strip()
            print(text)
                # Handle math: var: op: num1: num2:
        elif line.startswith("math:"):
            parts = line.split(":")
            var_name = parts[1].strip()
            operator = parts[2].strip()
        # NEW: printvar-n: (Print variable without newline)
        elif line.startswith("printvar-n:"):
            var_name = line.split(":", 1)[1].strip()
            if var_name in self.variables:
                print(self.variables[var_name], end="")
            else:
                print(f"Error: Variable '{var_name}' not found.", end="")

        # NEW: print-n: (Print text without newline)
        elif line.startswith("print-n:"):
            text = line.split(":", 1)[1].strip()
            print(text, end="")
            
            # Helper to get value: use variable if it exists, otherwise use the number
            def get_val(s):
                s = s.strip()
                if s in self.variables:
                    return float(self.variables[s])
                return float(s)

            try:
                val1 = get_val(parts[3])
                val2 = get_val(parts[4])

                if operator == "+": result = val1 + val2
                elif operator == "-": result = val1 - val2
                elif operator == "*": result = val1 * val2
                elif operator == "/": result = val1 / val2
                
                self.variables[var_name] = result
            except Exception as e:
                print(f"Math Error: {e}")


    def load_file(self, filename):
        try:
            with open(filename, "r") as file:
                for line in file:
                    self.run_line(line)
        except Exception as e:
            print(f"Error: {e}")

if __name__ == "__main__":
    interpreter = AdotsInterpreter()
    if len(sys.argv) > 1:
        interpreter.load_file(sys.argv[1])
