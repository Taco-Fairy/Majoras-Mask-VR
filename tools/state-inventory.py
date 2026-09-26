"""Bounded parallel native layout inventory. No game/device changes.

Uses compiler types, never guesses pointers from memory words. Files are merged
by native USR; incompatible definitions fail rather than silently overwrite.
"""
import concurrent.futures, importlib.util, json, pathlib, sys, time
ROOT=pathlib.Path(__file__).resolve().parents[1]
def analyze(source, declarations_only=False):
    spec=importlib.util.spec_from_file_location('state_layouts',ROOT/'tools/state-layouts.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module.analyze(source,declarations_only=declarations_only)
def main():
    import argparse
    cli=argparse.ArgumentParser();cli.add_argument('--jobs',type=int,default=4)
    cli.add_argument('--group',choices=['actors','core','all'],default='actors')
    cli.add_argument('--limit',type=int,default=0);cli.add_argument('--resume',action='store_true')
    refresh=cli.add_mutually_exclusive_group()
    refresh.add_argument('--refresh-declarations',action='store_true')
    refresh.add_argument('--refresh-locals',action='store_true',help='Refresh full compiler metadata only for sources with function-static storage')
    options=cli.parse_args()
    commands=json.loads((ROOT/'build/android-quest/compile_commands.json').read_text())
    sources=[]
    for entry in commands:
        path=pathlib.Path(entry['file']).as_posix()
        marker='/mm/src/'
        if marker not in path or not path.endswith('.c'):continue
        relative=path.split(marker,1)[1]
        actor=relative.startswith('overlays/actors/')
        if options.group=='actors' and not actor:continue
        if options.group=='core' and actor:continue
        sources.append(relative)
    sources=sorted(set(sources))
    if options.limit:sources=sources[:options.limit]
    out=ROOT/'build/exact-states-inventory/native';out.mkdir(parents=True,exist_ok=True)
    todo=[s for s in sources if not options.resume or not (out/(s.replace('/','__')+'.json')).exists()]
    if options.refresh_locals:
        todo=[s for s in todo if (out/(s.replace('/','__')+'.json')).exists() and json.loads((out/(s.replace('/','__')+'.json')).read_text()).get('functionStatics')]
    failures={};started=time.monotonic()
    with concurrent.futures.ProcessPoolExecutor(max_workers=max(1,min(options.jobs,6))) as pool:
        futures={pool.submit(analyze,s,options.refresh_declarations):s for s in todo}
        for n,future in enumerate(concurrent.futures.as_completed(futures),1):
            source=futures[future]
            try:
                data=future.result()
                previous=out/(source.replace('/','__')+'.json')
                if options.refresh_declarations and previous.exists():
                    old=json.loads(previous.read_text())
                    data['functions']=old['functions'];data['functionStatics']=old['functionStatics']
                    data['records']={**old['records'],**data['records']}
                # Generated asset declarations are immutable references, not live state.
                data['globals']=[g for g in data['globals'] if g['definition'] and '/assets/' not in g['file']]
                (out/(source.replace('/','__')+'.json')).write_text(json.dumps(data,separators=(',',':')))
            except Exception as error:failures[source]=str(error)
            if n%20==0 or n==len(todo):print(json.dumps({'completed':n,'requested':len(todo),'failures':len(failures),'seconds':round(time.monotonic()-started,1)}),flush=True)
    (out/('result-'+options.group+('-locals' if options.refresh_locals else '')+'.json')).write_text(json.dumps({'sources':sources,'failures':failures},indent=2))
    return 1 if failures else 0
if __name__=='__main__':sys.exit(main())
