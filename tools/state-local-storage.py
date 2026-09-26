"""Expose native function-local static storage in generated opt-in copies.
Uses compiler symbol identity for every reference; never textual name replacement.
The original game sources are not changed. A layout/build check must precede use.
"""
import argparse,concurrent.futures,hashlib,importlib.util,json,pathlib,re,sys,time,os,ctypes
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('state_layouts',ROOT/'tools/state-layouts.py')
layouts=importlib.util.module_from_spec(spec);spec.loader.exec_module(layouts)
ci=layouts.cindex
OUT=ROOT/'src/mmvr-runtime/save_states/native_locals'
def transform(source):
 path=ROOT/'src/2ship2harkinian/mm/src'/source
 data=path.read_bytes();tu=layouts.analyze(source,return_translation_unit=True)
 symbols={};declarations=[];functions={};references=[]
 def own(c):return c.location.file and pathlib.Path(c.location.file.name)==path
 def visit(c,function=None):
  if c.kind==ci.CursorKind.FUNCTION_DECL and own(c) and c.is_definition():
   function=c;functions[c.spelling]=c
  if function and own(c) and c.kind==ci.CursorKind.DECL_STMT:
   variables=[v for v in c.get_children() if v.kind==ci.CursorKind.VAR_DECL]
   statics=[v for v in variables if v.storage_class==ci.StorageClass.STATIC]
   if statics:
    if len(statics)!=len(variables):raise ValueError('Mixed storage declaration '+source)
    declarations.append((function,c,statics))
    for v in statics:
     usr=v.get_usr();suffix=hashlib.sha256(usr.encode()).hexdigest()[:16]
     symbols[usr]={'name':'mmvr_local_'+suffix,'original':v.spelling,'function':function.spelling,'line':v.location.line,'at':v.location.offset}
  if own(c) and c.kind==ci.CursorKind.DECL_REF_EXPR and c.referenced:
   offset=ctypes.c_uint()
   fn=ci.conf.lib.clang_getSpellingLocation
   fn.argtypes=[ci.SourceLocation,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint)]
   fn(c.location,None,None,None,ctypes.byref(offset))
   references.append((c.referenced.get_usr(),offset.value,c.spelling))
  for child in c.get_children():
   if child.location.file and not own(child):continue
   visit(child,function)
 visit(tu.cursor)
 names=[(v['at'],len(v['original'].encode()),v['name'].encode()) for v in symbols.values()]
 for usr,at,name in references:
  if usr in symbols:names.append((at,len(name.encode()),symbols[usr]['name'].encode()))
 names=list(set(names))
 for at,count,new in names:
  expected=next(v['original'].encode() for v in symbols.values() if v['name'].encode()==new)
  if data[at:at+count]!=expected:raise ValueError('Compiler reference does not match source token: '+source+':'+str(at))
 def renamed(start,end):
  chunk=data[start:end]
  for at,count,new in sorted(names,reverse=True):
   if start<=at<end:chunk=chunk[:at-start]+new+chunk[at-start+count:]
  return chunk
 edits=[];inserts={};ranges=[]
 for function,decl,variables in declarations:
  start,end=decl.extent.start.offset,decl.extent.end.offset
  # Keep the authored constant initializer exactly; C static initialization is
  # already performed at process load, not lazily when the function is called.
  statement=renamed(start,end)
  if not statement.rstrip().endswith(b';'):statement+=b';'
  inserts.setdefault(function.extent.start.offset,[]).append(statement)
  edits.append((start,end,b'/* persistent local storage is declared above */'))
  ranges.append((start,end))
 for at,count,new in names:
  if not any(a<=at<b for a,b in ranges):edits.append((at,at+count,new))
 for at,statements in inserts.items():edits.append((at,at,b'\n'.join(statements)+b'\n'))
 result=data
 for a,b,new in sorted(edits,reverse=True):result=result[:a]+new+result[b:]
 # Quoted actor-local includes must still resolve from the original directory.
 def include(match):
  target=path.parent/match.group(1).decode()
  if not target.is_file():return match.group(0)
  relative=os.path.relpath(target,OUT).replace('\\','/')
  return b'#include "'+relative.encode()+b'"'
 result=re.sub(rb'#include\s+"([^"\r\n]+)"',include,result)
 suffix=hashlib.sha256(source.encode()).hexdigest()[:16]
 OUT.mkdir(parents=True,exist_ok=True)
 (OUT/(suffix+'.inc')).write_bytes(result)
 metadata={'source':source,'include':suffix+'.inc','variables':list(symbols.values()),'sourceSha256':hashlib.sha256(data).hexdigest()}
 (OUT/(suffix+'.json')).write_text(json.dumps(metadata,indent=2),encoding='utf-8')
 return {'source':source,'locals':len(symbols)}
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 parser=argparse.ArgumentParser();parser.add_argument('--source');parser.add_argument('--jobs',type=int,default=4);args=parser.parse_args()
 manifest=json.loads((ROOT/'build/exact-states-inventory/bindings/manifest.json').read_text())
 sources=[args.source] if args.source else [m['source'] for m in manifest if m['functionStatics']]
 failures={};done=[]
 with concurrent.futures.ProcessPoolExecutor(max_workers=args.jobs) as pool:
  jobs={pool.submit(transform,s):s for s in sources}
  for job in concurrent.futures.as_completed(jobs):
   try:done.append(job.result())
   except Exception as e:failures[jobs[job]]=str(e)
   if (len(done)+len(failures))%20==0:print(json.dumps({'done':len(done),'failures':len(failures)}),flush=True)
 (OUT/'result.json').write_text(json.dumps({'done':done,'failures':failures},indent=2))
 print(json.dumps({'files':len(done),'locals':sum(x['locals'] for x in done),'failures':failures}),flush=True)
 return bool(failures)
if __name__=='__main__':sys.exit(main())
