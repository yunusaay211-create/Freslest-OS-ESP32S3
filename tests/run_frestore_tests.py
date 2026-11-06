"""Exercise actual Go publications with the firmware's mbed TLS parser."""
from pathlib import Path
import base64
import json
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
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

    catalogue, package = published / "catalog.fsc", published / "merhaba.1.fsp"
    negative = []
    for name, source, field, value in (
        ("wrong-target", package, "target", "x64"),
        ("wrong-abi", package, "abi", 2),
        ("bad-deps", catalogue, "dependencies", ["unavailable"]),
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
                    "-I", tls / "include", "-I", cjson, "-I", app, root / "tests/test_frestore_package.cpp",
                    app / "frestore_package.cpp", obj, build / "library/libmbedcrypto.a", "-o", exe], check=True)
    subprocess.run([exe, published / "publisher.pub", catalogue, package, *negative],
                   check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
print("PASS: HTTPS catalogue and packages, matching SHA-256, ECDSA verification, tamper and compatibility rejection")
