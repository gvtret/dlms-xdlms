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
- malformed APDU maps to `DecodeFailed`;
- non-GET APDU maps to `UnsupportedFeature`;
- GET-NEXT, GET-WITH-LIST, and selective access map to `UnsupportedFeature`;
- handler failure leaves response output empty.

## 5. Verification Commands

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
