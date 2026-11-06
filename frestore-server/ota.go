package main

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/sha256"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
)

const (
	maxOSBytes    = 0x1b0000
	maxOSManifest = 768
	osFormat      = "frestore-os-update-1"
	osLayout      = "freslest-s3-ab-v2"
)

var (
	osProfiles = [...]string{"ili9488", "ili9341", "vga"}
	osVersion  = regexp.MustCompile(`^(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})(\.(0|[1-9][0-9]{0,3}))?$`)
	osDigest   = regexp.MustCompile(`^[0-9a-f]{64}$`)
)

type osManifest struct {
	Format  string `json:"format"`
	Target  string `json:"target"`
	ABI     int    `json:"abi"`
	Layout  string `json:"layout"`
	Profile string `json:"profile"`
	Version string `json:"version"`
	Size    int    `json:"size"`
	SHA256  string `json:"sha256"`
}

func validOSProfile(profile string) bool {
	for _, supported := range osProfiles {
		if supported == profile {
			return true
		}
	}
	return false
}

func osBase(profile string) string         { return "/v1/os/" + profile + "/" }
func osManifestName(profile string) string { return "os-" + profile + ".fsm" }
func osImageName(m osManifest) string      { return "os-" + m.Profile + "-" + m.SHA256 + ".bin" }

func describeOS(image []byte, profile string) (osManifest, error) {
	bad := osManifest{}
	if !validOSProfile(profile) || len(image) < 288 || len(image) > maxOSBytes ||
		image[0] != 0xe9 || image[1] < 1 || image[1] > 16 || image[3]>>4 != 4 ||
		binary.LittleEndian.Uint16(image[12:]) != 9 || image[23] != 1 ||
		binary.LittleEndian.Uint32(image[32:]) != 0xabcd5432 {
		return bad, errors.New("expected an ESP32-S3 OS image for the 16 MB layout")
	}
	version, _, hasVersionEnd := bytes.Cut(image[48:80], []byte{0})
	project, _, hasProjectEnd := bytes.Cut(image[80:112], []byte{0})
	if !hasVersionEnd || !hasProjectEnd || string(project) != "freslest_os_s3" || !osVersion.Match(version) {
		return bad, errors.New("wrong OS project or version descriptor")
	}
	lcd := "ILI9341"
	if profile == "ili9488" {
		lcd = "ILI9488"
	}
	other := "ILI9488"
	if lcd == other {
		other = "ILI9341"
	}
	if !bytes.Contains(image, []byte("FRESLEST_LCD:"+lcd+"\x00")) ||
		bytes.Contains(image, []byte("FRESLEST_LCD:"+other+"\x00")) ||
		bytes.Contains(image, []byte("FRESLEST_VIDEO:VGA25175_25175000\x00")) != (profile == "vga") {
		return bad, errors.New("firmware display profile does not match")
	}
	offset, checksum := 24, byte(0xef)
	for i := 0; i < int(image[1]); i++ {
		if offset+8 > len(image) {
			return bad, errors.New("truncated ESP segment header")
		}
		length := uint64(binary.LittleEndian.Uint32(image[offset+4:]))
		offset += 8
		if length > uint64(len(image)-offset) || (i == 0 && length < 256) {
			return bad, errors.New("invalid ESP segment size")
		}
		for _, b := range image[offset : offset+int(length)] {
			checksum ^= b
		}
		offset += int(length)
	}
	aligned := (offset + 16) &^ 15
	if aligned+32 != len(image) || image[aligned-1] != checksum {
		return bad, errors.New("invalid ESP image checksum")
	}
	digest := sha256.Sum256(image[:aligned])
	if !bytes.Equal(digest[:], image[aligned:]) {
		return bad, errors.New("invalid ESP image digest")
	}
	return osManifest{
		Format: osFormat, Target: target, ABI: abi, Layout: osLayout,
		Profile: profile, Version: string(version), Size: len(image),
		SHA256: fmt.Sprintf("%x", sha256.Sum256(image)),
	}, nil
}

func verifyOSManifest(pub *ecdsa.PublicKey, envelope []byte, profile string) (osManifest, error) {
	var manifest osManifest
	body, err := verify(pub, envelope)
	if err != nil || len(envelope) > maxOSManifest || json.Unmarshal(body, &manifest) != nil ||
		manifest.Format != osFormat || manifest.Target != target || manifest.ABI != abi ||
		manifest.Layout != osLayout || !validOSProfile(profile) || manifest.Profile != profile ||
		!osVersion.MatchString(manifest.Version) || !osDigest.MatchString(manifest.SHA256) ||
		manifest.Size < 288 || manifest.Size > maxOSBytes {
		return osManifest{}, errors.New("invalid signed OS update manifest")
	}
	return manifest, nil
}

func saveOSUpdate(dir string, manifest osManifest, envelope, image []byte) error {
	checked, err := describeOS(image, manifest.Profile)
	if err != nil || checked != manifest {
		return errors.New("OS image does not match the signed manifest")
	}
	path := filepath.Join(dir, osImageName(manifest))
	if err := writeAtomic(path, image, true); err != nil {
		if !errors.Is(err, os.ErrExist) {
			return err
		}
		previous, readErr := readBounded(path, maxOSBytes)
		if readErr != nil || !bytes.Equal(previous, image) {
			return errors.New("existing OS update file differs")
		}
	}
	return writeAtomic(filepath.Join(dir, osManifestName(manifest.Profile)), envelope, false)
}

func publishOS(dir, profile, file string, key *ecdsa.PrivateKey) error {
	publishedKey, err := readBounded(filepath.Join(dir, publicFile), 256)
	pub, keyErr := publicKey(&key.PublicKey)
	if err != nil || keyErr != nil || string(publishedKey) != pub {
		return errors.New("publish the FreStore catalog with this key first")
	}
	image, err := readBounded(file, maxOSBytes)
	if err != nil {
		return err
	}
	manifest, err := describeOS(image, profile)
	if err != nil {
		return err
	}
	body, err := json.Marshal(manifest)
	if err != nil {
		return err
	}
	envelope, err := signed(key, body)
	if err != nil {
		return err
	}
	return saveOSUpdate(dir, manifest, envelope, image)
}

func loadOSRoutes(dir string, pub *ecdsa.PublicKey, routes map[string][]byte) {
	for _, profile := range osProfiles {
		envelope, err := readBounded(filepath.Join(dir, osManifestName(profile)), maxOSManifest)
		if errors.Is(err, os.ErrNotExist) {
			continue
		}
		if err != nil {
			log.Printf("OS update %s: %v", profile, err)
			continue
		}
		manifest, err := verifyOSManifest(pub, envelope, profile)
		if err != nil {
			log.Printf("OS update %s: %v", profile, err)
			continue
		}
		image, err := readBounded(filepath.Join(dir, osImageName(manifest)), maxOSBytes)
		checked, imageErr := describeOS(image, profile)
		if err != nil || imageErr != nil || manifest != checked {
			log.Printf("OS update %s: image missing or different from signed manifest", profile)
			continue
		}
		routes[osBase(profile)+"manifest"] = envelope
		routes[osBase(profile)+manifest.SHA256] = image
	}
}

func fetchOS(client *http.Client, url string, limit int) ([]byte, error) {
	response, err := client.Get(url)
	if err != nil {
		return nil, err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK || response.TLS == nil ||
		response.ContentLength <= 0 || response.ContentLength > int64(limit) {
		return nil, fmt.Errorf("invalid OS mirror response (%s)", response.Status)
	}
	data, err := io.ReadAll(io.LimitReader(response.Body, int64(limit)+1))
	if err != nil || len(data) != int(response.ContentLength) {
		return nil, errors.New("incomplete OS mirror response")
	}
	return data, nil
}

func mirrorOS(client *http.Client, source, profile, dir string) error {
	origin, err := url.Parse(source)
	if err != nil || origin.Scheme != "https" || origin.Hostname() == "" ||
		origin.User != nil || origin.Path != "" || origin.RawQuery != "" || origin.ForceQuery ||
		origin.Fragment != "" || !validOSProfile(profile) {
		return errors.New("set a HTTPS origin and a valid OS profile")
	}
	pub, err := readPublisher(dir)
	if err != nil {
		return err
	}
	base := source + osBase(profile)
	envelope, err := fetchOS(client, base+"manifest", maxOSManifest)
	if err != nil {
		return err
	}
	manifest, err := verifyOSManifest(pub, envelope, profile)
	if err != nil {
		return err
	}
	image, err := fetchOS(client, base+manifest.SHA256, manifest.Size)
	if err != nil {
		return err
	}
	return saveOSUpdate(dir, manifest, envelope, image)
}
