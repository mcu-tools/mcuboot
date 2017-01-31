package main

import (
	"errors"
	"os"
	"strconv"

	log "github.com/Sirupsen/logrus"
	"github.com/spf13/cobra"

	"mynewt.apache.org/newt/newt/image"
)

var version string
var headerSize uint
var signAlign alignment = 1
var padTo int64

func setupSign() *cobra.Command {
	sign := &cobra.Command{
		Use:   "sign infile outfile",
		Short: "Sign image with key",
		Run:   doSign,
	}

	fl := sign.Flags()
	fl.StringVarP(&version, "version", "v", "1.0", "Version to stamp image with")
	fl.UintVarP(&headerSize, "header-size", "H", 0, "Header size to use")
	fl.Var(&signAlign, "align", "Alignment of flash device")
	fl.Int64Var(&padTo, "pad", 0, "Pad image to this many bytes, adding trailer magic")

	return sign
}

type alignment uint

func (a alignment) String() string {
	return strconv.FormatUint(uint64(a), 10)
}

func (a alignment) Type() string {
	return "alignment"
}

func (a *alignment) Set(text string) error {
	value, err := strconv.ParseUint(text, 10, 8)
	if err != nil {
		return err
	}

	switch value {
	case 1, 2, 4, 8:
		*a = alignment(value)
		return nil
	default:
		return errors.New("Invalid alignment, must be one of 1,2,4,8")
	}
}

func doSign(cmd *cobra.Command, args []string) {
	if len(args) != 2 {
		cmd.Usage()
		log.Fatal("Expecting infile and outfile arguments")
	}

	img, err := image.NewImage(args[0], args[1])
	if err != nil {
		log.Fatal(err)
	}

	err = img.SetVersion(version)
	if err != nil {
		log.Fatal(err)
	}

	err = img.SetSigningKey(keyFile, 0)
	if err != nil {
		log.Fatal(err)
	}

	// For Zephyr, we want to both skip and use the header size.
	img.SrcSkip = headerSize
	img.HeaderSize = headerSize

	err = img.Generate(nil)
	if err != nil {
		log.Fatal(err)
	}

	if padTo > 0 {
		err = padImage(args[1])
		if err != nil {
			log.Fatal(err)
		}
	}
}

// Pad the image to a given position, and write the image trailer.
func padImage(name string) error {
	f, err := os.OpenFile(name, os.O_RDWR, 0)
	if err != nil {
		return err
	}
	defer f.Close()

	info, err := f.Stat()
	if err != nil {
		return err
	}

	var trailerSize int64
	switch signAlign {
	case 1:
		trailerSize = 402
	case 2:
		trailerSize = 788
	case 4:
		trailerSize = 1560
	case 8:
		trailerSize = 3104
	default:
		panic("Invalid alignment")
	}

	if info.Size() > padTo-trailerSize {
		return errors.New("Image is too large for specified padding")
	}

	_, err = f.WriteAt(bootMagic, padTo-trailerSize)
	if err != nil {
		return err
	}

	err = f.Truncate(padTo)
	if err != nil {
		return err
	}

	return nil
}

var bootMagic = []byte{
	0x77, 0xc2, 0x95, 0xf3,
	0x60, 0xd2, 0xef, 0x7f,
	0x35, 0x52, 0x50, 0x0f,
	0x2c, 0xb6, 0x79, 0x80,
}
