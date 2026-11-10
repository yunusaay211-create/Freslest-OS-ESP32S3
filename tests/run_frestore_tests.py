"""Exercise actual Go publications with the firmware's mbed TLS parser."""
from pathlib import Path
import base64
import json
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "Doom/tests"))
from update_target_fixtures import esp_image, OS_PROJECT

sdk = Path(os.environ["IDF_PATH"]) / "components"
tls = sdk / "mbedtls/mbedtls"
cjson = sdk / "json/cJSON"
app = root / "OS/freslest/src/freslest"

with tempfile.TemporaryDirectory() as tmp:
    work = Path(tmp)
    server = work / "frestore"
    subprocess.run(["go", "test", "-race", "./..."], cwd=root / "frestore-server", check=True)
    subprocess.run(["go", "build", "-o", str(server), "."], cwd=root / "frestore-server", check=True)
    private, published = work / "publisher.pem", work / "published"
    subprocess.run([server, "keygen", "-private", private], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([server, "publish", "-private", private,
                    "-input", root / "frestore-server/examples", "-out", published], check=True)
    source, assets = work / "source", work / "assets"
    source.mkdir()
    (source / "large.json").write_text(json.dumps({
        "id": "large", "name": "Large card", "developer": "Freslest", "description": "App data",
        "version": 1, "category": 1, "lines": ["Attached data"], "asset": "large.bin",
    }))
    (source / "large.bin").write_bytes(bytes(range(256)) * 2400)
    subprocess.run([server, "publish", "-private", private, "-input", source, "-out", assets], check=True)
    image = work / "os.bin"
    image.write_bytes(esp_image(OS_PROJECT, version="1.5.1", markers=[b"FRESLEST_LCD:ILI9488"]))
    subprocess.run([server, "publish-os", "-private", private, "-image", image,
                    "-profile", "ili9488", "-out", published], check=True)

    catalogue, package = published / "catalog.fsc", published / "merhaba.1.fsp"
    os_manifest = published / "os-ili9488.fsm"
    negative = []
    for name, source, field, value in (
        ("wrong-target", package, "target", "x64"),
        ("wrong-abi", package, "abi", 2),
        ("bad-deps", catalogue, "dependencies", ["unavailable"]),
        ("wrong-layout", os_manifest, "layout", "different"),
    ):
        body = json.loads(source.read_bytes().split(b"\n", 1)[1])
        if field == "dependencies":
            body["apps"][0][field] = value
        else:
            body[field] = value
        data = json.dumps(body, separators=(",", ":")).encode()
        signature = subprocess.check_output(["openssl", "dgst", "-sha256", "-sign", private], input=data)
        path = work / name
        path.write_bytes(base64.b64encode(signature) + b"\n" + data)
        negative.append(path)

    build = work / "mbedtls"
    subprocess.run(["cmake", "-G", "Ninja", "-S", tls, "-B", build, "-DENABLE_TESTING=OFF",
                    "-DENABLE_PROGRAMS=OFF"], check=True)
    subprocess.run(["cmake", "--build", build, "--target", "mbedcrypto"], check=True)
    obj, exe = work / "cJSON.o", work / "frestore-test"
    flags = ["-fsanitize=address,undefined", "-fno-pie"]
    subprocess.run(["gcc", *flags, "-c", cjson / "cJSON.c", "-o", obj], check=True)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags, "-no-pie",
                    "-I", tls / "include", "-I", cjson, "-I", app, "-I", root / "common/usb_update",
                    root / "tests/test_frestore_package.cpp",
                    app / "frestore_package.cpp", obj, build / "library/libmbedcrypto.a", "-o", exe], check=True)
    subprocess.run([exe, published / "publisher.pub", catalogue, package, *negative, os_manifest, image,
                    root / "frestore-server/starter/v1", assets / "catalog.fsc",
                    assets / "large.1.fsp", assets / "large.1.fsp.dat"],
                   check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
print("PASS: Go/C++ signed starter cards, personal store and OS manifests, SHA-256, tamper and compatibility rejection")
