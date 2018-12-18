# IETF SUIT

## Overview

The [IETF SUIT WG](https://datatracker.ietf.org/wg/suit/about/)
(Software Updates for Internet of Things Working Group) seeks to
define a standardized manifest format to be used when updating images
for IoT devices.  This document describes how SUIT support is being
added to MCUboot.

## The existing manifest

The metadata in MCUboot that is comparable to the manifest in SUIT
consists of the image header and the TLV data following the image
itself.  This particular format for MCUboot was chosen to allow the
image to be executed directly in place without having to relocate the
code or modify the image metadata.

## The SUIT manifest

As of draft 3 of this manifest, the SUIT manifest consists of a core
manifest (described as the "Manifest"), and zero or more severable
pieces of associated data.  All of these are collected into an
OuterWrapper that also includes authentication data (the signature).

The core manifest can either directly contain the severable
information, or a COSE Digest of that information.  The authentication
information is over this core manifest, and due to the contained
digests, the severable information is also covered by that signature.

## New header

For the new images, we will use the same header with the same
semantics.  However, the 'version' field is deprecated and the SUIT
manifest should be used for the definitive version.  This header is
included in the data that the SUIT manifest digests over
(`ih_hdr_size` + `ih_img_size` bytes of data).

The SUIT manifest immediately follows.  It doesn't matter if there is
severable data in the manifest, although MCUboot will not use it.

The allowed data in this manifest is as follows:

```CDDL
outer-wrapper = {
    authentication-wrapper => $auth-wrapper-types,
    manifest => suit-manifest,
}

authentication-wrapper = 1
manifest = 2

$auth-wrapper-types /= COSE_Sign1_Tagged

label = int / tstr
values = any

COSE_Sign1_Tagged = any ; TBD

COSE_Mac0 = [
      Headers,
      payload : bytes / nil,
      tag : bstr,
   ]

Headers = (
    protected : empty_or_serialized_map,
    unprotected : header_map
)

header_map = { Generic_Headers,
               * label => values
             }

empty_or_serialized_map = bstr .cbor header_map / bstr .size 0

Generic_Headers = (
   ? 1 => int / text,  ; algorithm identifier
)

COSE_Digest = COSE_Mac0

suit-manifest = {
    manifest-version => 1,
    sequence-number => uint,
    pre-install => pre-installation-info,
    install => installation-info,
    payloads => [ payload-info ],
    intent => COSE_Digest,
}

manifest-version = 1
sequence-number = 2
pre-install = 3
install = 5
payloads = 6
intent = 8

component-identifier = []

payload-info = {
    payload-component => component-identifier,
    payload-size => uint,
    payload-digest => COSE_Digest,
}
payload-component = 1
payload-size      = 2
payload-digest    = 3

pre-installation-info = {
    ? pre-conditions => [ + pre-condition ],
}

pre-conditions = 1

pre-condition = ( id-condition )

id-condition  = [
                  id-condition-vendor /
                  id-condition-class,
                  id : uuid,
                ]

id-condition-vendor = 1
id-condition-class = 2

uuid = bstr .size 16

installation-info = {
    payload-installation-infos => [ payload-installation-info ]
}
payload-installation-infos = 1
payload-installation-info = {
    install-component => component-identifier,
    payload-processors => [ processor ],
}
install-component = 1
payload-processors = 2

processor = {
    processor-id => [ 1, 1 ],
    processor-inputs => uri-list,
}

uri-list = [ [ 0,
                 link: uri,
               ]
           ]

processor-id = 1
processor-inputs = 3


firmware-intent = {
    * lang => tstr
}

lang = uint
```
