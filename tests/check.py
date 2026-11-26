from pathlib import Path
import struct
import xml.etree.ElementTree as ET
from zipfile import ZipFile

root=Path(__file__).resolve().parents[1]
binary=root.parent/'local/ttwvr117/original/NVSE/Plugins/TTWVR.dll'
if binary.is_file():
    b=binary.read_bytes()
    pe=struct.unpack_from('<I',b,60)[0]
    assert struct.unpack_from('<I',b,pe+8)[0]==0x6afa9b4f
    for site,target in [(0x12bc66,0x12f6d0),(0x13ca91,0x13c270),(0x13cb88,0x13d8c0),(0x13ca61,0x13af90),(0x101bfe,0x100700)]:
        off=site-0xc00
        assert b[off]==0xe8 and site+5+struct.unpack_from('<i',b,off+1)[0]==target
xml=(root/'Data/menus/prefabs/TTWVROBS/panel.xml').read_text()
for k,v in [('true','1'),('false','0'),('Pipboy','1')]:
    xml=xml.replace('&'+k+';',v)
page=ET.fromstring(xml)
monitor=page.find("./rect[@name='Body']/image[@name='Monitor']")
assert monitor is not None and monitor.findtext("zoom")=="-1", "Fit the entire OBS texture to the monitor"
assert int(monitor.findtext("width"))*9==int(monitor.findtext("height"))*16
buttons={b.attrib['name']:b.findtext('id') for b in page.iter('hotrect')}
assert buttons==dict(Tab='29471',Record='29472',Stream='29473',Replay='29474')
click=(root/'Data/NVSE/user_defined_functions/TTWVROBS/Click.txt').read_text()
assert all('tileID == '+v in click for v in buttons.values())
print('PauseBoy controls and target TTWVR build verified.')
