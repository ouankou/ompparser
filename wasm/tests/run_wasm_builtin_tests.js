#!/usr/bin/env node
"use strict";

const fs = require("fs");
const path = require("path");

const LangMode = {
  Auto: 0,
  C: 1,
  Cplusplus: 2,
  Fortran: 3
};

function parseArgs(argv) {
  const args = {};
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === "--js" && argv[i + 1]) {
      args.js = argv[++i];
    } else if (arg === "--wasm" && argv[i + 1]) {
      args.wasm = argv[++i];
    } else if (arg === "--tests" && argv[i + 1]) {
      args.tests = argv[++i];
    }
  }
  return args;
}

function isFortranDirective(line) {
  return /^[\s]*[!cC*]\$omp/i.test(line);
}

function isDirective(line) {
  return line.startsWith("#pragma") || isFortranDirective(line);
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const jsPath = path.resolve(args.js || "build-wasm/wasm/ompparser.js");
  const wasmPath = path.resolve(args.wasm || jsPath.replace(/\.js$/, ".wasm"));
  const testsDir = path.resolve(args.tests || "tests/builtin");

  if (!fs.existsSync(jsPath)) {
    console.error(`WASM JS not found: ${jsPath}`);
    process.exit(1);
  }
  if (!fs.existsSync(wasmPath)) {
    console.error(`WASM binary not found: ${wasmPath}`);
    process.exit(1);
  }
  if (!fs.existsSync(testsDir)) {
    console.error(`Tests directory not found: ${testsDir}`);
    process.exit(1);
  }

  const createModule = require(jsPath);
  const logBuffer = [];
  const errBuffer = [];

  const wasmModule = await createModule({
    print: (text) => logBuffer.push(text),
    printErr: (text) => errBuffer.push(text),
    locateFile: (file) => (file.endsWith(".wasm") ? wasmPath : file)
  });

  const files = fs
    .readdirSync(testsDir)
    .filter((name) => name.endsWith(".txt"))
    .sort();

  let failures = 0;

  for (const file of files) {
    const filePath = path.join(testsDir, file);
    const contents = fs.readFileSync(filePath, "utf8");
    const lines = contents.split(/\n/);
    const fileIsFortran = file.toLowerCase().includes("fortran");
    const langHint = fileIsFortran ? LangMode.Fortran : LangMode.C;

    let currentInput = "";
    let currentOutput = "";
    let expectInvalidNext = false;
    const allowExtensions = /^(?:ompx_fortran|requires(?:_fortran)?|target_data)\.txt$/
      .test(file);

    const reportFailure = (lineNo, message) => {
      failures += 1;
      console.error(`${file}:${lineNo}: ${message}`);
    };

    for (let idx = 0; idx < lines.length; idx++) {
      let line = lines[idx];
      const lineNo = idx + 1;
      if (line.endsWith("\r"))
        line = line.slice(0, -1);
      let trimmed = line.replace(/^\s+/, "");

      if (trimmed.startsWith("EXPECT:")) {
        const expectation = trimmed
          .slice(7)
          .trim()
          .toLowerCase();
        expectInvalidNext =
          expectation === "invalid" || expectation === "fail" ||
          expectation === "failure";
        continue;
      }

      if (isDirective(trimmed)) {
        if (currentInput) {
          reportFailure(lineNo, "Missing PASS line for previous directive.");
        }

        currentInput = trimmed;
        currentOutput = "";

        logBuffer.length = 0;
        errBuffer.length = 0;
        const parse = allowExtensions
          ? wasmModule.parseAndUnparseWithExtensions
          : wasmModule.parseAndUnparse;
        const result = parse(currentInput, LangMode.Auto, langHint);
        const output = result.text || "";
        const diagnostics = Array.from(result.diagnostics || []);
        const isExpectedInvalid = expectInvalidNext;

        if (isExpectedInvalid) {
          if (output.trim() || diagnostics.length === 0) {
            reportFailure(lineNo, "Expected invalid directive was accepted.");
          }
          expectInvalidNext = false;
          currentInput = "";
          currentOutput = "";
          continue;
        }

        if (diagnostics.length !== 0) {
          reportFailure(
            lineNo,
            `Parser error: ${diagnostics.map((item) => item.message).join("; ")}`
          );
          currentInput = "";
          continue;
        }

        currentOutput = output.trim();
        expectInvalidNext = false;
        continue;
      }

      if (trimmed.startsWith("PASS:")) {
        const expected = trimmed.slice(5).trim();
        if (!currentInput) {
          currentOutput = "";
          expectInvalidNext = false;
          continue;
        }

        if (currentOutput !== expected) {
          reportFailure(
            lineNo,
            `Mismatch: expected "${expected}", got "${currentOutput}"`
          );
        }

        currentInput = "";
        currentOutput = "";
        expectInvalidNext = false;
      }
    }

    if (currentInput) {
      reportFailure(lines.length, "Missing PASS line at end of file.");
    }
    if (expectInvalidNext) {
      reportFailure(lines.length,
                    "Dangling EXPECT: invalid without a directive.");
    }
  }

  if (failures > 0) {
    console.error(`WASM builtin tests failed: ${failures} error(s).`);
    process.exit(1);
  }

  console.log("WASM builtin tests passed.");
}

main().catch((err) => {
  console.error(err.stack || err);
  process.exit(1);
});
