"""Run the shipped updater under Windows PowerShell 5.1 against a local fixture server."""
import ctypes, hashlib, http.server, io, json, pathlib, subprocess, threading, zipfile, shutil
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'tests/artifacts/updater'
OUT.mkdir(parents=True,exist_ok=True)
responses={}
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  value=responses.get(self.path)
  if value is None:self.send_error(404);return
  if isinstance(value,tuple):
   self.send_response(302);self.send_header('Location',value[0]);self.end_headers();return
  self.send_response(200);self.send_header('Content-Length',str(len(value)));self.end_headers();self.wfile.write(value)
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
base=f'http://127.0.0.1:{server.server_port}'
sha=lambda b:hashlib.sha256(b).hexdigest()
results=[]
def fixture(name,build=1):
 folder=OUT/name
 if folder.exists():
  # Never delete old recovery fixtures; make this run independently inspectable.
  from time import time_ns
  folder=OUT/(name+'-'+str(time_ns()))
 folder.mkdir()
 (folder/'2ship.exe').write_bytes(b'old-executable')
 (folder/'2ship.o2r').write_bytes(b'old-app-assets')
 (folder/'version.json').write_text(json.dumps(dict(schema=1,product='mmvr',version='0.1.0',build=build)))
 for name in ['2ship2harkinian.json','saves/file3.json','mods/custom.o2r','mm.o2r']:
  f=folder/name;f.parent.mkdir(exist_ok=True);f.write_bytes(b'private-user-data:'+name.encode())
 return folder
protected=['2ship2harkinian.json','saves/file3.json','mods/custom.o2r','mm.o2r']
def snapshot(folder):return {name:(folder/name).read_bytes() for name in protected+['2ship.exe','2ship.o2r','version.json']}
def manifest(mode='valid'):
 files={'2ship.exe':b'new-executable','2ship.o2r':b'new-app-assets','version.json':json.dumps(dict(schema=1,product='mmvr',version='0.2.0',build=2)).encode()}
 for name in ['README.md','launch-mmvr.ps1','mmvr-runtime-probe.exe','update-mmvr.ps1','update-feed.json']:
  files[name]=('packaged '+name).encode()
 if mode=='traversal':files['../escape.txt']=b'bad'
 if mode=='protected':files['saves/file3.json']=b'bad'
 if mode=='reserved':files['assets/CON']=b'bad'
 records=[dict(path=n,size=len(b),sha256=sha(b)) for n,b in files.items()]
 stream=io.BytesIO()
 with zipfile.ZipFile(stream,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in files.items():
   if mode=='missing' and n=='2ship.o2r':continue
   z.writestr(n,b)
  if mode=='unlisted':z.writestr('assets/extra.txt',b'bad')
  if mode=='duplicate':z.writestr('2ship.exe',b'new-executable')
 data=stream.getvalue()
 if mode=='bad-file':records[0]['sha256']='0'*64
 m=dict(schema=1,product='mmvr',version='0.2.0',build=2,windows=dict(url=base+'/payload.zip',size=len(data),sha256=sha(data),files=records))
 if mode=='bad-digest':m['windows']['sha256']='0'*64
 if mode=='bad-size':m['windows']['size']+=10
 responses['/payload.zip']=data;responses['/manifest.json']=json.dumps(m).encode()
 if mode=='redirect':
  responses['/final.json']=responses['/manifest.json'];responses['/manifest.json']=('/final.json',)
 if mode=='redirect-loop':responses['/manifest.json']=('/manifest.json',)
 if mode=='redirect-file':responses['/manifest.json']=('file:///invalid',)
 return files
shell=r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
def run(folder,check=False,extra=(),override=True):
 args=[shell,'-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'platform/updater/update-mmvr.ps1'),'-Root',str(folder),'-FeedUri',base+'/manifest.json','-LocalTest','-NoRestart']
 if not override:
  index=args.index('-FeedUri');del args[index:index+2]
 if check:args.append('-CheckOnly')
 process=subprocess.run(args+list(extra),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=70,text=True,encoding='utf-8',errors='replace')
 (folder/'test-output.log').write_text(process.stdout,encoding='utf-8')
 state=json.loads((folder/'updates/status.json').read_text(encoding='utf-8-sig'))
 return process.returncode,state
try:
 for name in ['check','install','bad-digest','bad-file','bad-size','traversal','protected','reserved','missing','unlisted','duplicate','downgrade','wrong-process','redirect','redirect-loop','redirect-file','unconfigured','packaged-feed']:
  folder=fixture(name,3 if name=='downgrade' else 1)
  if name=='packaged-feed':
   path=folder/'version.json';value=json.loads(path.read_text());value['updateFeedUrl']=base+'/manifest.json';path.write_text(json.dumps(value))
  before=snapshot(folder);files=manifest(name)
  code,state=run(folder,name in ['check','packaged-feed'],['-WaitForPid',str(__import__('os').getpid())] if name=='wrong-process' else [],name not in ['unconfigured','packaged-feed'])
  for n in protected:assert (folder/n).read_bytes()==before[n],(name,'user data changed',n)
  if name in ['install','redirect']:
   assert code==0 and state['state']=='installed',(name,code,state)
   for n,b in files.items():assert (folder/n).read_bytes()==b,(name,n)
   backups=list((folder/'updates/rollback').glob('*/2ship.exe'));assert len(backups)==1 and backups[0].read_bytes()==before['2ship.exe']
  elif name=='unconfigured':
   assert code==2 and state['state']=='unconfigured' and snapshot(folder)==before,(name,code,state)
  elif name in ['check','downgrade','packaged-feed']:
   assert code==0 and state['state']==('current' if name=='downgrade' else 'available'),(name,code,state)
   assert snapshot(folder)==before,(name,'check altered installation')
  else:assert code!=0 and state['state']=='error' and snapshot(folder)==before,(name,code,state)
  results.append(dict(test=name,passed=True,state=state['state']))
 # Force an actual replacement failure; installed files must return to their old bytes.
 folder=fixture('locked-rollback');before=snapshot(folder);manifest()
 kernel=ctypes.WinDLL('kernel32',use_last_error=True)
 kernel.CreateFileW.restype=ctypes.c_void_p
 handle=kernel.CreateFileW(str(folder/'2ship.o2r'),0x80000000,1,None,3,0,None)
 assert handle not in (0,ctypes.c_void_p(-1).value)
 try:code,state=run(folder)
 finally:kernel.CloseHandle(ctypes.c_void_p(handle))
 assert code!=0 and state['state']=='error' and snapshot(folder)==before,('rollback',code,state)
 results.append(dict(test='locked-rollback',passed=True,state=state['state']))
 print(json.dumps(results,indent=2))
 (OUT/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
finally:server.shutdown();server.server_close()
