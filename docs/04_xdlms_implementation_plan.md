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

## Phase 7. Server APDU GET Documentation

Deliverables:

- APDU-level server GET requirements
- `XdlmsServerApduProcessor` API contract
- APDU decode/dispatch/encode sequence diagram
- APDU boundary test plan
- implementation phase split

Commit message:

```text
docs(xdlms): define server get apdu boundary
```

## Phase 8. Server APDU GET Processor

Deliverables:

- `XdlmsServerApduProcessor`
- GET-REQUEST-NORMAL decode to `GetIndication`
- GET-RESPONSE-NORMAL encode from `GetResult`
- rejection for unsupported APDU shapes
- tests for success, access result, malformed input, unsupported services, and
  handler failures

Commit message:

```text
feat(xdlms): process server get apdus
```

## Phase 9. Root Integration Update

Deliverables:

- root submodule pointer update
- full workspace build and test run
- commit message for the root repo

Commit message:

```text
build: update dlms-xdlms server apdu boundary
```

## Phase 10. Server Normal SET Documentation

Deliverables:

- server-side normal SET requirements
- `SetIndication` and `SetResult` API contract
- server SET dispatch sequence diagram
- server SET boundary test plan
- implementation phase split

Commit message:

```text
docs(xdlms): define server normal set boundary
```

## Phase 11. Server Normal SET Contract

Deliverables:

- `SetIndication`
- `SetResult`
- default `IXdlmsServerHandler::HandleSet` unsupported implementation
- `XdlmsServerDispatcher::DispatchSet`
- validation for invoke id, attribute descriptor, and value bytes
- tests for success, data-access-result, invalid input, default unsupported
  handler, and handler failure

Commit message:

```text
feat(xdlms): add server normal set dispatcher
```

## Phase 12. Root Integration Update

Deliverables:

- root submodule pointer update
- full workspace build and test run
- commit message for the root repo

Commit message:

```text
build: update dlms-xdlms set boundary
```

## Phase 13. Server APDU SET Documentation

Deliverables:

- APDU-level server SET requirements
- `XdlmsServerApduProcessor` API contract update
- APDU decode/dispatch/encode sequence diagram
- APDU SET boundary test plan
- implementation phase split

Commit message:

```text
docs(xdlms): define server set apdu boundary
```

## Phase 14. Server APDU SET Processor

Deliverables:

- SET-REQUEST-NORMAL decode to `SetIndication`
- encoded `Data` byte preservation for `SetIndication::data`
- SET-RESPONSE-NORMAL encode from `SetResult`
- rejection for unsupported SET APDU shapes
- tests for success, access result, malformed input, unsupported services,
  value forwarding, and handler failures

Commit message:

```text
feat(xdlms): process server set apdus
```

## Phase 15. Root Integration Update

Deliverables:

- root submodule pointer update
- full workspace build and test run
- commit message for the root repo

Commit message:

```text
build: update dlms-xdlms server set apdu boundary
```

## Phase 16. Server ACTION Documentation

Deliverables:

- server-side ACTION APDU requirements
- `ActionIndication` API contract
- server ACTION dispatch sequence diagram
- server ACTION boundary test plan

Commit message:

```text
docs(xdlms): define server action apdu boundary
```

## Phase 17. Server ACTION APDU Processor

Deliverables:

- ACTION-REQUEST-NORMAL decode to `ActionIndication`
- encoded invocation parameter preservation
- ACTION-RESPONSE-NORMAL encode from `ActionResult`
- rejection for unsupported ACTION APDU shapes
- tests for success, action result, return data, no parameter, malformed input,
  unsupported services, and handler failures

Commit message:

```text
feat(xdlms): add server action apdu boundary
```

## Phase 18. Root Integration Update

Deliverables:

- root submodule pointer update
- full workspace build and test run
- root integration tests for ACTION APDU reaching a COSEM object method

Commit message:

```text
test: cover server action apdu integration
```

## Phase 19. Client SET/ACTION Documentation

Deliverables:

- client-side SET/ACTION requirements
- `XdlmsClient::Set` and `XdlmsClient::Action` API contract
- client SET/ACTION architecture and sequence diagrams
- client SET/ACTION test plan

Commit message:

```text
docs(xdlms): define client set action boundary
```

## Phase 20. Client SET/ACTION Flow

Deliverables:

- confirmed `SET-REQUEST-NORMAL` encode and `SET-RESPONSE-NORMAL` decode
- confirmed `ACTION-REQUEST-NORMAL` encode and `ACTION-RESPONSE-NORMAL` decode
- encoded DLMS `Data` byte preservation for SET values, ACTION parameters, and
  ACTION return parameters
- fake-channel tests for success and error mappings

Commit message:

```text
feat(xdlms): implement client set action flow
```

## Phase 21. Security APDU Boundary Documentation

Deliverables:

- requirements for optional `dlms-security` use at the xDLMS APDU boundary
- `XdlmsClient` and `XdlmsServerApduProcessor` security constructor contract
- architecture and sequence diagrams for protect/unprotect flow
- security APDU boundary test plan

Commit message:

```text
docs(xdlms): define security apdu boundary
```

## Phase 22. Security APDU Boundary Implementation

Deliverables:

- optional `CipheredApduProcessor` reference in `XdlmsClient`
- optional `CipheredApduProcessor` reference in `XdlmsServerApduProcessor`
- request protect and response unprotect flow for client GET/SET/ACTION
- request unprotect and response protect flow for server APDU processing
- `SecurityFailed` xDLMS status mapping
- focused tests for protected request/response and authentication failures

Commit message:

```text
feat(xdlms): add security apdu boundary
```

## Phase 23. Block Transfer Documentation

Deliverables:

- client GET block-transfer requirements
- API contract for block-transfer service options
- architecture and sequence diagrams
- focused unit test plan
- split between GET, SET, and ACTION block-transfer phases

Commit message:

```text
docs(xdlms): define get block transfer
```

## Phase 24. Client GET Block Transfer

Deliverables:

- GET response `BlockTransferManager`
- GET-RESPONSE-WITH-DATABLOCK handling in `XdlmsClient`
- GET-REQUEST-NEXT encode loop
- block-number validation and collected-size guard
- focused client tests, including secure block APDU boundaries

Commit message:

```text
feat(xdlms): handle get response blocks
```

## Phase 25. Root Integration Update

Deliverables:

- root submodule pointer update
- root integration test for multi-block GET over a fake APDU channel
- full root build and test run

Commit message:

```text
test: cover xdlms get block transfer
```

## Phase 26. SET Block Transfer Documentation

Deliverables:

- client SET block-transfer requirements
- API contract for SET block service options
- architecture and sequence diagrams
- focused unit and root test plan

Commit message:

```text
docs(xdlms): define set block transfer
```

## Phase 27. Client SET Block Transfer

Deliverables:

- options-aware `XdlmsClient::Set` overload
- SET first-block and following-block request generation
- ACK and final-response validation
- focused unit tests, including error mappings

Commit message:

```text
feat(xdlms): send set request blocks
```

## Phase 28. Root Integration Update

Deliverables:

- root submodule pointer update
- root integration test for multi-block SET over a fake APDU channel
- full root build and test run

Commit message:

```text
test: cover xdlms set block transfer
```

## Phase 29. ACTION Block Transfer Documentation

Deliverables:

- ACTION block-transfer requirements
- API contract for future ACTION block service options
- architecture and sequence diagrams
- response-side and request-side phase split

Commit message:

```text
docs(xdlms): define action block transfer
```

## Phase 30. Client ACTION Response Blocks

Deliverables:

- options-aware `XdlmsClient::Action` overload
- ACTION-RESPONSE-WITH-PBLOCK collection
- ACTION-REQUEST-NEXT-PBLOCK encode loop
- normal single-method action response payload decode
- focused client unit tests

Commit message:

```text
feat(xdlms): handle action response blocks
```

## Phase 31. Root Integration Update

Deliverables:

- root submodule pointer update
- root integration test for multi-block ACTION response over a fake APDU
  channel
- full root build and test run

Commit message:

```text
test: cover xdlms action response block transfer
```

## Phase 32. ACTION Request Block Documentation

Deliverables:

- ACTION request-side block-transfer requirements
- API contract for `maxActionBlockPayloadBytes`
- request pblock architecture and sequence diagram
- focused unit and root integration test plan

Commit message:

```text
docs(xdlms): define action request blocks
```

## Phase 33. Client ACTION Request Blocks

Deliverables:

- `maxActionBlockPayloadBytes` in `ServiceOptions`
- ACTION first-pblock and following-pblock request generation
- `ACTION-RESPONSE-NEXT-PBLOCK` ACK validation
- final normal ACTION response handling
- final response pblock handling through the existing response collector
- focused client unit tests

Commit message:

```text
feat(xdlms): send action request blocks
```

## Phase 34. Root Integration Update

Deliverables:

- root submodule pointer update
- root integration test for multi-block ACTION request over a fake APDU channel
- full root build and test run

Commit message:

```text
test: cover xdlms action request blocks
```

## Phase 35. Server ACTION Request Block Documentation

Deliverables:

- server-side ACTION request block reassembly requirements
- `XdlmsServerApduProcessor` state ownership contract
- request block state architecture and sequence diagram
- focused unit and root integration test plan

Commit message:

```text
docs(xdlms): define server action request blocks
```

## Phase 36. Server ACTION Request Block Reassembly

Deliverables:

- processor-local ACTION request block state
- `ACTION-REQUEST-WITH-FIRST-PBLOCK` handling
- `ACTION-REQUEST-WITH-PBLOCK` handling
- `ACTION-RESPONSE-NEXT-PBLOCK` acknowledgements
- final dispatch through existing `DispatchAction`
- focused server APDU tests

Commit message:

```text
feat(xdlms): reassemble server action request blocks
```

## Phase 37. Root Integration Update

Deliverables:

- root submodule pointer update
- root integration test for client ACTION request blocks reaching server APDU
  processing
- full root build and test run

Commit message:

```text
test: cover server action request block integration
```
