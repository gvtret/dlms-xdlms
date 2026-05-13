# dlms-xdlms Test Plan

## 1. Unit Tests

Required first implementation tests:

- default service options are confirmed and normal priority;
- logical-name construction stores exactly six bytes;
- invalid logical-name input is rejected;
- invoke-id allocator emits stable 4-bit invoke ids;
- GET requires associated association state;
- GET encodes a GET-REQUEST-NORMAL with descriptor fields;
- GET send failure maps to `SendFailed`;
- GET receive failure maps to `ReceiveFailed`;
- malformed response maps to `DecodeFailed`;
- non-GET response maps to `DecodeFailed`;
- invoke-id mismatch maps to `InvokeIdMismatch`;
- GET-RESPONSE-NORMAL data copies encoded data into `GetResult`;
- GET-RESPONSE-NORMAL data-access-result maps to `ServiceRejected`;
- block-transfer response maps to `BlockTransferRequired`.

## 2. Integration Tests

Root integration shall later verify:

- open no-security LN association with `dlms-association`;
- perform normal GET over a fake or loopback Wrapper/TCP APDU channel;
- receive a synthetic GET-RESPONSE-NORMAL and expose result bytes.

No server-side COSEM object model is required for the first unit phase.

## 3. Server-Side Normal GET Tests

Required server boundary tests:

- default GET indication carries confirmed normal-priority options;
- invalid invoke id maps to `InvalidArgument`;
- invalid logical-name descriptor maps to `InvalidArgument`;
- dispatcher forwards descriptor and service options to the handler;
- successful handler data response preserves request invoke id;
- handler data-access-result response preserves request invoke id;
- handler failure status is propagated without mutating unrelated result data.

## 4. Server-Side APDU GET Tests

Required APDU boundary tests:

- GET-REQUEST-NORMAL decodes to a `GetIndication`;
- successful handler data response encodes GET-RESPONSE-NORMAL;
- data-access-result response encodes GET-RESPONSE-NORMAL access result;
- response invoke id and priority mirror the request;
- unconfirmed GET request maps to `UnsupportedFeature`;
- malformed APDU maps to `DecodeFailed`;
- non-GET APDU maps to `UnsupportedFeature`;
- GET-NEXT, GET-WITH-LIST, and selective access map to `UnsupportedFeature`;
- handler failure leaves response output empty.

## 5. Server-Side Normal SET Tests

Required server boundary tests:

- default SET indication carries confirmed normal-priority options;
- invalid invoke id maps to `InvalidArgument`;
- invalid logical-name descriptor maps to `InvalidArgument`;
- empty SET value bytes map to `InvalidArgument`;
- dispatcher forwards descriptor, service options, and encoded value bytes to
  the handler;
- successful handler result preserves request invoke id;
- handler data-access-result preserves request invoke id;
- default GET-only handler SET support maps to `UnsupportedFeature`;
- handler failure status is propagated without mutating unrelated result data.

## 6. Server-Side APDU SET Tests

Required APDU boundary tests:

- SET-REQUEST-NORMAL decodes to a `SetIndication`;
- successful handler result encodes SET-RESPONSE-NORMAL success;
- access-result response encodes SET-RESPONSE-NORMAL access result;
- response invoke id and priority mirror the request;
- encoded request value bytes are preserved in `SetIndication::data`;
- unconfirmed SET request maps to `UnsupportedFeature`;
- malformed APDU maps to `DecodeFailed`;
- GET and ACTION APDUs that are not otherwise supported map to
  `UnsupportedFeature`;
- SET-WITH-FIRST-DATABLOCK, SET-WITH-DATABLOCK, SET-WITH-LIST,
  SET-WITH-LIST-AND-FIRST-DATABLOCK, and selective access map to
  `UnsupportedFeature`;
- handler failure leaves response output empty.

## 7. Security APDU Boundary Tests

Required client tests:

- no-security client GET/SET/ACTION behavior remains unchanged;
- secure client sends a ciphered request APDU instead of the raw service APDU;
- secure client unprotects a ciphered response before normal service decode;
- request protection failure maps to `SecurityFailed`;
- response unprotection or authentication failure maps to `SecurityFailed`.

Required server APDU tests:

- no-security processor keeps existing unprotected GET/SET/ACTION behavior;
- secure processor unprotects a ciphered request before dispatch;
- secure processor protects the encoded response before returning it;
- malformed or unauthenticated ciphered request maps to `SecurityFailed`;
- response protection failure maps to `SecurityFailed`.

Required integration tests:

- a protected client request can be unprotected by a server processor configured
  with the peer system title and shared global keys;
- the server response can be unprotected by the client processor and decoded as
  the original xDLMS service response;
- invocation counters advance monotonically on both peers.

## 8. Client GET Block Transfer Tests

Required client tests:

- normal GET response still returns one-block data unchanged;
- first datablock plus final datablock returns concatenated data;
- generated `GET-REQUEST-NEXT` carries the latest received block number;
- send failure during `GET-REQUEST-NEXT` maps to `SendFailed`;
- receive failure during a following datablock maps to `ReceiveFailed`;
- malformed datablock maps to `DecodeFailed`;
- repeated or skipped block number maps to `DecodeFailed`;
- disabled automatic block transfer maps the first datablock to
  `BlockTransferRequired`;
- secure client protects every `GET-REQUEST-NEXT` and unprotects every
  datablock response.

## 9. Client SET Block Transfer Tests

Required client tests:

- normal SET response still uses the existing single-APDU path;
- blocked SET sends first and final blocks with the same invoke id and
  priority;
- non-final ACK block number must match the just-sent block;
- final last-datablock response fills `SetResult`;
- final non-zero access result maps to `ServiceRejected`;
- disabled block transfer maps oversized SET data to `BlockTransferRequired`;
- send and receive failures during any block map to existing statuses;
- secure client protects every SET block request and unprotects every SET block
  response.

## 10. Verification Commands

Use the existing workspace build directory when available:

```text
cmake --build <build-dir>
ctest --test-dir <build-dir> --output-on-failure
```

Targeted checks after phase 1:

```text
cmake --build <build-dir> --target dlms_xdlms_tests
ctest --test-dir <build-dir> -R Xdlms --output-on-failure
```
