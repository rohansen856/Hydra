#!/usr/bin/env node
/**
 * HTTP client demo: register and invoke a serverless function.
 */
import { readFileSync } from "fs";
import { dirname, resolve } from "path";
import { fileURLToPath } from "url";

const RUNTIMES = {
  cpp: {
    name: "hello",
    commandEnv: "SERVERLESS_CPP_HELLO",
    defaultCommand: "build/functions/hello-function",
    invokeName: "hello",
    expectSubstr: "Hello",
  },
  python: {
    name: "hello-python",
    commandEnv: "SERVERLESS_PY_HELLO",
    defaultCommand: "functions/hello_python/run.sh",
    invokeName: "hello-python",
    expectSubstr: "Python",
  },
  node: {
    name: "hello-node",
    commandEnv: "SERVERLESS_NODE_HELLO",
    defaultCommand: "functions/hello_node/run.sh",
    invokeName: "hello-node",
    expectSubstr: "Node",
  },
};

function parseArgs(argv) {
  const args = { baseUrl: "http://127.0.0.1:8080", runtime: "cpp", name: "ClientUser" };
  for (let i = 2; i < argv.length; i++) {
    if (argv[i] === "--base-url") args.baseUrl = argv[++i];
    else if (argv[i] === "--runtime") args.runtime = argv[++i];
    else if (argv[i] === "--name") args.name = argv[++i];
  }
  return args;
}

async function request(method, url, body) {
  const res = await fetch(url, {
    method,
    headers: body ? { "Content-Type": "application/json" } : {},
    body: body ? JSON.stringify(body) : undefined,
  });
  const text = await res.text();
  let json = {};
  try {
    json = text ? JSON.parse(text) : {};
  } catch {
    json = { raw: text };
  }
  return { status: res.status, json };
}

async function main() {
  const args = parseArgs(process.argv);
  const cfg = RUNTIMES[args.runtime];
  if (!cfg) {
    console.error("unknown runtime", args.runtime);
    process.exit(1);
  }

  const root = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
  const command =
    process.env[cfg.commandEnv] || resolve(root, cfg.defaultCommand);

  const base = args.baseUrl.replace(/\/$/, "");
  const reg = await request("POST", `${base}/api/v1/functions`, {
    name: cfg.name,
    version: "1",
    command,
    min_workers: 0,
    max_workers: 4,
  });
  if (reg.status !== 200 && reg.status !== 201) {
    const code = reg.json.error?.code;
    if (reg.status !== 400 || code !== "DUPLICATE_FUNCTION") {
      console.log(JSON.stringify(reg.json, null, 2));
      process.exit(1);
    }
  }

  const { status, json } = await request(
    "POST",
    `${base}/api/v1/functions/${cfg.invokeName}/invoke`,
    { name: args.name }
  );
  console.log(JSON.stringify(json, null, 2));
  if (status !== 200) process.exit(1);
  const message = json.body?.message || "";
  if (!message.includes(cfg.expectSubstr) || !message.includes(args.name)) {
    console.error("unexpected message:", message);
    process.exit(1);
  }
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
