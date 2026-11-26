from pathlib import Path
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[1]
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
print('PauseBoy controls and preview layout verified.')
