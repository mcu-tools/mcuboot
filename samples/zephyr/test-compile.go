// +build ignore
//
// Build all of the tests.
//
// Run as:
//
//     go run test-compile.go -out name.tar

package main

import (
	"archive/zip"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path"

	"github.com/mcu-tools/mcuboot/samples/zephyr/mcutests"
)

var outFile = flag.String("out", "test-images.zip", "Name of zip file to put built tests into")

func main() {
	err := run()
	if err != nil {
		log.Fatal(err)
	}
}

func run() error {
	flag.Parse()

	zipper, err := NewBuilds()
	if err != nil {
		return err
	}
	defer zipper.Close()

	for _, group := range mcutests.Tests {
		fmt.Printf("Compiling %q\n", group.ShortName)
		c := exec.Command("make",
			fmt.Sprintf("test-%s", group.ShortName))
		// TODO: Should capture the output and show it if
		// there is an error.
		err = c.Run()
		if err != nil {
			return err
		}

		err = zipper.Capture(group.ShortName)
		if err != nil {
			return err
		}
	}

	return nil
}

// A Builds create a zipfile of the contents of various builds.  The
// names will be constructed.
type Builds struct {
	// The file being written to.
	file *os.File

	// The zip writer writing the data.
	zip *zip.Writer
}

func NewBuilds() (*Builds, error) {
	name := *outFile
	file, err := os.OpenFile(name, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0644)
	if err != nil {
		return nil, err
	}

	z := zip.NewWriter(file)

	return &Builds{
		file: file,
		zip:  z,
	}, nil
}

func (b *Builds) Close() error {
	return b.zip.Close()
}

func (b *Builds) Capture(testName string) error {
	// Collect stat information from the test directory, which
	// should be close enough to make the zip file meaningful.
	info, err := os.Stat(".")
	if err != nil {
		return err
	}

	header, err := zip.FileInfoHeader(info)
	if err != nil {
		return err
	}

	header.Name = testName + "/"

	_, err = b.zip.CreateHeader(header)
	if err != nil {
		return err
	}

	for _, name := range []string{
		"mcuboot.bin",
		"signed-hello1.bin",
		"signed-hello2.bin",
	} {
		err = b.add(testName, name, name)
		if err != nil {
			return err
		}
	}

	return nil
}

func (b *Builds) add(baseName, zipName, fileName string) error {
	inp, err := os.Open(fileName)
	if err != nil {
		return err
	}
	defer inp.Close()

	info, err := inp.Stat()
	if err != nil {
		return err
	}

	header, err := zip.FileInfoHeader(info)
	if err != nil {
		return err
	}

	header.Name = path.Join(baseName, zipName)
	header.Method = zip.Deflate

	wr, err := b.zip.CreateHeader(header)
	if err != nil {
		return err
	}

	_, err = io.Copy(wr, inp)
	if err != nil {
		return err
	}

	return nil
}
