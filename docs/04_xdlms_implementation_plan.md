# dlms-xdlms Implementation Plan

## Phase 0. Documentation

Deliverables:

- requirements
- API contract
- architecture diagrams
- test plan
- implementation plan

Commit message:

```text
docs(xdlms): define normal get client layer
```

The first implementation remains client-side normal GET. Later phases will add
SET, ACTION, block transfer, and server-side dispatch contracts in the same
`dlms-xdlms` repo.

## Phase 1. Normal GET Types And Test Harness

Deliverables:

- CMake project and GoogleTest harness
- `XdlmsStatus`
- `CosemLogicalName`
- `CosemAttributeDescriptor`
- `ServiceOptions`
- `GetResult`
- `InvokeIdAllocator`
- tests for type defaults, validation, and invoke-id allocation

Commit message:

```text
feat(xdlms): add normal get service types
```

## Phase 2. Normal GET Request/Response

Deliverables:

- `XdlmsClient`
- GET-REQUEST-NORMAL encode through `dlms-apdu`
- GET-RESPONSE-NORMAL decode through `dlms-apdu`
- fake-channel tests for success, send failure, receive failure, decode
  failure, invoke-id mismatch, service rejection, and block-transfer required

Commit message:

```text
feat(xdlms): implement normal get flow
```

## Phase 3. Root Integration

Deliverables:

- root CMake subdirectory wiring
- root git submodule entry
- cross-layer test for no-security association plus normal GET over fake APDU
  channel

Commit message:

```text
build: add dlms-xdlms submodule
```

## Phase 4. Server Normal GET Documentation

Deliverables:

- server-side normal GET requirements
- `xdlms_server.hpp` API contract
- server dispatcher architecture diagram
- server boundary test plan
- implementation phase split

Commit message:

```text
docs(xdlms): define server normal get boundary
```

## Phase 5. Server Normal GET Contract

Deliverables:

- `GetIndication`
- `IXdlmsServerHandler`
- `XdlmsServerDispatcher`
- validation for invoke id and attribute descriptor
- tests for success, data-access-result, invalid input, and handler failure

Commit message:

```text
feat(xdlms): add server normal get dispatcher
```

## Phase 6. Root Integration Update

Deliverables:

- root submodule pointer update
- full workspace build and test run
- commit message for the root repo

Commit message:

```text
build: update dlms-xdlms server boundary
```
