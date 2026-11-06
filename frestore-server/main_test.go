package main

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"io"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strconv"
	"testing"
	"time"
)

func TestPublishAndServeTLS(t *testing.T) {
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		t.Fatal(err)
	}
	dir := t.TempDir()
	if err := publish("examples", dir, key); err != nil {
		t.Fatal(err)
	}
	routes, err := loadStore(dir)
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewTLSServer(storeHandler(routes))
	defer server.Close()
	for _, route := range []string{"/v1/catalog", "/v1/packages/merhaba/1"} {
		response, err := server.Client().Get(server.URL + route)
		if err != nil {
			t.Fatal(err)
		}
		body, readErr := io.ReadAll(response.Body)
		response.Body.Close()
		if readErr != nil || response.StatusCode != 200 || response.TLS == nil ||
			response.Header.Get("Content-Length") != strconv.Itoa(len(body)) || !bytes.Equal(body, routes[route]) {
			t.Fatalf("invalid HTTPS response for %s", route)
		}
		if _, err := verify(&key.PublicKey, body); err != nil {
			t.Fatal(err)
		}
	}
	for _, test := range []struct {
		method, path string
		status       int
	}{
		{"POST", "/v1/catalog", 405}, {"GET", "/v1/packages/merhaba/2", 404},
		{"GET", "/v1/packages/%2e%2e/1", 404},
	} {
		r := httptest.NewRequest(test.method, test.path, nil)
		w := httptest.NewRecorder()
		storeHandler(routes).ServeHTTP(w, r)
		if w.Code != test.status {
			t.Fatalf("%s %s: %d", test.method, test.path, w.Code)
		}
	}
	path := filepath.Join(dir, packageName("merhaba", 1))
	original := routes["/v1/packages/merhaba/1"]
	if err := publish("examples", dir, key); err != nil {
		t.Fatal(err)
	}
	reused, err := os.ReadFile(path)
	if err != nil || !bytes.Equal(reused, original) {
		t.Fatal("republishing altered an existing version")
	}
	reused[len(reused)-1] ^= 1
	if err := os.WriteFile(path, reused, 0644); err != nil {
		t.Fatal(err)
	}
	if _, err := loadStore(dir); err == nil {
		t.Fatal("changed package accepted")
	}
	if err := publish("examples", dir, key); err == nil {
		t.Fatal("rewrote a conflicting version")
	}
}

func TestBoundsAndRateLimit(t *testing.T) {
	now := time.Unix(0, 0)
	l := limit{tokens: 30, last: now}
	for i := 0; i < 30; i++ {
		if !l.allow(now) {
			t.Fatal("initial burst rejected")
		}
	}
	if l.allow(now) || !l.allow(now.Add(time.Second)) {
		t.Fatal("rate limit did not recover correctly")
	}
	for _, id := range []string{"../escape", "a/b", "-test", "TURK", ""} {
		if appID.MatchString(id) {
			t.Fatalf("accepted invalid ID %q", id)
		}
	}
}
