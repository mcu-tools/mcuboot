// Package mcutests
package mcutests // github.com/JuulLabs-OSS/mcuboot/samples/zephyr/mcutests

// The main driver of this consists of a series of tests.  Each test
// then contains a series of commands and expect results.
var Tests = []struct {
	Name  string
	Tests []OneTest
}{
	{
		Name: "Good RSA",
		Tests: []OneTest{
			{
				Commands: [][]string{
					{"make", "test-good-rsa"},
					{"make", "flash_boot"},
				},
				Expect: "Unable to find bootable image",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello1"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello2"},
				},
				Expect: "Hello World from hello2",
			},
			{
				Commands: [][]string{
					{"pyocd", "commander", "-c", "reset"},
				},
				Expect: "Hello World from hello1",
			},
		},
	},
	{
		Name: "Good ECDSA",
		Tests: []OneTest{
			{
				Commands: [][]string{
					{"make", "test-good-ecdsa"},
					{"make", "flash_boot"},
				},
				Expect: "Unable to find bootable image",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello1"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello2"},
				},
				Expect: "Hello World from hello2",
			},
			{
				Commands: [][]string{
					{"pyocd", "commander", "-c", "reset"},
				},
				Expect: "Hello World from hello1",
			},
		},
	},
	{
		Name: "Overwrite",
		Tests: []OneTest{
			{
				Commands: [][]string{
					{"make", "test-overwrite"},
					{"make", "flash_boot"},
				},
				Expect: "Unable to find bootable image",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello1"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello2"},
				},
				Expect: "Hello World from hello2",
			},
			{
				Commands: [][]string{
					{"pyocd", "commander", "-c", "reset"},
				},
				Expect: "Hello World from hello2",
			},
		},
	},
	{
		Name: "Bad RSA",
		Tests: []OneTest{
			{
				Commands: [][]string{
					{"make", "test-bad-rsa-upgrade"},
					{"make", "flash_boot"},
				},
				Expect: "Unable to find bootable image",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello1"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello2"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"pyocd", "commander", "-c", "reset"},
				},
				Expect: "Hello World from hello1",
			},
		},
	},
	{
		Name: "Bad RSA",
		Tests: []OneTest{
			{
				Commands: [][]string{
					{"make", "test-bad-ecdsa-upgrade"},
					{"make", "flash_boot"},
				},
				Expect: "Unable to find bootable image",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello1"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello2"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"pyocd", "commander", "-c", "reset"},
				},
				Expect: "Hello World from hello1",
			},
		},
	},
	{
		Name: "No bootcheck",
		Tests: []OneTest{
			{
				Commands: [][]string{
					{"make", "test-no-bootcheck"},
					{"make", "flash_boot"},
				},
				Expect: "Unable to find bootable image",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello1"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello2"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"pyocd", "commander", "-c", "reset"},
				},
				Expect: "Hello World from hello1",
			},
		},
	},
	{
		Name: "Wrong RSA",
		Tests: []OneTest{
			{
				Commands: [][]string{
					{"make", "test-wrong-rsa"},
					{"make", "flash_boot"},
				},
				Expect: "Unable to find bootable image",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello1"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello2"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"pyocd", "commander", "-c", "reset"},
				},
				Expect: "Hello World from hello1",
			},
		},
	},
	{
		Name: "Wrong ECDSA",
		Tests: []OneTest{
			{
				Commands: [][]string{
					{"make", "test-wrong-ecdsa"},
					{"make", "flash_boot"},
				},
				Expect: "Unable to find bootable image",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello1"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"make", "flash_hello2"},
				},
				Expect: "Hello World from hello1",
			},
			{
				Commands: [][]string{
					{"pyocd", "commander", "-c", "reset"},
				},
				Expect: "Hello World from hello1",
			},
		},
	},
}

type OneTest struct {
	Commands [][]string
	Expect   string
}
