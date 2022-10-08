#!/usr/bin/env node

import { Map } from "./map.js";

/**
 * Usage: quemap-fu /path/to/quake.map /path/to/quake.rtlight
 * The converted .map will be written to stdout.
 * @param {Array} args The command line args. 
 */
async function main(args) {
  const map = new Map(args.slice(2));
  await map.read();
  map.convert().write(process.stdout);
}

main(process.argv);
