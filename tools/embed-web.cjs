const fs = require('node:fs');
const html = fs.readFileSync('web/pocket.html','utf8');
const delimiter = 'AIMP_POCKET_WEB';
if(html.includes(')'+delimiter+'"')) throw Error('Raw string delimiter collision');
fs.writeFileSync('src/pocket_embedded.h', '#pragma once\n// Generated from web/pocket.html by tools/embed-web.cjs.\nstatic constexpr char kAimpPocketHtml[] = R"'+delimiter+'('+html+')'+delimiter+'";\n');
console.log('Embedded UI:',Buffer.byteLength(html),'UTF-8 bytes');
