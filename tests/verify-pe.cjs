const fs=require('node:fs');
const assert=require('node:assert/strict');
const crypto=require('node:crypto');
const results=[];
for(const [bits,machine] of [[32,0x14c],[64,0x8664]]){
 const file='build/AimpHttpControl'+bits+'.dll',b=fs.readFileSync(file),pe=b.readUInt32LE(0x3c);
 assert.equal(b.toString('ascii',0,2),'MZ');assert.equal(b.readUInt32LE(pe),0x4550);
 assert.equal(b.readUInt16LE(pe+4),machine);assert(b.readUInt16LE(pe+22)&0x2000,'DLL flag');
 assert(b.includes(Buffer.from('AIMPPluginGetHeader')),'AIMP entry point present');
 assert(b.includes(Buffer.from('AIMP POCKET'))||b.includes(Buffer.from('AIMP <b>POCKET')),'Embedded UI');
 const report=fs.readFileSync('build/pe'+bits+'.txt','utf8');
 assert(/\bAIMPPluginGetHeader\b/.test(report));
 const imports=[...report.matchAll(/DLL Name:\s*(\S+)/g)].map(x=>x[1]);
 assert(!imports.some(x=>/libgcc|libstdc\+\+|libwinpthread/i.test(x)),'No external MinGW runtime');
 results.push({file,bits,bytes:b.length,sha256:crypto.createHash('sha256').update(b).digest('hex'),imports});
}
fs.writeFileSync('build/verification.json',JSON.stringify({passed:true,files:results},null,2));console.log(results);
