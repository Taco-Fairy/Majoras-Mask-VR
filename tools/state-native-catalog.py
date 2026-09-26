"""Generate native C visitors in the scope owning private variables/functions.
Output is staged under build/ until reviewed; this tool never edits native code.
Union members with pointers require an explicit runtime discriminator.
"""
import hashlib,json,pathlib,re,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
INPUT=ROOT/'build/exact-states-inventory/native'
OUT=ROOT/'build/exact-states-inventory/bindings'
def pointer_bearing(t, records, seen=None):
    if t['kind']=='pointer':return True
    if t['kind']=='array':return pointer_bearing(t['element'],records,seen)
    if t['kind']!='record':return False
    seen=set() if seen is None else seen
    if t['id'] in seen:return False
    seen=seen|{t['id']}
    return any(pointer_bearing(f['type'],records,seen) for f in records[t['id']]['fields'])
def quoted(s):return json.dumps(s)
def write_changed(path,text):
    if not path.exists() or path.read_text(encoding='utf-8')!=text:
        path.write_text(text,encoding='utf-8')
def emit_fields(t, records, expression, root, path, out, depth=0):
    if not pointer_bearing(t,records):return
    if t['kind']=='pointer':
        target=t['target']
        text=target['kind']=='scalar' and re.sub(r'\b(?:const|volatile|restrict)\b','',target['type']).strip()=='char'
        kind=1 if target['kind']=='function' else (2 if text else 0)
        out.append(f'    sink->pointer(sink->context, (const void*)&({expression}), {kind}, {quoted(path)});')
    elif t['kind']=='array':
        index=f'mmvr_i{depth}'
        if t['count']<0:raise ValueError('Unbounded array '+path)
        out.append(f'    for (size_t {index}=0; {index}<{t["count"]}; ++{index}) {{')
        # Effect storage outlives active trails. Only live elements contain references;
        # inactive bytes are preserved but must not be interpreted as pointers.
        if path.endswith('sEffectContext.tireMarks'):
            out.append(f'    if (!({expression}[{index}].status.active)) continue;')
        if path.endswith('sEffectContext.tireMarks[].effect.elements'):
            owner=expression.rsplit('.elements',1)[0]
            out.append(f'    if ({index} >= (size_t)({owner}.numElements)) break;')
        emit_fields(t['element'],records,expression+'['+index+']',root,path+'[]',out,depth+1)
        out.append('    }')
    elif t['kind']=='record':
        record=records[t['id']]
        if record['name']=='AudioCmd':
            out.append(f'    MMVR_StateVisitAudioCmd(sink, (const void*)&({expression}), {quoted(path)});')
            return
        aliases=record['fields']
        if record['union'] and aliases and all(f['type']['kind']=='pointer' for f in aliases):
            kinds=[f['type']['target']['kind']=='function' for f in aliases]
            if all(k==kinds[0] for k in kinds):
                field=aliases[0]
                emit_fields(field['type'],records,expression+'.'+field['name'],root,path+'.'+field['name'],out,depth+1)
                return
        if record['union']:
            selector=f'sink->variant(sink->context, (const void*)&({root}), (const void*)&({expression}), sizeof({expression}), {quoted(path)}, {len(record["fields"])})'
            if t['id'].split('@location:',1)[0]=='c:@S@PlayerCsActionEntry@Ua':
                selector=f'(({expression}).type <= PLAYER_CSTYPE_ACTION ? 1 : (({expression}).type > PLAYER_CSTYPE_NONE ? 2 : -2))'
            elif record['name']=='CamParamData':
                selector=f'MMVR_StateCameraVariant((const void*)&({expression}))'
            elif 'CamParamData' in t['id']:
                # Anonymous door union shares the CamParamData expression in C.
                selector=f'MMVR_StateCameraDoorVariant((const void*)&({expression}))'
            out.append(f'    switch({selector}) {{')
            out.append('    case -2: break; /* Inactive union: no live reference. */')
        for index,field in enumerate(record['fields']):
            name=field['name']
            # This ABI field is unused storage, not a live actor reference. The
            # KeepOn4 implementation never initializes or dereferences it; old
            # camera numeric parameters can remain here. Preserve its bytes.
            if record['name']=='KeepOn4ReadWriteData' and name=='unk_0C':
                continue
            anonymous=not re.fullmatch(r'[A-Za-z_]\w*',name)
            child=expression if anonymous else expression+'.'+name
            subpath=path+('.@union' if anonymous else '.'+name)
            if record['union']:out.append(f'    case {index}: {{')
            emit_fields(field['type'],records,child,root,subpath,out,depth+1)
            if record['union']:out.append('    } break;')
        if record['union']:
            out.append(f'    default: sink->unsupported(sink->context, {quoted(path)}); break;\n    }}')
        if record['name'] in ('ColliderJntSph','ColliderTris'):
            visitor='MMVR_StateVisit'+record['name']+'Element'
            out.append(f'    sink->array(sink->context, ({expression}).elements, ({expression}).count, sizeof(*({expression}).elements), {visitor}, {quoted(path+".elements[]")});')
def layout_tag(t, records):
    kind=t['kind']
    if kind=='scalar':return 'scalar:'+t['type']
    if kind=='pointer':return 'pointer:'+layout_tag(t['target'],records)
    if kind=='array':return 'array:'+str(t['count'])+':'+layout_tag(t['element'],records)
    if kind=='function':return 'function:'+t['signature']
    r=records[t['id']]
    return ('union:' if r['union'] else 'record:')+('anonymous' if r['anonymous'] else r['name'])

def layout_alignment(t, records, expression):
    if t['kind']=='array':return layout_alignment(t['element'],records,expression+'[0]')
    if t['kind']=='pointer':return 'MMVR_STATE_ALIGN_TYPE(void*)'
    if t['kind']=='scalar':
        # MSVC C11 keeps enums distinct from integer storage in _Generic.
        scalar_name=re.sub(r'\b(const|volatile|restrict)\b','',t['type']).strip()
        builtin={'_Bool','char','signed char','unsigned char','short','unsigned short','int','unsigned int','long','unsigned long','long long','unsigned long long','float','double','long double','void'}
        # Preserve compiler-native typedef/enum identity. Primitive reflection
        # names can differ across targets (ARM64 long versus Windows long long),
        # so those still select the actual member expression's type.
        if scalar_name not in builtin and re.fullmatch(r'(?:enum )?[A-Za-z_]\w*',scalar_name):
            return 'MMVR_STATE_ALIGN_TYPE('+scalar_name+')'
        return 'MMVR_STATE_SCALAR_ALIGN('+expression+')'
    if t['kind']=='record':
        record=records[t['id']]
        if not record['anonymous'] and re.fullmatch(r'(?:struct |union )?[A-Za-z_]\w*',record['name']):
            return 'MMVR_STATE_ALIGN_TYPE('+record['name']+')'
        # Anonymous embedded records have no spellable C type. Their root's
        # compiler alignment and all actual leaf offsets/sizes define storage.
        # Mark this descriptor explicitly instead of copying Android alignment.
        return '1'
    return '0'

def emit_layout(t, records, expression, root, path, out, probe=None, trail=''):
    # Address differences and sizeof are target-compiler expressions; no ARM64
    # reflection offsets are copied into the Windows compatibility contract.
    tag=layout_tag(t,records)
    if t['kind']=='record' and layout_alignment(t,records,expression)=='1':tag+=':embedded-alignment-covered-by-root'
    count=t.get('count',1)
    if count<0:
        out.append(f'    if(sink->layoutUnsupported) sink->layoutUnsupported(sink->context, {quoted(path)}, "incomplete array");')
        return
    offset='0' if expression==root else f'(size_t)((const unsigned char*)&({expression})-(const unsigned char*)&({root}))'
    out.append(f'    sink->layout(sink->context, {quoted(path)}, {quoted(tag)}, {offset}, sizeof({expression}), {layout_alignment(t,records,expression)}, {count});')
    if t['kind']=='array':
        if count:emit_layout(t['element'],records,expression+'[0]',root,path+'[]',out,probe,trail+'[0]')
    elif t['kind']=='record':
        record=records[t['id']]
        if not record['anonymous'] and re.fullmatch(r'(?:struct |union )?[A-Za-z_]\w*',record['name']):
            probe=record['name'];trail=''
        for index,field in enumerate(record['fields']):
            name=field['name'];anonymous=not re.fullmatch(r'[A-Za-z_]\w*',name)
            suffix='' if anonymous else '.'+name
            child=expression+suffix;subpath=path+('.@anonymous'+str(index) if anonymous else suffix)
            if field['bitfield']:
                if anonymous:continue # unnamed padding has no value to restore
                if probe:
                    out.append(f'    if(sink->layoutBytes) {{ {probe} mmvr_layout_probe; memset(&mmvr_layout_probe,0,sizeof(mmvr_layout_probe)); mmvr_layout_probe{trail+suffix}=~0u; sink->layoutBytes(sink->context, {quoted(subpath+":bitfield:"+layout_tag(field["type"],records))}, &mmvr_layout_probe, sizeof(mmvr_layout_probe)); }}')
                else:
                    out.append(f'    if(sink->layoutUnsupported) sink->layoutUnsupported(sink->context, {quoted(subpath)}, "unnamed bitfield owner");')
            else:emit_layout(field['type'],records,child,root,subpath,out,probe,trail+suffix)

def local_identity(source, local, variables):
    same=[v for v in variables if v['function']==local['function'] and v['original']==local['original']]
    # Source line movement is not state identity. Shadowed declarations retain
    # distinct stable declaration-order ordinals; insertion requires migration.
    ordinal=sorted(same,key=lambda v:v['line']).index(local)
    return source+'::'+local['function']+'::'+local['original']+'::local'+str(ordinal)

def main():
    camera_source=(ROOT/'src/2ship2harkinian/mm/src/code/z_camera.c').read_text(encoding='utf-8')
    keep4=camera_source.split('s32 Camera_KeepOn4(Camera* camera) {',1)[1].split('s32 Camera_KeepOn0(',1)[0]
    if re.search(r'rwData\s*->\s*unk_0C',keep4):
        raise ValueError('KeepOn4 unused-field policy needs review: field is now accessed')
    OUT.mkdir(parents=True,exist_ok=True)
    manifest=[];decls=[];calls=[]
    selected={sys.argv[i+1] for i,arg in enumerate(sys.argv[:-1]) if arg=='--source'}
    for file in sorted(INPUT.glob('*.json')):
        if selected and file.name not in {value.replace('/','__')+'.json' for value in selected}:continue
        data=json.loads(file.read_text())
        if 'records' not in data:continue
        source=data['source'];full='src/2ship2harkinian/mm/src/'+source
        suffix=hashlib.sha256(source.encode()).hexdigest()[:16]
        fn='MMVR_StateVisit_'+suffix
        actor_lines=[]; actor_root=None
        source_text=(ROOT/full).read_text(encoding='utf-8')
        local_file=ROOT/'src/mmvr-runtime/save_states/native_locals'/(suffix+'.json')
        locals_info=json.loads(local_file.read_text(encoding='utf-8')) if local_file.exists() else None
        if locals_info and locals_info['sourceSha256']!=hashlib.sha256((ROOT/full).read_bytes()).hexdigest():
            raise ValueError('Regenerate stale function storage: '+source)
        profile=re.search(r'ActorProfile\s+(\w+)\s*=\s*\{.*?sizeof\(([^)]+)\)',source_text,re.S)
        if profile:
            root_name=profile.group(2).strip()
            match=next(((key,r) for key,r in data['records'].items() if r['name'] in (root_name,'struct '+root_name)),None)
            if not match:raise ValueError('Actor layout missing: '+source)
            key,record=match;actor_root={'type':root_name,'record':key,'bytes':record['bytes'],'profile':profile.group(1)}
            actor_lines=[f'static void {fn}_actor(MMVR_StateSink* sink, void* address) {{',f'    {root_name}* value=({root_name}*)address;']
            emit_fields({'kind':'record','id':key},data['records'],'(*value)','(*value)',root_name,actor_lines)
            actor_lines.append('}')
        roots=[]
        if source=='code/z_play.c':roots=[('PlayState','MMVR_StateVisitPlay')]
        if source=='audio/lib/effects.c':roots=[('Note','MMVR_StateVisitNote'),('SequenceChannel','MMVR_StateVisitSequenceChannel'),('NoteSampleState','MMVR_StateVisitNoteSampleState')]
        if source=='code/z_collision_check.c':roots=[(name,'MMVR_StateVisit'+name) for name in ('ColliderJntSphElement','ColliderTrisElement','ColliderQuad','ColliderCylinder','ColliderSphere','ColliderTris')]
        if source=='code/z_effect_soft_sprite.c':roots=[('EffectSs','MMVR_StateVisitEffectSs')]
        if source=='boot/O2/system_malloc.c':roots=[('ArenaNode','MMVR_StateVisitArenaNode')]
        if source=='code/gamealloc.c':roots=[('GameAllocEntry','MMVR_StateVisitGameAllocEntry')]
        for root_name,root_fn in roots:
            key,record=next((key,r) for key,r in data['records'].items() if r['name'] in (root_name,'struct '+root_name))
            actor_lines.extend([f'void {root_fn}(MMVR_StateSink* sink, void* address) {{',f'    {root_name}* value=({root_name}*)address;'])
            emit_fields({'kind':'record','id':key},data['records'],'(*value)','(*value)',root_name,actor_lines)
            actor_lines.append('}')
        if source=='code/z_camera.c':
            members=['NORMAL1','NORMAL3','NORMAL0','PARALLEL1','JUMP2','JUMP3','BATTLE1','KEEPON1','KEEPON3','KEEPON4','FIXED1','FIXED2','SUBJECT1','UNIQUE2','UNIQUE0','UNIQUE6','DEMO1','DEMO2','DEMO3','DEMO4','DEMO5','DEMO0','SPECIAL5','SPECIAL8']
            actor_lines += [
                'static int MMVR_StateCameraFunction(const void* address) {',
                '    const Camera* camera=(const Camera*)address;',
                '    if(camera->setting<0 || camera->setting>=ARRAY_COUNT(sCameraSettings) || camera->mode<0 || camera->mode>=32) return CAM_FUNC_NONE;',
                '    const CameraSetting* setting=&sCameraSettings[camera->setting];',
                '    if(!setting->cameraModes || !(setting->validModes & (1u<<camera->mode))) return CAM_FUNC_NONE;',
                '    return setting->cameraModes[camera->mode].funcId;',
                '}',
                'int MMVR_StateCameraVariant(const void* address) {',
                '    switch(MMVR_StateCameraFunction(address)) {']
            for i,name in enumerate(members):actor_lines.append(f'    case CAM_FUNC_{name}: return {i};')
            for name,index in [('DATA1',0),('UNIQUE3',0),('UNIQUE4',3),('UNIQUE5',6)]:
                actor_lines.append(f'    case CAM_FUNC_{name}: return {index};')
            inactive=['NONE','NORMAL2','NORMAL4','PARALLEL0','PARALLEL2','PARALLEL3','PARALLEL4','KEEPON0','KEEPON2','SUBJECT0','SUBJECT2','SUBJECT3','SUBJECT4','JUMP0','JUMP1','JUMP4','BATTLE0','BATTLE2','BATTLE3','BATTLE4','FIXED0','FIXED3','FIXED4','DATA0','DATA2','DATA3','DATA4','UNIQUE1','UNIQUE7','UNIQUE8','UNIQUE9','DEMO6','DEMO7','DEMO8','DEMO9','SPECIAL0','SPECIAL1','SPECIAL2','SPECIAL3','SPECIAL4','SPECIAL6','SPECIAL7']
            for name in inactive:actor_lines.append(f'    case CAM_FUNC_{name}: return -2;')
            actor_lines += ['    case CAM_FUNC_SPECIAL9: return 23;', '    default: return -1;', '    }', '}',
                            'int MMVR_StateCameraDoorVariant(const void* address) { return MMVR_StateCameraFunction(address)==CAM_FUNC_SPECIAL9 ? 1 : 0; }']
        lines=['/* Generated by tools/state-native-catalog.py; do not hand edit. */',
               '#include "2s2h/VR/NativeStateVisitor.h"', '#include <string.h>']+actor_lines+[f'void {fn}(MMVR_StateSink* sink) {{']
        if actor_root:
            lines.append('    if(sink->layout) { '+actor_root['type']+' mmvr_layout_value;')
            emit_layout({'kind':'record','id':actor_root['record']},data['records'],'mmvr_layout_value','mmvr_layout_value',source+'::actor-layout',lines)
            lines.append('    }')
            lines.append(f'    sink->actor(sink->context, {actor_root["profile"]}.id, sizeof({actor_root["type"]}), {fn}_actor);')
        for root_name, root_fn in roots:
            root_key=next(key for key,r in data['records'].items() if r['name'] in (root_name,'struct '+root_name))
            lines.append('    if(sink->layout) { '+root_name+' mmvr_layout_value;')
            emit_layout({'kind':'record','id':root_key},data['records'],'mmvr_layout_value','mmvr_layout_value',source+'::heap-layout::'+root_name,lines)
            lines.append('    }')
        own_globals=[g for g in data['globals'] if g['file'].replace('\\','/')==full or (source=='code/z_camera.c' and g['file'].replace('\\','/').endswith('/z_camera_data.inc')) or (source=='overlays/actors/ovl_En_Kanban/z_en_kanban.c' and g['file'].replace('\\','/').endswith('/z_en_kanban_gfx.inc'))]
        for g in own_globals:
            if not re.fullmatch(r'[A-Za-z_]\w*',g['name']):raise ValueError('Invalid C symbol')
            name=g['name'];identity=source+'::'+name
            lines.append('    if(sink->layout) {')
            emit_layout(g['type'],data['records'],name,name,identity,lines)
            lines.append('    }')
            if g['const']:
                lines.append(f'    sink->constant(sink->context, {quoted(identity)}, &{name}, sizeof({name}));')
            else:
                lines.append(f'    sink->block(sink->context, {quoted(identity)}, &{name}, sizeof({name}));')
                emit_fields(g['type'],data['records'],name,name,name,lines)
        if locals_info:
            for local in locals_info['variables']:
                g=next(v for v in data['functionStatics'] if v['name']==local['original'] and v['function']==local['function'] and v['line']==local['line'])
                name=local['name'];identity=local_identity(source,local,locals_info['variables'])
                lines.append('    if(sink->layout) {')
                emit_layout(g['type'],data['records'],name,name,identity,lines)
                lines.append('    }')
                if g['const']:
                    lines.append(f'    sink->constant(sink->context, {quoted(identity)}, &{name}, sizeof({name}));')
                else:
                    lines.append(f'    sink->block(sink->context, {quoted(identity)}, &{name}, sizeof({name}));')
                    emit_fields(g['type'],data['records'],name,name,identity,lines)
        for func in data['functions']:
            if func['file'].replace('\\','/')==full:
                # Reflection includes private message-render fixtures. Match their
                # definition guard so public builds never reference absent helpers.
                private_message_helper = source == 'code/z_message.c' and func['name'] in (
                    'MMVR_TestMessageRectangle', 'MMVR_TestMessageBackground')
                if private_message_helper:
                    lines.append('#ifdef MMVR_LOCAL_TEST_TOOLS')
                lines.append(f'    sink->function(sink->context, {quoted(source+"::"+func["name"])}, (void (*)(void)){func["name"]});')
                if private_message_helper:
                    lines.append('#endif')
        if source=='overlays/gamestates/ovl_file_choose/z_file_choose_NES.c':
            # These externally linked callbacks are implemented by a C++ enhancement,
            # not the C reflection TU. Their prototypes are in z_file_select.h.
            for name in ('FileSelect_RotateToNewFileSetup','FileSelect_UpdateNewFileSetup'):
                lines.append(f'    sink->function(sink->context, {quoted("2s2h/Enhancements/Saving/NewFileSetup.cpp::"+name)}, (void (*)(void)){name});')
        lines.append('}')
        write_changed(OUT/(suffix+'.inc'),'\n'.join(lines)+'\n')
        if '--stage' in sys.argv:
            stage=ROOT/'src/mmvr-runtime/save_states/native_generated'
            stage.mkdir(parents=True,exist_ok=True)
            write_changed(stage/(suffix+'.inc'),'\n'.join(lines)+'\n')
            native=('../native_locals/'+suffix+'.inc') if locals_info else ('../../../2ship2harkinian/mm/src/'+source)
            write_changed(stage/(suffix+'.c'),'#include "'+native+'"\n#include "'+suffix+'.inc"\n')

        mutable_local=[v for v in data['functionStatics'] if not v['const']]
        manifest.append({'source':source,'provider':fn,'include':suffix+'.inc',
                         'globals':len(own_globals),'actor':actor_root,'functionStatics':mutable_local})
        decls.append(f'void {fn}(MMVR_StateSink*);');calls.append(f'    {fn}(sink);')
    if selected:
        print(json.dumps({'updatedProviders':[m['source'] for m in manifest]}));return
    (OUT/'Providers.inc').write_text('\n'.join(decls)+'\nvoid MMVR_VisitNativeState(MMVR_StateSink* sink) {\n'+'\n'.join(calls)+'\n}\n')
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2))
    print(json.dumps({'providers':len(manifest),'globals':sum(m['globals'] for m in manifest),
                      'functionStatics':sum(len(m['functionStatics']) for m in manifest)}))
if __name__=='__main__':main()
