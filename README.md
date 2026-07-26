# SimpleWebServ

A simple web server made (mostly, with the help of some libraries) from scratch in C.

## Capabilities

- [ ] HTTPS support
- [x] Support chunked, gzip, defalte transfer codings for sending and uploading data
- [x] Support for content negotiation via
    - [x] MIME types
    - [x] content-encoding
    - [x] method 
- [x] Server static files from webroot
- [x] Upload and remove files from webroot
- [ ] Authentication and authorization for uploading files to webroot
- [ ] .ACME authentication for certificate renewal 
- [ ] Support for HTTP caching 

## Building

```{bash}
cmake -B build
```

# Libraries

- Collections-C : https://github.com/srdja/Collections-C 
