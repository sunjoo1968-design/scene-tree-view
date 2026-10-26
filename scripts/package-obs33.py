"""OBS33 release allowlist packager. SunjooAn. Run from any directory."""
from pathlib import Path
import json,shutil,zipfile,hashlib,re
root=Path(__file__).resolve().parents[1]
if (root/'scripts/camera-mix-hybrid.lua').exists():
 component='controller';module='camera-mix-hybrid';version=re.search("local VERSION, AUTHOR = '([^']+)'",(root/'scripts/camera-mix-hybrid.lua').read_text(encoding='utf-8'))[1];dll=root/'.local/obs33/controller-build/Release'/f'{module}.dll';display='Controller'
elif (root/'source/hub/package.json').exists():
 component='tally';module='obs-tally-bridge';version=json.loads((root/'source/hub/package.json').read_text())['version'];dll=root/'.build-tools/obs33-bridge/Release'/f'{module}.dll';display='Tally'
elif (root/'buildspec.json').exists():
 component='scene-tree';module='scene-anchor';version=json.loads((root/'buildspec.json').read_text())['version'];dll=root/'build-obs33/Release'/f'{module}.dll';display='Scene Tree View'
else:
 component='multiview';module='obs-multiview-plus';version=re.search(r'project\(obs-multiview-plus VERSION ([0-9.]+)',(root/'CMakeLists.txt').read_text())[1];dll=root/'build-obs33/Release'/f'{module}.dll';display='Multiview'
stage=root/'release'/f'obs33-{component}-{version}'
stage.mkdir(parents=True,exist_ok=True)
def copy(source,target):
 target=stage/target;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/source,target)
copy(dll.relative_to(root),f'plugins/{module}/{module}.dll')
if component=='scene-tree': shutil.copytree(root/'data',stage/f'plugins/{module}/data',dirs_exist_ok=True)
data=stage/f'plugins/{module}/data';data.mkdir(parents=True,exist_ok=True)
(data/'manifest.json').write_text(json.dumps(dict(name=module,id='com.sunjoo.obs-link.'+module,display_name='Sunjoo OBS Link '+display,version=version,os_arch='windows-x64',description='SunjooAn / OBS 33.0.0',urls={})),encoding='utf-8')
copy('docs/OBS33.md','README.md');copy('docs/OBS33-VALIDATION.md','VALIDATION.md')
# Build from the complete source ZIP using scripts/build-obs33.ps1.
if component=='controller':
 copy('scripts/camera-mix-hybrid.lua','scripts/camera-mix-hybrid.lua')
 copy('scripts/camera-mix-hybrid.lua','source/scripts/camera-mix-hybrid.lua')
 for f in ('output.cpp','CMakeLists.txt','LICENSE'): copy('native/camera-mix-hybrid/'+f,'source/native/camera-mix-hybrid/'+f)
 for f in ('LICENSE-OBS','LICENSE-source-switcher'):copy('vendor/'+f,'licenses/'+f)
elif component=='tally':
 for name in ('vtally-server','vtally-web'):copy('source/hub/portable/'+('vtally-server.exe' if name=='vtally-server' else 'obs-single-build/vtally-web.exe'),'hub/'+name+'-obs.exe')
 shutil.copytree(root/'source/hub/firmware',stage/'hub/firmware',dirs_exist_ok=True)
 shutil.copytree(root/'source/obs-tally-bridge',stage/'source/obs-tally-bridge',dirs_exist_ok=True)
 copy('source/hub/package.json','source/hub/package.json')
 copy('LICENSE','licenses/LICENSE-wifi-tally-MIT')
else:copy('LICENSE','LICENSE')
files=sorted(f for f in stage.rglob('*') if f.is_file() and f.name!='SHA256SUMS.txt')
for f in files:
 assert not any(n in str(f.relative_to(stage)).lower() for n in ['wifi-tally.json','local-resume','node_modules','layout.json','.local']),f
rows=[hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(stage).as_posix() for f in files]
(stage/'SHA256SUMS.txt').write_text('\n'.join(rows)+'\n',encoding='utf-8')
output=root/'release'/f'sunjoo-obs-link-{component}-{version}-obs33-windows-x64.zip'
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
 for f in stage.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(stage).as_posix())
print(output,hashlib.sha256(output.read_bytes()).hexdigest())
