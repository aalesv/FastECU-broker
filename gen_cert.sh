#!/bin/sh
openssl req -x509 -nodes -newkey rsa:4096 -keyout localhost.key -out localhost.cert -sha512 -days 365 -subj "/C=/ST=/L=/O=FastEcu Broker/OU=/CN=localhost"
