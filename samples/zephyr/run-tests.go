// +build ignore
//
// Build multiple configurations of MCUboot for Zephyr, making sure
// that they run properly.
//
// Run as:
//
//    go run run-tests.go [flags]
//
// Add -help as a flag to get help.  See comment below for logIn on
// how to configure terminal output to a file so this program can see
// the output of the Zephyr device.

package main

import (
	"archive/zip"
	"bufio"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"strings"
	"time"

	"github.com/mcu-tools/mcuboot/samples/zephyr/mcutests"
)

// logIn gives the pathname of the log output from the Zephyr device.
// In order to see the serial output, but still be useful for human
// debugging, the output of the terminal emulator should be teed to a
// file that this program will read from.  This can be done with
// something like:
//
//     picocom -b 115200 /dev/ttyACM0 | tee /tmp/zephyr.out
//
// Other terminal programs should also have logging options.
var logIn = flag.String("login", "/tmp/zephyr.out", "File name of terminal log from Zephyr device")

// Output from this test run is written to the given log file.
var logOut = flag.String("logout", "tests.log", "Log file to write to")

var preBuilt = flag.String("prebuilt", "", "Name of file with prebuilt tests")

func main() {
	err := run()
	if err != nil {
		log.Fatal(err)
	}
}

func run() error {
	flag.Parse()

	lines := make(chan string, 30)
	go readLog(lines)

	// Write output to a log file
	logFile, err := os.Create(*logOut)
	if err != nil {
		return err
	}
	defer logFile.Close()
	lg := bufio.NewWriter(logFile)
	defer lg.Flush()

	var extractor *Extractor

	if *preBuilt != "" {
		// If there are pre-built images, open them.
		extractor, err = NewExtractor(*preBuilt)
		if err != nil {
			return err
		}
		defer extractor.Close()
	}

	for _, group := range mcutests.Tests {
		fmt.Printf("Running %q\n", group.Name)
		fmt.Fprintf(lg, "-------------------------------------\n")
		fmt.Fprintf(lg, "---- Running %q\n", group.Name)

		for _, test := range group.Tests {
			if *preBuilt == "" {
				// No prebuilt, build the tests
				// ourselves.
				err = runCommands(test.Build, lg)
				if err != nil {
					return err
				}
			} else {
				// Extract the build artifacts from
				// the zip file.
				err = extractor.Extract(group.ShortName)
				if err != nil {
					return err
				}
			}

			err = runCommands(test.Commands, lg)
			if err != nil {
				return err
			}

			err = expect(lg, lines, test.Expect)
			if err != nil {
				return err
			}

			fmt.Fprintf(lg, "---- Passed\n")
		}
		fmt.Printf("    Passed!\n")
	}

	return nil
}

// Run a set of commands
func runCommands(cmds [][]string, lg io.Writer) error {
	for _, cmd := range cmds {
		fmt.Printf("    %s\n", cmd)
		fmt.Fprintf(lg, "---- Run: %s\n", cmd)
		err := runCommand(cmd, lg)
		if err != nil {
			return err
		}
	}

	return nil
}

// Run a single command.
func runCommand(cmd []string, lg io.Writer) error {
	c := exec.Command(cmd[0], cmd[1:]...)
	c.Stdout = lg
	c.Stderr = lg
	return c.Run()
}

// Expect the given string.
func expect(lg io.Writer, lines <-chan string, exp string) error {
	// Read lines, and if we hit a timeout before seeing our
	// expected line, then consider that an error.
	fmt.Fprintf(lg, "---- expect: %q\n", exp)

	stopper := time.NewTimer(10 * time.Second)
	defer stopper.Stop()
outer:
	for {
		select {
		case line := <-lines:
			fmt.Fprintf(lg, "---- target: %q\n", line)
			if strings.Contains(line, exp) {
				break outer
			}
		case <-stopper.C:
			fmt.Fprintf(lg, "timeout, didn't receive output\n")
			return fmt.Errorf("timeout, didn't receive expected string: %q", exp)
		}
	}

	return nil
}

// Read things from the log file, discarding everything already there.
func readLog(sink chan<- string) {
	file, err := os.Open(*logIn)
	if err != nil {
		log.Fatal(err)
	}

	_, err = file.Seek(0, 2)
	if err != nil {
		log.Fatal(err)
	}

	prefix := ""
	for {
		// Read lines until EOF, then delay a bit, and do it
		// all again.
		rd := bufio.NewReader(file)

		for {
			line, err := rd.ReadString('\n')
			if err == io.EOF {
				// A partial line can happen because
				// we are racing with the writer.
				if line != "" {
					prefix = line
				}
				break
			}
			if err != nil {
				log.Fatal(err)
			}

			line = prefix + line
			prefix = ""
			sink <- line
			// fmt.Printf("line: %q\n", line)
		}

		// Pause a little
		time.Sleep(250 * time.Millisecond)
	}
}

// An Extractor holds an opened Zip file, and is able to extract files
// based on the directory name.
type Extractor struct {
	file *os.File
	zip  *zip.Reader
}

// NewExtractor returns an Extractor based on the contents of a zip
// file.
func NewExtractor(name string) (*Extractor, error) {
	f, err := os.Open(name)
	if err != nil {
		return nil, err
	}
	size, err := f.Seek(0, 2)
	if err != nil {
		f.Close()
		return nil, err
	}

	rd, err := zip.NewReader(f, size)
	if err != nil {
		f.Close()
		return nil, err
	}

	return &Extractor{
		file: f,
		zip:  rd,
	}, nil
}

func (e *Extractor) Close() error {
	return e.file.Close()
}

// Extract extracts the files of the given directory name into the
// current directory.  These files will overwrite any files of these
// names that already exist (presumably from previous extractions).
func (e *Extractor) Extract(dir string) error {
	prefix := dir + "/"

	count := 0
	for _, file := range e.zip.File {
		if len(file.Name) > len(prefix) && strings.HasPrefix(file.Name, prefix) {
			outName := file.Name[len(prefix):len(file.Name)]
			fmt.Printf("->%q\n", outName)

			err := e.single(file, outName)
			if err != nil {
				return err
			}

			count += 1
		}
	}

	if count == 0 {
		return fmt.Errorf("File for %s missing from archive", dir)
	}

	return nil
}

// single extracts a single file from the zip archive, writing the
// results to a file 'outName'.
func (e *Extractor) single(file *zip.File, outName string) error {
	inf, err := file.Open()
	if err != nil {
		return err
	}

	outf, err := os.Create(outName)
	if err != nil {
		return err
	}
	defer outf.Close()

	_, err = io.Copy(outf, inf)
	return err
}
