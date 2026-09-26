"""Exercise the actual release ZIP through the shipped Windows updater."""
import hashlib, http.server, json, pathlib, subprocess, threading, time, zipfile, io
ROOT = pathlib.Path(__file__).resolve().parents[1]
VERSION = json.loads((ROOT/'platform/version.json').read_text(encoding='utf-8-sig'))
RELEASE = ROOT/'releases'/VERSION['version']
OUT = ROOT/'tests/artifacts'/('release-updater-'+str(time.time_ns()))
OUT.mkdir(parents=True)
responses = {}
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        value = responses.get(self.path)
        if value is None:
            self.send_error(404); return
        if isinstance(value, tuple):
            self.send_response(302); self.send_header('Location', value[0]); self.end_headers(); return
        self.send_response(200); self.send_header('Content-Length', str(len(value))); self.end_headers(); self.wfile.write(value)
    def log_message(self, *args): pass
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
base = f'http://127.0.0.1:{server.server_port}'
shell = r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
sha = lambda b: hashlib.sha256(b).hexdigest()
results = []
def invoke(folder, *flags):
    result = subprocess.run([shell, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(ROOT/'platform/updater/update-mmvr.ps1'), '-Root', str(folder), '-FeedUri', base+'/channel/latest.json', '-LocalTest', *flags], capture_output=True, timeout=120)
    state = json.loads((folder/'updates/status.json').read_text(encoding='utf-8-sig'))
    assert result.returncode == 0, state
    return state['state']
try:
    folder = OUT/'actual-package'; folder.mkdir()
    old = dict(VERSION, build=VERSION['build']-1, version='previous-build')
    (folder/'version.json').write_text(json.dumps(old), encoding='utf-8')
    (folder/'2ship.exe').write_bytes(b'previous exe')
    protected = ['mm.o2r', '2ship2harkinian.json', 'saves/file1.json', 'saves/save-states/slot-1.mmstate', 'mods/pack.o2r', 'texturepacks/pack.otr']
    for name in protected:
        path=folder/name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(('user data '+name).encode())
    before={name:(folder/name).read_bytes() for name in protected}
    manifest=json.loads((RELEASE/'latest.json').read_text(encoding='utf-8'))
    package=RELEASE/('MMVR-Windows-'+VERSION['version']+'.zip')
    payload=package.read_bytes()
    assert sha(payload)==manifest['windows']['sha256']
    manifest['windows']['url']=base+'/releases/download/v'+VERSION['version']+'/'+package.name
    responses['/channel/latest.json']=('/signed/manifest.json',)
    responses['/signed/manifest.json']=json.dumps(manifest).encode()
    responses['/releases/download/v'+VERSION['version']+'/'+package.name]=('/cdn/payload.zip',)
    responses['/cdn/payload.zip']=payload
    assert invoke(folder, '-NoRestart', '-CheckOnly')=='available'
    assert (folder/'2ship.exe').read_bytes()==b'previous exe'
    assert invoke(folder, '-NoRestart')=='installed'
    for item in manifest['windows']['files']:
        assert sha((folder/item['path']).read_bytes())==item['sha256'],item['path']
    for name, data in before.items(): assert (folder/name).read_bytes()==data,name
    assert invoke(folder, '-NoRestart', '-CheckOnly')=='current'
    assert any(p.read_bytes()==b'previous exe' for p in (folder/'updates/rollback').glob('*/2ship.exe'))
    results.append(dict(test='actual-release-check-install-current',passed=True,files=len(manifest['windows']['files']),protectedFiles=len(protected),redirects=True))
    # No headset launch: a harmless fixture proves restart routes through the launcher.
    folder=OUT/'restart'; folder.mkdir()
    (folder/'version.json').write_text(json.dumps(old),encoding='utf-8')
    files={'2ship.exe':b'fixture; must not execute', 'version.json':json.dumps(VERSION).encode(), 'launch-mmvr.ps1':b"[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'launcher-ran.txt'),'runtime-launcher')"}
    stream=io.BytesIO()
    with zipfile.ZipFile(stream,'w') as archive:
        for name,data in files.items():archive.writestr(name,data)
    payload=stream.getvalue()
    manifest['windows']=dict(url=base+'/cdn/payload.zip',size=len(payload),sha256=sha(payload),files=[dict(path=n,size=len(b),sha256=sha(b)) for n,b in files.items()])
    responses['/signed/manifest.json']=json.dumps(manifest).encode();responses['/cdn/payload.zip']=payload
    assert invoke(folder)=='installed'
    for _ in range(100):
        if (folder/'launcher-ran.txt').exists():break
        time.sleep(.1)
    assert (folder/'launcher-ran.txt').read_text()=='runtime-launcher'
    results.append(dict(test='runtime-aware-restart',passed=True))
    (OUT/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    print(json.dumps(dict(folder=str(OUT),results=results),indent=2))
finally:
    server.shutdown();server.server_close()
