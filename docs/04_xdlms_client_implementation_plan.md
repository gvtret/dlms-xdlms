# dlms-xdlms-client Implementation Plan

## Phase 0. Documentation

Deliverables:

- requirements
- API contract
- architecture diagrams
- test plan
- implementation plan

Commit message:

```text
docs(xdlms-client): define normal get client layer
```

## Phase 1. Normal GET Types And Test Harness

Deliverables:

- CMake project and GoogleTest harness
- `XdlmsClientStatus`
- `CosemLogicalName`
- `CosemAttributeDescriptor`
- `ServiceOptions`
- `GetResult`
- `InvokeIdAllocator`
- tests for type defaults, validation, and invoke-id allocation

Commit message:

```text
feat(xdlms-client): add normal get service types
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
feat(xdlms-client): implement normal get flow
```

## Phase 3. Root Integration

Deliverables:

- root CMake subdirectory wiring
- root git submodule entry
- cross-layer test for no-security association plus normal GET over fake APDU
  channel

Commit message:

```text
build: add dlms-xdlms-client submodule
```
