const fs = require('node:fs');
const cp = require('node:child_process');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const base = 'b42fccd65c964535fcfbdcc760967ccb27b48bf4';
const git = (...args)=>cp.execFileSync('git',['-c','safe.directory='+process.cwd().replaceAll('\\','/'),...args],{encoding:'utf8'}).replaceAll('\r\n','\n');
const original=git('show',base+':src/http_server.cpp');
const current=fs.readFileSync('src/http_server.cpp','utf8').replaceAll('\r\n','\n');
const restored=current.replace('#include "pocket_web.h"\n','').replace(/            \/\/ AIMP_POCKET_BEGIN[\s\S]*?            \/\/ AIMP_POCKET_END\n/,'');
assert.equal(restored,original,'Existing HTTP parser, responses and API handlers must remain unchanged');
const files=git('ls-tree','-r','--name-only',base,'src','sdk','third_party','Langs').trim().split('\n');
for(const file of files){
  if(file==='src/http_server.cpp') continue;
  const expected=cp.execFileSync('git',['-c','safe.directory='+process.cwd().replaceAll('\\','/'),'show',base+':'+file]);
  const actual=fs.readFileSync(file);
  assert.equal(actual.toString('utf8').replaceAll('\r\n','\n'),expected.toString('utf8').replaceAll('\r\n','\n'),'Unmodified original file: '+file);
}
const html=fs.readFileSync('web/pocket.html','utf8');
new vm.Script(html.match(/<script>([\s\S]*?)<\/script>/)[1]);
assert(html.includes('new URL(p,location.origin)'));
assert(!html.includes('localStorage'));
assert(current.indexOf('IsAllowedClient(')<current.indexOf('SendPocketPage('));
console.log('PASS: Original API source restored exactly; '+files.length+' original files checked; UI syntax and same-origin checks passed.');
