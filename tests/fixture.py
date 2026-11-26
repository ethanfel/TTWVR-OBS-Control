"""Run the Windows probe against an authenticated, loopback-only mock OBS."""
import asyncio
import base64
import hashlib
import json
import os
from pathlib import Path
import secrets
import struct
import sys
import tempfile

from websockets.asyncio.server import serve

async def main():
    exe = Path(sys.argv[1]).resolve()
    secret, salt, challenge = (secrets.token_hex(16) for _ in range(3))
    def digest(s):
        return base64.b64encode(hashlib.sha256(s.encode()).digest()).decode()
    actions = []
    async def handle(ws):
        async def send(op, data):
            await ws.send(json.dumps(dict(op=op, d=data)))
        await send(0, dict(rpcVersion=1, authentication=dict(salt=salt, challenge=challenge)))
        ident = json.loads(await ws.recv())
        subscriptions = ident['d']['eventSubscriptions']
        assert subscriptions in (0, 64)
        assert ident == dict(op=1,d=dict(rpcVersion=1, eventSubscriptions=subscriptions,
                                          authentication=digest(digest(secret+salt)+challenge)))
        await send(2, dict(negotiatedRpcVersion=1))
        live = rec = replay = False
        save_polls = 0
        async def saved(path):
            await send(5, dict(eventType='ReplayBufferSaved', eventIntent=64,
                               eventData=dict(savedReplayPath=path)))
        async for raw in ws:
            message = json.loads(raw)
            assert message['op'] == 6
            req = message['d']
            t = req['requestType']
            ok = True
            if t == 'GetRecordStatus':
                data = dict(outputActive=rec,outputPaused=False)
            elif t == 'GetStreamStatus':
                data = dict(outputActive=live,outputReconnecting=False)
            elif t == 'GetReplayBufferStatus':
                assert subscriptions == 64
                ok = len(actions) >= 5
                data = dict(outputActive=replay) if ok else {}
                if save_polls:
                    save_polls -= 1
                    if save_polls == 0: await saved('mock/first.mkv')
            elif t == 'StartReplayBuffer':
                assert not replay
                replay = True
                actions.append(t)
                data = {}
                await saved('mock/earlier.mkv')
            elif t == 'SaveReplayBuffer':
                assert replay
                actions.append(t)
                saves = actions.count(t)
                if saves == 1:
                    save_polls = 4
                    await saved('')  # Missing path cannot confirm a clip.
                elif saves == 2:
                    await saved('mock/second.mkv')  # Event can precede the request response.
                else:
                    ok = False
                data = {}
            elif t == 'GetCurrentProgramScene':
                assert subscriptions == 0
                data = dict(currentProgramSceneName='Game Program')
            elif t == 'GetSourceScreenshot':
                assert req['requestData']['sourceName'] == 'Game Program'
                assert req['requestData']['imageWidth'] == 320
                assert req['requestData']['imageFormat'] == 'bmp'
                # One padded row tests BMP stride; all rows are sent bottom-up.
                w, h = 319, 180
                row = bytes((32, 69, 217)) * w + b'\x00' * ((-w*3) % 4)
                raw = row*h
                head = b'BM' + struct.pack('<IHHI',54+len(raw),0,0,54)
                dib = struct.pack('<IiiHHIIiiII',40,w,h,1,24,0,len(raw),0,0,0,0)
                data = dict(imageData='data:image/bmp;base64,'+base64.b64encode(head+dib+raw).decode())
            else:
                actions.append(t)
                ok = len(actions) < 5
                if ok:
                    if t in ('StartRecord','StopRecord'): rec = t == 'StartRecord'
                    elif t in ('StartStream','StopStream'): live = t == 'StartStream'
                    else: raise AssertionError(t)
                data = {}
            await send(7,dict(requestType=t,requestId=req['requestId'],
                              requestStatus=dict(result=ok, code=100 if ok else 500), responseData=data))
    async with serve(handle, '127.0.0.1', 0, subprotocols=['obswebsocket.json']) as server:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root/'obs-studio/plugin_config/obs-websocket/config.json'
            config.parent.mkdir(parents=True)
            config.write_text(json.dumps(dict(server_port=server.sockets[0].getsockname()[1],
                                              server_password=secret)))
            (root/'mock-obs-only.flag').touch()
            args = [str(exe), '--fixture']
            process = await asyncio.create_subprocess_exec(*args, cwd=root,
                                  env={**os.environ,'APPDATA':str(root)})
            code = await asyncio.wait_for(process.wait(), 30)
            assert code == 0, code
            assert actions == ['StartRecord','StartStream','StopRecord','StopStream','StartRecord',
                               'StartReplayBuffer','SaveReplayBuffer','SaveReplayBuffer','SaveReplayBuffer'], actions
            print('Authenticated, verified four output transitions, duplicate/stale suppression and OBS rejection. Verified replay start, delayed/early save events and rejection.')
if __name__=='__main__':
    asyncio.run(main())
