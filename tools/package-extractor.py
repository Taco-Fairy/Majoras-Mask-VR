"""Bundle only upstream extraction metadata, never game ROMs or extracted game assets."""
import pathlib,zipfile
root=pathlib.Path(__file__).resolve().parents[1]
output=root/'build/quest-package/assets/extractor.zip';output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for source,prefix in [('src/2ship2harkinian/mm/assets/extractor','assets'),('src/2ship2harkinian/mm/assets/xml','assets/xml')]:
  base=root/source
  for p in sorted(base.rglob('*')):
   if not p.is_file():continue
   if p.suffix not in ('.xml','.txt'):raise RuntimeError('Unexpected extractor metadata: '+str(p))
   name=prefix+'/'+p.relative_to(base).as_posix()
   info=zipfile.ZipInfo(name,(2020,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,p.read_bytes())
print('Packaged extractor metadata:',output.stat().st_size,'bytes')
