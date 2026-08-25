import pw from '/home/claude/.npm-global/lib/node_modules/playwright/index.js';
const { chromium } = pw;
import fs from 'fs';

const html = fs.readFileSync('editor.html','utf8');
const page_html = `<!doctype html><html><head><meta charset="utf-8"><style>*{margin:0}</style></head><body>${html}</body></html>`;
fs.writeFileSync('/tmp/full.html', page_html);

const browser = await chromium.launch();
const page = await browser.newPage({ viewport:{width:1400,height:900}, deviceScaleFactor:2 });
const errors=[], logs=[];
page.on('pageerror', e => errors.push('PAGEERROR: '+e.message));
page.on('console', m => { if(m.type()==='error') errors.push('CONSOLE: '+m.text()); else logs.push(m.text()); });

await page.goto('file:///tmp/full.html');
await page.waitForTimeout(1800);

// Interações reais: selecionar um objeto, arrastar o gizmo, abrir o radial.
const box = await page.locator('#view').boundingBox();
const cx = box.x + box.width/2, cy = box.y + box.height/2;

await page.mouse.click(cx-40, cy+10);           // seleciona algo
await page.waitForTimeout(300);
const sel1 = await page.locator('#inspName').textContent();

// arrasta o gizmo (procura uma seta perto do centro do objeto)
await page.mouse.move(cx, cy);
await page.mouse.down(); await page.mouse.move(cx+90, cy-30, {steps:12}); await page.mouse.up();
await page.waitForTimeout(300);

// órbita
await page.mouse.move(cx+250, cy+120);
await page.mouse.down(); await page.mouse.move(cx+150, cy+90,{steps:10}); await page.mouse.up();
await page.waitForTimeout(300);

const tri = await page.locator('#tTri').textContent();
const fps = await page.locator('#tFps').textContent();
const undoDisabled = await page.locator('#btnUndo').isDisabled();

await page.screenshot({ path:'shot-dark.png' });

// tema claro
await page.locator('#btnTheme').click();
await page.waitForTimeout(600);
await page.screenshot({ path:'shot-light.png' });
await page.locator('#btnTheme').click();
await page.waitForTimeout(300);

// térmico severo
await page.selectOption('#thermSel','severe');
await page.waitForTimeout(900);
const chip = await page.locator('#profilechip').textContent();

// menu radial via long-press
const empty = { x: box.x + box.width*0.18, y: box.y + box.height*0.22 };
await page.mouse.move(empty.x, empty.y);
await page.mouse.down();
await page.waitForTimeout(520);
const radialVisible = await page.locator('#radial').evaluate(el=>getComputedStyle(el).display);
await page.mouse.move(empty.x, empty.y-110,{steps:8});
await page.waitForTimeout(200);
await page.screenshot({ path:'shot-radial.png' });
await page.mouse.up();
await page.waitForTimeout(300);

// aba Flow
await page.locator('.tab', {hasText:'Flow'}).click();
await page.waitForTimeout(500);
await page.screenshot({ path:'shot-flow.png' });

console.log('--- seleção:', JSON.stringify(sel1));
console.log('--- triângulos:', tri, '| fps:', fps, '| undo habilitado:', !undoDisabled);
console.log('--- perfil sob térmico severo:', chip);
console.log('--- radial abriu no long-press:', radialVisible);
console.log('--- erros:', errors.length ? errors : 'nenhum');
await browser.close();
