#!/usr/bin/env node
// Static contract check for modules/Patobeur/Map.
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
const root = new URL('../modules/Patobeur/Map/', import.meta.url);
const path = (name) => new URL(name, root);
const main = readFileSync(path('main.lua'), 'utf8');
const errors = [];
if (!main.includes('farever.map_data')) errors.push('main.lua ne lit pas farever.map_data()');
if (!main.includes('ui.canvas(')) errors.push('main.lua ne declare pas ui.canvas()');
if (!main.includes('ui.draw_image(')) errors.push('main.lua ne dessine aucun fond avec ui.draw_image()');
if (/\b(?:player|camera|ent|first)\s*\[\s*[124567]\s*\]/.test(main))
  errors.push('acces numerique detecte: utiliser player.x, camera.targetX, ent.x, ent.isBoss');
if (!main.includes('map.world') && !main.includes('data.world'))
  errors.push('main.lua ne selectionne pas le fond depuis map.world');
const asset = path('assets/maps/w1_siagarta.png');
const meta = path('assets/maps/w1_siagarta.json');
if (!existsSync(asset) || !existsSync(meta))
  errors.push('fonds locaux absents: lancer python tools/gen-real-map-assets.py');
if (errors.length) {
  console.error('Map contract: ECHEC');
  for (const e of errors) console.error('- ' + e);
  process.exit(1);
}
console.log('Map contract: OK');