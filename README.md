# dlms-xdlms-client

`dlms-xdlms-client` will provide high-level client-side xDLMS service calls
over an already established DLMS/COSEM association.

The first implementation target is a normal LN GET request/response flow:

- require an established `dlms-association` client;
- build a GET-REQUEST-NORMAL APDU through `dlms-apdu`;
- send and receive APDUs through the association/profile channel boundary;
- decode GET-RESPONSE-NORMAL into caller-owned result data;
- keep block transfer, SET, ACTION, ciphering, and public facade ownership out
  of the first phase.

## Build

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Documentation

- [Requirements](docs/00_xdlms_client_requirements.md)
- [API](docs/01_xdlms_client_api.md)
- [Test Plan](docs/03_xdlms_client_test_plan.md)
- [Architecture](docs/architecture.md)
- [Implementation Plan](docs/04_xdlms_client_implementation_plan.md)
