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
body=page.find("./rect[@name='Body']")
def bounds(tile):
    return tuple(int(tile.findtext(trait)) for trait in ("x","y","width","height"))

mx,my,mw,mh=bounds(monitor)
assert mx+mw <= int(body.findtext("width")) and my+mh <= int(body.findtext("height"))
status=body.find("./text[@name='Status']")
column_x=int(status.findtext("x"))
assert column_x+int(status.findtext("wrapwidth")) < mx
last_bottom=int(status.findtext("y"))
for action in ("Record","Stream","Replay"):
    tile=body.find(f"./hotrect[@name='{action}']")
    x,y,w,h=bounds(tile)
    assert x==column_x and x+w < mx and y>=last_bottom
    assert tile.find("./text/wrapwidth/copy").attrib=={"src":"parent()", "trait":"width"}
    last_bottom=y+h
assert last_bottom==my+mh and last_bottom+80 <= int(body.findtext("height"))
overlay=body.find("./rect[@name='NoPreview']")
for trait in ("x","y","width","height"):
    assert overlay.find(f"{trait}/copy").attrib=={"src":"sibling(Monitor)", "trait":trait}
buttons={b.attrib['name']:b.findtext('id') for b in page.iter('hotrect')}
assert buttons==dict(Tab='29471',Record='29472',Stream='29473',Replay='29474')
click=(root/'Data/NVSE/user_defined_functions/TTWVROBS/Click.txt').read_text()
assert all('tileID == '+v in click for v in buttons.values())
print('PauseBoy controls and preview layout verified.')
