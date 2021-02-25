#! /bin/bash

curve=secp256r1

# Generate key for A, the root
openssl ecparam -name $curve -genkey -out A.key
# openssl genrsa -out A.key 4096

# Generate a cert for it
openssl req -new -x509 -days 3650 -key A.key -out A.crt \
	-subj "/CN=MCUboot sample root"

# Generate an intermediate cert
# openssl genrsa -out B.key 4096
openssl ecparam -name $curve -genkey -out B.key

openssl req -new -key B.key -out B.csr \
	-subj "/CN=MCUboot sample intermediate"

	#-addext basicConstraints=CA:TRUE \

echo "basicConstraints=CA:TRUE" > cansign$$.ext
echo "subjectKeyIdentifier=hash" >> cansign$$.ext
echo "authorityKeyIdentifier=keyid,issuer" >> cansign$$.ext

# Sign B with A.  The extension is needed so that clients will be able
# to trust that B is also able to sign certificates.
openssl x509 -req -days 3650 -in B.csr -CA A.crt -CAkey A.key \
	-CAcreateserial \
	-extfile cansign$$.ext \
	-out B.crt

# Now lets generate a certificate we can use to sign images.
# openssl genrsa -out C.key 2048
# openssl ecparam -name $curve -genkey -out C.key
openssl req -new -key ../root-ec-p256.pem -out C.csr \
	-subj "/CN=MCUboot sample signing key"

# Create a config snippet that includes the required extensions.
echo "subjectKeyIdentifier=hash" > exts$$.ext
echo "authorityKeyIdentifier=keyid,issuer" >> exts$$.ext

openssl x509 -req -days 3650 -in C.csr -CA B.crt -CAkey B.key \
	-set_serial 101 \
	-extfile exts$$.ext \
	-out C.crt

# Remove the certificate request files, as they aren't particularly
# useful.
rm B.csr C.csr cansign$$.ext exts$$.ext
