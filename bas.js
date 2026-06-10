const fs = require('fs');
const path = require('path');

const args = process.argv; 

if (args.length < 3) {
    console.error("Usage: node basic_interpreter.js <filename.bas>");
    process.exit(1);
}

const targetFile = args[2];

try {
    const filePath = path.resolve(targetFile);
    const code = fs.readFileSync(filePath, 'utf-8');
    runBASIC(code);
} catch (error) {
    console.error(`Error reading file "${targetFile}": ${error.message}`);
    process.exit(1);
}

function runBASIC(sourceCode) {
    // 1. Clean and parse lines into an executable array
    const rawLines = sourceCode.split(/\r?\n/);
    const program = [];
    const lineMap = {}; // Maps BASIC line numbers (e.g., 10, 20) to array indices

    for (let rawLine of rawLines) {
        rawLine = rawLine.trim();
        if (!rawLine || rawLine.startsWith("REM")) continue;

        // Extract line number and the actual command
        const match = rawLine.match(/^(\d+)\s+(.*)$/);
        if (match) {
            const lineNum = parseInt(match[1], 10);
            const statement = match[2].trim();
            
            lineMap[lineNum] = program.length;
            program.push({ lineNum, statement });
        }
    }

    const variables = {};
    let pc = 0; // Program Counter (index pointer)

    // Helper to evaluate variables or raw numbers
    const evaluate = (expr) => {
        expr = expr.trim();
        if (variables[expr] !== undefined) return variables[expr];
        return isNaN(expr) ? expr.replace(/^["']|["']$/g, '') : Number(expr);
    };

    // 2. Execution Loop
    while (pc < program.length) {
        const { statement } = program[pc];
        let jumped = false;

        // Case A: IF ... THEN GOTO ...
        if (statement.startsWith("IF ")) {
            // Regex to capture: LeftSide Operator RightSide THEN GOTO LineNumber
            const ifMatch = statement.match(/^IF\s+(.+?)\s*([<>=!]+)\s*(.+?)\s+THEN\s+GOTO\s+(\d+)$/i);
            
            if (ifMatch) {
                const left = evaluate(ifMatch[1]);
                const op = ifMatch[2];
                const right = evaluate(ifMatch[3]);
                const targetLine = parseInt(ifMatch[4], 10);

                let conditionMet = false;
                if (op === "=" || op === "==") conditionMet = (left == right);
                else if (op === "<") conditionMet = (left < right);
                else if (op === ">") conditionMet = (left > right);
                else if (op === "<=") conditionMet = (left <= right);
                else if (op === ">=") conditionMet = (left >= right);
                else if (op === "!=" || op === "<>") conditionMet = (left != right);

                if (conditionMet) {
                    if (lineMap[targetLine] !== undefined) {
                        pc = lineMap[targetLine];
                        jumped = true;
                    } else {
                        console.error(`Runtime Error: Line ${targetLine} not found.`);
                        process.exit(1);
                    }
                }
            }
        }
        // Case B: Unconditional GOTO ...
        else if (statement.startsWith("GOTO ")) {
            const targetLine = parseInt(statement.substring(5).trim(), 10);
            if (lineMap[targetLine] !== undefined) {
                pc = lineMap[targetLine];
                jumped = true;
            } else {
                console.error(`Runtime Error: Line ${targetLine} not found.`);
                process.exit(1);
            }
        }
        // Case C: PRINT ...
        else if (statement.startsWith("PRINT ")) {
            const valStr = statement.substring(6).trim();
            console.log(evaluate(valStr));
        } 
        // Case D: Variable Assignment (X = X + 1)
        else if (statement.includes("=")) {
            const [varName, expr] = statement.split("=").map(s => s.trim());
            
            // Basic math support parser (handles simple "X + 1" or "X - 1")
            const mathMatch = expr.match(/^([a-zA-Z0-9"']+)\s*([\+\-\*\/])\s*([a-zA-Z0-9"']+)$/);
            if (mathMatch) {
                const left = evaluate(mathMatch[1]);
                const op = mathMatch[2];
                const right = evaluate(mathMatch[3]);
                
                if (op === "+") variables[varName] = left + right;
                else if (op === "-") variables[varName] = left - right;
                else if (op === "*") variables[varName] = left * right;
                else if (op === "/") variables[varName] = left / right;
            } else {
                variables[varName] = evaluate(expr);
            }
        }

        // Move to the next line only if we didn't just execute a jump
        if (!jumped) {
            pc++;
        }
    }
}
