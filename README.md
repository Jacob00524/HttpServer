# Simple C Http Server
This project is a fun and simple way to make an http server completely with C.

## Building
Debug:
```bash
make MODE=debug
```
Release:
```bash
make
```

## Running
When the project is built, it will also build example.c -> example.out. See example.c to see how you can get up and running really quickly.
The first time example.out is run it will create the servers file structure and a server_settings.json which can be used to set the address, port and various paths.

## License
This project is licensed under the GNU Lesser General Public License v3.0 (LGPL-3.0-or-later).

You may link against this library in proprietary software, but any modifications to the library itself must be released under the same license.
