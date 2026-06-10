const fs = require('fs');

// process.argv[2] grabs the first argument passed after the script name
const filePath = process.argv[2];

if (!filePath) {
    console.error("Usage: node interpreter.js <brainfuck_file.bf>");
    process.exit(1);
}

// Read the Brainfuck source file
const code = fs.readFileSync(filePath, 'utf-8');
const tape = new Uint8Array(30000); // Memory tape initialized to 0
let ptr = 0; // Tape pointer
let pc = 0; // Program counter (Instruction pointer)

// Precompute loop pairs to jump efficiently
const loopMap = new Map();
const stack = [];
for (let i = 0; i < code.length; i++) {
    if (code[i] === '[') stack.push(i);
    else if (code[i] === ']') {
        const start = stack.pop();
        loopMap.set(start, i);
        loopMap.set(i, start);
    }
}

// Execution loop
while (pc < code.length) {
    const char = code[pc];

    switch (char) {
        case '>':
            ptr++;
            break;
        case '<':
            ptr--;
            break;
        case '+':
            tape[ptr]++;
            break;
        case '-':
            tape[ptr]--;
            break;
        case '.':
            process.stdout.write(String.fromCharCode(tape[ptr]));
            break;
        case ',':
            // Reading from STDIN (synchronous for simplicity)
            const buf = Buffer.alloc(1);
            if (fs.readSync(0, buf, 0, 1, null) > 0) {
                tape[ptr] = buf[0];
            }
            break;
        case '[':
            if (tape[ptr] === 0) {
                pc = loopMap.get(pc);
            }
            break;
        case ']':
            if (tape[ptr] !== 0) {
                pc = loopMap.get(pc);
            }
            break;
    }
    pc++;
}
