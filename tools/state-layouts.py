"""Compiler-derived layout inventory. Does not guess pointer fields from values."""
import ctypes,json,pathlib,sys,hashlib,os
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/state-reflection'))
from clang import cindex
if not cindex.Config.loaded:
    cindex.Config.set_library_file(str(ROOT/'tools/state-reflection/clang/native/libclang.dll'))
def split(command):
    argc=ctypes.c_int();fn=ctypes.windll.shell32.CommandLineToArgvW
    fn.restype=ctypes.POINTER(ctypes.c_wchar_p);fn.argtypes=[ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_int)]
    result=fn(command,ctypes.byref(argc))
    try:return [result[i] for i in range(argc.value)]
    finally:ctypes.windll.kernel32.LocalFree(result)
def analyze(source, declarations_only=False, return_translation_unit=False, extra_args=()):
    commands=json.loads((ROOT/'build/android-quest/compile_commands.json').read_text())
    entry=next((e for e in commands if pathlib.Path(e['file']).as_posix().endswith(source)),None)
    if entry is None:
        # Exact-state builds compile generated wrappers instead of original C
        # files. Reuse their flags, but parse the original source for regeneration.
        key=hashlib.sha256(source.encode()).hexdigest()[:16]
        entry=next((e for e in commands if pathlib.Path(e['file']).as_posix().endswith('/native_generated/'+key+'.c')),None)
        if entry is None:raise RuntimeError('No compile command for '+source)
        entry=dict(entry,file=str(ROOT/'src/2ship2harkinian/mm/src'/source))
    parts=split(entry['command'])[1:];args=[];i=0
    while i<len(parts):
        a=parts[i]
        if a in ('-o','-c'):i+=2;continue
        if a=='-Xclang' and i+3<len(parts) and parts[i+1] in ('-include-pch','-include'):
            if parts[i+1]=='-include':
                # Do not let another libclang silently select the NDK's binary PCH.
                header=ROOT/('build/exact-states-inventory/forced-'+hashlib.sha256(source.encode()).hexdigest()[:16]+'.h')
                header.parent.mkdir(parents=True,exist_ok=True)
                header.write_text(pathlib.Path(parts[i+3]).read_text())
                args+=['-include',str(header)]
            i+=4;continue
        if a=='-Winvalid-pch':i+=1;continue
        if a not in ('-Wno-discarded-qualifiers','-Wno-discarded-array-qualifiers'):args.append(a)
        i+=1
    args += ["-resource-dir=" + str(pathlib.Path(os.environ["ANDROID_HOME"])/"ndk/27.0.12077973/toolchains/llvm/prebuilt/windows-x86_64/lib/clang/18")]
    args += list(extra_args)
    # Each worker owns its parse inputs; no shared mutable diagnostic file.
    tu=cindex.Index.create().parse(entry['file'],args=args,options=cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES if declarations_only else 0)
    errors=[str(d) for d in tu.diagnostics if d.severity>=cindex.Diagnostic.Error]
    if errors:raise RuntimeError('\n'.join(errors))
    if return_translation_unit:return tu
    records={};pending={};seen=set();globals=[];functions=[];locals=[]
    def typeinfo(t):
        t=t.get_canonical();k=t.kind
        if k==cindex.TypeKind.POINTER:
            target=t.get_pointee();name=target.spelling
            return {'kind':'pointer','bytes':t.get_size(),'target':typeinfo(target) if target.kind not in (cindex.TypeKind.FUNCTIONPROTO,cindex.TypeKind.FUNCTIONNOPROTO) else {'kind':'function','signature':name}}
        if k in (cindex.TypeKind.CONSTANTARRAY,cindex.TypeKind.INCOMPLETEARRAY):
            return {'kind':'array','count':t.get_array_size(),'bytes':t.get_size(),'element':typeinfo(t.get_array_element_type())}
        if k==cindex.TypeKind.RECORD:
            decl=t.get_declaration();key=decl.get_usr() or t.spelling
            # libclang can give sibling anonymous structs the same @Sa USR.
            # Their declaration locations distinguish the actual C layouts.
            if decl.is_anonymous():
                key += '@location:'+str(localpath(decl.location))+':'+str(decl.location.line)+':'+str(decl.location.column)
            if key not in pending and key not in records:pending[key]=t
            return {'kind':'record','id':key,'bytes':t.get_size()}
        if k in (cindex.TypeKind.FUNCTIONPROTO,cindex.TypeKind.FUNCTIONNOPROTO):return {'kind':'function','signature':t.spelling}
        return {'kind':'scalar','type':t.spelling,'bytes':t.get_size()}
    def localpath(location):
        try:return pathlib.Path(location.file.name).relative_to(ROOT).as_posix() if location.file else None
        except ValueError:return str(location.file)
    def immutable(t):
        t=t.get_canonical()
        # Clang places const on the canonical array, while its returned element
        # type can be unqualified. Check both levels before descending.
        if t.is_const_qualified():return True
        if t.kind in (cindex.TypeKind.CONSTANTARRAY,cindex.TypeKind.INCOMPLETEARRAY):
            return immutable(t.get_array_element_type())
        return False
    def walk(cursor,function=None):
        path=localpath(cursor.location)
        if cursor.kind==cindex.CursorKind.FUNCTION_DECL:
            if path and path.startswith('src/2ship2harkinian/mm/') and cursor.is_definition():
                functions.append({'name':cursor.spelling,'file':path,'line':cursor.location.line,'static':cursor.storage_class==cindex.StorageClass.STATIC})
                function=cursor.spelling
        if cursor.kind==cindex.CursorKind.VAR_DECL and path and path.startswith('src/2ship2harkinian/mm/'):
            if cursor.storage_class!=cindex.StorageClass.EXTERN and (function is None or cursor.storage_class==cindex.StorageClass.STATIC):
                info={'name':cursor.spelling,'file':path,'line':cursor.location.line,'function':function,'const':immutable(cursor.type),'definition':cursor.is_definition() or cursor.storage_class!=cindex.StorageClass.EXTERN,'static':cursor.storage_class==cindex.StorageClass.STATIC,'type':typeinfo(cursor.type)}
                (globals if function is None else locals).append(info)
        if cursor.kind==cindex.CursorKind.TYPEDEF_DECL and path and path.startswith('src/2ship2harkinian/mm/'):
            typeinfo(cursor.underlying_typedef_type)
        for child in cursor.get_children():walk(child,function)
    walk(tu.cursor)
    while pending:
        key,t=pending.popitem()
        if key in records:continue
        decl=t.get_declaration();record={'name':t.spelling,'bytes':t.get_size(),'align':t.get_align(),'union':decl.kind==cindex.CursorKind.UNION_DECL,'anonymous':decl.is_anonymous(),'file':localpath(decl.location),'fields':[]};records[key]=record
        for field in t.get_fields():record['fields'].append({'name':field.spelling,'anonymous':field.is_anonymous(),'offsetBits':field.get_field_offsetof(),'bitfield':field.is_bitfield(),'type':typeinfo(field.type)})
    return {'source':source,'sourceSha256':hashlib.sha256(pathlib.Path(entry['file']).read_bytes()).hexdigest(),'abi':'aarch64-android','records':records,'globals':globals,'functionStatics':locals,'functions':functions}
if __name__=='__main__':
    source=sys.argv[1];result=analyze(source);out=ROOT/sys.argv[2];out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps({'records':len(result['records']),'globals':len(result['globals']),'functionStatics':len(result['functionStatics']),'functions':len(result['functions'])}))
