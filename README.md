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
- [x] Authentication and authorization architecture for uploading files to webroot
- [x] CSRF protection for sign up, file modification forms and login forms
- [ ] Storing users and permissions to SQLITE database
- [ ] Using a basic config file for
    - [ ] admin username, password
    - [ ] webroot location
    - [ ] SSl certificate 
    - [ ] private keys for tokens
    - [ ] number of workers
- [ ] Supported for Ranged HTTP requests needed for video streaming.
- [ ] .ACME authentication for certificate renewal 
- [ ] Support for HTTP caching 
    - [ ] Usinge Modified Date
- [ ] Frontend
    - [ ] Login, sign up
    - [ ] Manage users
        - [ ] add user
        - [ ] delete user
    - [ ] View folder contents
    - [ ] modify folder contents

- [ ] Pass unit tests  
    - [ ] Test against directory traversal
    - [ ] Test against buffer overflows
        - [ ] HTTP status line
        - [ ] Parsing Headers
            - [ ] Range
            - [ ] Accept
            - [ ] Accept encoding
            - [ ] TE
        - [ ] Content-Length
    - [ ] Test against SQL Injections
    - [ ] Test correcto parsing

## Building

```{bash}
cmake -B build
```

# Libraries

- Collections-C : https://github.com/srdja/Collections-C 
- Sodium : https://github.com/jedisct1/libsodium
