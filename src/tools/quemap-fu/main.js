#!/usr/bin/env node

import { Map } from "./map.js";

async function main(args) {
  const map = new Map(args[2]);
  await map.read();
  map.convert().write(process.stdout);
}

main(process.argv);
