const cp=require('node:child_process');
const fs=require('node:fs');
const assert=require('node:assert/strict');
const http=require('node:http');
const wait=ms=>new Promise(r=>setTimeout(r,ms));
function request(port,path,method='GET',body=''){
 return new Promise((resolve,reject)=>{
  const req=http.request({host:'127.0.0.1',port,path,method,headers:{'Content-Type':'application/json','Content-Length':Buffer.byteLength(body)}},res=>{
   const chunks=[];res.on('data',c=>chunks.push(c));res.on('end',()=>resolve({status:res.statusCode,headers:res.headers,body:Buffer.concat(chunks).toString('utf8')}));
  });req.on('error',reject);req.setTimeout(4000,()=>req.destroy(Error('HTTP timeout')));req.end(body);
 });
}
(async()=>{
 const original=cp.spawn('build/http-original.exe',['29123'],{windowsHide:true});
 const pocket=cp.spawn('build/http-pocket.exe',['29124'],{windowsHide:true});
 original.on('error',console.error);pocket.on('error',console.error);
 try{
  for(const port of [29123,29124]){
   let ready=false;
   for(let i=0;i<40;i++){try{await request(port,'/api/player/status');ready=true;break}catch{await wait(150)}}
   assert(ready,'HTTP host starts on '+port);
  }
  const cases=[
   ['/api/player/status','GET'],['/api/player/volume','GET'],['/api/player/shuffle','GET'],['/api/player/repeat','GET'],['/api/player/auto-jump','GET'],
   ['/api','GET'],['/api/','GET'],['/api/playlists','GET'],['/api/playlists/0','GET'],['/api/playlists/0/tracks?limit=100&offset=0','GET'],
   ['/api/playlists/0/tracks/0','GET'],['/api/playlists/0/tracks/0/duration','GET'],['/api/playlists/nope','GET'],
   ['/api/player/volume?volume=101','PUT'],['/api/player/volume','PUT','{"volume":-1}'],['/api/player/volume?volume=101','POST'],
   ['/api/player/position?position=-1','PUT'],['/api/player/position','POST','{"position":-1}'],
   ['/api/player/volume?volume=50','PUT'],['/api/player/volume','POST','{"volume":50}'],['/api/player/position?position=0','PUT'],
   ...['play','pause','stop','prev','next','mute','shuffle','repeat','auto-jump'].map(x=>['/api/player/'+x,'POST']),
   ['/api/playlists/0/play','POST'],['/api/playlists/0/tracks/0/play','POST'],['/api/focus/play','POST'],
   ['/api/player/status','OPTIONS'],['/','OPTIONS'],['/unknown','GET'],['/','POST'],['/pocket','PUT'],['/api/player/status','PUT']
  ];
  for(const args of cases){
   const a=await request(29123,...args),b=await request(29124,...args);
   assert.deepEqual(b,a,args.join(' '));
  }
  const html=fs.readFileSync('web/pocket.html','utf8');
  for(const path of ['/','/index.html','/pocket','/pocket?from=home']){
   const r=await request(29124,path);assert.equal(r.status,200);assert.equal(r.body,html);
   assert.equal(Number(r.headers['content-length']),Buffer.byteLength(html));
   assert.equal(r.headers['content-type'],'text/html; charset=utf-8');
  }
  const result={passed:true,apiCases:cases.length,webCases:4,scope:'Actual original and modified Windows HTTP server, AIMP core absent. Source preservation independently checked; real AIMP playback not exercised.'};
  fs.writeFileSync('build/http-parity.json',JSON.stringify(result,null,2));console.log(result);
 }finally{original.kill();pocket.kill()}
})().catch(e=>{console.error(e);process.exitCode=1});
