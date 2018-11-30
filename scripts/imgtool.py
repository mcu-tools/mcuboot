#! /usr/bin/env python3
#
# Copyright 2017 Linaro Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import click
import getpass
from imgtool import keys
from imgtool import image
from imgtool.version import decode_version


def gen_rsa2048(keyfile, passwd):
    keys.RSA2048.generate().export_private(path=keyfile, passwd=passwd)


def gen_ecdsa_p256(keyfile, passwd):
    keys.ECDSA256P1.generate().export_private(keyfile, passwd=passwd)


def gen_ecdsa_p224(keyfile, passwd):
    print("TODO: p-224 not yet implemented")


valid_langs = ['c', 'rust']
keygens = {
    'rsa-2048':   gen_rsa2048,
    'ecdsa-p256': gen_ecdsa_p256,
    'ecdsa-p224': gen_ecdsa_p224,
}


def load_key(keyfile):
    # TODO: better handling of invalid pass-phrase
    key = keys.load(keyfile)
    if key is not None:
        return key
    passwd = getpass.getpass("Enter key passphrase: ").encode('utf-8')
    return keys.load(keyfile, passwd)


def get_password():
    while True:
        passwd = getpass.getpass("Enter key passphrase: ")
        passwd2 = getpass.getpass("Reenter passphrase: ")
        if passwd == passwd2:
            break
        print("Passwords do not match, try again")

    # Password must be bytes, always use UTF-8 for consistent
    # encoding.
    return passwd.encode('utf-8')


@click.option('-p', '--password', is_flag=True,
              help='Prompt for password to protect key')
@click.option('-t', '--type', metavar='type', required=True,
              type=click.Choice(keygens.keys()))
@click.option('-k', '--key', metavar='filename', required=True)
@click.command(help='Generate pub/private keypair')
def keygen(type, key, password):
    password = get_password() if password else None
    keygens[type](key, password)


@click.option('-l', '--lang', metavar='lang', default=valid_langs[0],
              type=click.Choice(valid_langs))
@click.option('-k', '--key', metavar='filename', required=True)
@click.command(help='Get public key from keypair')
def getpub(key, lang):
    key = load_key(key)
    if key is None:
        print("Invalid passphrase")
    elif lang == 'c':
        key.emit_c()
    elif lang == 'rust':
        key.emit_rust()
    else:
        raise ValueError("BUG: should never get here!")


def validate_version(ctx, param, value):
    try:
        decode_version(value)
        return value
    except ValueError as e:
        raise click.BadParameter("{}".format(e))


def validate_header_size(ctx, param, value):
    min_hdr_size = image.IMAGE_HEADER_SIZE
    if value < min_hdr_size:
        raise click.BadParameter(
            "Minimum value for -H/--header-size is {}".format(min_hdr_size))
    return value


class BasedIntParamType(click.ParamType):
    name = 'integer'

    def convert(self, value, param, ctx):
        try:
            if value[:2].lower() == '0x':
                return int(value[2:], 16)
            elif value[:1] == '0':
                return int(value, 8)
            return int(value, 10)
        except ValueError:
            self.fail('%s is not a valid integer' % value, param, ctx)


@click.argument('outfile')
@click.argument('infile')
@click.option('-E', '--encrypt', metavar='filename',
              help='Encrypt image using the provided public key')
@click.option('-e', '--endian', type=click.Choice(['little', 'big']),
              default='little', help="Select little or big endian")
@click.option('--overwrite-only', default=False, is_flag=True,
              help='Use overwrite-only instead of swap upgrades')
@click.option('-M', '--max-sectors', type=int,
              help='When padding allow for this amount of sectors (defaults to 128)')
@click.option('--pad', default=False, is_flag=True,
              help='Pad image to --slot-size bytes, adding trailer magic')
@click.option('-S', '--slot-size', type=BasedIntParamType(), required=True,
              help='Size of the slot where the image will be written')
@click.option('--pad-header', default=False, is_flag=True,
              help='Add --header-size zeroed bytes at the beginning of the image')
@click.option('-H', '--header-size', callback=validate_header_size,
              type=BasedIntParamType(), required=True)
@click.option('-v', '--version', callback=validate_version,  required=True)
@click.option('--align', type=click.Choice(['1', '2', '4', '8']),
              required=True)
@click.option('-k', '--key', metavar='filename')
@click.command(help='Create a signed or unsigned image')
def sign(key, align, version, header_size, pad_header, slot_size, pad,
         max_sectors, overwrite_only, endian, encrypt, infile, outfile):
    img = image.Image.load(infile, version=decode_version(version),
                           header_size=header_size, pad_header=pad_header,
                           pad=pad, align=int(align), slot_size=slot_size,
                           max_sectors=max_sectors,
                           overwrite_only=overwrite_only,
                           endian=endian)
    key = load_key(key) if key else None
    enckey = load_key(encrypt) if encrypt else None
    if enckey:
        if not isinstance(enckey, (keys.RSA2048, keys.RSA2048Public)):
            raise Exception("Encryption only available with RSA")
        if key and not isinstance(key, (keys.RSA2048, keys.RSA2048Public)):
            raise Exception("Encryption with sign only available with RSA")
    if key or enckey:
        img.create(key, enckey)

    if pad:
        img.pad_to(slot_size)

    img.save(outfile)


class AliasesGroup(click.Group):

    _aliases = {
        "create": "sign",
    }

    def list_commands(self, ctx):
        cmds = [k for k in self.commands]
        aliases = [k for k in self._aliases]
        return sorted(cmds + aliases)

    def get_command(self, ctx, cmd_name):
        rv = click.Group.get_command(self, ctx, cmd_name)
        if rv is not None:
            return rv
        if cmd_name in self._aliases:
            return click.Group.get_command(self, ctx, self._aliases[cmd_name])
        return None


@click.command(cls=AliasesGroup,
               context_settings=dict(help_option_names=['-h', '--help']))
def imgtool():
    pass


imgtool.add_command(keygen)
imgtool.add_command(getpub)
imgtool.add_command(sign)


if __name__ == '__main__':
    imgtool()
