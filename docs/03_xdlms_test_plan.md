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

## 3. Verification Commands

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
