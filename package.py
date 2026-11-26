from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

root = Path(__file__).parent
version = (root / 'VERSION').read_text().strip()
out = root / 'build' / f'TTWVR-OBS-Control-{version}.zip'
with ZipFile(out, 'w', ZIP_DEFLATED) as z:
    for file in sorted((root / 'Data').rglob('*')):
        if file.is_file():
            z.write(file, file.relative_to(root / 'Data'))
    z.write(root / 'build/TTWVROBS.dll', 'NVSE/Plugins/TTWVROBS.dll')
    z.write(root / 'README.md', 'docs/TTWVROBS/README.md')
with ZipFile(out) as z:
    assert z.testzip() is None
print(out)
