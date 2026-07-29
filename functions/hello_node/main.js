#!/usr/bin/env node
"use strict";

const fs = require("fs");

function main() {
  try {
    const raw = fs.readFileSync(0, "utf8");
    const data = JSON.parse(raw);
    const payload =
      data.payload && typeof data.payload === "object" ? data.payload : data;
    const name = payload.name || "World";
    const output = {
      request_id: data.request_id || "",
      status: 200,
      body: { message: `Hello ${name} from Node.js` },
    };
    process.stdout.write(JSON.stringify(output) + "\n");
    return 0;
  } catch (err) {
    const output = {
      status: 500,
      error: { code: "FUNCTION_ERROR", message: String(err) },
    };
    process.stdout.write(JSON.stringify(output) + "\n");
    return 1;
  }
}

process.exit(main());
