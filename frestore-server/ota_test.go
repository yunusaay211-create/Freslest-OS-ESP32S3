package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"
)

func osFixture() []byte {
	header := make([]byte, 32)
	header[0], header[1], header[3], header[12], header[23] = 0xe9, 1, 0x40, 9, 1
	segment := make([]byte, 320)
	binary.LittleEndian.PutUint32(header[28:], uint32(len(segment)))
	binary.LittleEndian.PutUint32(segment, 0xabcd5432)
	copy(segment[16:], "1.5.1")
	copy(segment[48:], "freslest_os_s3")
	copy(segment[256:], "FRESLEST_LCD:ILI9488")
	image := append(header, segment...)
	checksum := byte(0xef)
	for _, b := range segment {
		checksum ^= b
	}
	for (len(image)+1)%16 != 0 {
		image = append(image, 0)
	}
	image = append(image, checksum)
	hash := sha256.Sum256(image)
	return append(image, hash[:]...)
}

func TestPublishOSOnHTTPS(t *testing.T) {
	root := t.TempDir()
	keyPath := filepath.Join(root, "publisher.pem")
	keygenErr := run([]string{"keygen", "-private", keyPath})
	if keygenErr != nil {
		t.Fatal(keygenErr)
	}
	dir := filepath.Join(root, "published")
	if err := run([]string{"publish", "-private", keyPath, "-input", "examples", "-out", dir}); err != nil {
		t.Fatal(err)
	}
	image := osFixture()
	file := filepath.Join(root, "os.bin")
	if err := os.WriteFile(file, image, 0600); err != nil {
		t.Fatal(err)
	}
	publishArgs := []string{"publish-os", "-private", keyPath, "-profile", "ili9488", "-image", file, "-out", dir}
	if err := run(publishArgs); err != nil {
		t.Fatalf("valid OS publication failed: %v", err)
	}
	publishArgs[4] = "ili9341"
	if err := run(publishArgs); err == nil {
		t.Fatal("OS published for the wrong display profile")
	}
	routes, err := loadStore(dir)
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewTLSServer(storeHandler(routes))
	defer server.Close()
	base := "/v1/os/ili9488/"
	hash := fmt.Sprintf("%x", sha256.Sum256(image))
	for _, route := range []string{base + "manifest", base + hash} {
		res, err := server.Client().Get(server.URL + route)
		if err != nil {
			t.Fatal(err)
		}
		got, readErr := io.ReadAll(res.Body)
		res.Body.Close()
		if readErr != nil || res.StatusCode != 200 || res.TLS == nil || len(got) == 0 {
			t.Fatalf("invalid HTTPS response on %s", route)
		}
		if route == base+hash && !bytes.Equal(got, image) {
			t.Fatal("OS bytes changed in transit")
		}
	}
	mirror := filepath.Join(root, "mirror")
	if err := run([]string{"publish", "-private", keyPath, "-input", "examples", "-out", mirror}); err != nil {
		t.Fatal(err)
	}
	if err := mirrorOS(server.Client(), server.URL, "ili9488", mirror); err != nil {
		t.Fatal(err)
	}
	mirrored, err := loadStore(mirror)
	if err != nil || !bytes.Equal(mirrored[base+hash], image) ||
		!bytes.Equal(mirrored[base+"manifest"], routes[base+"manifest"]) {
		t.Fatal("mirror did not preserve the signed release")
	}
	changed := append([]byte(nil), image...)
	changed[len(changed)-1] ^= 1
	invalid := httptest.NewTLSServer(storeHandler(map[string][]byte{
		base + "manifest": routes[base+"manifest"],
		base + hash:       changed,
	}))
	defer invalid.Close()
	if err := mirrorOS(invalid.Client(), invalid.URL, "ili9488", mirror); err == nil {
		t.Fatal("mirror accepted a corrupted image")
	}
	stillValid, err := loadStore(mirror)
	if err != nil || !bytes.Equal(stillValid[base+hash], image) {
		t.Fatal("failed mirror replaced the last published image")
	}
	if err := mirrorOS(&http.Client{}, "http://127.0.0.1:1", "ili9488", mirror); err == nil {
		t.Fatal("mirror accepted plaintext transport")
	}
}
