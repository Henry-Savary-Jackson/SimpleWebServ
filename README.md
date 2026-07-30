# SimpleWebServ

A simple web server made (mostly, with the help of some libraries) from scratch in C.

## Capabilities

- [ ] HTTPS support
- [x] Support chunked, gzip, deflate transfer-encodings for sending and uploading data
- [x] Support for content negotiation via
    - [x] MIME types
    - [x] content-encoding
    - [x] method 
- [x] Server static files from webroot
- [x] Upload and remove files from webroot
- [ ] Authentication and authorization for uploading files to webroot
- [x] CSRF protection for sign up, file modification forms and login forms
- [ ] Supported for Ranged HTTP requests needed for video streaming.
- [ ] .ACME authentication for certificate renewal 
- [ ] Support for HTTP caching 

## Building

```{bash}
cmake -B build
```

# Libraries

- Collections-C : https://github.com/srdja/Collections-C 
