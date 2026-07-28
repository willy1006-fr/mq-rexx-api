# IBM MQ Rexx API (formerly SupportPac MA95)

This repository contains a Rexx interface to IBM MQ. The project was previously distributed as IBM SupportPac MA95.

## Message handles and message properties

This branch adds Rexx interfaces for IBM MQ message handles and message properties:

- `RXMQMH` — create a message handle (`MQCRTMH`)
- `RXMQDMH` — delete a message handle (`MQDLTMH`)
- `RXMQSMP` — set a message property (`MQSETMP`)
- `RXMQIMP` — inquire a message property (`MQINQMP`)
- `RXMQDMP` — delete a message property (`MQDLTMP`)
- `RXMQMBF` — convert a message handle to a buffer (`MQMHBUF`)
- `RXMQBMH` — convert a buffer to a message handle (`MQBUFMH`)

The implementation also supports Rexx stems corresponding to `MQCMHO`, `MQDMHO`, `MQSMPO`, `MQPD`, `MQIMPO`, `MQDMPO`, `MQMHBO`, and `MQBMHO`.

## Samples

`HANDLE1.CLIST` through `HANDLE10.CLIST` demonstrate property put/get, enumeration, data types, property copying, deletion, MQRFH2 conversion, user context, and message selection. The samples use generic values such as `QM1` and `DEV.QUEUE.1`; adjust them for your installation.

## Documentation

The updated guide is available in `doc/MA95_v1.1_User_Guide.pdf`. The editable Word source is also included.

## Platform status

The message-handle and message-property extensions have been tested on **z/OS with IBM MQ**. They have **not been tested on Windows**, and no claim of Windows compatibility is made for this version. Existing Windows files from the upstream project are retained unchanged.

## License

This project is licensed under the Eclipse Public License 1.0. See `LICENSE`. Existing copyright and attribution notices are retained.

## Project status

This branch is a community contribution based on the IBM Messaging `mq-rexx-api` repository. It is not an official IBM product release and is not supported or warranted by IBM.
