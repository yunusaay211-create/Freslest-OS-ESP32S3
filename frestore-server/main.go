package main

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"crypto/tls"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"sync"
	"time"
)

const (
	maxApps     = 16
	maxCatalog  = 12288
	maxPackage  = 4096
	maxAsset    = 1<<32 - 1 - maxPackage
	catalogFile = "catalog.fsc"
	publicFile  = "publisher.pub"
	target      = "esp32s3"
	abi         = 1
)

var appID = regexp.MustCompile(`^[a-z0-9][a-z0-9-]{0,23}$`)

type source struct {
	ID          string   `json:"id"`
	Name        string   `json:"name"`
	Developer   string   `json:"developer"`
	Description string   `json:"description"`
	Version     int      `json:"version"`
	Category    int      `json:"category"`
	Lines       []string `json:"lines"`
	Asset       string   `json:"asset,omitempty"`
}
type offer struct {
	ID           string   `json:"id"`
	Name         string   `json:"name"`
	Developer    string   `json:"developer"`
	Description  string   `json:"description"`
	Version      int      `json:"version"`
	Size         int      `json:"size"`
	Category     int      `json:"category"`
	SHA256       string   `json:"sha256"`
	Dependencies []string `json:"dependencies"`
	AssetSize    int64    `json:"asset_size,omitempty"`
}
type catalog struct {
	Format string  `json:"format"`
	Target string  `json:"target"`
	ABI    int     `json:"abi"`
	Apps   []offer `json:"apps"`
}
type card struct {
	Format    string   `json:"format"`
	Target    string   `json:"target"`
	ABI       int      `json:"abi"`
	ID        string   `json:"id"`
	Name      string   `json:"name"`
	Version   int      `json:"version"`
	Lines     []string `json:"lines"`
	AssetSize int64    `json:"asset_size,omitempty"`
	AssetSHA  string   `json:"asset_sha256,omitempty"`
}

func printable(s string, limit int) bool {
	for _, ch := range s {
		if ch < 32 || ch > 126 {
			return false
		}
	}
	return len(s) <= limit
}
func validSource(app source) bool {
	if !appID.MatchString(app.ID) || app.Version < 1 || app.Version > 999999 ||
		app.Category < 1 || app.Category > 3 || app.Name == "" ||
		app.Developer == "" || app.Description == "" || !printable(app.Name, 34) ||
		!printable(app.Developer, 29) || !printable(app.Description, 96) ||
		len(app.Lines) < 1 || len(app.Lines) > 20 {
		return false
	}
	for _, line := range app.Lines {
		if !printable(line, 45) {
			return false
		}
	}
	return true
}
func signed(key *ecdsa.PrivateKey, payload []byte) ([]byte, error) {
	hash := sha256.Sum256(payload)
	signature, err := ecdsa.SignASN1(rand.Reader, key, hash[:])
	if err != nil {
		return nil, err
	}
	prefix := base64.StdEncoding.EncodeToString(signature) + "\n"
	return append([]byte(prefix), payload...), nil
}
func verify(key *ecdsa.PublicKey, envelope []byte) ([]byte, error) {
	signature, payload, ok := bytes.Cut(envelope, []byte{'\n'})
	if !ok || len(signature) > 100 || len(payload) == 0 {
		return nil, errors.New("invalid signed envelope")
	}
	sig, err := base64.StdEncoding.DecodeString(string(signature))
	hash := sha256.Sum256(payload)
	if err != nil || !ecdsa.VerifyASN1(key, hash[:], sig) {
		return nil, errors.New("invalid publisher signature")
	}
	return payload, nil
}
func publicKey(key *ecdsa.PublicKey) (string, error) {
	data, err := x509.MarshalPKIXPublicKey(key)
	return base64.StdEncoding.EncodeToString(data), err
}
func keygen(path string) error {
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return err
	}
	der, err := x509.MarshalECPrivateKey(key)
	if err != nil {
		return err
	}
	f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0600)
	if err != nil {
		return err
	}
	writeErr := pem.Encode(f, &pem.Block{Type: "EC PRIVATE KEY", Bytes: der})
	if err := errors.Join(writeErr, f.Close()); err != nil {
		os.Remove(path)
		return err
	}
	pub, err := publicKey(&key.PublicKey)
	if err == nil {
		fmt.Println(pub)
	}
	return err
}
func readPrivate(path string) (*ecdsa.PrivateKey, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	block, _ := pem.Decode(data)
	if block == nil || block.Type != "EC PRIVATE KEY" {
		return nil, errors.New("expected an EC private key PEM")
	}
	key, err := x509.ParseECPrivateKey(block.Bytes)
	if err != nil || key.Curve != elliptic.P256() {
		return nil, errors.New("expected an ECDSA P-256 private key")
	}
	return key, nil
}
func readBounded(path string, limit int) ([]byte, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	data, err := io.ReadAll(io.LimitReader(f, int64(limit+1)))
	if len(data) > limit {
		return nil, fmt.Errorf("%s exceeds %d bytes", filepath.Base(path), limit)
	}
	return data, err
}
func writeAtomic(path string, data []byte, exclusive bool) error {
	return writeAtomicContent(path, exclusive, func(file *os.File) error {
		_, err := file.Write(data)
		return err
	})
}
func writeAtomicContent(path string, exclusive bool, write func(*os.File) error) error {
	f, err := os.CreateTemp(filepath.Dir(path), ".publish-*")
	if err != nil {
		return err
	}
	defer os.Remove(f.Name())
	if err = write(f); err == nil {
		err = f.Chmod(0644)
	}
	if err == nil {
		err = f.Sync()
	}
	if err := errors.Join(err, f.Close()); err != nil {
		return err
	}
	if exclusive {
		return os.Link(f.Name(), path)
	}
	return os.Rename(f.Name(), path)
}
func packageName(id string, version int) string { return fmt.Sprintf("%s.%d.fsp", id, version) }

func validAssetName(name string) bool {
	return name != "" && name != "." && name != ".." && filepath.Base(name) == name &&
		!bytes.ContainsAny([]byte(name), "\\:") && printable(name, 120)
}

func verifyAsset(path string, size int64, digest string) error {
	file, err := os.Open(path)
	if err != nil {
		return err
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil || !info.Mode().IsRegular() || info.Size() != size {
		return fmt.Errorf("asset has an unexpected size: %s", filepath.Base(path))
	}
	hash := sha256.New()
	if _, err := io.CopyN(hash, file, size); err != nil {
		return err
	}
	if fmt.Sprintf("%x", hash.Sum(nil)) != digest {
		return fmt.Errorf("asset checksum mismatch: %s", filepath.Base(path))
	}
	return nil
}

func publishAsset(source, destination string) (int64, string, error) {
	file, err := os.Open(source)
	if err != nil {
		return 0, "", err
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil || !info.Mode().IsRegular() || info.Size() <= 0 || info.Size() > maxAsset {
		return 0, "", errors.New("asset must be a nonempty file that fits on FAT")
	}
	size := info.Size()
	hash := sha256.New()
	err = writeAtomicContent(destination, true, func(out *os.File) error {
		n, err := io.Copy(io.MultiWriter(out, hash), io.LimitReader(file, size+1))
		if err == nil && n != size {
			return errors.New("asset changed during publication")
		}
		return err
	})
	digest := fmt.Sprintf("%x", hash.Sum(nil))
	if errors.Is(err, os.ErrExist) {
		err = verifyAsset(destination, size, digest)
	}
	return size, digest, err
}

func publish(input, out string, key *ecdsa.PrivateKey) error {
	entries, err := os.ReadDir(input)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(out, 0755); err != nil {
		return err
	}
	list := catalog{"frestore-catalog-1", target, abi, []offer{}}
	seen := map[string]bool{}
	for _, entry := range entries {
		if entry.IsDir() || filepath.Ext(entry.Name()) != ".json" {
			continue
		}
		data, err := readBounded(filepath.Join(input, entry.Name()), maxPackage)
		if err != nil {
			return err
		}
		var app source
		if json.Unmarshal(data, &app) != nil || !validSource(app) || seen[app.ID] || len(list.Apps) == maxApps {
			return fmt.Errorf("invalid, duplicate or excess app %s", entry.Name())
		}
		seen[app.ID] = true
		pack := card{Format: "frestore-card-1", Target: target, ABI: abi,
			ID: app.ID, Name: app.Name, Version: app.Version, Lines: app.Lines}
		path := filepath.Join(out, packageName(app.ID, app.Version))
		if app.Asset != "" {
			if !validAssetName(app.Asset) {
				return fmt.Errorf("invalid asset filename for %s", app.ID)
			}
			pack.AssetSize, pack.AssetSHA, err = publishAsset(filepath.Join(input, app.Asset), path+".dat")
			if err != nil {
				return err
			}
		}
		payload, err := json.Marshal(pack)
		if err != nil {
			return err
		}
		envelope, err := signed(key, payload)
		if err != nil {
			return err
		}
		if len(envelope) > maxPackage {
			return fmt.Errorf("%s exceeds package size limit", app.ID)
		}
		if err := writeAtomic(path, envelope, true); err != nil {
			if !errors.Is(err, os.ErrExist) {
				return err
			}
			envelope, err = readBounded(path, maxPackage)
			if err != nil {
				return err
			}
			original, err := verify(&key.PublicKey, envelope)
			if err != nil || !bytes.Equal(original, payload) {
				return fmt.Errorf("%s version %d already published with different content or key", app.ID, app.Version)
			}
		}
		hash := sha256.Sum256(envelope)
		list.Apps = append(list.Apps, offer{app.ID, app.Name, app.Developer, app.Description,
			app.Version, len(envelope), app.Category, fmt.Sprintf("%x", hash), []string{}, pack.AssetSize})
	}
	data, err := json.Marshal(list)
	if err != nil {
		return err
	}
	envelope, err := signed(key, data)
	if err != nil {
		return err
	}
	if len(envelope) > maxCatalog {
		return errors.New("catalog exceeds byte limit")
	}
	pub, err := publicKey(&key.PublicKey)
	if err != nil {
		return err
	}
	if err := writeAtomic(filepath.Join(out, publicFile), []byte(pub), true); err != nil {
		if !errors.Is(err, os.ErrExist) {
			return err
		}
		existing, err := readBounded(filepath.Join(out, publicFile), 256)
		if err != nil || string(existing) != pub {
			return errors.New("publication directory belongs to a different signing key")
		}
	}
	return writeAtomic(filepath.Join(out, catalogFile), envelope, false)
}

func readPublisher(dir string) (*ecdsa.PublicKey, error) {
	encoded, err := readBounded(filepath.Join(dir, publicFile), 256)
	if err != nil {
		return nil, err
	}
	der, err := base64.StdEncoding.DecodeString(string(encoded))
	if err != nil {
		return nil, err
	}
	key, err := x509.ParsePKIXPublicKey(der)
	pub, ok := key.(*ecdsa.PublicKey)
	if err != nil || !ok || pub.Curve != elliptic.P256() {
		return nil, errors.New("invalid publisher public key")
	}
	return pub, nil
}

type assetRoute struct {
	path string
	size int64
}

type storeRoutes struct {
	documents map[string][]byte
	assets    map[string]assetRoute
}

func loadStore(dir string) (*storeRoutes, error) {
	pub, err := readPublisher(dir)
	if err != nil {
		return nil, err
	}
	envelope, err := readBounded(filepath.Join(dir, catalogFile), maxCatalog)
	if err != nil {
		return nil, err
	}
	payload, err := verify(pub, envelope)
	var list catalog
	if err != nil || json.Unmarshal(payload, &list) != nil || list.Format != "frestore-catalog-1" ||
		list.Target != target || list.ABI != abi || len(list.Apps) > maxApps {
		return nil, errors.New("invalid catalog")
	}
	publisher, err := publicKey(pub)
	if err != nil {
		return nil, err
	}
	routes := &storeRoutes{
		documents: map[string][]byte{"/v1/catalog": envelope, "/v1/publisher.pub": []byte(publisher)},
		assets:    make(map[string]assetRoute),
	}
	for _, item := range list.Apps {
		if !appID.MatchString(item.ID) || item.Version < 1 || item.Version > 999999 {
			return nil, errors.New("invalid package identity")
		}
		file := packageName(item.ID, item.Version)
		packagePath := filepath.Join(dir, file)
		data, err := readBounded(packagePath, maxPackage)
		if err != nil {
			return nil, err
		}
		contents, err := verify(pub, data)
		var app card
		if err != nil || len(data) != item.Size || fmt.Sprintf("%x", sha256.Sum256(data)) != item.SHA256 ||
			json.Unmarshal(contents, &app) != nil || app.Format != "frestore-card-1" ||
			app.Target != target || app.ABI != abi || app.ID != item.ID ||
			app.Name != item.Name || app.Version != item.Version || app.AssetSize != item.AssetSize ||
			app.AssetSize < 0 || app.AssetSize > maxAsset {
			return nil, fmt.Errorf("invalid package %s", file)
		}
		route := "/v1/packages/" + item.ID + "/" + strconv.Itoa(item.Version)
		routes.documents[route] = data
		if app.AssetSize > 0 {
			path := packagePath + ".dat"
			if err := verifyAsset(path, app.AssetSize, app.AssetSHA); err != nil {
				return nil, err
			}
			routes.assets[route+"/asset"] = assetRoute{path, app.AssetSize}
		}
	}
	loadOSRoutes(dir, pub, routes.documents)
	return routes, nil
}

type limit struct {
	mu     sync.Mutex
	last   time.Time
	tokens float64
}

func (l *limit) allow(now time.Time) bool {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.tokens = min(30, l.tokens+now.Sub(l.last).Seconds()*15)
	l.last = now
	if l.tokens < 1 {
		return false
	}
	l.tokens--
	return true
}

type assetWriter struct {
	http.ResponseWriter
	controller *http.ResponseController
}

func (writer assetWriter) Write(data []byte) (int, error) {
	err := writer.controller.SetWriteDeadline(time.Now().Add(20 * time.Second))
	if err != nil && !errors.Is(err, http.ErrNotSupported) {
		return 0, err
	}
	return writer.ResponseWriter.Write(data)
}

func storeHandler(routes *storeRoutes) http.Handler {
	budget := &limit{last: time.Now(), tokens: 30}
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if !budget.allow(time.Now()) {
			w.Header().Set("Retry-After", "1")
			http.Error(w, "try again shortly", http.StatusTooManyRequests)
			return
		}
		if r.Method != http.MethodGet {
			w.Header().Set("Allow", "GET")
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		path := r.URL.EscapedPath()
		body, found := routes.documents[path]
		asset, hasAsset := routes.assets[path]
		if !found && !hasAsset {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "application/octet-stream")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("Cache-Control", "no-cache")
		if hasAsset {
			file, err := os.Open(asset.path)
			if err != nil {
				http.NotFound(w, r)
				return
			}
			defer file.Close()
			info, err := file.Stat()
			if err != nil || !info.Mode().IsRegular() || info.Size() != asset.size {
				http.Error(w, "asset unavailable", http.StatusServiceUnavailable)
				return
			}
			w.Header().Set("Content-Length", strconv.FormatInt(asset.size, 10))
			n, err := io.CopyN(assetWriter{w, http.NewResponseController(w)}, file, asset.size)
			log.Printf("%s %d bytes=%d err=%v", path, http.StatusOK, n, err)
			return
		}
		w.Header().Set("Content-Length", strconv.Itoa(len(body)))
		n, err := w.Write(body)
		log.Printf("%s %d bytes=%d err=%v", r.URL.EscapedPath(), http.StatusOK, n, err)
	})
}

func run(args []string) error {
	usage := errors.New("usage: frestore keygen|publish|publish-os|mirror-os|serve [options]")
	if len(args) == 0 {
		return usage
	}
	flags := flag.NewFlagSet(args[0], flag.ContinueOnError)
	switch args[0] {
	case "keygen":
		private := flags.String("private", "", "new publisher key file")
		if err := flags.Parse(args[1:]); err != nil {
			return err
		}
		if *private == "" {
			return errors.New("-private is required")
		}
		return keygen(*private)
	case "publish":
		private := flags.String("private", "", "publisher key file")
		input := flags.String("input", "examples", "app JSON directory")
		output := flags.String("out", "published", "public files directory")
		if err := flags.Parse(args[1:]); err != nil {
			return err
		}
		key, err := readPrivate(*private)
		if err != nil {
			return err
		}
		return publish(*input, *output, key)
	case "publish-os":
		private := flags.String("private", "", "publisher key file")
		profile := flags.String("profile", "", "ili9488, ili9341 or vga")
		image := flags.String("image", "", "Freslest OS application image")
		dir := flags.String("out", "published", "public files directory")
		if err := flags.Parse(args[1:]); err != nil {
			return err
		}
		key, err := readPrivate(*private)
		if err != nil {
			return err
		}
		return publishOS(*dir, *profile, *image, key)
	case "mirror-os":
		source := flags.String("from", "", "trusted source HTTPS origin")
		profile := flags.String("profile", "", "ili9488, ili9341 or vga")
		dir := flags.String("out", "published", "public files directory with publisher.pub")
		if err := flags.Parse(args[1:]); err != nil {
			return err
		}
		client := &http.Client{
			Timeout:       80 * time.Second,
			CheckRedirect: func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse },
		}
		return mirrorOS(client, *source, *profile, *dir)
	case "serve":
		dir := flags.String("dir", "published", "public files directory")
		cert := flags.String("cert", "", "TLS certificate chain PEM")
		key := flags.String("tls-key", "", "TLS key PEM")
		addr := flags.String("addr", ":8443", "listen address")
		if err := flags.Parse(args[1:]); err != nil {
			return err
		}
		if *cert == "" || *key == "" {
			return errors.New("-cert and -tls-key are required")
		}
		routes, err := loadStore(*dir)
		if err != nil {
			return err
		}
		server := &http.Server{Addr: *addr, Handler: storeHandler(routes),
			ReadHeaderTimeout: 4 * time.Second, ReadTimeout: 8 * time.Second,
			WriteTimeout: 90 * time.Second, IdleTimeout: 15 * time.Second, MaxHeaderBytes: 4096,
			TLSConfig: &tls.Config{MinVersion: tls.VersionTLS12}}
		log.Printf("FreStore HTTPS on %s", *addr)
		return server.ListenAndServeTLS(*cert, *key)
	default:
		return usage
	}
}
func main() {
	if err := run(os.Args[1:]); err != nil {
		log.Fatal(err)
	}
}
