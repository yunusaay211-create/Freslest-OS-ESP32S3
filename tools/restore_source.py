"""Restore large upstream files byte-for-byte from the approved source archive."""
import base64
import hashlib
import json
import os
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]


def restore(item):
    path = ROOT / item['path']
    if 'base64' in item:
        data = base64.b64decode(item['base64'])
    else:
        url = 'https://api.github.com/repos/' + item['upstream'] + '/git/blobs/' + item['sha']
        headers = {'Accept': 'application/vnd.github.raw+json', 'User-Agent': 'Freslest-source-restore'}
        token = os.environ.get('GH_TOKEN')
        if token:
            headers['Authorization'] = 'Bearer ' + token
        with urlopen(Request(url, headers=headers), timeout=180) as response:
            data = response.read()
            if 'json' in response.headers.get('Content-Type', ''):
                data = base64.b64decode(json.loads(data)['content'])
    actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    if actual != item['sha']:
        raise RuntimeError('Source hash mismatch: ' + item['path'])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return item['path']


if __name__ == '__main__':
    manifest = json.loads((ROOT / 'tools/source_restore_manifest.json').read_text())
    with ThreadPoolExecutor(max_workers=4) as pool:
        for path in pool.map(restore, manifest):
            print('Verified:', path, flush=True)
